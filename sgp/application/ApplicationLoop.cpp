#include "application/ApplicationLoop.h"

namespace ja2
{
namespace application
{
ApplicationLoopResult RunApplicationLoop(
	ApplicationLoopClient& client, Platform::ApplicationHost& host)
{
	while (client.isRunning())
	{
		if (client.updateClock())
		{
			if (client.isActive())
			{
				client.runFrame();
			}
			else
			{
				client.runBackgroundFrame();
			}
		}

		const Platform::HostPumpResult pumpResult =
			host.waitAndDispatchOne(client.nextWakeMilliseconds());
		switch (pumpResult.status)
		{
			case Platform::HostPumpStatus::deadlineReached:
			case Platform::HostPumpStatus::eventDispatched:
				break;
			case Platform::HostPumpStatus::quitRequested:
				return {ApplicationLoopExitReason::quitRequested,
					pumpResult.exitCode};
			case Platform::HostPumpStatus::failed:
				return {ApplicationLoopExitReason::hostFailure, 0};
		}
	}

	return {ApplicationLoopExitReason::stopped, 0};
}
}
}
