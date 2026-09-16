#ifndef JA2_VIDEO_WINDOWS_H
#define JA2_VIDEO_WINDOWS_H

#ifdef _WIN32

#include "types.h"

#include <windows.h>
#include <ddraw.h>

// Native host and DirectDraw access needed only by the current Windows
// implementation. Engine-facing presentation operations belong in video.h.
extern HWND ghWindow;

BOOLEAN InitializeVideoManager(
	HINSTANCE instance, UINT16 commandShow, void* windowProcedure);

LPDIRECTDRAW2 GetDirectDraw2Object(void);
LPDIRECTDRAWSURFACE2 GetPrimarySurfaceObject(void);
LPDIRECTDRAWSURFACE2 GetBackBufferObject(void);
LPDIRECTDRAWSURFACE2 GetFrameBufferObject(void);
LPDIRECTDRAWSURFACE2 GetMouseBufferObject(void);

#endif

#endif
