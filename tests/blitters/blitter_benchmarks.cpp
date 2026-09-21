#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "BlitterBackend.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr std::size_t GuardPixels = 64;
constexpr std::size_t BatchSize = 8;
constexpr UINT16 IncomingZ = 100;
constexpr UINT16 ColorGuard = 0xa55a;
constexpr UINT16 DepthGuard = 0x5aa5;
volatile std::uint64_t observedResults = 0;

struct Options
{
	unsigned samples = 7;
	double batchMilliseconds = 5;
	double maxRatio = 0;
	std::string baseline = "asm";
	std::string filter;
	std::string csvPath;
	bool verifyOnly = false;
	bool list = false;
};

void usage()
{
	std::puts(
		"Usage: ja2_blitter_benchmarks [options]\n"
		"  --baseline NAME     Reference backend: asm or generic (default asm)\n"
		"  --verify-only       Compare results and guarded buffers without timing\n"
		"  --list              List matching workload names without loading DLLs\n"
		"  --filter TEXT       Run only names containing TEXT (no matches is an error)\n"
		"  --samples N         Paired samples per workload, 3..101 (default 7)\n"
		"  --batch-ms MS       Minimum timed work per backend/sample, 1..1000 (default 5)\n"
		"  --max-ratio R       Fail if median portable/baseline time exceeds R\n"
		"  --csv FILE          Write CSV results to FILE instead of stdout\n"
		"  --help              Show this help\n\n"
		"CSV results go to stdout; provenance and diagnostics go to stderr.\n"
		"Without --max-ratio, timing is report-only; correctness failures still fail.\n"
		"Exit codes: 0 success, 1 timing regression, 2 correctness failure, 3 setup/usage error.\n"
		"The selected baseline and portable DLLs load from the executable directory.\n"
		"asm uses the original assembly; generic uses the unchanged portable oracle.\n"
		"Use optimized x86 builds. CSV baseline identifies the reference for each row.\n"
		"Each timed batch draws once into each of eight freshly reset buffers. Reset,\n"
		"allocation and verification are outside the timer. Samples alternate backend\n"
		"order. Times include the common batch loop, dispatch and timer overhead.\n"
		"These are synthetic, warmed-buffer microbenchmarks, not full-game frame times.");
}

double number(const char* text)
{
	char* end = nullptr;
	const double value = std::strtod(text, &end);
	if (end == text || *end != '\0' || !std::isfinite(value) || value <= 0)
	{
		throw std::runtime_error(std::string("Invalid positive number: ") + text);
	}
	return value;
}

Options parseOptions(int argc, char** argv)
{
	Options options;
	for (int i = 1; i < argc; ++i)
	{
		const std::string argument = argv[i];
		if (argument == "--help")
		{
			usage();
			std::exit(0);
		}
		if (argument == "--verify-only") options.verifyOnly = true;
		else if (argument == "--list") options.list = true;
		else if (argument == "--filter" || argument == "--csv" || argument == "--samples" ||
			argument == "--batch-ms" || argument == "--max-ratio" || argument == "--baseline")
		{
			if (++i == argc) throw std::runtime_error("Missing value for " + argument);
			if (argument == "--filter") options.filter = argv[i];
			else if (argument == "--csv") options.csvPath = argv[i];
			else if (argument == "--baseline")
			{
				options.baseline = argv[i];
				if (options.baseline != "asm" && options.baseline != "generic")
					throw std::runtime_error("--baseline must be asm or generic");
			}
			else if (argument == "--max-ratio") options.maxRatio = number(argv[i]);
			else if (argument == "--batch-ms")
			{
				options.batchMilliseconds = number(argv[i]);
				if (options.batchMilliseconds < 1 || options.batchMilliseconds > 1000)
					throw std::runtime_error("--batch-ms must be in 1..1000");
			}
			else
			{
				const double samples = number(argv[i]);
				if (samples < 3 || samples > 101 || samples != std::floor(samples))
					throw std::runtime_error("--samples must be an integer in 3..101");
				options.samples = static_cast<unsigned>(samples);
			}
		}
		else throw std::runtime_error("Unknown option: " + argument);
	}
	if (options.verifyOnly && options.maxRatio != 0)
		throw std::runtime_error("--verify-only cannot enforce --max-ratio");
	if (options.list && (!options.csvPath.empty() || options.maxRatio != 0 || options.verifyOnly))
		throw std::runtime_error("--list cannot be combined with verification, CSV output or timing gates");
	return options;
}

