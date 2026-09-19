#include "application/ApplicationLoop.h"
#include "application/ShutdownOnce.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
class FakeClient final : public ja2::application::ApplicationLoopClient
{
public:
	bool isRunning() const override
	{
		calls.push_back("is-running");
		return running;
	}

	bool isActive() const override
	{
		calls.push_back("is-active");
		return active;
	}

	bool updateClock() override
	{
		calls.push_back("update-clock");
		return clockDue;
	}

	std::uint32_t nextWakeMilliseconds() const override
	{
		calls.push_back("next-wake");
		return nextWake;
	}

	void runFrame() override
	{
		calls.push_back("run-frame");
		if (stopDuringFrame)
		{
			running = false;
		}
	}

	void runBackgroundFrame() override
	{
		calls.push_back("run-background-frame");
	}

	mutable std::vector<std::string> calls;
	bool running = true;
	bool active = true;
	bool clockDue = true;
	bool stopDuringFrame = false;
	std::uint32_t nextWake = 17;
};

class FakeHost final : public Platform::ApplicationHost
{
public:
	explicit FakeHost(std::vector<Platform::HostPumpResult> results,
		FakeClient* clientToStop = nullptr)
		: results_(std::move(results)), clientToStop_(clientToStop)
	{
	}

	Platform::HostPumpResult waitAndDispatchOne(
		std::uint32_t timeoutMilliseconds) override
	{
		timeouts.push_back(timeoutMilliseconds);
		assert(nextResult_ < results_.size());
		const Platform::HostPumpResult result = results_[nextResult_++];
		if (clientToStop_ != nullptr &&
			result.status == Platform::HostPumpStatus::eventDispatched)
		{
			clientToStop_->running = false;
		}
		return result;
	}

	std::vector<std::uint32_t> timeouts;

private:
	std::vector<Platform::HostPumpResult> results_;
	std::size_t nextResult_ = 0;
	FakeClient* clientToStop_;
};

void TestFrameOrderingAndQuitCode()
{
	FakeClient client;
	FakeHost host({{Platform::HostPumpStatus::quitRequested, 37}});

	const ja2::application::ApplicationLoopResult result =
		ja2::application::RunApplicationLoop(client, host);

	assert(result.reason ==
		ja2::application::ApplicationLoopExitReason::quitRequested);
	assert(result.exitCode == 37);
	assert((client.calls == std::vector<std::string>{
		"is-running", "update-clock", "is-active", "run-frame", "next-wake"}));
	assert((host.timeouts == std::vector<std::uint32_t>{17}));
}

void TestInactiveClientRunsBackgroundFrameAndAdvancesClock()
{
	FakeClient client;
	client.active = false;
	FakeHost host({{Platform::HostPumpStatus::quitRequested, 0}});

	ja2::application::RunApplicationLoop(client, host);

	assert((client.calls == std::vector<std::string>{
		"is-running", "update-clock", "is-active", "run-background-frame",
		"next-wake"}));
}

void TestNonDueClockDoesNotQueryActivity()
{
	FakeClient client;
	client.clockDue = false;
	FakeHost host({{Platform::HostPumpStatus::quitRequested, 0}});

	ja2::application::RunApplicationLoop(client, host);

	assert((client.calls == std::vector<std::string>{
		"is-running", "update-clock", "next-wake"}));
}

void TestOneEventPerIterationAndStoppedResult()
{
	FakeClient client;
	FakeHost host({{Platform::HostPumpStatus::eventDispatched, 0}}, &client);

	const ja2::application::ApplicationLoopResult result =
		ja2::application::RunApplicationLoop(client, host);

	assert(result.reason == ja2::application::ApplicationLoopExitReason::stopped);
	assert(host.timeouts.size() == 1);
	assert((client.calls == std::vector<std::string>{
		"is-running", "update-clock", "is-active", "run-frame", "next-wake",
		"is-running"}));
}

void TestFrameStopStillPerformsCurrentIterationWait()
{
	FakeClient client;
	client.stopDuringFrame = true;
	FakeHost host({{Platform::HostPumpStatus::deadlineReached, 0}});

	const ja2::application::ApplicationLoopResult result =
		ja2::application::RunApplicationLoop(client, host);

	assert(result.reason == ja2::application::ApplicationLoopExitReason::stopped);
	assert(host.timeouts.size() == 1);
}

void TestHostFailureIsReported()
{
	FakeClient client;
	FakeHost host({{Platform::HostPumpStatus::failed, 91}});

	const ja2::application::ApplicationLoopResult result =
		ja2::application::RunApplicationLoop(client, host);

	assert(result.reason ==
		ja2::application::ApplicationLoopExitReason::hostFailure);
	assert(result.exitCode == 0);
}

void TestAlreadyStoppedClientDoesNotTouchHost()
{
	FakeClient client;
	client.running = false;
	FakeHost host({});

	const ja2::application::ApplicationLoopResult result =
		ja2::application::RunApplicationLoop(client, host);

	assert(result.reason == ja2::application::ApplicationLoopExitReason::stopped);
	assert(host.timeouts.empty());
	assert((client.calls == std::vector<std::string>{"is-running"}));
}

void TestShutdownGateRejectsDuplicateAndReentrantAttempts()
{
	ja2::application::ShutdownOnce shutdown;
	assert(shutdown.state() == ja2::application::ShutdownOnce::State::ready);
	assert(shutdown.begin());
	assert(shutdown.state() == ja2::application::ShutdownOnce::State::inProgress);
	assert(!shutdown.begin());
	shutdown.complete();
	assert(shutdown.state() == ja2::application::ShutdownOnce::State::complete);
	assert(!shutdown.begin());
	shutdown.complete();
	assert(shutdown.state() == ja2::application::ShutdownOnce::State::complete);
}
}

int main()
{
	TestFrameOrderingAndQuitCode();
	TestInactiveClientRunsBackgroundFrameAndAdvancesClock();
	TestNonDueClockDoesNotQueryActivity();
	TestOneEventPerIterationAndStoppedResult();
	TestFrameStopStillPerformsCurrentIterationWait();
	TestHostFailureIsReported();
	TestAlreadyStoppedClientDoesNotTouchHost();
	TestShutdownGateRejectsDuplicateAndReentrantAttempts();
}
