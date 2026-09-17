#ifndef JA2_SDL3_LEGACY_INPUT_SINK_H
#define JA2_SDL3_LEGACY_INPUT_SINK_H

#include "platform/sdl/Sdl3InputTranslator.h"

#include <array>

namespace Platform
{

// Delivers translated SDL events to the existing input manager. Focus policy
// remains owned by the application host through the optional callback.
class Sdl3LegacyInputSink final : public Sdl3InputSink
{
public:
	using FocusChangedHandler = void (*)(bool active);

	explicit Sdl3LegacyInputSink(
		FocusChangedHandler focusChangedHandler = nullptr) noexcept;

	void keyDown(UINT32 legacyVirtualKey, UINT32 legacyKeyData) override;
	void keyUp(UINT32 legacyVirtualKey, UINT32 legacyKeyData) override;
	void mouse(UINT16 event, SGPPoint position, INT16 wheelDelta,
		bool updateWheelState) override;
	void focusChanged(bool active) override;

private:
	FocusChangedHandler focusChangedHandler_;
	std::array<UINT32, 256> pressedKeyData_{};
};

}

#endif
