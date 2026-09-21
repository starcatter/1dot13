#include "BlitterBackend.h"
#include "ScreenGeometry.h"
#include "DEBUG.H"
#include "sgp_logger.h"

#include <cstdio>
#include <cstdlib>
#include <map>

UINT16 SCREEN_WIDTH = 0;
UINT16 SCREEN_HEIGHT = 0;
UINT16 ShadeTable[65536];
UINT16 IntensityTable[65536];
UINT16 White16BPPPalette[256];
extern std::map<UINT32, ClipRectangle> g_SurfaceRectangle;

namespace SurfaceData
{
namespace
{
std::map<BYTE*, tID> surfaceIds;
}

// Match the game's application-buffer registration, without DirectDraw surfaces.
BYTE* SetApplicationData(BYTE* data)
{
	surfaceIds[data] = reinterpret_cast<tID>(data);
	return data;
}

void ReleaseApplicationData(BYTE* data)
{
	const auto entry = surfaceIds.find(data);
	if (entry == surfaceIds.end()) return;
	g_SurfaceRectangle.erase(static_cast<UINT32>(entry->second));
	surfaceIds.erase(entry);
}

tID GetSurfaceID(BYTE* data)
{
	const auto entry = surfaceIds.find(data);
	return entry == surfaceIds.end() ? 0 : entry->second;
}
}

void _FailMessage(const char* message, unsigned line, const char* function, const char* file)
{
	std::fprintf(stderr, "%s:%u: %s: %s\n", file ? file : "blitter", line,
		function ? function : "assertion", message ? message : "assertion failed");
	std::abort();
}

// Logging denotes unsupported/error paths in this raw-buffer harness.
sgp::Logger& sgp::Logger::instance() { std::abort(); }
sgp::Logger::LogInstance sgp::Logger::logger(Logger_ID) { std::abort(); }
template <>
sgp::Logger::LogInstance& sgp::Logger::LogInstance::operator<< <sgp::_endl>(const sgp::_endl&)
{
	std::abort();
}
vfs::String::String(const char*) { std::abort(); }
std::wostream& operator<<(std::wostream&, const vfs::String&) { std::abort(); }
vfs::Log& vfs::Log::operator<<(const vfs::String&) { std::abort(); }
vfs::Log& vfs::Log::operator<<(const char*) { std::abort(); }

UINT16 Get16BPPColor(UINT32 color)
{
	const auto rgb565 = static_cast<UINT16>(((color & 0xf8) << 8) |
		((color & 0xfc00) >> 5) | ((color & 0xf80000) >> 19));
	return rgb565 == 0 && color != 0 ? 1 : rgb565;
}

namespace
{
void registerBuffer(UINT16* buffer, UINT32 width, UINT32 height)
{
	BYTE* data = SurfaceData::SetApplicationData(reinterpret_cast<BYTE*>(buffer));
	g_SurfaceRectangle[static_cast<UINT32>(SurfaceData::GetSurfaceID(data))].SetRect(width, height);
}

void releaseBuffer(UINT16* buffer)
{
	SurfaceData::ReleaseApplicationData(reinterpret_cast<BYTE*>(buffer));
}

void initialize(UINT16 width, UINT16 height)
{
	SCREEN_WIDTH = width;
	SCREEN_HEIGHT = height;
	ClippingRect = {0, 0, width, height};
	guiTranslucentMask = 0x7bef;
	for (unsigned i = 0; i < 65536; ++i)
	{
		ShadeTable[i] = static_cast<UINT16>((i >> 1) & 0x7bef);
		IntensityTable[i] = static_cast<UINT16>((i >> 2) & 0x39e7);
	}
	for (auto& color : White16BPPPalette) color = 0xffff;
}

#define JA2_STRINGIFY_IMPL(value) #value
#define JA2_STRINGIFY(value) JA2_STRINGIFY_IMPL(value)
#if defined(__clang__)
constexpr const char* compiler = "clang-cl " __clang_version__;
#else
constexpr const char* compiler = "MSVC " JA2_STRINGIFY(_MSC_FULL_VER);
#endif

const BlitterBackend backend = {
	JA2_BLITTER_BACKEND_NAME,
	compiler,
	initialize,
	registerBuffer,
	releaseBuffer,
	Blt8BPPDataTo16BPPBufferTransparent,
	Blt8BPPDataTo16BPPBufferTransparentClip,
	Blt8BPPDataTo16BPPBufferTransZ,
	Blt8BPPDataTo16BPPBufferTransZNB,
	Blt8BPPDataTo16BPPBufferTransZClip,
	Blt8BPPDataTo16BPPBufferTransZNBClip,
	Blt8BPPDataTo16BPPBufferTransShadow,
	Blt8BPPDataTo16BPPBufferTransShadowClip,
	Blt8BPPDataTo16BPPBufferTransShadowZ,
	Blt8BPPDataTo16BPPBufferTransShadowZNB,
	Blt8BPPDataTo16BPPBufferTransShadowZNBObscured,
	Blt8BPPDataTo16BPPBufferTransShadowAlpha,
	Blt8BPPDataTo16BPPBufferShadow,
	Blt8BPPDataTo16BPPBufferShadowClip,
	Blt8BPPDataTo16BPPBufferShadowZ,
	Blt8BPPDataTo16BPPBufferIntensity,
	Blt8BPPDataTo16BPPBufferIntensityZ,
	Blt8BPPDataTo16BPPBufferOutline,
	Blt8BPPDataTo16BPPBufferOutlineClip,
	Blt8BPPDataTo16BPPBufferOutlineZ,
	Blt8BPPDataTo16BPPBufferOutlineShadow,
	Blt8BPPDataTo16BPPBufferTransZPixelate,
	Blt8BPPDataTo16BPPBufferTransZPixelateObscured,
	Blt8BPPDataTo16BPPBufferTransZTranslucent,
	Blt8BPPDataTo16BPPBufferMonoShadowClip,
	Blt16BPPTo16BPP,
	FillRect16BPP,
	Blt16BPPBufferHatchRectWithColor,
	Blt16BPPBufferShadowRect,
};
}

extern "C" const BlitterBackend* GetBlitterBackend()
{
	return &backend;
}
