#include "vobject_blitters.h"
#include "PortableBlitterCore.h"
#include "renderworld.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <initializer_list>
#include <map>
#include <numeric>
#include <vector>

BOOLEAN IsTileRedundent(UINT32 uiDestPitchBYTES, UINT16* pZBuffer,
	UINT16 usZValue, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex);

UINT16 SCREEN_WIDTH = 8;
UINT16 SCREEN_HEIGHT = 6;
UINT16 IntensityTable[65536];
UINT16 ShadeTable[65536];
UINT16 White16BPPPalette[256];

extern std::map<UINT32, ClipRectangle> g_SurfaceRectangle;

namespace SurfaceData
{
namespace
{
std::map<BYTE*, tID> testSurfaceIds;
}

BYTE* SetApplicationData(BYTE* data)
{
	if (data != nullptr)
		testSurfaceIds[data] = reinterpret_cast<tID>(data);
	return data;
}

void ReleaseApplicationData(BYTE* data)
{
	const auto entry = testSurfaceIds.find(data);
	if (entry == testSurfaceIds.end()) return;
	g_SurfaceRectangle.erase(static_cast<UINT32>(entry->second));
	testSurfaceIds.erase(entry);
}

tID GetSurfaceID(BYTE* data)
{
	const auto entry = testSurfaceIds.find(data);
	return entry == testSurfaceIds.end() ? 0 : entry->second;
}
}

namespace
{
constexpr UINT32 kWidth = 8;
constexpr UINT32 kHeight = 6;
constexpr UINT32 kPitchBytes = kWidth * sizeof(UINT16);

struct TestObject
{
	std::array<UINT16, 256> palette{};
	std::array<INT8, 4> zChanges{};
	ZStripInfo zStrip{};
	std::array<ZStripInfo*, 1> zStrips{};
	ETRLEObject frame{};
	SGPVObject object{};

	TestObject(UINT8* data, std::size_t size, UINT16 width, UINT16 height)
	{
		for (std::size_t i = 0; i < palette.size(); ++i)
			palette[i] = static_cast<UINT16>(0x1000 + i);
		frame.uiDataLength = static_cast<UINT32>(size);
		frame.usWidth = width;
		frame.usHeight = height;
		object.pPixData = data;
		object.pETRLEObject = &frame;
		object.pShadeCurrent = palette.data();
		object.usNumberOfObjects = 1;
	}

