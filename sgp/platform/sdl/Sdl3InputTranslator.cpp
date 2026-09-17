#include "platform/sdl/Sdl3InputTranslator.h"

#include "input.h"
#include "presentation/sdl/Sdl3Presenter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Platform
{

namespace
{

struct LegacyKey
{
	UINT32 virtualKey = 0;
	UINT8 scanCode = 0;
	bool extended = false;
};

LegacyKey keypadKey(const SDL_KeyboardEvent& event)
{
	const bool numLock = (event.mod & SDL_KMOD_NUM) != 0;
	switch (event.key)
	{
		case SDLK_KP_0: return {numLock ? 96U : 45U, 82, false};
		case SDLK_KP_1: return {numLock ? 97U : 35U, 79, false};
		case SDLK_KP_2: return {numLock ? 98U : 40U, 80, false};
		case SDLK_KP_3: return {numLock ? 99U : 34U, 81, false};
		case SDLK_KP_4: return {numLock ? 100U : 37U, 75, false};
		case SDLK_KP_5: return {numLock ? 101U : 12U, 76, false};
		case SDLK_KP_6: return {numLock ? 102U : 39U, 77, false};
		case SDLK_KP_7: return {numLock ? 103U : 36U, 71, false};
		case SDLK_KP_8: return {numLock ? 104U : 38U, 72, false};
		case SDLK_KP_9: return {numLock ? 105U : 33U, 73, false};
		case SDLK_KP_PERIOD: return {numLock ? 110U : 46U, 83, false};
		case SDLK_KP_DIVIDE: return {111, 53, true};
		case SDLK_KP_MULTIPLY: return {106, 55, false};
		case SDLK_KP_MINUS: return {109, 74, false};
		case SDLK_KP_PLUS: return {107, 78, false};
		case SDLK_KP_ENTER: return {13, 28, true};
		default: return {};
	}
}

LegacyKey navigationKey(SDL_Keycode key)
{
	switch (key)
	{
		case SDLK_INSERT: return {45, 82, true};
		case SDLK_DELETE: return {46, 83, true};
		case SDLK_END: return {35, 79, true};
		case SDLK_DOWN: return {40, 80, true};
		case SDLK_PAGEDOWN: return {34, 81, true};
		case SDLK_LEFT: return {37, 75, true};
		case SDLK_RIGHT: return {39, 77, true};
		case SDLK_HOME: return {36, 71, true};
		case SDLK_UP: return {38, 72, true};
		case SDLK_PAGEUP: return {33, 73, true};
		default: return {};
	}
}

LegacyKey translateKey(const SDL_KeyboardEvent& event)
{
	if (event.key >= SDLK_A && event.key <= SDLK_Z)
	{
		return {0x41U + static_cast<UINT32>(event.key - SDLK_A),
			static_cast<UINT8>(event.scancode), false};
	}
	if (event.key >= SDLK_0 && event.key <= SDLK_9)
	{
		return {0x30U + static_cast<UINT32>(event.key - SDLK_0),
			static_cast<UINT8>(event.scancode), false};
	}
	if (event.key >= SDLK_F1 && event.key <= SDLK_F12)
	{
		return {112U + static_cast<UINT32>(event.key - SDLK_F1),
			static_cast<UINT8>(event.scancode), false};
	}
	if (const LegacyKey keypad = keypadKey(event); keypad.virtualKey != 0)
	{
		return keypad;
	}
	if (const LegacyKey navigation = navigationKey(event.key);
		navigation.virtualKey != 0)
	{
		return navigation;
	}

	const UINT8 scanCode = static_cast<UINT8>(event.scancode);
	switch (event.key)
	{
		case SDLK_BACKSPACE: return {8, scanCode, false};
		case SDLK_TAB: return {9, scanCode, false};
		case SDLK_RETURN: return {13, scanCode, false};
		case SDLK_LSHIFT:
		case SDLK_RSHIFT: return {16, scanCode, false};
		case SDLK_LCTRL:
		case SDLK_RCTRL: return {17, scanCode, false};
		case SDLK_LALT:
		case SDLK_RALT:
		case SDLK_MODE: return {18, scanCode, false};
		case SDLK_PAUSE: return {19, scanCode, false};
		case SDLK_CAPSLOCK: return {20, scanCode, false};
		case SDLK_ESCAPE: return {27, scanCode, false};
		case SDLK_SPACE: return {32, scanCode, false};
		case SDLK_PRINTSCREEN: return {44, scanCode, true};
		case SDLK_NUMLOCKCLEAR: return {144, scanCode, true};
		case SDLK_SCROLLLOCK: return {145, scanCode, false};
		case SDLK_SEMICOLON: return {186, scanCode, false};
		case SDLK_EQUALS:
		case SDLK_PLUS: return {187, scanCode, false};
		case SDLK_COMMA: return {188, scanCode, false};
		case SDLK_MINUS: return {189, scanCode, false};
		case SDLK_PERIOD: return {190, scanCode, false};
		case SDLK_SLASH: return {191, scanCode, false};
		case SDLK_GRAVE: return {192, scanCode, false};
		case SDLK_LEFTBRACKET: return {219, scanCode, false};
		case SDLK_BACKSLASH: return {220, scanCode, false};
		case SDLK_RIGHTBRACKET: return {221, scanCode, false};
		case SDLK_APOSTROPHE: return {222, scanCode, false};
		default: return {};
	}
}

UINT32 legacyKeyData(const SDL_KeyboardEvent& event, const LegacyKey& key)
{
	UINT32 data = static_cast<UINT32>(key.scanCode) << 16;
	if (key.extended)
	{
		data |= EXT_CODE_MASK;
	}
	if (event.repeat || event.type == SDL_EVENT_KEY_UP)
	{
		data |= 0x40000000U;
	}
	if (event.type == SDL_EVENT_KEY_UP)
	{
		data |= TRANSITION_MASK;
	}
	return data;
}

SGPPoint roundedPosition(float x, float y)
{
	return {static_cast<INT32>(std::lround(x)),
		static_cast<INT32>(std::lround(y))};
}

UINT16 mouseButtonEvent(Uint8 button, bool down)
{
	switch (button)
	{
		case SDL_BUTTON_LEFT: return down ? LEFT_BUTTON_DOWN : LEFT_BUTTON_UP;
		case SDL_BUTTON_RIGHT: return down ? RIGHT_BUTTON_DOWN : RIGHT_BUTTON_UP;
		case SDL_BUTTON_MIDDLE:
			return down ? MIDDLE_BUTTON_DOWN : MIDDLE_BUTTON_UP;
		case SDL_BUTTON_X1: return down ? X1_BUTTON_DOWN : X1_BUTTON_UP;
		case SDL_BUTTON_X2: return down ? X2_BUTTON_DOWN : X2_BUTTON_UP;
		default: return 0;
	}
}

INT16 wheelDelta(float amount)
{
	const long rounded = std::lround(amount);
	return static_cast<INT16>(std::clamp<long>(rounded,
		std::numeric_limits<INT16>::min(),
		std::numeric_limits<INT16>::max()));
}

}

Sdl3InputTranslator::Sdl3InputTranslator(
	ja2::presentation::Sdl3Presenter& presenter, SDL_WindowID windowId,
	Sdl3InputSink& sink)
	: presenter_(presenter), windowId_(windowId), sink_(sink)
{
}

void Sdl3InputTranslator::dispatch(const SDL_Event& source)
{
	SDL_Event event = source;
	switch (event.type)
	{
		case SDL_EVENT_KEY_DOWN:
		case SDL_EVENT_KEY_UP:
		{
			if (event.key.windowID != windowId_)
			{
				return;
			}
			const LegacyKey key = translateKey(event.key);
			if (key.virtualKey == 0)
			{
				return;
			}
			const UINT32 data = legacyKeyData(event.key, key);
			if (event.type == SDL_EVENT_KEY_DOWN)
			{
				sink_.keyDown(key.virtualKey, data);
			}
			else
			{
				sink_.keyUp(key.virtualKey, data);
			}
			return;
		}
		case SDL_EVENT_MOUSE_MOTION:
			if (event.motion.windowID == windowId_ &&
				event.motion.which != SDL_TOUCH_MOUSEID &&
				presenter_.convertEventToLogical(event))
			{
				sink_.mouse(MOUSE_POS,
					roundedPosition(event.motion.x, event.motion.y), 0, false);
			}
			return;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			if (event.button.windowID == windowId_ &&
				event.button.which != SDL_TOUCH_MOUSEID &&
				presenter_.convertEventToLogical(event))
			{
				sink_.mouse(mouseButtonEvent(event.button.button,
						event.type == SDL_EVENT_MOUSE_BUTTON_DOWN),
					roundedPosition(event.button.x, event.button.y), 0, false);
			}
			return;
		case SDL_EVENT_MOUSE_WHEEL:
			if (event.wheel.windowID == windowId_ &&
				presenter_.convertEventToLogical(event))
			{
				float amount = event.wheel.y;
				if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
				{
					amount = -amount;
				}
				const INT16 delta = wheelDelta(amount);
				const UINT16 inputEvent = delta == 1 ? MOUSE_WHEEL_UP :
					delta == -1 ? MOUSE_WHEEL_DOWN : 0;
				sink_.mouse(inputEvent,
					roundedPosition(event.wheel.mouse_x, event.wheel.mouse_y),
					delta, true);
			}
			return;
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			if (event.window.windowID == windowId_)
			{
				sink_.focusChanged(
					event.type == SDL_EVENT_WINDOW_FOCUS_GAINED);
			}
			return;
		default:
			return;
	}
}

}
