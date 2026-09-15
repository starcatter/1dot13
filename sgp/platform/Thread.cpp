#include "platform/Thread.h"

#include <thread>
#include <utility>

bool Platform::RunDetached(ThreadTask task) noexcept
{
	if (!task)
		return false;

	try
	{
		std::thread worker([task = std::move(task)]() noexcept {
			try
			{
				task();
			}
			catch (...)
			{
				// A background best-effort task must not terminate the game.
			}
		});
		worker.detach();
		return true;
	}
	catch (...)
	{
		return false;
	}
}
