#include "input.h"
#include "platform/sdl/Sdl3InputTranslator.h"
#include "presentation/sdl/Sdl3Presenter.h"

#include <SDL3/SDL.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

struct KeyRecord
{
	bool down;
	UINT32 virtualKey;
	UINT32 keyData;
};

struct MouseRecord
{
	UINT16 event;
	SGPPoint position;
	INT16 wheelDelta;
	bool updateWheelState;
};

class RecordingInputSink final : public Platform::Sdl3InputSink
{
public:
	void keyDown(UINT32 virtualKey, UINT32 keyData) override
	{
		keys.push_back({true, virtualKey, keyData});
	}
	void keyUp(UINT32 virtualKey, UINT32 keyData) override
	{
		keys.push_back({false, virtualKey, keyData});
	}
	void mouse(UINT16 event, SGPPoint position, INT16 wheelDelta,
		bool updateWheelState) override
	{
		mouseEvents.push_back(
			{event, position, wheelDelta, updateWheelState});
	}
	void focusChanged(bool active) override
	{
		focus.push_back(active);
	}

	std::vector<KeyRecord> keys;
	std::vector<MouseRecord> mouseEvents;
	std::vector<bool> focus;
};

}

int main()
{
	assert(SDL_Init(SDL_INIT_VIDEO));
	SDL_Window* window = SDL_CreateWindow(
		"JA2 SDL input translator test", 64, 48, SDL_WINDOW_HIDDEN);
	assert(window != nullptr);
	std::string error;
	auto presenter = ja2::presentation::Sdl3Presenter::create(
		window, 4, 3, error);
	assert(presenter != nullptr);
	const SDL_WindowID windowId = SDL_GetWindowID(window);
	RecordingInputSink sink;
	Platform::Sdl3InputTranslator translator(*presenter, windowId, sink);

	SDL_Event event{};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.windowID = windowId;
	event.key.key = SDLK_A;
	event.key.scancode = SDL_SCANCODE_A;
	translator.dispatch(event);
	assert(sink.keys.size() == 1);
	assert(sink.keys.back().down);
	assert(sink.keys.back().virtualKey == 0x41);
	assert(sink.keys.back().keyData == (0x1EU << 16));

	event.key.repeat = true;
	translator.dispatch(event);
	assert((sink.keys.back().keyData & 0x40000000U) != 0);
	event.type = SDL_EVENT_KEY_UP;
	event.key.repeat = false;
	translator.dispatch(event);
	assert(!sink.keys.back().down);
	assert((sink.keys.back().keyData & TRANSITION_MASK) != 0);

	event = {};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.windowID = windowId;
	event.key.key = SDLK_UP;
	event.key.scancode = SDL_SCANCODE_UP;
	translator.dispatch(event);
	assert(sink.keys.back().virtualKey == 38);
	assert((sink.keys.back().keyData & SCAN_CODE_MASK) == (72U << 16));
	assert((sink.keys.back().keyData & EXT_CODE_MASK) != 0);

	event.key.key = SDLK_KP_1;
	event.key.scancode = SDL_SCANCODE_KP_1;
	event.key.mod = SDL_KMOD_NONE;
	translator.dispatch(event);
	assert(sink.keys.back().virtualKey == 35);
	assert((sink.keys.back().keyData & SCAN_CODE_MASK) == (79U << 16));
	assert((sink.keys.back().keyData & EXT_CODE_MASK) == 0);
	event.key.mod = SDL_KMOD_NUM;
	translator.dispatch(event);
	assert(sink.keys.back().virtualKey == 97);

	event = {};
	event.type = SDL_EVENT_MOUSE_MOTION;
	event.motion.windowID = windowId;
	event.motion.x = 32.0F;
	event.motion.y = 16.0F;
	translator.dispatch(event);
	assert(sink.mouseEvents.size() == 1);
	assert(sink.mouseEvents.back().event == MOUSE_POS);
	assert(sink.mouseEvents.back().position.iX == 2);
	assert(sink.mouseEvents.back().position.iY == 1);

	event = {};
	event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
	event.button.windowID = windowId;
	event.button.button = SDL_BUTTON_LEFT;
	event.button.x = 16.0F;
	event.button.y = 32.0F;
	translator.dispatch(event);
	assert(sink.mouseEvents.back().event == LEFT_BUTTON_DOWN);
	assert(sink.mouseEvents.back().position.iX == 1);
	assert(sink.mouseEvents.back().position.iY == 2);

	event = {};
	event.type = SDL_EVENT_MOUSE_WHEEL;
	event.wheel.windowID = windowId;
	event.wheel.y = 1.0F;
	event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
	event.wheel.mouse_x = 48.0F;
	event.wheel.mouse_y = 16.0F;
	translator.dispatch(event);
	assert(sink.mouseEvents.back().event == MOUSE_WHEEL_DOWN);
	assert(sink.mouseEvents.back().wheelDelta == -1);
	assert(sink.mouseEvents.back().updateWheelState);
	assert(sink.mouseEvents.back().position.iX == 3);
	assert(sink.mouseEvents.back().position.iY == 1);

	event = {};
	event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
	event.window.windowID = windowId;
	translator.dispatch(event);
	event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
	translator.dispatch(event);
	assert(sink.focus.size() == 2);
	assert(!sink.focus[0]);
	assert(sink.focus[1]);

	const std::size_t keyCount = sink.keys.size();
	event = {};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.windowID = windowId + 1;
	event.key.key = SDLK_B;
	translator.dispatch(event);
	assert(sink.keys.size() == keyCount);

	presenter.reset();
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
