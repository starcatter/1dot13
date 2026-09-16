#ifndef JA2_PLATFORM_WINDOW_H
#define JA2_PLATFORM_WINDOW_H

namespace Platform
{
// Requests a host-window state change without exposing a native window handle
// to engine input policy. Hosts without a window treat the request as a no-op.
void MinimizeMainWindow() noexcept;
}

#endif
