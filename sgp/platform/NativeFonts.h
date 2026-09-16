#pragma once

namespace Platform
{
// The current native-font renderer is a GDI escape hatch through DirectDraw.
// Windows retains it; portable hosts use JA2 bitmap fonts until there is a
// backend-neutral native-font renderer.
bool SupportsNativeFonts() noexcept;
int ResolveNativeFontSetting(int requestedSetting) noexcept;
}
