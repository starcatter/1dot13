#include "video.h"
#include "WinFont.h"

#include <type_traits>

int main()
{
	static_assert(BUFFER_READY != BUFFER_DIRTY);
	static_assert(VIDEO_NO_CURSOR > MAX_CURSOR_WIDTH);
	static_assert(std::is_same<decltype(&InvalidateRegion),
		void (*)(INT32, INT32, INT32, INT32)>::value);
	static_assert(std::is_same<decltype(&LockFrameBuffer),
		PTR (*)(UINT32*)>::value);
	static_assert(std::is_same<decltype(&Set8BPPPalette),
		BOOLEAN (*)(SGPPaletteEntry*)>::value);
	static_assert(std::is_same<decltype(&WinFontStringPixLength),
		INT16 (*)(const CHAR16*, INT32)>::value);
	return 0;
}
