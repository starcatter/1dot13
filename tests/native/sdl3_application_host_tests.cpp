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
		events.push_back(event.type);
	}

	std::vector<Uint32> events;
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
	assert(sink.events[0] == SDL_EVENT_USER);

	SDL_Event quitEvent{};
	quitEvent.type = SDL_EVENT_QUIT;
	assert(SDL_PushEvent(&quitEvent));
	const Platform::HostPumpResult quit = host->waitAndDispatchOne(100);
	assert(quit.status == Platform::HostPumpStatus::quitRequested);
	assert(sink.events.size() == 1);

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
