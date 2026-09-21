#include "platform/sdl/Sdl3ApplicationHost.h"
#include "presentation/PixelSurface.h"
#include "presentation/sdl/Sdl3Presenter.h"

#include <SDL3/SDL.h>

#include <cassert>
#include <vector>

namespace
{

class RecordingEventSink final : public Platform::Sdl3EventSink
{
public:
	void dispatch(const SDL_Event& event) override
	{
		events.push_back(event);
	}

	std::vector<SDL_Event> events;
};

void drainEvents()
{
	SDL_Event event{};
	while (SDL_PollEvent(&event))
	{
	}
}

}

int main()
{
	RecordingEventSink sink;
	std::string error;
	Platform::Sdl3HostConfig config;
	config.title = "JA2 SDL application host test";
	config.width = 64;
	config.height = 48;
	config.hidden = true;
	auto host = Platform::Sdl3ApplicationHost::create(config, sink, error);
	assert(host != nullptr);
	assert(host->window() != nullptr);
	assert(error.empty());
	assert(SDL_ShowCursor());

	auto presenter = ja2::presentation::Sdl3Presenter::create(
		host->window(), 4, 3, error);
	assert(presenter != nullptr);
	ja2::presentation::PixelSurface frame(
		4, 3, ja2::presentation::PixelFormat::rgb565);
	frame.fill(0x1234);
	assert(presenter->present({frame.pixels(), nullptr, 0, true, false}));

	drainEvents();
	const Platform::HostPumpResult timeout = host->waitAndDispatchOne(0);
	assert(timeout.status == Platform::HostPumpStatus::deadlineReached);

	SDL_Event userEvent{};
	userEvent.type = SDL_EVENT_USER;
	assert(SDL_PushEvent(&userEvent));
	const Platform::HostPumpResult dispatched = host->waitAndDispatchOne(100);
	assert(dispatched.status == Platform::HostPumpStatus::eventDispatched);
	assert(sink.events.size() == 1);
	assert(sink.events[0].type == SDL_EVENT_USER);

	const SDL_WindowID windowId = SDL_GetWindowID(host->window());
	for (float x = 1.0f; x <= 3.0f; x += 1.0f)
	{
		SDL_Event motion{};
		motion.type = SDL_EVENT_MOUSE_MOTION;
		motion.motion.windowID = windowId;
		motion.motion.which = 1;
		motion.motion.x = x;
		assert(SDL_PushEvent(&motion));
	}
	const Platform::HostPumpResult coalesced = host->waitAndDispatchOne(100);
	assert(coalesced.status == Platform::HostPumpStatus::eventDispatched);
	assert(sink.events.size() == 2);
	assert(sink.events.back().type == SDL_EVENT_MOUSE_MOTION);
	assert(sink.events.back().motion.x == 3.0f);
	assert(host->waitAndDispatchOne(0).status ==
		Platform::HostPumpStatus::deadlineReached);

	SDL_Event motionBeforeUser{};
	motionBeforeUser.type = SDL_EVENT_MOUSE_MOTION;
	motionBeforeUser.motion.windowID = windowId;
	motionBeforeUser.motion.which = 1;
	motionBeforeUser.motion.x = 4.0f;
	assert(SDL_PushEvent(&motionBeforeUser));
	assert(SDL_PushEvent(&userEvent));
	SDL_Event motionAfterUser = motionBeforeUser;
	motionAfterUser.motion.x = 5.0f;
	assert(SDL_PushEvent(&motionAfterUser));
	assert(host->waitAndDispatchOne(100).status ==
		Platform::HostPumpStatus::eventDispatched);
	assert(sink.events.back().type == SDL_EVENT_MOUSE_MOTION);
	assert(sink.events.back().motion.x == 4.0f);
	assert(host->waitAndDispatchOne(100).status ==
		Platform::HostPumpStatus::eventDispatched);
	assert(sink.events.back().type == SDL_EVENT_USER);
	assert(host->waitAndDispatchOne(100).status ==
		Platform::HostPumpStatus::eventDispatched);
	assert(sink.events.back().type == SDL_EVENT_MOUSE_MOTION);
	assert(sink.events.back().motion.x == 5.0f);

	SDL_Event focusGained{};
	focusGained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
	focusGained.window.windowID = windowId;
	assert(SDL_PushEvent(&focusGained));
	const Platform::HostPumpResult focused = host->waitAndDispatchOne(100);
	assert(focused.status == Platform::HostPumpStatus::eventDispatched);
	assert(!SDL_CursorVisible());
	assert(sink.events.back().type == SDL_EVENT_WINDOW_FOCUS_GAINED);

	SDL_Event focusLost{};
	focusLost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
	focusLost.window.windowID = SDL_GetWindowID(host->window());
	assert(SDL_PushEvent(&focusLost));
	const Platform::HostPumpResult unfocused = host->waitAndDispatchOne(100);
	assert(unfocused.status == Platform::HostPumpStatus::eventDispatched);
	assert(SDL_CursorVisible());
	assert(sink.events.back().type == SDL_EVENT_WINDOW_FOCUS_LOST);

	SDL_Event quitEvent{};
	quitEvent.type = SDL_EVENT_QUIT;
	assert(SDL_PushEvent(&quitEvent));
	const Platform::HostPumpResult quit = host->waitAndDispatchOne(100);
	assert(quit.status == Platform::HostPumpStatus::quitRequested);
	assert(sink.events.size() == 7);

	SDL_Event closeEvent{};
	closeEvent.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
	closeEvent.window.windowID = SDL_GetWindowID(host->window());
	assert(SDL_PushEvent(&closeEvent));
	const Platform::HostPumpResult close = host->waitAndDispatchOne(100);
	assert(close.status == Platform::HostPumpStatus::quitRequested);

	host->minimize();
	presenter.reset();
	host.reset();
	return 0;
}
