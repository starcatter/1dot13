#ifndef JA2_PLATFORM_CLOCK_H
#define JA2_PLATFORM_CLOCK_H

#include <cstdint>

namespace Platform
{
// Process-relative monotonic time. The epoch is intentionally unspecified;
// callers compare timestamps or durations rather than interpreting wall time.
std::uint64_t GetClockMicroseconds() noexcept;
std::uint64_t GetClockMilliseconds64() noexcept;
std::uint32_t GetClockMilliseconds() noexcept;

// Returns the remaining duration for a 32-bit millisecond deadline. This keeps
// the legacy wraparound behavior well-defined for delays shorter than 2^31 ms.
std::uint32_t ClockMillisecondsRemaining(
	std::uint32_t now, std::uint32_t deadline) noexcept;
}

#endif
