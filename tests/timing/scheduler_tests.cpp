#include "timing/MainLoopScheduler.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

void testNormalScheduling()
{
	ja2::timing::MainLoopScheduler scheduler;
	scheduler.reset(1000, 16000);

	auto update = scheduler.update(1000, 10000, 16000, false);
	require(update.ticksDue == 1, "initial fixed tick was not delivered");
	require(!update.notificationDue, "notification fired during initialization");
	require(scheduler.timeUntilNext(1000, false) == 10000,
		"initial wake deadline is wrong");

	update = scheduler.update(10999, 10000, 16000, false);
	require(update.ticksDue == 0 && !update.notificationDue,
		"scheduler fired before its deadline");
	update = scheduler.update(11000, 10000, 16000, false);
	require(update.ticksDue == 1 && !update.notificationDue,
		"fixed tick did not fire at equality");
	update = scheduler.update(17000, 10000, 16000, false);
	require(update.ticksDue == 0 && update.notificationDue,
		"periodic game-loop notification did not fire");
	require(scheduler.timeUntilNext(17000, false) == 4000,
		"next fixed tick was not selected as the wake deadline");
}

void testDelayedCatchUp()
{
	ja2::timing::MainLoopScheduler scheduler;
	scheduler.reset(0, 16000);
	require(scheduler.update(0, 10000, 16000, false).ticksDue == 1,
		"initial tick missing");
	const auto delayed = scheduler.update(35000, 10000, 16000, false);
	require(delayed.ticksDue == 3,
		"scheduler did not recover fixed ticks after delayed main-loop work");
	require(delayed.notificationDue, "delayed notification was not reported");
	require(scheduler.timeUntilNext(35000, false) == 5000,
		"catch-up did not preserve the fixed-tick phase");
}

void testFastForwardScheduling()
{
	ja2::timing::MainLoopScheduler scheduler;
	scheduler.reset(500, 16000);
	const auto update = scheduler.update(500, 1000, 16000, true);
	require(update.ticksDue == 1, "fast-forward initial tick missing");
	require(update.notificationDue, "fast-forward did not request a game loop");
	require(scheduler.timeUntilNext(500, true) == 0,
		"fast-forward attempted to sleep");

	const auto later = scheduler.update(3500, 1000, 16000, true);
	require(later.ticksDue == 3 && later.notificationDue,
		"fast-forward catch-up cadence is wrong");
}

void testLegacyCountdowns()
{
	std::int32_t counter = 25;
	require(!ja2::timing::AdvanceLegacyCountdown(counter, 2, 10) && counter == 5,
		"partial countdown advancement is wrong");
	require(ja2::timing::AdvanceLegacyCountdown(counter, 1, 10) && counter == 0,
		"sub-step countdown did not signal expiry");

	counter = 20;
	require(!ja2::timing::AdvanceLegacyCountdown(counter, 2, 10) && counter == 0,
		"exact-step countdown changed its historical notification behavior");
	require(!ja2::timing::AdvanceLegacyCountdown(counter, 5, 10),
		"zero countdown signalled repeatedly");

	counter = -1;
	require(ja2::timing::AdvanceLegacyCountdown(counter, 1, 10) && counter == 0,
		"negative legacy countdown did not normalize on its next tick");
}
}

int main()
{
	try
	{
		testNormalScheduling();
		testDelayedCatchUp();
		testFastForwardScheduling();
		testLegacyCountdowns();
		std::cout << "main-loop scheduler tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "main-loop scheduler tests failed: " << error.what() << '\n';
		return 1;
	}
}
