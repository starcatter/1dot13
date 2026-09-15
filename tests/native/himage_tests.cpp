#include "himage.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace
{
struct LegacyEtrleLayout
{
	UINT8* pPixData8;
	UINT32 uiSizePixData;
	ETRLEObject* pETRLEObject;
	UINT16 usNumberOfObjects;
};

struct LegacyImageLayout
{
	UINT16 usWidth;
	UINT16 usHeight;
	UINT8 ubBitDepth;
	UINT16 fFlags;
	SGPFILENAME ImageFile;
	UINT32 iFileLoader;
	SGPPaletteEntry* pPalette;
	UINT16* pui16BPPPalette;
	UINT8* pAppData;
	UINT32 uiAppDataSize;
	union
	{
		PTR pImageData;
		PTR pCompressedImageData;
		UINT8* p8BPPData;
		UINT16* p16BPPData;
		UINT32* p32BPPData;
		LegacyEtrleLayout etrle;
	};
};

static_assert(std::is_standard_layout_v<image_type>);
static_assert(sizeof(image_type) == sizeof(LegacyImageLayout));
static_assert(alignof(image_type) == alignof(LegacyImageLayout));
static_assert(offsetof(image_type, pImageData) == offsetof(LegacyImageLayout, pImageData));
static_assert(offsetof(image_type, pPixData8) == offsetof(LegacyImageLayout, etrle.pPixData8));
static_assert(offsetof(image_type, uiSizePixData) == offsetof(LegacyImageLayout, etrle.uiSizePixData));
static_assert(offsetof(image_type, pETRLEObject) == offsetof(LegacyImageLayout, etrle.pETRLEObject));
static_assert(offsetof(image_type, usNumberOfObjects) == offsetof(LegacyImageLayout, etrle.usNumberOfObjects));

constexpr UINT32 rgb(UINT8 red, UINT8 green, UINT8 blue)
{
	return static_cast<UINT32>(red) |
		(static_cast<UINT32>(green) << 8) |
		(static_cast<UINT32>(blue) << 16);
}

void useRgb565()
{
	gusAlphaMask = 0;
	gusRedMask = 0xf800;
	gusGreenMask = 0x07e0;
	gusBlueMask = 0x001f;
	gusRedShift = 8;
	gusGreenShift = 3;
	gusBlueShift = -3;
}
}

// himage owns format dispatch but the loader implementations are outside the
// current native-core boundary. These stubs let this test link that object and
// exercise its platform-neutral image operations in isolation.
BOOLEAN LoadTGAFileToImage(HIMAGE, UINT16) { return FALSE; }
BOOLEAN LoadPCXFileToImage(HIMAGE, UINT16) { return FALSE; }
BOOLEAN LoadSTCIFileToImage(HIMAGE, UINT16) { return FALSE; }
bool LoadPNGFileToImage(HIMAGE, UINT16) { return false; }
bool LoadJPCFileToImage(HIMAGE, UINT16) { return false; }
BOOLEAN Blt32BPPTo16BPPTrans(
	UINT16*, UINT32, UINT32*, UINT32, INT32, INT32, INT32, INT32, UINT32, UINT32)
{
	return FALSE;
}

int main()
{
	useRgb565();
	if (Get16BPPColor(rgb(255, 255, 255)) != 0xffff ||
		Get16BPPColor(rgb(255, 0, 0)) != 0xf800 ||
		Get16BPPColor(rgb(0, 255, 0)) != 0x07e0 ||
		Get16BPPColor(rgb(0, 0, 255)) != 0x001f ||
		Get16BPPColor(rgb(0, 0, 1)) != 0x0001)
	{
		return 1;
	}

	std::array<SGPPaletteEntry, 256> palette{};
	palette[1] = SGPPaletteEntry{200, 100, 50, 0};
	UINT16* normal = Create16BPPPalette(palette.data());
	UINT16* saturated = Create16BPPPaletteShaded(palette.data(), 2048, 2048, 2048, FALSE);
	if (normal == nullptr || saturated == nullptr ||
		normal[1] != Get16BPPColor(rgb(200, 100, 50)) || saturated[1] != 0xffff)
	{
		std::free(normal);
		std::free(saturated);
		return 2;
	}
	std::free(normal);
	std::free(saturated);

	std::array<UINT8, 12> source8{{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}};
	std::array<UINT8, 30> destination8{};
	image_type image{};
	image.usWidth = 4;
	image.usHeight = 3;
	image.ubBitDepth = 8;
	image.p8BPPData = source8.data();
	SGPRect sourceRect{1, 1, 2, 2};
	if (!Copy8BPPImageTo8BPPBuffer(&image, destination8.data(), 6, 5, 2, 1, &sourceRect) ||
		destination8[8] != 6 || destination8[9] != 7 ||
		destination8[14] != 10 || destination8[15] != 11)
	{
		return 3;
	}

	std::array<UINT16, 6> source16{{0x1111, 0x2222, 0x3333, 0x4444, 0x5555, 0x6666}};
	std::array<UINT16, 12> destination16{};
	image.usWidth = 3;
	image.usHeight = 2;
	image.ubBitDepth = 16;
	image.p16BPPData = source16.data();
	sourceRect = SGPRect{1, 0, 2, 1};
	if (!Copy16BPPImageTo16BPPBuffer(
			&image, reinterpret_cast<BYTE*>(destination16.data()), 4, 3, 1, 1, &sourceRect) ||
		destination16[5] != 0x2222 || destination16[6] != 0x3333 ||
		destination16[9] != 0x5555 || destination16[10] != 0x6666)
	{
		return 4;
	}

	std::array<UINT8, 5> etrlePixels{{7, 8, 9, 10, 11}};
	std::array<ETRLEObject, 2> etrleObjects{{
		ETRLEObject{0, 2, -1, 2, 3, 4},
		ETRLEObject{2, 3, 5, -6, 7, 8},
	}};
	image.pPixData8 = etrlePixels.data();
	image.uiSizePixData = etrlePixels.size();
	image.pETRLEObject = etrleObjects.data();
	image.usNumberOfObjects = etrleObjects.size();
	ETRLEData copied{};
	if (!GetETRLEImageData(&image, &copied) ||
		copied.pPixData == etrlePixels.data() || copied.pETRLEObject == etrleObjects.data() ||
		copied.uiSizePixData != etrlePixels.size() ||
		copied.usNumberOfObjects != etrleObjects.size() ||
		std::memcmp(copied.pPixData, etrlePixels.data(), etrlePixels.size()) != 0 ||
		std::memcmp(copied.pETRLEObject, etrleObjects.data(), sizeof(etrleObjects)) != 0)
	{
		std::free(copied.pPixData);
		std::free(copied.pETRLEObject);
		return 5;
	}
	std::free(copied.pPixData);
	std::free(copied.pETRLEObject);

	std::array<UINT16, 4> pixels{{0xf81f, 0x07e0, 0xffff, 0}};
	ConvertRGBDistribution565To555(pixels.data(), pixels.size());
	if (pixels != std::array<UINT16, 4>{{0x7c1f, 0x03e0, 0x7fff, 0}})
	{
		return 6;
	}

	return 0;
}
