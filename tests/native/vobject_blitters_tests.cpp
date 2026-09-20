#include "vobject_blitters.h"
#include "renderworld.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <map>
#include <numeric>

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
}

int main()
{
	std::iota(std::begin(ShadeTable), std::end(ShadeTable), static_cast<UINT16>(0));
	std::iota(std::begin(IntensityTable), std::end(IntensityTable), static_cast<UINT16>(0));
	if (!paletteShadowAndIgnore()) return 1;
	if (!outlineShadowShadesSilhouette()) return 2;
	if (!depthRulesAndIgnoredShadowWrite()) return 3;
	if (!obscuredColorAndOutlineDepthRules()) return 4;
	if (!clippingAndMirroring()) return 5;
	if (!monoTransparentRuns()) return 6;
	if (!alphaAndShadowMarker()) return 7;
	if (!obscuredPixelationDoesNotLeakShadow()) return 8;
	if (!zStripDepthAndBurnThrough()) return 9;
	if (!worldRenderUtilityPolicies()) return 10;
	if (!rawCopyClipsToRegisteredBuffers()) return 11;
	return 0;
}
