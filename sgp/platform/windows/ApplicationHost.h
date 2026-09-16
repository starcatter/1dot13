#ifndef JA2_PLATFORM_WINDOWS_APPLICATION_HOST_H
#define JA2_PLATFORM_WINDOWS_APPLICATION_HOST_H

#include "platform/ApplicationHost.h"

namespace Platform
{
class WindowsApplicationHost final : public ApplicationHost
{
public:
	HostPumpResult waitAndDispatchOne(
		std::uint32_t timeoutMilliseconds) override;
};
}

#endif
