#ifndef JA2_PRESENTATION_PALETTE_H
#define JA2_PRESENTATION_PALETTE_H

#include "types.h"

// Legacy engine palette entry. Its layout intentionally matches the Win32
// PALETTEENTRY ABI, but the type itself is platform neutral.
typedef struct tagSGPPaletteEntry
{
	UINT8 peRed;
	UINT8 peGreen;
	UINT8 peBlue;
	UINT8 peFlags;
} SGPPaletteEntry;

#endif