class BackendLibrary
{
public:
	explicit BackendLibrary(const wchar_t* filename)
	{
		std::array<wchar_t, 32768> executable{};
		const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
			static_cast<DWORD>(executable.size()));
		if (length == 0 || length >= executable.size())
			throw std::runtime_error("Cannot locate executable directory");
		std::wstring path(executable.data(), length);
		const auto separator = path.find_last_of(L"\\/");
		if (separator == std::wstring::npos)
			throw std::runtime_error("Executable path has no directory");
		path.resize(separator + 1);
		path += filename;
		module = LoadLibraryW(path.c_str());
		if (!module)
			throw std::runtime_error("Cannot load backend DLL; Windows error " +
				std::to_string(GetLastError()));
		const auto getBackend = reinterpret_cast<const BlitterBackend* (*)()>(
			GetProcAddress(module, "GetBlitterBackend"));
		if (!getBackend)
		{
			FreeLibrary(module);
			module = nullptr;
			throw std::runtime_error("Backend DLL has no GetBlitterBackend export");
		}
		api = getBackend();
		if (!api)
		{
			FreeLibrary(module);
			module = nullptr;
			throw std::runtime_error("Backend returned a null API");
		}
	}

	~BackendLibrary() { if (module) FreeLibrary(module); }
	BackendLibrary(const BackendLibrary&) = delete;
	BackendLibrary& operator=(const BackendLibrary&) = delete;
	const BlitterBackend& get() const { return *api; }

private:
	HMODULE module = nullptr;
	const BlitterBackend* api = nullptr;
};

enum class Operation
{
	Transparent, TransparentClip, TransZ, TransZNB, TransZClip, TransZNBClip,
	TransShadow, TransShadowClip, TransShadowZ, TransShadowZNB, ObscuredShadow, ShadowAlpha,
	Shadow, ShadowClip, ShadowZ, Intensity, IntensityZ, Outline, OutlineClip, OutlineZ,
	OutlineShadow, Pixelate, PixelateObscured, Translucent, MonoClip,
	Copy16, Fill16, Hatch, ShadeRect,
};

struct BlitterCase
{
	const char* name;
	Operation operation;
	bool depth;
	bool clipped;
	bool raw;
};

constexpr BlitterCase Blitters[] =
{
	{"transparent", Operation::Transparent, false, false, false},
	{"transparent_clip", Operation::TransparentClip, false, true, false},
	{"trans_z", Operation::TransZ, true, false, false},
	{"trans_znb", Operation::TransZNB, true, false, false},
	{"trans_z_clip", Operation::TransZClip, true, true, false},
	{"trans_znb_clip", Operation::TransZNBClip, true, true, false},
	{"trans_shadow", Operation::TransShadow, false, false, false},
	{"trans_shadow_clip", Operation::TransShadowClip, false, true, false},
	{"trans_shadow_z", Operation::TransShadowZ, true, false, false},
	{"trans_shadow_znb", Operation::TransShadowZNB, true, false, false},
	{"obscured_shadow", Operation::ObscuredShadow, true, false, false},
	{"shadow_alpha", Operation::ShadowAlpha, false, false, false},
	{"shadow", Operation::Shadow, false, false, false},
	{"shadow_clip", Operation::ShadowClip, false, true, false},
	{"shadow_z", Operation::ShadowZ, true, false, false},
	{"intensity", Operation::Intensity, false, false, false},
	{"intensity_z", Operation::IntensityZ, true, false, false},
	{"outline", Operation::Outline, false, false, false},
	{"outline_clip", Operation::OutlineClip, false, true, false},
	{"outline_z", Operation::OutlineZ, true, false, false},
	{"outline_shadow", Operation::OutlineShadow, false, false, false},
	{"pixelate", Operation::Pixelate, true, false, false},
	{"pixelate_obscured", Operation::PixelateObscured, true, false, false},
	{"translucent", Operation::Translucent, true, false, false},
	{"mono_clip", Operation::MonoClip, false, true, false},
	{"copy16", Operation::Copy16, false, false, true},
	{"fill16", Operation::Fill16, false, false, true},
	{"hatch", Operation::Hatch, false, false, true},
	{"shade_rect", Operation::ShadeRect, false, false, true},
};

