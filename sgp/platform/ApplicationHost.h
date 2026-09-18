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

// The legacy assertion screen runs its own nested game loop. Registering the
// process host lets that path pump events without reaching into Win32 or SDL.
inline ApplicationHost*& CurrentApplicationHostStorage() noexcept
{
	static ApplicationHost* host = nullptr;
	return host;
}

inline void SetCurrentApplicationHost(ApplicationHost* host) noexcept
{
	CurrentApplicationHostStorage() = host;
}

inline ApplicationHost* GetCurrentApplicationHost() noexcept
{
	return CurrentApplicationHostStorage();
}
}

#endif
