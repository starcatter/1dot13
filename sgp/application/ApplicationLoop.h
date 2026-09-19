#ifndef JA2_APPLICATION_APPLICATION_LOOP_H
#define JA2_APPLICATION_APPLICATION_LOOP_H

#include "platform/ApplicationHost.h"

#include <cstdint>

namespace ja2
{
namespace application
{
class ApplicationLoopClient
{
public:
	virtual ~ApplicationLoopClient() = default;

	virtual bool isRunning() const = 0;
	virtual bool isActive() const = 0;
	virtual bool updateClock() = 0;
	virtual std::uint32_t nextWakeMilliseconds() const = 0;
	virtual void runFrame() = 0;
	virtual void runBackgroundFrame() = 0;
};

enum class ApplicationLoopExitReason
{
	stopped,
	quitRequested,
	hostFailure
};

struct ApplicationLoopResult
{
	ApplicationLoopExitReason reason;
	int exitCode = 0;
};

// Runs the portable part of the legacy main loop. Each iteration advances the
// game clock, optionally runs one frame, then waits for and dispatches at most
// one host event. The host owns event translation; the client owns game state.
ApplicationLoopResult RunApplicationLoop(
	ApplicationLoopClient& client, Platform::ApplicationHost& host);
}
}

#endif
