#include "INIReader.h"
#include "WinFont.h"
#include "input.h"
#include "platform/LegacyKeyCodes.h"
#include "platform/NativeFonts.h"
#include "video.h"

#include <cstddef>
#include <type_traits>

namespace
{
constexpr InputAtom packedMousePosition{0, 0, MOUSE_POS, 0, 0x12345678U};
static_assert(GETXPOS(&packedMousePosition) == 0x5678U);
static_assert(GETYPOS(&packedMousePosition) == 0x1234U);
static_assert(std::is_standard_layout<SGPPoint>::value);
static_assert(sizeof(SGPPoint) == 2 * sizeof(INT32));
static_assert(offsetof(SGPPoint, iX) == 0);
static_assert(offsetof(SGPPoint, iY) == sizeof(INT32));
static_assert(std::is_constructible<CIniReader, const CHAR8*>::value);
static_assert(std::is_same<decltype(&WinFontStringPixLength),
	INT16 (*)(const CHAR16*, INT32)>::value);
static_assert(LegacyKeyCode::kLBUTTON == 0x01);
static_assert(LegacyKeyCode::kESCAPE == 0x1b);
static_assert(LegacyKeyCode::kF1 == 0x70);
static_assert(LegacyKeyCode::kOEM_CLEAR == 0xfe);
}

int main()
{
	return !Platform::SupportsNativeFonts() &&
		Platform::ResolveNativeFontSetting(1) == 0 ? 0 : 1;
}
