#ifndef JA2_PIXEL_BLITTERS_H
#define JA2_PIXEL_BLITTERS_H

#include "types.h"

// Flat-buffer conversion used by the image loader. It lives in the legacy
// blitter implementation, but does not depend on video objects or surfaces.
BOOLEAN Blt32BPPTo16BPPTrans(
	UINT16* pDest,
	UINT32 uiDestPitch,
	UINT32* pSrc,
	UINT32 uiSrcPitch,
	INT32 iDestXPos,
	INT32 iDestYPos,
	INT32 iSrcXPos,
	INT32 iSrcYPos,
	UINT32 uiWidth,
	UINT32 uiHeight);

#endif