enum class Runs { Dense, Sparse, Fragmented };
enum class Depth { Visible, Equal, Hidden, Mixed };

struct Workload
{
	const char* name;
	UINT16 width;
	UINT16 height;
	Runs runs = Runs::Dense;
	Depth depth = Depth::Visible;
	bool clipEdges = false;
};

std::vector<Workload> workloads(const BlitterCase& blitter)
{
	if (blitter.raw) return {{"small_rect", 64, 64}, {"frame", 640, 480}};
	std::vector<Workload> result{
		{"tiny_dense", 8, 8}, {"sprite_dense", 64, 64},
		{"sprite_sparse", 64, 64, Runs::Sparse},
		{"sprite_fragmented", 64, 64, Runs::Fragmented}, {"large_dense", 256, 128},
	};
	if (blitter.depth)
	{
		result.push_back({"equal_z", 64, 64, Runs::Dense, Depth::Equal});
		result.push_back({"hidden_z", 64, 64, Runs::Dense, Depth::Hidden});
		result.push_back({"mixed_z", 64, 64, Runs::Dense, Depth::Mixed});
	}
	if (blitter.clipped)
		result.push_back({"clip_edges", 64, 64, Runs::Sparse, Depth::Mixed, true});
	return result;
}

struct Fixture
{
	Workload workload;
	UINT16 surfaceWidth;
	UINT16 surfaceHeight;
	UINT32 pitchPixels;
	UINT32 pitchBytes;
	INT32 x;
	INT32 y;
	SGPRect clip;
	SGPRect area;
	std::array<UINT16, 256> palette{};
	std::vector<UINT8> source;
	std::vector<UINT8> alpha;
	std::vector<UINT16> rawSource;
	std::vector<UINT16> initialColor;
	std::vector<UINT16> initialDepth;
	ETRLEObject frame{};
	SGPVObject object{};
	SGPVObject alphaObject{};
	std::size_t opaquePixels = 0;

	explicit Fixture(Workload input)
		: workload(input), surfaceWidth(input.width + 16), surfaceHeight(input.height + 16),
		  // Keep at least eight padding pixels per row so row overruns cannot hide in alignment.
		  pitchPixels(((surfaceWidth + 7u) & ~7u) + 8u), pitchBytes(pitchPixels * sizeof(UINT16)),
		  x(input.clipEdges ? -static_cast<INT32>(input.width) / 4 : 8),
		  y(input.clipEdges ? -static_cast<INT32>(input.height) / 4 : 8),
		  clip{0, 0, surfaceWidth, surfaceHeight},
		  // Pattern/shading rectangle APIs use inclusive right and bottom edges.
		  area{x, y, x + input.width - 1, y + input.height - 1}
	{
		if (input.clipEdges) clip = {4, 4, input.width / 2, input.height / 2};
		for (std::size_t i = 0; i < palette.size(); ++i)
			palette[i] = static_cast<UINT16>((i * 251u + 123u) & 0xffffu);
		for (UINT32 row = 0; row < input.height; ++row)
		{
			UINT32 column = 0;
			while (column < input.width)
			{
				const bool skip = transparent(column, row);
				UINT32 count = 1;
				while (count < 127 && column + count < input.width &&
					transparent(column + count, row) == skip) ++count;
				const UINT8 control = static_cast<UINT8>(count | (skip ? 0x80 : 0));
				source.push_back(control);
				alpha.push_back(control);
				if (!skip)
				{
					for (UINT32 pixel = 0; pixel < count; ++pixel)
					{
						const UINT32 position = column + pixel + row * input.width;
						const UINT32 selector = position % 17;
						// The game's STI compressors encode index zero as transparent runs.
						source.push_back(selector == 0 ? 254 : selector == 1 ? 1 :
							static_cast<UINT8>(2 + position % 251));
						constexpr UINT8 alphaLevels[]{0, 64, 128, 192, 255};
						alpha.push_back(alphaLevels[position % 5]);
					}
					opaquePixels += count;
				}
				column += count;
			}
			source.push_back(0);
			alpha.push_back(0);
		}
		frame.uiDataLength = static_cast<UINT32>(source.size());
		frame.usWidth = input.width;
		frame.usHeight = input.height;
		// Allow harmless wide loads at the end of the assembly decoder's input.
		source.resize(source.size() + 64, 0);
		alpha.resize(alpha.size() + 64, 0);
		object.pPixData = source.data();
		object.pETRLEObject = &frame;
		object.pShadeCurrent = palette.data();
		object.p16BPPPalette = palette.data();
		object.usNumberOfObjects = 1;
		object.uiSizePixData = frame.uiDataLength;
		object.ubBitDepth = 8;
		alphaObject = object;
		alphaObject.pPixData = alpha.data();

		const std::size_t pixels = static_cast<std::size_t>(pitchPixels) * surfaceHeight;
		initialColor.resize(pixels + 2 * GuardPixels, ColorGuard);
		initialDepth.resize(pixels + 2 * GuardPixels, DepthGuard);
		rawSource.resize(pixels + 2 * GuardPixels, ColorGuard);
		for (UINT32 row = 0; row < surfaceHeight; ++row)
		{
			for (UINT32 column = 0; column < surfaceWidth; ++column)
			{
				const std::size_t offset = GuardPixels + static_cast<std::size_t>(row) * pitchPixels + column;
				initialColor[offset] = static_cast<UINT16>((row * 997u + column * 67u + 0x1234u) & 0xffffu);
				rawSource[offset] = static_cast<UINT16>((row * 131u + column * 313u + 7u) & 0xffffu);
				const Depth depth = input.depth == Depth::Mixed ?
					static_cast<Depth>((row + column) % 3) : input.depth;
				initialDepth[offset] = depth == Depth::Visible ? IncomingZ - 1 :
					depth == Depth::Equal ? IncomingZ : IncomingZ + 1;
			}
		}
	}

