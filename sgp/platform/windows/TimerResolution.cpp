#include "platform/TimerResolution.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>

namespace Platform
{
namespace
{
bool resolutionEnabled = false;
}

void EnableHighResolutionSleep() noexcept
{
	if (!resolutionEnabled)
		resolutionEnabled = timeBeginPeriod(1) == TIMERR_NOERROR;
}

void DisableHighResolutionSleep() noexcept
{
	if (!resolutionEnabled) return;
	timeEndPeriod(1);
	resolutionEnabled = false;
}
}
