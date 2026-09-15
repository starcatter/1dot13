#ifndef JA2_TIMING_MAINLOOPSCHEDULER_H
#define JA2_TIMING_MAINLOOPSCHEDULER_H

#include <cstdint>

namespace ja2
{
namespace timing
{
struct SchedulerUpdate
{
	std::uint64_t ticksDue;
	bool notificationDue;
};

// Keeps host-clock scheduling separate from the engine's synthetic clocks.
// Callers decide what one fixed tick means and what work a notification runs.
class MainLoopScheduler
{
public:
	void reset(std::uint64_t nowMicroseconds,
		std::uint64_t notificationIntervalMicroseconds) noexcept;

	SchedulerUpdate update(std::uint64_t nowMicroseconds,
		std::uint64_t tickIntervalMicroseconds,
		std::uint64_t notificationIntervalMicroseconds,
		bool fastForward) noexcept;

	std::uint64_t timeUntilNext(std::uint64_t nowMicroseconds,
		bool fastForward) const noexcept;

private:
	std::uint64_t nextTickMicroseconds_ = 0;
	std::uint64_t nextNotificationMicroseconds_ = 0;
};

// Advances the legacy decrementing counters by fixed ticks while preserving
// their historical expiry behavior, including exact step-size boundaries.
bool AdvanceLegacyCountdown(std::int32_t& counter,
	std::uint64_t ticks, std::int32_t tickMilliseconds) noexcept;
}
}

#endif