	void setZStrip(UINT8 firstWidth, INT8 initialChange,
		std::initializer_list<INT8> changes)
	{
		std::copy(changes.begin(), changes.end(), zChanges.begin());
		zStrip.bInitialZChange = initialChange;
		zStrip.ubFirstZStripWidth = firstWidth;
		zStrip.ubNumberOfZChanges = static_cast<UINT8>(changes.size());
		zStrip.pbZChange = zChanges.data();
		zStrips[0] = &zStrip;
		object.ppZStripInfo = zStrips.data();
	}
};

using Buffer = std::array<UINT16, kWidth * kHeight>;

bool paletteShadowAndIgnore()
{
	UINT8 data[]{3, 10, 254, 20, 0};
	TestObject sprite(data, sizeof(data), 3, 1);
	Buffer buffer{};
	buffer.fill(0x2222);
	ShadeTable[0x2222] = 0x0111;

	if (!Blt8BPPDataTo16BPPBufferTransparent(buffer.data(), kPitchBytes, &sprite.object, 2, 1, 0)) return false;
	if (buffer[kWidth + 2] != 0x100a || buffer[kWidth + 3] != 0x10fe || buffer[kWidth + 4] != 0x1014) return false;

	buffer.fill(0x2222);
	if (!Blt8BPPDataTo16BPPBufferTransShadow(buffer.data(), kPitchBytes, &sprite.object, 2, 1, 0,
		sprite.palette.data(), FALSE)) return false;
	if (buffer[kWidth + 2] != 0x100a || buffer[kWidth + 3] != 0x0111 || buffer[kWidth + 4] != 0x1014) return false;

	buffer.fill(0x2222);
	if (!Blt8BPPDataTo16BPPBufferTransShadow(buffer.data(), kPitchBytes, &sprite.object, 2, 1, 0,
		sprite.palette.data(), TRUE)) return false;
	return buffer[kWidth + 2] == 0x100a && buffer[kWidth + 3] == 0x2222 && buffer[kWidth + 4] == 0x1014;
}

bool outlineShadowShadesSilhouette()
{
	UINT8 data[]{3, 7, 254, 8, 0};
	TestObject sprite(data, sizeof(data), 3, 1);
	Buffer buffer{};
	buffer.fill(0x2222);
	ShadeTable[0x2222] = 0x0111;

	if (!Blt8BPPDataTo16BPPBufferOutlineShadow(
		buffer.data(), kPitchBytes, &sprite.object, 2, 1, 0)) return false;
	return buffer[kWidth + 2] == 0x0111 &&
		buffer[kWidth + 3] == 0x2222 &&
		buffer[kWidth + 4] == 0x0111;
}

bool depthRulesAndIgnoredShadowWrite()
{
	UINT8 colorData[]{1, 7, 0};
	TestObject color(colorData, sizeof(colorData), 1, 1);
	Buffer buffer{};
	Buffer depth{};
	buffer.fill(0x2222);
	depth.fill(50);
	if (!Blt8BPPDataTo16BPPBufferTransZ(buffer.data(), kPitchBytes, depth.data(), 50, &color.object, 1, 1, 0)) return false;
	if (buffer[kWidth + 1] != 0x1007 || depth[kWidth + 1] != 50) return false;

	UINT8 shadowData[]{1, 254, 0};
	TestObject shadow(shadowData, sizeof(shadowData), 1, 1);
	buffer.fill(0x2222);
	depth.fill(50);
	ShadeTable[0x2222] = 0x0111;
	if (!Blt8BPPDataTo16BPPBufferTransShadowZ(buffer.data(), kPitchBytes, depth.data(), 50,
		&shadow.object, 1, 1, 0, shadow.palette.data(), FALSE)) return false;
	if (buffer[kWidth + 1] != 0x2222) return false;

	depth.fill(49);
	if (!Blt8BPPDataTo16BPPBufferTransShadowZ(buffer.data(), kPitchBytes, depth.data(), 50,
		&shadow.object, 1, 1, 0, shadow.palette.data(), TRUE)) return false;
	return buffer[kWidth + 1] == 0x2222 && depth[kWidth + 1] == 50;
}

bool znbShadowEqualDepthRules()
{
	UINT8 data[]{2, 7, 254, 0};
	TestObject sprite(data, sizeof(data), 2, 1);
	Buffer buffer{};
	Buffer depth{};
	buffer.fill(0x2222);
	depth.fill(50);
	ShadeTable[0x2222] = 0x0111;
	if (!Blt8BPPDataTo16BPPBufferTransShadowZNB(buffer.data(), kPitchBytes,
		depth.data(), 50, &sprite.object, 1, 1, 0, sprite.palette.data(), FALSE)) return false;
	// Ordinary ZNB pixels draw at equal Z; shadow markers require strictly foreground Z.
	return buffer[kWidth + 1] == 0x1007 && buffer[kWidth + 2] == 0x2222 &&
		depth[kWidth + 1] == 50 && depth[kWidth + 2] == 50;
}

bool obscuredPixelationPreservesOccluderDepth()
{
	UINT8 data[]{2, 7, 8, 0};
	TestObject sprite(data, sizeof(data), 2, 1);
	Buffer buffer{};
	Buffer depth{};
	buffer.fill(0x2222);
	depth.fill(60);
	if (!Blt8BPPDataTo16BPPBufferTransZPixelateObscured(buffer.data(), kPitchBytes,
		depth.data(), 50, &sprite.object, 1, 1, 0)) return false;
	// (1,1) is drawn as an obscured checker pixel, but must not claim the occluder's Z.
	return buffer[kWidth + 1] == 0x1007 && depth[kWidth + 1] == 60 &&
		buffer[kWidth + 2] == 0x2222 && depth[kWidth + 2] == 60;
}

bool rectangleEndpointAndPatternSemantics()
{
	Buffer buffer{};
	buffer.fill(0x2222);
	if (!FillRect16BPP(buffer.data(), kPitchBytes, 1, 1, 3, 3, 0x7777)) return false;
	for (UINT32 y = 0; y < kHeight; ++y)
	{
		for (UINT32 x = 0; x < kWidth; ++x)
		{
			const UINT16 expected = x >= 1 && x < 3 && y >= 1 && y < 3 ? 0x7777 : 0x2222;
			if (buffer[y * kWidth + x] != expected) return false;
		}
	}

	buffer.fill(0x2222);
	SGPRect area{1, 1, 4, 3};
	if (!Blt16BPPBufferHatchRectWithColor(buffer.data(), kPitchBytes, &area, 0x7777))
		return false;
	for (INT32 y = area.iTop; y <= area.iBottom; ++y)
	{
		for (INT32 x = area.iLeft; x <= area.iRight; ++x)
		{
			const bool hatched = ((x - area.iLeft) & 1) == ((y - area.iTop) & 1);
			if (buffer[y * kWidth + x] != (hatched ? 0x7777 : 0x2222)) return false;
		}
	}
	return true;
}

bool obscuredColorAndOutlineDepthRules()
{
	UINT8 data[]{2, 7, 254, 0};
	TestObject sprite(data, sizeof(data), 2, 1);
	Buffer buffer{};
	Buffer depth{};
	buffer.fill(0x2222);
	depth.fill(60);
	depth[kWidth + 1] = 50;
	if (!Blt8BPPDataTo16BPPBufferTransZNBColor(buffer.data(), kPitchBytes, depth.data(), 50,
		&sprite.object, 1, 1, 0, 0x7777)) return false;
	if (buffer[kWidth + 1] != 0x1007 || buffer[kWidth + 2] != 0x7777) return false;

	buffer.fill(0x2222);
	depth.fill(10);
	if (!Blt8BPPDataTo16BPPBufferOutlineZ(buffer.data(), kPitchBytes, depth.data(), 50,
		&sprite.object, 1, 1, 0, 0x6666, TRUE)) return false;
	return buffer[kWidth + 1] == 0x1007 && depth[kWidth + 1] == 50 &&
		buffer[kWidth + 2] == 0x6666 && depth[kWidth + 2] == 10;
}

bool clippingAndMirroring()
{
	UINT8 data[]{4, 1, 2, 3, 4, 0};
	TestObject sprite(data, sizeof(data), 4, 1);
	Buffer buffer{};
	buffer.fill(0x2222);
	SGPRect clip{3, 1, 5, 2};
	if (!Blt8BPPDataTo16BPPBufferTransparentClip(buffer.data(), kPitchBytes, &sprite.object, 2, 1, 0, &clip)) return false;
	if (buffer[kWidth + 2] != 0x2222 || buffer[kWidth + 3] != 0x1002 ||
		buffer[kWidth + 4] != 0x1003 || buffer[kWidth + 5] != 0x2222) return false;

	buffer.fill(0x2222);
	if (!Blt8BPPDataTo16BPPBufferTransMirror(buffer.data(), kPitchBytes, &sprite.object, 2, 1, 0)) return false;
	return buffer[kWidth + 2] == 0x1004 && buffer[kWidth + 3] == 0x1003 &&
		buffer[kWidth + 4] == 0x1002 && buffer[kWidth + 5] == 0x1001;
}

bool monoTransparentRuns()
{
	UINT8 data[]{0x81, 3, 0, 1, 2, 0};
	TestObject sprite(data, sizeof(data), 4, 1);
	Buffer buffer{};
	buffer.fill(0x2222);
	SGPRect clip{0, 0, static_cast<INT32>(kWidth), static_cast<INT32>(kHeight)};
	if (!Blt8BPPDataTo16BPPBufferMonoShadowClip(buffer.data(), kPitchBytes, &sprite.object, 2, 1, 0,
		&clip, 0xaaaa, 0xbbbb, 0xcccc)) return false;
	return buffer[kWidth + 2] == 0xbbbb && buffer[kWidth + 3] == 0xbbbb &&
		buffer[kWidth + 4] == 0xcccc && buffer[kWidth + 5] == 0xaaaa;
}

bool alphaAndShadowMarker()
{
	UINT8 data[]{2, 5, 254, 0};
	UINT8 alphaData[]{2, 0, 0, 0};
	TestObject sprite(data, sizeof(data), 2, 1);
	TestObject alpha(alphaData, sizeof(alphaData), 2, 1);
	Buffer buffer{};
	buffer.fill(0x2222);
	ShadeTable[0x2222] = 0x0111;
	if (!Blt8BPPDataTo16BPPBufferTransShadowAlpha(buffer.data(), kPitchBytes, &sprite.object, &alpha.object,
		2, 1, 0, sprite.palette.data(), FALSE)) return false;
	return buffer[kWidth + 2] == 0x2222 && buffer[kWidth + 3] == 0x0111;
}

bool obscuredPixelationDoesNotLeakShadow()
{
	UINT8 data[]{2, 6, 254, 0};
	TestObject sprite(data, sizeof(data), 2, 1);
	Buffer buffer{};
	Buffer depth{};
	buffer.fill(0x2222);
	depth.fill(60);
	ShadeTable[0x2222] = 0x0111;
	if (!Blt8BPPDataTo16BPPBufferTransShadowZNBObscured(buffer.data(), kPitchBytes, depth.data(), 50,
		&sprite.object, 2, 2, 0, sprite.palette.data(), FALSE)) return false;
	return buffer[kWidth * 2 + 2] == 0x1006 && buffer[kWidth * 2 + 3] == 0x2222;
}

bool zStripDepthAndBurnThrough()
{
	UINT8 data[]{5, 1, 2, 3, 4, 5, 0};
	TestObject sprite(data, sizeof(data), 5, 1);
	sprite.setZStrip(2, 0, {1});
	Buffer buffer{};
	Buffer depth{};
	buffer.fill(0x2222);
	depth.fill(120);
	SGPRect clip{0, 0, static_cast<INT32>(kWidth), static_cast<INT32>(kHeight)};
	if (!Blt8BPPDataTo16BPPBufferTransZIncClip(buffer.data(), kPitchBytes,
		depth.data(), 100, &sprite.object, 1, 1, 0, &clip)) return false;
	if (buffer[kWidth + 1] != 0x2222 || buffer[kWidth + 2] != 0x2222 ||
		buffer[kWidth + 3] != 0x1003 || depth[kWidth + 3] != 180) return false;

	buffer.fill(0x2222);
	depth.fill(100);
	if (!Blt8BPPDataTo16BPPBufferTransZIncClipZSameZBurnsThrough(buffer.data(),
		kPitchBytes, depth.data(), 100, &sprite.object, 1, 1, 0, &clip, 0)) return false;
	return buffer[kWidth + 1] == 0x1001 && buffer[kWidth + 2] == 0x1002;
}

bool worldRenderUtilityPolicies()
{
	UINT8 data[]{2, 7, 8, 0};
	TestObject sprite(data, sizeof(data), 2, 1);
	Buffer buffer{};
	Buffer depth{};
	buffer.fill(0x2222);
	if (!Zero8BPPDataTo16BPPBufferTransparent(buffer.data(), kPitchBytes,
		&sprite.object, 2, 1, 0)) return false;
	if (buffer[kWidth + 2] != 0 || buffer[kWidth + 3] != 0) return false;

	buffer.fill(0x2222);
	depth.fill(49);
	depth[kWidth + 2] = 50;
	if (!Blt8BPPDataTo16BPPBufferTransInvZ(buffer.data(), kPitchBytes,
		depth.data(), 50, &sprite.object, 2, 1, 0)) return false;
	if (buffer[kWidth + 2] != 0x1007 || buffer[kWidth + 3] != 0x2222) return false;
	if (IsTileRedundent(kPitchBytes, depth.data(), 50, &sprite.object, 2, 1, 0))
		return false;
	depth.fill(50);
	return IsTileRedundent(kPitchBytes, depth.data(), 50, &sprite.object, 2, 1, 0);
}

bool rawCopyClipsToRegisteredBuffers()
{
	constexpr std::size_t guardPixels = kWidth;
	std::array<UINT16, kWidth * kHeight + guardPixels * 2> guardedDestination{};
	Buffer source{};
	std::iota(source.begin(), source.end(), static_cast<UINT16>(1));
	guardedDestination.fill(0x7777);
	UINT16* destination = guardedDestination.data() + guardPixels;

	SurfaceData::SetApplicationData(reinterpret_cast<BYTE*>(destination));
	g_SurfaceRectangle[static_cast<UINT32>(SurfaceData::GetSurfaceID(
		reinterpret_cast<BYTE*>(destination)))].SetRect(kWidth, kHeight);
	SurfaceData::SetApplicationData(reinterpret_cast<BYTE*>(source.data()));
	g_SurfaceRectangle[static_cast<UINT32>(SurfaceData::GetSurfaceID(
		reinterpret_cast<BYTE*>(source.data())))].SetRect(kWidth, kHeight);

	const bool leftClipped = Blt16BPPTo16BPP(destination, kPitchBytes,
		source.data(), kPitchBytes, -2, 0, 0, 0, 4, 1) &&
		std::all_of(guardedDestination.begin(), guardedDestination.begin() + guardPixels,
			[](UINT16 pixel) { return pixel == 0x7777; }) &&
		destination[0] == source[2] && destination[1] == source[3];

	std::fill_n(destination, kWidth * kHeight, static_cast<UINT16>(0x7777));
	const bool topClipped = Blt16BPPTo16BPP(destination, kPitchBytes,
		source.data(), kPitchBytes, 0, -1, 0, 0, 2, 2) &&
		std::all_of(guardedDestination.begin(), guardedDestination.begin() + guardPixels,
			[](UINT16 pixel) { return pixel == 0x7777; }) &&
		destination[0] == source[kWidth] && destination[1] == source[kWidth + 1];

	std::fill_n(destination, kWidth * kHeight, static_cast<UINT16>(0x7777));
	const bool rightClipped = Blt16BPPTo16BPP(destination, kPitchBytes,
		source.data(), kPitchBytes, static_cast<INT32>(kWidth - 1), 0,
		0, 0, 4, 1) && destination[kWidth - 1] == source[0] &&
		std::all_of(guardedDestination.end() - guardPixels, guardedDestination.end(),
			[](UINT16 pixel) { return pixel == 0x7777; });

	std::fill_n(destination, kWidth * kHeight, static_cast<UINT16>(0x7777));
	const bool sourceClipped = Blt16BPPTo16BPP(destination, kPitchBytes,
		source.data(), kPitchBytes, 0, 0, -2, 0, 4, 1) &&
		destination[0] == 0x7777 && destination[1] == 0x7777 &&
		destination[2] == source[0] && destination[3] == source[1];

	SurfaceData::ReleaseApplicationData(reinterpret_cast<BYTE*>(source.data()));
	SurfaceData::ReleaseApplicationData(reinterpret_cast<BYTE*>(destination));
	return leftClipped && topClipped && rightClipped && sourceClipped;
}

enum class SpanAlias { None, DestinationDepth, PaletteDestination, PaletteDepth, SourceDepth, SourceDestination };
enum class SpanInvalid { None, Destination, Object, Frames, Pixels, Index, ObjectCount, Palette };

struct SpanCase
{
	std::vector<UINT8> data;
	UINT16 width = 3, height = 1;
	INT32 x = 2, y = 1;
	INT16 offsetX = 0, offsetY = 0;
	UINT32 dataOffset = 0, pitchBytes = 320;
	INT32 paletteOffset = 0, sourceByteOffset = -1;
	SGPRect clip{0, 0, 160, 12};
	UINT16 zValue = 50;
	INT32 depthValue = -1;
	bool nullZ = false, globalClip = false;
	SpanAlias alias = SpanAlias::None;
	SpanInvalid invalid = SpanInvalid::None;
};

bool comparePaletteSpan(const SpanCase& test, const char* group, std::size_t ordinal)
{
	constexpr std::size_t guard = 32, pixels = 160 * 12;
	using Guarded = std::array<UINT16, pixels + 2 * guard>;
	const char* names[]{"Transparent", "TransparentClip", "TransZ", "TransZNB",
		"TransZClip", "TransZNBClip"};
	for (unsigned variant = 0; variant < 6; ++variant)
	{
		Guarded initialDestination{}, initialDepth{};
		initialDestination.fill(0xa55a);
		initialDepth.fill(0x5aa5);
		for (std::size_t i = 0; i < pixels; ++i)
		{
			initialDepth[guard + i] = static_cast<UINT16>(
				test.depthValue < 0 ? 49 + i % 3 : test.depthValue);
			initialDestination[guard + i] = test.alias == SpanAlias::DestinationDepth
				? initialDepth[guard + i] : static_cast<UINT16>(0x1000 + i % 256);
		}
		Guarded expectedDestination = initialDestination, actualDestination = initialDestination;
		Guarded expectedDepth = initialDepth, actualDepth = initialDepth;
		std::vector<UINT8> expectedSource(test.dataOffset, 0xcd);
		expectedSource.insert(expectedSource.end(), test.data.begin(), test.data.end());
		// A non-null address with length zero exercises exhaustion, not null-input rejection.
		if (expectedSource.empty()) expectedSource.push_back(0xcd);
		std::vector<UINT8> actualSource = expectedSource;
		TestObject expected(expectedSource.data(), test.data.size(), test.width, test.height);
		TestObject actual(actualSource.data(), test.data.size(), test.width, test.height);
		UINT16* expectedBuffer = expectedDestination.data() + guard;
		UINT16* actualBuffer = actualDestination.data() + guard;
		UINT16* expectedZ = test.alias == SpanAlias::DestinationDepth
			? expectedBuffer : expectedDepth.data() + guard;
		UINT16* actualZ = test.alias == SpanAlias::DestinationDepth
			? actualBuffer : actualDepth.data() + guard;
		for (unsigned side = 0; side < 2; ++side)
		{
			TestObject& sprite = side ? actual : expected;
			UINT16* destination = side ? actualBuffer : expectedBuffer;
			UINT16* depth = side ? actualZ : expectedZ;
			for (std::size_t i = 0; i < sprite.palette.size(); ++i)
				sprite.palette[i] = static_cast<UINT16>(test.alias == SpanAlias::SourceDestination
					? (i + 1) * 0x0101u : i * 40503u + ordinal * 7919u);
			sprite.palette[0] = 0;
			sprite.palette[254] = 0xffff;
			sprite.frame.sOffsetX = test.offsetX;
			sprite.frame.sOffsetY = test.offsetY;
			sprite.frame.uiDataOffset = test.dataOffset;
			// p16BPPPalette must not replace a missing pShadeCurrent.
			sprite.object.p16BPPPalette = sprite.palette.data();
			if (test.alias == SpanAlias::PaletteDestination)
				sprite.object.pShadeCurrent = destination + test.paletteOffset;
			if (test.alias == SpanAlias::PaletteDepth)
				sprite.object.pShadeCurrent = depth + test.paletteOffset;
			if (test.alias == SpanAlias::SourceDepth || test.alias == SpanAlias::SourceDestination)
			{
				// Byte views of live UINT16 storage preserve alignment and allow stores
				// to change later literals/control bytes, but not an already-read index.
				Guarded& storage = test.alias == SpanAlias::SourceDestination
					? (side ? actualDestination : expectedDestination) : (side ? actualDepth : expectedDepth);
				const std::ptrdiff_t position = guard + test.y * (test.pitchBytes / sizeof(UINT16)) + test.x;
				UINT8* source = reinterpret_cast<UINT8*>(storage.data()) +
					position * static_cast<std::ptrdiff_t>(sizeof(UINT16)) + test.sourceByteOffset;
				std::copy(test.data.begin(), test.data.end(), source);
				sprite.object.pPixData = source - test.dataOffset;
			}
			if (test.invalid == SpanInvalid::Frames) sprite.object.pETRLEObject = nullptr;
			if (test.invalid == SpanInvalid::Pixels) sprite.object.pPixData = nullptr;
			if (test.invalid == SpanInvalid::ObjectCount) sprite.object.usNumberOfObjects = 0;
			if (test.invalid == SpanInvalid::Palette) sprite.object.pShadeCurrent = nullptr;
		}
		if (test.nullZ || variant < 2) expectedZ = actualZ = nullptr;
		if (test.invalid == SpanInvalid::Destination) expectedBuffer = actualBuffer = nullptr;
		HVOBJECT expectedObject = test.invalid == SpanInvalid::Object ? nullptr : &expected.object;
		HVOBJECT actualObject = test.invalid == SpanInvalid::Object ? nullptr : &actual.object;
		const UINT16 index = test.invalid == SpanInvalid::Index ? 1 : 0;
		const SGPRect savedClip = ClippingRect;
		SGPRect clip = test.clip;
		ClippingRect = test.globalClip ? clip : SGPRect{6, 5, 9, 8};
		SGPRect* wrapperClip = test.globalClip ? nullptr : &clip;
		ja2::blitter::BlitPolicy policy;
		policy.zBuffer = expectedZ;
		policy.zValue = test.zValue;
		policy.depthTest = expectedZ ? ja2::blitter::DepthTest::LessEqual : ja2::blitter::DepthTest::None;
		policy.updateZ = variant == 2 || variant == 4;
		if (variant == 1 || variant >= 4) policy.clip = wrapperClip ? wrapperClip : &ClippingRect;
		const BOOLEAN expectedResult = ja2::blitter::blitEtrle(expectedBuffer, test.pitchBytes,
			expectedObject, test.x, test.y, index, policy);
		BOOLEAN actualResult = FALSE;
		switch (variant)
		{
			case 0: actualResult = Blt8BPPDataTo16BPPBufferTransparent(actualBuffer, test.pitchBytes,
				actualObject, test.x, test.y, index); break;
			case 1: actualResult = Blt8BPPDataTo16BPPBufferTransparentClip(actualBuffer, test.pitchBytes,
				actualObject, test.x, test.y, index, wrapperClip); break;
			case 2: actualResult = Blt8BPPDataTo16BPPBufferTransZ(actualBuffer, test.pitchBytes,
				actualZ, test.zValue, actualObject, test.x, test.y, index); break;
			case 3: actualResult = Blt8BPPDataTo16BPPBufferTransZNB(actualBuffer, test.pitchBytes,
				actualZ, test.zValue, actualObject, test.x, test.y, index); break;
			case 4: actualResult = Blt8BPPDataTo16BPPBufferTransZClip(actualBuffer, test.pitchBytes,
				actualZ, test.zValue, actualObject, test.x, test.y, index, wrapperClip); break;
			case 5: actualResult = Blt8BPPDataTo16BPPBufferTransZNBClip(actualBuffer, test.pitchBytes,
				actualZ, test.zValue, actualObject, test.x, test.y, index, wrapperClip); break;
		}
		ClippingRect = savedClip;
		const auto guardsIntact = [](const Guarded& storage, UINT16 sentinel)
		{
			return std::all_of(storage.begin(), storage.begin() + guard,
				[sentinel](UINT16 value) { return value == sentinel; }) &&
				std::all_of(storage.end() - guard, storage.end(),
					[sentinel](UINT16 value) { return value == sentinel; });
		};
		if (expectedResult != actualResult || expectedDestination != actualDestination ||
			expectedDepth != actualDepth || expectedSource != actualSource ||
			expected.palette != actual.palette ||
			!guardsIntact(expectedDestination, 0xa55a) || !guardsIntact(actualDestination, 0xa55a) ||
			!guardsIntact(expectedDepth, 0x5aa5) || !guardsIntact(actualDepth, 0x5aa5))
		{
			std::fprintf(stderr, "Palette span mismatch: %s case %zu, %s, BOOLEAN %u/%u, "
				"destination=%d depth=%d\n", group, ordinal, names[variant],
				static_cast<unsigned>(expectedResult), static_cast<unsigned>(actualResult),
				expectedDestination == actualDestination, expectedDepth == actualDepth);
			return false;
		}
	}
	return true;
}

bool paletteSpanDifferentialCorpus()
{
	std::size_t cases = 0;
	const auto check = [&cases](const SpanCase& test, const char* group)
	{
		return comparePaletteSpan(test, group, ++cases);
	};
	// Small dimensions include empty frames, one-pixel runs, short rows and literal zero.
	for (UINT16 width = 0; width <= 4; ++width)
		for (UINT16 height = 0; height <= 3; ++height)
			for (unsigned pattern = 0; pattern < 4; ++pattern)
			{
				SpanCase test;
				test.width = width;
				test.height = height;
				for (UINT16 y = 0; y < height; ++y)
				{
					test.data.push_back(0x80);
					for (UINT16 x = 0; x < (pattern == 3 ? width / 2 : width); ++x)
					{
						if (pattern == 1 || (pattern == 2 && (x + y) % 2)) test.data.push_back(0x81);
						else
						{
							test.data.push_back(1);
							const UINT8 indices[]{0, 1, 254, 255};
							test.data.push_back(indices[(x + y) % 4]);
						}
					}
					test.data.push_back(0);
				}
				if (!check(test, "small")) return false;
				test.x = test.y = -1;
				test.clip = {0, 0, 3, 2};
				test.globalClip = true;
				if (!check(test, "small-clipped")) return false;
			}

	for (bool transparent : {false, true})
	{
		SpanCase test;
		test.width = 129;
		test.data = {1, 255, static_cast<UINT8>(transparent ? 0xff : 127)};
		if (!transparent)
			for (unsigned i = 0; i < 127; ++i) test.data.push_back(static_cast<UINT8>(i * 17));
		test.data.insert(test.data.end(), {1, 0, 0});
		for (unsigned placement = 0; placement < 5; ++placement)
		{
			test.x = placement == 4 ? -130 : 2;
			test.clip = placement == 0 ? SGPRect{0, 0, 160, 12} : SGPRect{3, 1, 129, 2};
			test.globalClip = placement % 2 != 0;
			test.nullZ = placement == 3;
			if (!check(test, "run-127")) return false;
		}
	}

	// Every run length and all modulo-eight prefix/suffix tails exercise unrolled loops.
	for (UINT16 length = 1; length <= 127; ++length)
	{
		SpanCase test;
		test.width = length + 2;
		test.dataOffset = length % 4;
		test.data = {1, 255, static_cast<UINT8>(length)};
		for (UINT16 i = 0; i < length; ++i)
		{
			const UINT8 indices[]{0, 1, 254, 255, 7, 128, 42, 99};
			test.data.push_back(indices[i % 8]);
		}
		test.data.insert(test.data.end(), {1, 254, 0});
		if (!check(test, "run-length")) return false;
		for (INT32 trim = 0; trim < 8 && trim < length; ++trim)
		{
			test.globalClip = trim % 2 != 0;
			test.clip = {test.x + 1 + trim, test.y, test.x + 1 + length, test.y + 1};
			if (!check(test, "run-length-suffix")) return false;
			test.clip = {test.x + 1, test.y, test.x + 1 + length - trim, test.y + 1};
			if (!check(test, "run-length-prefix")) return false;
		}
		// Missing EOL, a truncated final run, and a truncated long run must keep
		// exactly the writes completed before the malformed token is encountered.
		for (std::size_t missing : {1u, 2u, 4u})
		{
			SpanCase truncated = test;
			truncated.clip = {0, 0, 160, 12};
			truncated.data.resize(test.data.size() - missing);
			if (!check(truncated, "run-length-truncated")) return false;
		}
	}

	SpanCase base;
	base.height = 3;
	base.data = {3, 0, 254, 255, 0, 0x81, 2, 7, 0, 0, 3, 9, 8, 7, 0};
	const SGPRect clips[]{{0, 0, 160, 12}, {3, 2, 4, 3}, {-6, -5, 5, 4},
		{-8, -7, -1, -1}, {6, 5, 2, 1}, {0, 0, 0, 0}, {200, 20, 210, 22}, {0, 0, 2, 1}};
	for (const SGPRect& clip : clips)
		for (bool global : {false, true})
			for (INT16 offset : {-3, 0, 2})
			{
				SpanCase test = base;
				test.clip = clip;
				test.globalClip = global;
				test.offsetX = offset;
				test.offsetY = static_cast<INT16>(-offset);
				test.dataOffset = 4;
				if (!check(test, "clip-offset")) return false;
			}
	for (UINT32 pitch : {0u, 1u, 2u, 3u, 14u, 15u, 288u, 289u, 320u, 321u})
		for (bool nullZ : {false, true})
		{
			SpanCase test = base;
			test.pitchBytes = pitch;
			test.nullZ = nullZ;
			if (!check(test, "pitch")) return false;
		}
	for (UINT16 incoming : {0, 50, 65535})
		for (INT32 existing : {0, 49, 50, 51, 65535})
		{
			SpanCase test = base;
			test.zValue = incoming;
			test.depthValue = existing;
			if (!check(test, "depth")) return false;
		}
	for (SpanInvalid invalid : {SpanInvalid::Destination, SpanInvalid::Object, SpanInvalid::Frames,
		SpanInvalid::Pixels, SpanInvalid::Index, SpanInvalid::ObjectCount, SpanInvalid::Palette})
		for (bool empty : {false, true})
		{
			SpanCase test = base;
			test.invalid = invalid;
			if (empty) test.width = test.height = 0;
			if (!check(test, "invalid")) return false;
		}

	// Every prefix includes missing headers, partial payloads and missing EOL, including
	// failures on skipped rows and after visible writes. Offscreen input still needs parsing.
	const SGPRect parserClips[]{{0, 0, 160, 12}, {0, 2, 160, 12}, {0, 0, 160, 2},
		{0, 8, 160, 10}, {20, 0, 30, 12}};
	for (std::size_t length = 0; length <= base.data.size(); ++length)
		for (const SGPRect& clip : parserClips)
		{
			SpanCase test = base;
			test.data.resize(length);
			test.dataOffset = 3;
			test.clip = clip;
			test.globalClip = length % 2 != 0;
			if (!check(test, "truncated")) return false;
		}
	const std::vector<UINT8> malformed[]{{4, 1, 2, 3, 4, 0}, {0x84, 0},
		{1, 7, 4, 1, 2, 3, 4, 0}, {1, 7, 0x83, 0}, {0x80}, {1, 7},
		{1, 7, 0, 2, 8}, {1, 0, 0, 0x83, 1, 8, 0}};
	for (const auto& data : malformed)
		for (const SGPRect& clip : parserClips)
		{
			SpanCase test = base;
			test.data = data;
			test.clip = clip;
			if (!check(test, "malformed")) return false;
		}
	for (SpanAlias alias : {SpanAlias::DestinationDepth, SpanAlias::PaletteDestination,
		SpanAlias::PaletteDepth, SpanAlias::SourceDepth})
		for (bool global : {false, true})
		{
			SpanCase test;
			test.alias = alias;
			test.globalClip = global;
			test.x = test.y = 0;
			test.width = 4;
			test.height = 2;
			test.data = {4, 1, 0, 1, 2, 0, 4, 0, 2, 1, 0, 0};
			if (alias == SpanAlias::SourceDepth)
			{
				test.x = 2;
				test.y = 1;
				test.width = test.height = 1;
				test.zValue = 0x7f7f;
				test.data = {1, 7, 0};
			}
			if (!check(test, "alias")) return false;
		}

	const UINT16 aliasLengths[]{1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127};
	for (UINT16 length : aliasLengths)
		for (INT32 shift : {-1, 0, 1, 2, 3, 7, 8, -static_cast<INT32>(length + 1),
			-static_cast<INT32>(length + 3)})
			for (bool clipped : {false, true})
			{
				SpanCase test;
				test.alias = SpanAlias::SourceDestination;
				test.x = 16;
				test.width = length + 1;
				test.depthValue = 49;
				test.dataOffset = 3;
				test.sourceByteOffset = shift;
				test.data = {static_cast<UINT8>(length)};
				test.data.insert(test.data.end(), length, 7);
				test.data.insert(test.data.end(), {1, 7, 0});
				// Positive/near-zero shifts overwrite later indices. The two large
				// negative shifts put the next run header/EOL under the first store.
				// Repeated palette bytes make those mutations endian-independent.
				if (clipped) test.clip = {test.x + 1, test.y, test.x + length, test.y + 1};
				test.globalClip = clipped;
				if (!check(test, "source-destination-alias")) return false;
			}
	for (SpanAlias alias : {SpanAlias::PaletteDestination, SpanAlias::PaletteDepth})
		for (UINT16 length : aliasLengths)
			for (INT32 shift : {-8, -1, 1, 8})
				for (INT32 trim : {0, 1, 3})
				{
					SpanCase test;
					test.alias = alias;
					test.paletteOffset = shift;
					test.x = 16;
					test.y = 0;
					test.width = length;
					test.depthValue = 49;
					test.data = {static_cast<UINT8>(length), 200};
					// Each later lookup reads the preceding destination/depth word,
					// propagating earlier stores across both unrolled groups and tails.
					for (UINT16 i = 1; i < length; ++i)
						test.data.push_back(static_cast<UINT8>(test.x + i - 1 - shift));
					test.data.push_back(0);
					test.clip = {test.x + trim, 0, test.x + length - trim, 1};
					test.globalClip = trim % 2 != 0;
					if (!check(test, "shifted-palette-alias")) return false;
				}

	UINT32 seed = 0x5eed1234;
	const auto random = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed; };
	for (unsigned trial = 0; trial < 256; ++trial)
	{
		SpanCase test;
		const UINT16 widths[]{0, 1, 2, 3, 7, 15, 31, 63, 127};
		test.width = widths[random() % 9];
		test.height = static_cast<UINT16>(random() % 6);
		test.x = static_cast<INT32>(random() % 18) - 7;
		test.y = static_cast<INT32>(random() % 6) - 2;
		test.offsetX = static_cast<INT16>(random() % 7) - 3;
		test.offsetY = static_cast<INT16>(random() % 5) - 2;
		test.dataOffset = random() % 5;
		test.pitchBytes = 2 * (144 + random() % 17) + trial % 2;
		test.globalClip = trial % 2 != 0;
		test.nullZ = trial % 5 == 0;
		test.zValue = trial % 7 == 0 ? 65535 : trial % 11 == 0 ? 0 : 50;
		test.clip = {static_cast<INT32>(random() % 12) - 4, static_cast<INT32>(random() % 5) - 2,
			static_cast<INT32>(random() % 145), static_cast<INT32>(random() % 13)};
		for (UINT16 y = 0; y < test.height; ++y)
		{
			const UINT32 rowWidth = trial % 9 == 0 ? random() % (test.width + 1) : test.width;
			for (UINT32 x = 0; x < rowWidth;)
			{
				if (random() % 4 == 0) test.data.push_back(0x80);
				const UINT32 count = 1 + random() % std::min<UINT32>(127, rowWidth - x);
				const bool transparent = random() % 3 == 0;
				test.data.push_back(static_cast<UINT8>(count | (transparent ? 0x80 : 0)));
				if (!transparent)
					for (UINT32 i = 0; i < count; ++i) test.data.push_back(static_cast<UINT8>(random() >> 24));
				x += count;
			}
			test.data.push_back(0);
		}
		if (trial % 4 == 0 && !test.data.empty()) test.data.resize(random() % (test.data.size() + 1));
		if (trial % 13 == 0) test.data.insert(test.data.begin(), 0xff);
		if (!check(test, "seed-5eed1234")) return false;
	}
	std::fprintf(stderr, "Palette span differential: %zu cases, %zu wrapper comparisons\n", cases, cases * 6);
	return true;
}
}

