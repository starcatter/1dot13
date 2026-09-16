#ifndef JA2_APPLICATION_SHUTDOWN_ONCE_H
#define JA2_APPLICATION_SHUTDOWN_ONCE_H

#include <atomic>

namespace ja2
{
namespace application
{
// Coordinates normal, window-destruction, and emergency-exit paths without
// owning any subsystem-specific teardown policy. Once begin() succeeds, every
// later attempt is rejected, including reentrant attempts during shutdown.
class ShutdownOnce
{
public:
	enum class State
	{
		ready,
		inProgress,
		complete
	};

	ShutdownOnce() noexcept = default;
	ShutdownOnce(const ShutdownOnce&) = delete;
	ShutdownOnce& operator=(const ShutdownOnce&) = delete;

	bool begin() noexcept;
	void complete() noexcept;
	State state() const noexcept;

private:
	std::atomic<State> state_{State::ready};
};
}
}

#endif
