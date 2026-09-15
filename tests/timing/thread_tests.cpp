#include "platform/Thread.h"

#include <chrono>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace
{
	using namespace std::chrono_literals;

	bool check(bool condition, const char* message)
	{
		if (!condition)
			std::cerr << "FAIL: " << message << '\n';
		return condition;
	}

	bool detachedTaskRunsWithOwnedState()
	{
		auto result = std::make_shared<std::promise<int>>();
		std::future<int> future = result->get_future();
		auto value = std::make_shared<int>(42);

		const bool started = Platform::RunDetached([value, result]() {
			result->set_value(*value);
		});
		value.reset();

		return check(started, "detached task did not start") &&
			check(future.wait_for(2s) == std::future_status::ready,
				"detached task did not complete") &&
			check(future.get() == 42, "detached task lost its captured state");
	}

	bool emptyTaskIsRejected()
	{
		return check(!Platform::RunDetached({}), "empty detached task was accepted");
	}

	bool taskExceptionIsContained()
	{
		auto entered = std::make_shared<std::promise<void>>();
		std::future<void> future = entered->get_future();
		const bool started = Platform::RunDetached([entered]() {
			entered->set_value();
			throw std::runtime_error("expected test exception");
		});

		return check(started, "throwing detached task did not start") &&
			check(future.wait_for(2s) == std::future_status::ready,
				"throwing detached task did not run");
	}
}

int main()
{
	const bool passed = detachedTaskRunsWithOwnedState() &&
		emptyTaskIsRejected() &&
		taskExceptionIsContained();

	if (passed)
		std::cout << "portable thread tests passed\n";
	return passed ? 0 : 1;
}
