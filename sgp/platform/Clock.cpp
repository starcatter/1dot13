#include "Clock.h"

#include <chrono>

namespace Platform
{
namespace
{
using SteadyClock = std::chrono::steady_clock;

const SteadyClock::time_point& processClockEpoch() noexcept
{
	static const SteadyClock::time_point epoch = SteadyClock::now();
	return epoch;
}
}

std::uint64_t GetClockMicroseconds() noexcept
{
	static_assert(SteadyClock::is_steady, "JA2 requires a monotonic steady clock");
	const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
		SteadyClock::now() - processClockEpoch()).count();
	return elapsed > 0 ? static_cast<std::uint64_t>(elapsed) : 0;
}

std::uint64_t GetClockMilliseconds64() noexcept
{
	return GetClockMicroseconds() / 1000;
}

std::uint32_t GetClockMilliseconds() noexcept
{
	return static_cast<std::uint32_t>(GetClockMilliseconds64());
}

std::uint32_t ClockMillisecondsRemaining(
	std::uint32_t now, std::uint32_t deadline) noexcept
{
	const std::uint32_t remaining = deadline - now;
	return remaining < 0x80000000U ? remaining : 0;
}
}
