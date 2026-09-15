#include "MainLoopScheduler.h"

#include <algorithm>

namespace ja2
{
namespace timing
{
namespace
{
std::uint64_t positiveInterval(std::uint64_t interval) noexcept
{
	return interval == 0 ? 1 : interval;
}
}

void MainLoopScheduler::reset(std::uint64_t nowMicroseconds,
	std::uint64_t notificationIntervalMicroseconds) noexcept
{
	nextTickMicroseconds_ = nowMicroseconds;
	nextNotificationMicroseconds_ = nowMicroseconds +
		positiveInterval(notificationIntervalMicroseconds);
}

SchedulerUpdate MainLoopScheduler::update(std::uint64_t nowMicroseconds,
	std::uint64_t tickIntervalMicroseconds,
	std::uint64_t notificationIntervalMicroseconds,
	bool fastForward) noexcept
{
	const std::uint64_t tickInterval = positiveInterval(tickIntervalMicroseconds);
	std::uint64_t ticksDue = 0;
	if (nowMicroseconds >= nextTickMicroseconds_)
	{
		ticksDue = (nowMicroseconds - nextTickMicroseconds_) / tickInterval + 1;
		nextTickMicroseconds_ += ticksDue * tickInterval;
	}

	const bool notificationDue =
		fastForward || nowMicroseconds >= nextNotificationMicroseconds_;
	if (notificationDue)
	{
		nextNotificationMicroseconds_ = nowMicroseconds +
			positiveInterval(notificationIntervalMicroseconds);
	}

	return {ticksDue, notificationDue};
}

std::uint64_t MainLoopScheduler::timeUntilNext(
	std::uint64_t nowMicroseconds, bool fastForward) const noexcept
{
	if (fastForward) return 0;
	const std::uint64_t untilTick = nextTickMicroseconds_ > nowMicroseconds
		? nextTickMicroseconds_ - nowMicroseconds : 0;
	const std::uint64_t untilNotification =
		nextNotificationMicroseconds_ > nowMicroseconds
		? nextNotificationMicroseconds_ - nowMicroseconds : 0;
	return std::min(untilTick, untilNotification);
}

bool AdvanceLegacyCountdown(std::int32_t& counter,
	std::uint64_t ticks, std::int32_t tickMilliseconds) noexcept
{
	if (ticks == 0 || counter == 0) return false;
	if (tickMilliseconds <= 0) return false;
	if (counter < tickMilliseconds)
	{
		counter = 0;
		return true;
	}

	const std::uint64_t value = static_cast<std::uint64_t>(counter);
	const std::uint64_t step = static_cast<std::uint64_t>(tickMilliseconds);
	const std::uint64_t ticksToZero = (value + step - 1) / step;
	if (ticks >= ticksToZero)
	{
		counter = 0;
		return value % step != 0;
	}

	counter -= static_cast<std::int32_t>(ticks * step);
	return false;
}
}
}
