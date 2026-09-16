#include "platform/NativeFonts.h"

namespace Platform
{
bool SupportsNativeFonts() noexcept { return false; }
int ResolveNativeFontSetting(int) noexcept { return 0; }
}