	Fixture(const Fixture&) = delete;
	Fixture& operator=(const Fixture&) = delete;

	bool transparent(UINT32 column, UINT32 row) const
	{
		if (workload.runs == Runs::Sparse) return (column / 8 + row / 8) % 4 != 0;
		if (workload.runs == Runs::Fragmented) return (column + row) % 2 != 0;
		return false;
	}
};

struct Buffers
{
	std::array<std::vector<UINT16>, BatchSize> color;
	std::array<std::vector<UINT16>, BatchSize> depth;
	std::array<BOOLEAN, BatchSize> results{};

	void reset(const Fixture& fixture)
	{
		for (std::size_t i = 0; i < BatchSize; ++i)
		{
			if (color[i].size() != fixture.initialColor.size()) color[i] = fixture.initialColor;
			else std::copy(fixture.initialColor.begin(), fixture.initialColor.end(), color[i].begin());
			if (depth[i].size() != fixture.initialDepth.size()) depth[i] = fixture.initialDepth;
			else std::copy(fixture.initialDepth.begin(), fixture.initialDepth.end(), depth[i].begin());
		}
		results.fill(FALSE);
	}
};

class RegisteredBuffers
{
public:
	RegisteredBuffers(const BlitterBackend& backend, Fixture& fixture, Buffers& buffers)
		: api(backend), source(fixture.rawSource.data() + GuardPixels), buffers(buffers)
	{
		api.registerBuffer(source, fixture.surfaceWidth, fixture.surfaceHeight);
		for (auto& color : buffers.color)
			api.registerBuffer(color.data() + GuardPixels, fixture.surfaceWidth, fixture.surfaceHeight);
	}

	~RegisteredBuffers()
	{
		for (auto& color : buffers.color) api.releaseBuffer(color.data() + GuardPixels);
		api.releaseBuffer(source);
	}
	RegisteredBuffers(const RegisteredBuffers&) = delete;
	RegisteredBuffers& operator=(const RegisteredBuffers&) = delete;

private:
	const BlitterBackend& api;
	UINT16* source;
	Buffers& buffers;
};

