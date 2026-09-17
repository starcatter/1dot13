#ifndef JA2_SDL3_APPLICATION_HOST_H
#define JA2_SDL3_APPLICATION_HOST_H

#include "platform/ApplicationHost.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <string>

namespace Platform
{

struct Sdl3HostConfig
{
	std::string title;
	std::int32_t width = 0;
	std::int32_t height = 0;
	bool fullscreen = false;
	bool hidden = false;
};

class Sdl3EventSink
{
public:
	virtual ~Sdl3EventSink() = default;
	virtual void dispatch(const SDL_Event& event) = 0;
};

// Owns the SDL video subsystem and game window. The presenter must be
// destroyed before this host because its renderer is attached to the window.
class Sdl3ApplicationHost final : public ApplicationHost
{
public:
	static std::unique_ptr<Sdl3ApplicationHost> create(
		const Sdl3HostConfig& config, Sdl3EventSink& eventSink,
		std::string& error);
	~Sdl3ApplicationHost() override;

	HostPumpResult waitAndDispatchOne(
		std::uint32_t timeoutMilliseconds) override;
	SDL_Window* window() const noexcept;
	void minimize() noexcept;

private:
	explicit Sdl3ApplicationHost(Sdl3EventSink& eventSink);
	bool initialize(const Sdl3HostConfig& config, std::string& error);

	Sdl3EventSink& eventSink_;
	SDL_Window* window_ = nullptr;
	SDL_WindowID windowId_ = 0;
	bool ownsVideoSubsystem_ = false;
};

}

#endif
