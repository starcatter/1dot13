#ifndef JA2_SDL3_INPUT_TRANSLATOR_H
#define JA2_SDL3_INPUT_TRANSLATOR_H

#include "platform/sdl/Sdl3ApplicationHost.h"

#include "types.h"

namespace ja2::presentation
{
class Sdl3Presenter;
}

namespace Platform
{

class Sdl3InputSink
{
public:
	virtual ~Sdl3InputSink() = default;
	virtual void keyDown(UINT32 legacyVirtualKey, UINT32 legacyKeyData) = 0;
	virtual void keyUp(UINT32 legacyVirtualKey, UINT32 legacyKeyData) = 0;
	virtual void mouse(UINT16 event, SGPPoint position, INT16 wheelDelta,
		bool updateWheelState) = 0;
	virtual void focusChanged(bool active) = 0;
};

// Converts SDL events into the Win32-compatible key and existing InputAtom
// vocabulary consumed by the legacy input manager. It owns no SDL resources.
class Sdl3InputTranslator final : public Sdl3EventSink
{
public:
	Sdl3InputTranslator(ja2::presentation::Sdl3Presenter& presenter,
		SDL_WindowID windowId, Sdl3InputSink& sink);

	void dispatch(const SDL_Event& event) override;

private:
	ja2::presentation::Sdl3Presenter& presenter_;
	SDL_WindowID windowId_;
	Sdl3InputSink& sink_;
};

}

#endif
