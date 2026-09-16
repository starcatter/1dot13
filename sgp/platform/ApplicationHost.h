#ifndef JA2_PLATFORM_APPLICATION_HOST_H
#define JA2_PLATFORM_APPLICATION_HOST_H

#include <cstdint>

namespace Platform
{
enum class HostPumpStatus
{
	deadlineReached,
	eventDispatched,
	quitRequested,
	failed
};

struct HostPumpResult
{
	HostPumpStatus status;
	int exitCode = 0;
};

// Waits until the supplied main-loop deadline or one host event is available.
// If an event is available, exactly one event is removed and dispatched before
// this function returns. This preserves the legacy Windows host's scheduling
// behavior and maps directly to an SDL wait/poll implementation later.
class ApplicationHost
{
public:
	virtual ~ApplicationHost() = default;
	virtual HostPumpResult waitAndDispatchOne(
		std::uint32_t timeoutMilliseconds) = 0;
};
}

#endif