void runBatch(const BlitterBackend& api, Operation operation, Fixture& fixture, Buffers& buffers)
{
	// Dispatch once per batch, not once per pixel. Both backends cross the same DLL boundary.
	const auto draw = [&](auto blit)
	{
		for (std::size_t i = 0; i < BatchSize; ++i)
			buffers.results[i] = blit(buffers.color[i].data() + GuardPixels,
				buffers.depth[i].data() + GuardPixels);
	};
	const UINT32 pitch = fixture.pitchBytes;
	HVOBJECT object = &fixture.object;
	const INT32 x = fixture.x;
	const INT32 y = fixture.y;
	SGPRect* clip = &fixture.clip;
	UINT16* palette = fixture.palette.data();
	switch (operation)
	{
		case Operation::Transparent:
			draw([&](UINT16* color, UINT16*) { return api.transparent(color, pitch, object, x, y, 0); }); break;
		case Operation::TransparentClip:
			draw([&](UINT16* color, UINT16*) { return api.transparentClip(color, pitch, object, x, y, 0, clip); }); break;
		case Operation::TransZ:
			draw([&](UINT16* color, UINT16* z) { return api.transZ(color, pitch, z, IncomingZ, object, x, y, 0); }); break;
		case Operation::TransZNB:
			draw([&](UINT16* color, UINT16* z) { return api.transZNB(color, pitch, z, IncomingZ, object, x, y, 0); }); break;
		case Operation::TransZClip:
			draw([&](UINT16* color, UINT16* z) { return api.transZClip(color, pitch, z, IncomingZ, object, x, y, 0, clip); }); break;
		case Operation::TransZNBClip:
			draw([&](UINT16* color, UINT16* z) { return api.transZNBClip(color, pitch, z, IncomingZ, object, x, y, 0, clip); }); break;
		case Operation::TransShadow:
			draw([&](UINT16* color, UINT16*) { return api.transShadow(color, pitch, object, x, y, 0, palette, FALSE); }); break;
		case Operation::TransShadowClip:
			draw([&](UINT16* color, UINT16*) { return api.transShadowClip(color, pitch, object, x, y, 0, clip, palette, FALSE); }); break;
		case Operation::TransShadowZ:
			draw([&](UINT16* color, UINT16* z) { return api.transShadowZ(color, pitch, z, IncomingZ, object, x, y, 0, palette, FALSE); }); break;
		case Operation::TransShadowZNB:
			draw([&](UINT16* color, UINT16* z) { return api.transShadowZNB(color, pitch, z, IncomingZ, object, x, y, 0, palette, FALSE); }); break;
		case Operation::ObscuredShadow:
			draw([&](UINT16* color, UINT16* z) { return api.obscuredShadow(color, pitch, z, IncomingZ, object, x, y, 0, palette, FALSE); }); break;
		case Operation::ShadowAlpha:
			draw([&](UINT16* color, UINT16*) { return api.shadowAlpha(color, pitch, object, &fixture.alphaObject, x, y, 0, palette, FALSE); }); break;
		case Operation::Shadow:
			draw([&](UINT16* color, UINT16*) { return api.shadow(color, pitch, object, x, y, 0); }); break;
		case Operation::ShadowClip:
			draw([&](UINT16* color, UINT16*) { return api.shadowClip(color, pitch, object, x, y, 0, clip); }); break;
		case Operation::ShadowZ:
			draw([&](UINT16* color, UINT16* z) { return api.shadowZ(color, pitch, z, IncomingZ, object, x, y, 0); }); break;
		case Operation::Intensity:
			draw([&](UINT16* color, UINT16*) { return api.intensity(color, pitch, object, x, y, 0); }); break;
		case Operation::IntensityZ:
			draw([&](UINT16* color, UINT16* z) { return api.intensityZ(color, pitch, z, IncomingZ, object, x, y, 0); }); break;
		case Operation::Outline:
			draw([&](UINT16* color, UINT16*) { return api.outline(color, pitch, object, x, y, 0, 0x7e0, TRUE); }); break;
		case Operation::OutlineClip:
			draw([&](UINT16* color, UINT16*) { return api.outlineClip(color, pitch, object, x, y, 0, 0x7e0, TRUE, clip); }); break;
		case Operation::OutlineZ:
			draw([&](UINT16* color, UINT16* z) { return api.outlineZ(color, pitch, z, IncomingZ, object, x, y, 0, 0x7e0, TRUE); }); break;
		case Operation::OutlineShadow:
			draw([&](UINT16* color, UINT16*) { return api.outlineShadow(color, pitch, object, x, y, 0); }); break;
		case Operation::Pixelate:
			draw([&](UINT16* color, UINT16* z) { return api.pixelate(color, pitch, z, IncomingZ, object, x, y, 0); }); break;
		case Operation::PixelateObscured:
			draw([&](UINT16* color, UINT16* z) { return api.pixelateObscured(color, pitch, z, IncomingZ, object, x, y, 0); }); break;
		case Operation::Translucent:
			draw([&](UINT16* color, UINT16* z) { return api.translucent(color, pitch, z, IncomingZ, object, x, y, 0); }); break;
		case Operation::MonoClip:
			draw([&](UINT16* color, UINT16*) { return api.monoClip(color, pitch, object, x, y, 0, clip, 0xffff, 0x1234, 0x4567); }); break;
		case Operation::Copy16:
			draw([&](UINT16* color, UINT16*) { return api.copy16(color, pitch,
				fixture.rawSource.data() + GuardPixels, pitch, x, y, 0, 0,
				fixture.workload.width, fixture.workload.height); }); break;
		case Operation::Fill16:
			draw([&](UINT16* color, UINT16*) { return api.fill16(color, pitch, x, y,
				x + fixture.workload.width, y + fixture.workload.height, 0x7e0); }); break;
		case Operation::Hatch:
			draw([&](UINT16* color, UINT16*) { return api.hatch(color, pitch, &fixture.area, 0x7e0); }); break;
		case Operation::ShadeRect:
			draw([&](UINT16* color, UINT16*) { return api.shadeRect(color, pitch, &fixture.area); }); break;
	}
}

