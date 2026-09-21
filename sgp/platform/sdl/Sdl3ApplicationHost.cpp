#include "platform/sdl/Sdl3ApplicationHost.h"

#include <algorithm>
#include <limits>

namespace Platform
{

namespace
{

constexpr std::size_t maximumCoalescedMouseMotions = 4096;

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
	setFocusedMouseOwnership(
		(SDL_GetWindowFlags(window_) & SDL_WINDOW_INPUT_FOCUS) != 0);
	error.clear();
	return true;
}

Sdl3ApplicationHost::~Sdl3ApplicationHost()
{
	if (window_ != nullptr)
	{
		setFocusedMouseOwnership(false);
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
	bool eventAvailable = hasDeferredEvent_;
	if (hasDeferredEvent_)
	{
		event = deferredEvent_;
		hasDeferredEvent_ = false;
	}
	else if (timeoutMilliseconds == 0)
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

	// SDL retains every mouse-motion event, unlike the effectively coalesced
	// WM_MOUSEMOVE stream used by the old host.  Replaying those events one per
	// game-loop iteration creates an arbitrarily long input delay whenever a
	// frame is expensive (weather effects exposed this dramatically).  Collapse
	// each contiguous run to its newest absolute position while preserving the
	// ordering of buttons, keys, focus, and quit events.
	if (event.type == SDL_EVENT_MOUSE_MOTION)
	{
		for (std::size_t count = 1;
			count < maximumCoalescedMouseMotions; ++count)
		{
			SDL_Event next{};
			if (!SDL_PollEvent(&next))
			{
				break;
			}
			if (next.type == SDL_EVENT_MOUSE_MOTION &&
				next.motion.windowID == event.motion.windowID &&
				next.motion.which == event.motion.which)
			{
				event = next;
				continue;
			}

			deferredEvent_ = next;
			hasDeferredEvent_ = true;
			break;
		}
	}

	if (event.type == SDL_EVENT_QUIT ||
		(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
			event.window.windowID == windowId_))
	{
		return {HostPumpStatus::quitRequested, 0};
	}
	if ((event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
		event.type == SDL_EVENT_WINDOW_FOCUS_LOST) &&
		event.window.windowID == windowId_)
	{
		setFocusedMouseOwnership(event.type == SDL_EVENT_WINDOW_FOCUS_GAINED);
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

void Sdl3ApplicationHost::setFocusedMouseOwnership(bool focused) noexcept
{
	if (window_ == nullptr)
	{
		return;
	}

	// JA2 draws its own cursor and consumes absolute mouse coordinates. A
	// window grab confines those coordinates without enabling relative mode.
	// Release and reveal the host cursor whenever focus leaves the game so the
	// desktop remains usable.
	if (focused)
	{
		SDL_SetWindowMouseGrab(window_, true);
		SDL_HideCursor();
	}
	else
	{
		SDL_SetWindowMouseGrab(window_, false);
		SDL_ShowCursor();
	}
}

}
