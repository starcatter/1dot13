#ifndef JA2_VIDEO_WINDOWS_H
#define JA2_VIDEO_WINDOWS_H

#ifdef _WIN32

#include "types.h"

#include <windows.h>

// Native host access needed only by the current Windows implementation.
// Engine-facing presentation operations belong in video.h.
extern HWND ghWindow;

BOOLEAN InitializeVideoManager(
	HINSTANCE instance, UINT16 commandShow, void* windowProcedure);

#endif

#endif
