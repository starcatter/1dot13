#include "platform/NativeFonts.h"

namespace Platform
{
bool SupportsNativeFonts() noexcept { return true; }
int ResolveNativeFontSetting(int requestedSetting) noexcept { return requestedSetting; }
}
