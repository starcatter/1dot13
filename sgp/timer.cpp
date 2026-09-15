#include "platform/Clock.h"
#include "timer.h"

namespace
{
std::uint64_t clockManagerEpoch = 0;
}

bool InitializeClockManager(void)
{

	clockManagerEpoch = Platform::GetClockMilliseconds64();
	return true;
}

void	ShutdownClockManager(void)
{

}

TIMER	GetClock(void)
{
	return static_cast<TIMER>(Platform::GetClockMilliseconds64() - clockManagerEpoch);
}

TIMER	SetCountdownClock(std::uint32_t uiTimeToElapse)
{
	return GetClock() + uiTimeToElapse;
}

std::uint32_t ClockIsTicking(TIMER uiTimer)
{
	return Platform::ClockMillisecondsRemaining(GetClock(), uiTimer);
}
