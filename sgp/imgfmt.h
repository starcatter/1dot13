#if !defined( STCI_H )

#define STCI_H

// Sir-Tech's Crazy Image (STCI) file format specifications.	Each file is composed of:
// 1		ImageFileHeader, uncompressed
// *		Palette (STCI_INDEXED, size = uiNumberOfColours * PALETTE_ELEMENT_SIZE), uncompressed
// *		SubRectInfo's (usNumberOfRects > 0, size = usNumberOfSubRects * sizeof(SubRectInfo) ), uncompressed
// *		Bytes of image data, possibly compressed

#include "types.h"

#include <cstddef>

#define STCI_ID_STRING		"STCI"
#define STCI_ID_LEN			4

#define STCI_ETRLE_COMPRESSED		0x0020
#define STCI_ZLIB_COMPRESSED		0x0010
#define STCI_INDEXED						0x0008
#define STCI_RGB								0x0004
#define STCI_ALPHA							0x0002
#define STCI_TRANSPARENT				0x0001

// ETRLE defines
#define COMPRESS_TRANSPARENT				0x80
#define COMPRESS_NON_TRANSPARENT			0x00
#define COMPRESS_RUN_LIMIT					0x7F

// NB if you're going to change the header definition:
// - make sure that everything in this header is nicely aligned
// - don't exceed the 64-byte maximum
typedef struct
{
	UINT32	uiRedMask;
	UINT32	uiGreenMask;
	UINT32	uiBlueMask;
	UINT32	uiAlphaMask;
	UINT8		ubRedDepth;
	UINT8		ubGreenDepth;
	UINT8		ubBlueDepth;
	UINT8		ubAlphaDepth;
} STCIRGBHeader;

typedef struct
{ // For indexed files, the palette will contain 3 separate bytes for red, green, and blue
	UINT32	uiNumberOfColours;
	UINT16	usNumberOfSubImages;
	UINT8		ubRedDepth;
	UINT8		ubGreenDepth;
	UINT8		ubBlueDepth;
	UINT8		cIndexedUnused[11];
} STCIIndexedHeader;

typedef struct
{
	UINT8		cID[STCI_ID_LEN];
	UINT32	uiOriginalSize;
	UINT32	uiStoredSize; // equal to uiOriginalSize if data uncompressed
	UINT32	uiTransparentValue;
	UINT32	fFlags;
	UINT16	usHeight;
	UINT16	usWidth;
	union
	{
		STCIRGBHeader RGB;
		STCIIndexedHeader Indexed;
	};
	UINT8		ubDepth;	// size in bits of one pixel as stored in the file
	UINT32	uiAppDataSize;
	UINT8		cUnused[15];
} STCIHeader;

#define STCI_HEADER_SIZE 64

static_assert(sizeof(STCIRGBHeader) == 20, "STCI RGB header layout changed");
static_assert(sizeof(STCIIndexedHeader) == 20, "STCI indexed header layout changed");
// The legacy format serializes the first 64 bytes. Natural UINT32 alignment
// makes the in-memory structure 68 bytes; the final bytes are unused padding.
static_assert(offsetof(STCIHeader, ubDepth) == 44, "STCI pixel-depth offset changed");
static_assert(offsetof(STCIHeader, uiAppDataSize) == 48, "STCI app-data offset changed");
static_assert(offsetof(STCIHeader, cUnused) == 52, "STCI unused-tail offset changed");
static_assert(sizeof(STCIHeader) == 68, "STCI in-memory header layout changed");

typedef struct
{
	UINT32			uiDataOffset;
	UINT32			uiDataLength;
	INT16				sOffsetX;
	INT16				sOffsetY;
	UINT16			usHeight;
	UINT16			usWidth;
} STCISubImage;

#define STCI_SUBIMAGE_SIZE 16

typedef struct
{
	UINT8				ubRed;
	UINT8				ubGreen;
	UINT8				ubBlue;
} STCIPaletteElement;

#define STCI_PALETTE_ELEMENT_SIZE 3
#define STCI_8BIT_PALETTE_SIZE 768

#endif