bool guardsIntact(const std::vector<UINT16>& pixels, const Fixture& fixture, UINT16 sentinel)
{
	for (std::size_t i = 0; i < GuardPixels; ++i)
		if (pixels[i] != sentinel || pixels[pixels.size() - 1 - i] != sentinel) return false;
	for (UINT32 row = 0; row < fixture.surfaceHeight; ++row)
		for (UINT32 column = fixture.surfaceWidth; column < fixture.pitchPixels; ++column)
			if (pixels[GuardPixels + static_cast<std::size_t>(row) * fixture.pitchPixels + column] != sentinel)
				return false;
	return true;
}

bool verify(const std::string& name, const BlitterBackend& baseline, const BlitterBackend& portable,
	Operation operation, Fixture& fixture)
{
	Buffers expected;
	Buffers actual;
	expected.reset(fixture);
	actual.reset(fixture);
	RegisteredBuffers baselineBuffers(baseline, fixture, expected);
	RegisteredBuffers portableBuffers(portable, fixture, actual);
	const auto source = fixture.source;
	const auto alpha = fixture.alpha;
	const auto palette = fixture.palette;
	const auto raw = fixture.rawSource;
	runBatch(baseline, operation, fixture, expected);
	if (source != fixture.source || alpha != fixture.alpha || palette != fixture.palette || raw != fixture.rawSource)
	{
		std::fprintf(stderr, "%s: baseline (%s) modified source data\n", name.c_str(), baseline.name);
		return false;
	}
	runBatch(portable, operation, fixture, actual);
	if (source != fixture.source || alpha != fixture.alpha || palette != fixture.palette || raw != fixture.rawSource)
	{
		std::fprintf(stderr, "%s: portable implementation modified source data\n", name.c_str());
		return false;
	}
	for (std::size_t slot = 0; slot < BatchSize; ++slot)
	{
		if (!guardsIntact(expected.color[slot], fixture, ColorGuard) ||
			!guardsIntact(actual.color[slot], fixture, ColorGuard) ||
			!guardsIntact(expected.depth[slot], fixture, DepthGuard) ||
			!guardsIntact(actual.depth[slot], fixture, DepthGuard))
		{
			std::fprintf(stderr, "%s: destination/Z guard or row padding overwritten\n", name.c_str());
			return false;
		}
		if (!expected.results[slot] || !actual.results[slot] ||
			expected.results[slot] != actual.results[slot])
		{
			std::fprintf(stderr, "%s: valid blit failed or return values differ: baseline (%s)=%u portable=%u\n",
				name.c_str(), baseline.name,
				static_cast<unsigned>(expected.results[slot]), static_cast<unsigned>(actual.results[slot]));
			return false;
		}
		std::size_t differences = 0;
		std::size_t firstPixel = 0;
		for (std::size_t pixel = 0; pixel < expected.color[slot].size(); ++pixel)
		{
			if (expected.color[slot][pixel] != actual.color[slot][pixel] ||
				expected.depth[slot][pixel] != actual.depth[slot][pixel])
			{
				if (differences++ == 0) firstPixel = pixel;
			}
		}
		if (differences != 0)
		{
			const std::size_t logical = firstPixel - GuardPixels;
			const std::size_t pixelY = logical / fixture.pitchPixels;
			const std::size_t pixelX = logical % fixture.pitchPixels;
			std::fprintf(stderr,
				"%s: slot %zu has %zu differing pixels; first at (%zu,%zu): "
				"baseline=%s; color baseline=%04x portable=%04x; Z baseline=%04x portable=%04x\n",
				name.c_str(), slot, differences, pixelX, pixelY, baseline.name,
				static_cast<unsigned>(expected.color[slot][firstPixel]),
				static_cast<unsigned>(actual.color[slot][firstPixel]),
				static_cast<unsigned>(expected.depth[slot][firstPixel]),
				static_cast<unsigned>(actual.depth[slot][firstPixel]));
			return false;
		}
	}
	return true;
}

