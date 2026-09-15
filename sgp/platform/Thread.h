#pragma once

#include <functional>

namespace Platform
{
	using ThreadTask = std::function<void()>;

	// Starts a fire-and-forget task. The task owns its captured state, and an
	// exception escaping it is contained rather than terminating the process.
	// Returns false if the task is empty or the host cannot create the thread.
	bool RunDetached(ThreadTask task) noexcept;
}
