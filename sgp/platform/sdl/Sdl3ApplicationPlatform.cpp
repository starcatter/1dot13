#include "platform/sdl/Sdl3ApplicationPlatform.h"

#include "ScreenGeometry.h"
#include "presentation/sdl/Sdl3Presenter.h"
#include "video_init.h"

#include <stdexcept>
#include <string>

extern int iScreenMode;
extern BOOLEAN gfApplicationActive;

namespace Platform
{

void Sdl3ApplicationPlatform::EventRouter::dispatch(const SDL_Event& event)
{
	if (target != nullptr)
	{
		target->dispatch(event);
	}
}

void Sdl3ApplicationPlatform::handleFocusChange(bool active)
{
	gfApplicationActive = active ? TRUE : FALSE;
}

BOOLEAN Sdl3ApplicationPlatform::initializeVideo()
{
	Sdl3HostConfig config;
	config.title = "Jagged Alliance 2";
	config.width = SCREEN_WIDTH;
	config.height = SCREEN_HEIGHT;
	config.fullscreen = iScreenMode == 0;

	std::string error;
	host_ = Sdl3ApplicationHost::create(config, eventRouter_, error);
	if (!host_)
	{
		return FALSE;
	}

	auto presenter = ja2::presentation::Sdl3Presenter::create(
		host_->window(), SCREEN_WIDTH, SCREEN_HEIGHT, error);
	if (!presenter)
	{
		return FALSE;
	}
	ja2::presentation::Sdl3Presenter* presenterView = presenter.get();
	if (!InitializeVideoManagerWithPresenter(std::move(presenter)))
	{
		return FALSE;
	}

	inputSink_ = std::make_unique<Sdl3LegacyInputSink>(&handleFocusChange);
	inputTranslator_ = std::make_unique<Sdl3InputTranslator>(
		*presenterView, SDL_GetWindowID(host_->window()), *inputSink_);
	eventRouter_.target = inputTranslator_.get();
	return TRUE;
}

ApplicationHost& Sdl3ApplicationPlatform::applicationHost()
{
	if (!host_)
	{
		throw std::logic_error("SDL application host is not initialized");
	}
	return *host_;
}

}
