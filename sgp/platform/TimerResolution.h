#ifndef JA2_PLATFORM_TIMERRESOLUTION_H
#define JA2_PLATFORM_TIMERRESOLUTION_H

namespace Platform
{
// Preserves short-sleep precision for the existing timer thread. This becomes
// unnecessary once thread scheduling moves to its portable backend.
void EnableHighResolutionSleep() noexcept;
void DisableHighResolutionSleep() noexcept;
}

#endif
