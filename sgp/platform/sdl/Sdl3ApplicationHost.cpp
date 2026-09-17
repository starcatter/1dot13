#include "platform/sdl/Sdl3ApplicationHost.h"

#include <algorithm>
#include <limits>

namespace Platform
{

namespace
{

void setSdlError(std::string& error, const char* operation)
{
	error = operation;
	error += ": ";
	error += SDL_GetError();
}

}

Sdl3ApplicationHost::Sdl3ApplicationHost(Sdl3EventSink& eventSink)
	: eventSink_(eventSink)
{
}

std::unique_ptr<Sdl3ApplicationHost> Sdl3ApplicationHost::create(
	const Sdl3HostConfig& config, Sdl3EventSink& eventSink, std::string& error)
{
	std::unique_ptr<Sdl3ApplicationHost> host(
		new Sdl3ApplicationHost(eventSink));
	if (!host->initialize(config, error))
	{
		return nullptr;
	}
	return host;
}

bool Sdl3ApplicationHost::initialize(
	const Sdl3HostConfig& config, std::string& error)
{
	if (config.title.empty() || config.width <= 0 || config.height <= 0)
	{
		error = "SDL host requires a title and positive window dimensions";
		return false;
	}
	if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
	{
		setSdlError(error, "SDL_InitSubSystem failed");
		return false;
	}
	ownsVideoSubsystem_ = true;

	SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
	if (config.fullscreen)
	{
		flags |= SDL_WINDOW_FULLSCREEN;
	}
	if (config.hidden)
	{
		flags |= SDL_WINDOW_HIDDEN;
	}
	window_ = SDL_CreateWindow(
		config.title.c_str(), config.width, config.height, flags);
	if (window_ == nullptr)
	{
		setSdlError(error, "SDL_CreateWindow failed");
		return false;
	}
	windowId_ = SDL_GetWindowID(window_);
	if (windowId_ == 0)
	{
		setSdlError(error, "SDL_GetWindowID failed");
		return false;
	}
	error.clear();
	return true;
}

Sdl3ApplicationHost::~Sdl3ApplicationHost()
{
	if (window_ != nullptr)
	{
		SDL_DestroyWindow(window_);
		window_ = nullptr;
	}
	if (ownsVideoSubsystem_)
	{
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		ownsVideoSubsystem_ = false;
	}
}

HostPumpResult Sdl3ApplicationHost::waitAndDispatchOne(
	std::uint32_t timeoutMilliseconds)
{
	SDL_ClearError();
	SDL_Event event{};
	bool eventAvailable = false;
	if (timeoutMilliseconds == 0)
	{
		eventAvailable = SDL_PollEvent(&event);
	}
	else
	{
		const std::uint32_t maximumTimeout =
			static_cast<std::uint32_t>(std::numeric_limits<int>::max());
		const int timeout = static_cast<int>(
			std::min(timeoutMilliseconds, maximumTimeout));
		eventAvailable = SDL_WaitEventTimeout(&event, timeout);
	}
	if (!eventAvailable)
	{
		return SDL_GetError()[0] == '\0'
			? HostPumpResult{HostPumpStatus::deadlineReached, 0}
			: HostPumpResult{HostPumpStatus::failed, 0};
	}

	if (event.type == SDL_EVENT_QUIT ||
		(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
			event.window.windowID == windowId_))
	{
		return {HostPumpStatus::quitRequested, 0};
	}

	eventSink_.dispatch(event);
	return {HostPumpStatus::eventDispatched, 0};
}

SDL_Window* Sdl3ApplicationHost::window() const noexcept
{
	return window_;
}

void Sdl3ApplicationHost::minimize() noexcept
{
	if (window_ != nullptr)
	{
		SDL_MinimizeWindow(window_);
	}
}

}