std::int64_t ticks()
{
	LARGE_INTEGER counter;
	if (!QueryPerformanceCounter(&counter)) throw std::runtime_error("QueryPerformanceCounter failed");
	return counter.QuadPart;
}

double sample(const BlitterBackend& api, Operation operation, Fixture& fixture, Buffers& buffers,
	double milliseconds, std::int64_t frequency)
{
	const double targetTicks = milliseconds * static_cast<double>(frequency) / 1000.0;
	std::int64_t elapsed = 0;
	std::uint64_t calls = 0;
	while (static_cast<double>(elapsed) < targetTicks)
	{
		// A repeated strict-Z or shading blit would otherwise become a different workload.
		buffers.reset(fixture);
		std::atomic_signal_fence(std::memory_order_seq_cst);
		const std::int64_t start = ticks();
		runBatch(api, operation, fixture, buffers);
		const std::int64_t end = ticks();
		std::atomic_signal_fence(std::memory_order_seq_cst);
		if (end < start) throw std::runtime_error("Performance counter moved backwards");
		elapsed += end - start;
		calls += BatchSize;
		if (calls > 100000000) throw std::runtime_error("Could not collect a timed sample");
	}
	observedResults = observedResults ^ buffers.color[0][GuardPixels] ^ buffers.results[0];
	return static_cast<double>(elapsed) * 1e9 / static_cast<double>(frequency) / static_cast<double>(calls);
}

double median(std::vector<double> values)
{
	std::sort(values.begin(), values.end());
	const std::size_t middle = values.size() / 2;
	return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
}

double spread(const std::vector<double>& values)
{
	const auto extremes = std::minmax_element(values.begin(), values.end());
	return 100.0 * (*extremes.second - *extremes.first) / median(values);
}
}

