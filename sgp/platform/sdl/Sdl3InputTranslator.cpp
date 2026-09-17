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

UINT8 windowsScanCode(SDL_Scancode scanCode)
{
	switch (scanCode)
	{
		case SDL_SCANCODE_ESCAPE: return 0x01;
		case SDL_SCANCODE_1: return 0x02;
		case SDL_SCANCODE_2: return 0x03;
		case SDL_SCANCODE_3: return 0x04;
		case SDL_SCANCODE_4: return 0x05;
		case SDL_SCANCODE_5: return 0x06;
		case SDL_SCANCODE_6: return 0x07;
		case SDL_SCANCODE_7: return 0x08;
		case SDL_SCANCODE_8: return 0x09;
		case SDL_SCANCODE_9: return 0x0A;
		case SDL_SCANCODE_0: return 0x0B;
		case SDL_SCANCODE_MINUS: return 0x0C;
		case SDL_SCANCODE_EQUALS: return 0x0D;
		case SDL_SCANCODE_BACKSPACE: return 0x0E;
		case SDL_SCANCODE_TAB: return 0x0F;
		case SDL_SCANCODE_Q: return 0x10;
		case SDL_SCANCODE_W: return 0x11;
		case SDL_SCANCODE_E: return 0x12;
		case SDL_SCANCODE_R: return 0x13;
		case SDL_SCANCODE_T: return 0x14;
		case SDL_SCANCODE_Y: return 0x15;
		case SDL_SCANCODE_U: return 0x16;
		case SDL_SCANCODE_I: return 0x17;
		case SDL_SCANCODE_O: return 0x18;
		case SDL_SCANCODE_P: return 0x19;
		case SDL_SCANCODE_LEFTBRACKET: return 0x1A;
		case SDL_SCANCODE_RIGHTBRACKET: return 0x1B;
		case SDL_SCANCODE_RETURN: return 0x1C;
		case SDL_SCANCODE_LCTRL:
		case SDL_SCANCODE_RCTRL: return 0x1D;
		case SDL_SCANCODE_A: return 0x1E;
		case SDL_SCANCODE_S: return 0x1F;
		case SDL_SCANCODE_D: return 0x20;
		case SDL_SCANCODE_F: return 0x21;
		case SDL_SCANCODE_G: return 0x22;
		case SDL_SCANCODE_H: return 0x23;
		case SDL_SCANCODE_J: return 0x24;
		case SDL_SCANCODE_K: return 0x25;
		case SDL_SCANCODE_L: return 0x26;
		case SDL_SCANCODE_SEMICOLON: return 0x27;
		case SDL_SCANCODE_APOSTROPHE: return 0x28;
		case SDL_SCANCODE_GRAVE: return 0x29;
		case SDL_SCANCODE_LSHIFT: return 0x2A;
		case SDL_SCANCODE_BACKSLASH: return 0x2B;
		case SDL_SCANCODE_Z: return 0x2C;
		case SDL_SCANCODE_X: return 0x2D;
		case SDL_SCANCODE_C: return 0x2E;
		case SDL_SCANCODE_V: return 0x2F;
		case SDL_SCANCODE_B: return 0x30;
		case SDL_SCANCODE_N: return 0x31;
		case SDL_SCANCODE_M: return 0x32;
		case SDL_SCANCODE_COMMA: return 0x33;
		case SDL_SCANCODE_PERIOD: return 0x34;
		case SDL_SCANCODE_SLASH: return 0x35;
		case SDL_SCANCODE_RSHIFT: return 0x36;
		case SDL_SCANCODE_PRINTSCREEN: return 0x37;
		case SDL_SCANCODE_LALT:
		case SDL_SCANCODE_RALT:
		case SDL_SCANCODE_MODE: return 0x38;
		case SDL_SCANCODE_SPACE: return 0x39;
		case SDL_SCANCODE_CAPSLOCK: return 0x3A;
		case SDL_SCANCODE_F1: return 0x3B;
		case SDL_SCANCODE_F2: return 0x3C;
		case SDL_SCANCODE_F3: return 0x3D;
		case SDL_SCANCODE_F4: return 0x3E;
		case SDL_SCANCODE_F5: return 0x3F;
		case SDL_SCANCODE_F6: return 0x40;
		case SDL_SCANCODE_F7: return 0x41;
		case SDL_SCANCODE_F8: return 0x42;
		case SDL_SCANCODE_F9: return 0x43;
		case SDL_SCANCODE_F10: return 0x44;
		case SDL_SCANCODE_NUMLOCKCLEAR:
		case SDL_SCANCODE_PAUSE: return 0x45;
		case SDL_SCANCODE_SCROLLLOCK: return 0x46;
		case SDL_SCANCODE_F11: return 0x57;
		case SDL_SCANCODE_F12: return 0x58;
		default: return 0;
	}
}

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
			windowsScanCode(event.scancode), false};
	}
	if (event.key >= SDLK_0 && event.key <= SDLK_9)
	{
		return {0x30U + static_cast<UINT32>(event.key - SDLK_0),
			windowsScanCode(event.scancode), false};
	}
	if (event.key >= SDLK_F1 && event.key <= SDLK_F12)
	{
		return {112U + static_cast<UINT32>(event.key - SDLK_F1),
			windowsScanCode(event.scancode), false};
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

	const UINT8 scanCode = windowsScanCode(event.scancode);
	switch (event.key)
	{
		case SDLK_BACKSPACE: return {8, scanCode, false};
		case SDLK_TAB: return {9, scanCode, false};
		case SDLK_RETURN: return {13, scanCode, false};
		case SDLK_LSHIFT:
		case SDLK_RSHIFT: return {16, scanCode, false};
		case SDLK_LCTRL: return {17, scanCode, false};
		case SDLK_RCTRL: return {17, scanCode, true};
		case SDLK_LALT: return {18, scanCode, false};
		case SDLK_RALT:
		case SDLK_MODE: return {18, scanCode, true};
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
