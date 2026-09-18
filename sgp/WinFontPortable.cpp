#include "WinFont.h"

// Native system-font rendering is deliberately disabled until it has a backend.
// The game falls back to its portable bitmap fonts; these symbols preserve the
// optional WinFont interface for callers that are compiled unconditionally.

INT32 WinFontMap[MAX_WINFONTMAP]{};
INT32 TOOLTIP_IFONT = -1;
INT32 TOOLTIP_IFONT_BOLD = -1;

void InitWinFonts() {}
void ShutdownWinFonts() {}
void InitTooltipFonts() {}
void ShutdownTooltipFonts() {}
void DeleteWinFont(INT32) {}
void SetWinFontBackColor(INT32, COLORVAL*) {}
void SetWinFontForeColor(INT32, COLORVAL*) {}
void PrintWinFont(UINT32, INT32, INT32, INT32, STR16, ...) {}
INT16 WinFontStringPixLength(const CHAR16*, INT32) { return 0; }
INT16 GetWinFontHeight(INT32) { return 0; }