int main()
{
	std::iota(std::begin(ShadeTable), std::end(ShadeTable), static_cast<UINT16>(0));
	std::iota(std::begin(IntensityTable), std::end(IntensityTable), static_cast<UINT16>(0));
	SGPRect clippingRect{0, 0, static_cast<INT32>(kWidth), static_cast<INT32>(kHeight)};
	SetClippingRect(&clippingRect);
	if (!paletteShadowAndIgnore()) return 1;
	if (!outlineShadowShadesSilhouette()) return 2;
	if (!depthRulesAndIgnoredShadowWrite()) return 3;
	if (!znbShadowEqualDepthRules()) return 4;
	if (!obscuredPixelationPreservesOccluderDepth()) return 5;
	if (!rectangleEndpointAndPatternSemantics()) return 6;
	if (!obscuredColorAndOutlineDepthRules()) return 7;
	if (!clippingAndMirroring()) return 8;
	if (!monoTransparentRuns()) return 9;
	if (!alphaAndShadowMarker()) return 10;
	if (!obscuredPixelationDoesNotLeakShadow()) return 11;
	if (!zStripDepthAndBurnThrough()) return 12;
	if (!worldRenderUtilityPolicies()) return 13;
	if (!rawCopyClipsToRegisteredBuffers()) return 14;
	if (!paletteSpanDifferentialCorpus()) return 15;
	return 0;
}
