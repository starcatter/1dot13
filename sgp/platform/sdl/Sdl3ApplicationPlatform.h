#ifndef JA2_SDL3_APPLICATION_PLATFORM_H
#define JA2_SDL3_APPLICATION_PLATFORM_H

#include "application/ApplicationPlatform.h"
#include "platform/sdl/Sdl3ApplicationHost.h"
#include "platform/sdl/Sdl3LegacyInputSink.h"

#include <memory>

namespace ja2::presentation
{
class Sdl3Presenter;
}

namespace Platform
{

class Sdl3ApplicationPlatform final : public ja2::application::ApplicationPlatform
{
public:
	BOOLEAN initializeVideo() override;
	ApplicationHost& applicationHost() override;

private:
	class EventRouter final : public Sdl3EventSink
	{
	public:
		void dispatch(const SDL_Event& event) override;
		Sdl3EventSink* target = nullptr;
	};

	static void handleFocusChange(bool active);

	EventRouter eventRouter_;
	std::unique_ptr<Sdl3ApplicationHost> host_;
	std::unique_ptr<Sdl3LegacyInputSink> inputSink_;
	std::unique_ptr<Sdl3InputTranslator> inputTranslator_;
};

}

#endif