int main(int argc, char** argv)
{
	static_assert(sizeof(void*) == 4, "The x86 benchmark harness requires a 32-bit process");
	try
	{
		const Options options = parseOptions(argc, argv);
		std::size_t selected = 0;
		for (const auto& blitter : Blitters)
			for (const auto& workload : workloads(blitter))
			{
				const std::string name = std::string(blitter.name) + "/" + workload.name;
				if (name.find(options.filter) == std::string::npos) continue;
				++selected;
				if (options.list) std::puts(name.c_str());
			}
		if (!selected) throw std::runtime_error("No workloads match --filter");
		if (options.list) return 0;
		if (!options.csvPath.empty())
		{
			std::FILE* output = nullptr;
			if (freopen_s(&output, options.csvPath.c_str(), "w", stdout) != 0)
				throw std::runtime_error("Cannot open CSV output: " + options.csvPath);
		}

		BackendLibrary baselineLibrary(options.baseline == "asm" ?
			L"ja2_blitters_asm.dll" : L"ja2_blitters_generic.dll");
		BackendLibrary portableLibrary(L"ja2_blitters_portable.dll");
		const BlitterBackend& baseline = baselineLibrary.get();
		const BlitterBackend& portable = portableLibrary.get();
		LARGE_INTEGER frequency;
		if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
			throw std::runtime_error("QueryPerformanceCounter is unavailable");
		const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
		const bool wine = ntdll && GetProcAddress(ntdll, "wine_get_version");
		std::fprintf(stderr, "Backends: baseline=%s [%s] vs %s [%s]; x86, %s; QPC=%lld Hz\n",
			baseline.name, baseline.compiler, portable.name, portable.compiler,
			wine ? "Wine" : "Windows", static_cast<long long>(frequency.QuadPart));
		std::fprintf(stderr, "Workloads=%zu samples=%u timed_ms/sample=%.3f max_ratio=%.6g (%s)\n",
			selected, options.samples, options.batchMilliseconds, options.maxRatio,
			options.maxRatio ? "timing gate enabled" : "timing report-only");
		std::puts("case,baseline,width,height,opaque_pixels,baseline_ns_per_call,portable_ns_per_call,paired_median_ratio,baseline_spread_pct,portable_spread_pct,status");
		std::size_t mismatches = 0;
		std::size_t regressions = 0;
		for (const auto& blitter : Blitters)
		{
			for (const auto& workload : workloads(blitter))
			{
				const std::string name = std::string(blitter.name) + "/" + workload.name;
				if (name.find(options.filter) == std::string::npos) continue;
				Fixture fixture(workload);
				baseline.initialize(fixture.surfaceWidth, fixture.surfaceHeight);
				portable.initialize(fixture.surfaceWidth, fixture.surfaceHeight);
				if (!verify(name, baseline, portable, blitter.operation, fixture))
				{
					++mismatches;
					std::printf("%s,%s,%u,%u,%zu,,,,,,MISMATCH\n", name.c_str(), baseline.name,
						static_cast<unsigned>(workload.width), static_cast<unsigned>(workload.height), fixture.opaquePixels);
					continue; // Timing unequal work would produce a misleading speed ratio.
				}
				if (options.verifyOnly)
				{
					std::printf("%s,%s,%u,%u,%zu,,,,,,VERIFIED\n", name.c_str(), baseline.name,
						static_cast<unsigned>(workload.width), static_cast<unsigned>(workload.height), fixture.opaquePixels);
					continue;
				}
				Buffers buffers;
				buffers.reset(fixture);
				RegisteredBuffers baselineBuffers(baseline, fixture, buffers);
				RegisteredBuffers portableBuffers(portable, fixture, buffers);
				runBatch(baseline, blitter.operation, fixture, buffers);
				buffers.reset(fixture);
				runBatch(portable, blitter.operation, fixture, buffers);
				std::vector<double> baselineTimes;
				std::vector<double> portableTimes;
				std::vector<double> ratios;
				for (unsigned index = 0; index < options.samples; ++index)
				{
					double baselineTime;
					double portableTime;
					if (index % 2 == 0)
					{
						baselineTime = sample(baseline, blitter.operation, fixture, buffers,
							options.batchMilliseconds, frequency.QuadPart);
						portableTime = sample(portable, blitter.operation, fixture, buffers,
							options.batchMilliseconds, frequency.QuadPart);
					}
					else
					{
						portableTime = sample(portable, blitter.operation, fixture, buffers,
							options.batchMilliseconds, frequency.QuadPart);
						baselineTime = sample(baseline, blitter.operation, fixture, buffers,
							options.batchMilliseconds, frequency.QuadPart);
					}
					baselineTimes.push_back(baselineTime);
					portableTimes.push_back(portableTime);
					ratios.push_back(portableTime / baselineTime);
				}
				const double ratio = median(ratios);
				const bool regression = options.maxRatio && ratio > options.maxRatio;
				if (regression) ++regressions;
				std::printf("%s,%s,%u,%u,%zu,%.3f,%.3f,%.4f,%.2f,%.2f,%s\n", name.c_str(), baseline.name,
					static_cast<unsigned>(workload.width), static_cast<unsigned>(workload.height), fixture.opaquePixels,
					median(baselineTimes), median(portableTimes), ratio, spread(baselineTimes), spread(portableTimes),
					regression ? "REGRESSION" : "OK");
				std::fflush(stdout);
			}
		}
		std::fprintf(stderr, "%zu workloads: %zu correctness failures, %zu timing regressions\n",
			selected, mismatches, regressions);
		if (std::fflush(stdout) != 0 || std::ferror(stdout))
			throw std::runtime_error("Failed to write benchmark results");
		return mismatches ? 2 : regressions ? 1 : 0;
	}
	catch (const std::exception& error)
	{
		std::fprintf(stderr, "blitter benchmark: %s\n", error.what());
		return 3;
	}
}
