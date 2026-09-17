#include "platform/sdl/Sdl3LegacyInputSink.h"

#include "input.h"
#include "platform/Input.h"

namespace Platform
{

Sdl3LegacyInputSink::Sdl3LegacyInputSink(
	FocusChangedHandler focusChangedHandler) noexcept
	: focusChangedHandler_(focusChangedHandler)
{
}

void Sdl3LegacyInputSink::keyDown(
	UINT32 legacyVirtualKey, UINT32 legacyKeyData)
{
	const UINT8 key = static_cast<UINT8>(legacyVirtualKey);
	Input::SetLegacyKeyPressed(key, true);
	pressedKeyData_[key] = legacyKeyData;
	KeyDown(legacyVirtualKey, legacyKeyData);
	gfSGPInputReceived = TRUE;
}

void Sdl3LegacyInputSink::keyUp(
	UINT32 legacyVirtualKey, UINT32 legacyKeyData)
{
	const UINT8 key = static_cast<UINT8>(legacyVirtualKey);
	Input::SetLegacyKeyPressed(key, false);
	pressedKeyData_[key] = 0;
	KeyUp(legacyVirtualKey, legacyKeyData);
}

void Sdl3LegacyInputSink::mouse(UINT16 event, SGPPoint position,
	INT16 wheelDelta, bool updateWheelState)
{
	lastMousePosition_ = position;
	switch (event)
	{
		case LEFT_BUTTON_DOWN: pressedMouseButtons_[0] = true; break;
		case LEFT_BUTTON_UP: pressedMouseButtons_[0] = false; break;
		case RIGHT_BUTTON_DOWN: pressedMouseButtons_[1] = true; break;
		case RIGHT_BUTTON_UP: pressedMouseButtons_[1] = false; break;
		case MIDDLE_BUTTON_DOWN: pressedMouseButtons_[2] = true; break;
		case MIDDLE_BUTTON_UP: pressedMouseButtons_[2] = false; break;
		case X1_BUTTON_DOWN: pressedMouseButtons_[3] = true; break;
		case X1_BUTTON_UP: pressedMouseButtons_[3] = false; break;
		case X2_BUTTON_DOWN: pressedMouseButtons_[4] = true; break;
		case X2_BUTTON_UP: pressedMouseButtons_[4] = false; break;
		default: break;
	}
	Input::SetCursorPosition(position);
	InputInjectMouseEvent(event, position, wheelDelta,
		updateWheelState ? TRUE : FALSE);
}

void Sdl3LegacyInputSink::focusChanged(bool active)
{
	if (!active)
	{
		for (std::size_t key = 0; key < pressedKeyData_.size(); ++key)
		{
			const UINT32 keyData = pressedKeyData_[key];
			if (keyData != 0)
			{
				KeyUp(static_cast<UINT32>(key), keyData |
					0x40000000U | TRANSITION_MASK);
				pressedKeyData_[key] = 0;
			}
		}
		Input::ClearLegacyKeyState();
		constexpr std::array<UINT16, 5> releaseEvents{
			LEFT_BUTTON_UP, RIGHT_BUTTON_UP, MIDDLE_BUTTON_UP,
			X1_BUTTON_UP, X2_BUTTON_UP};
		for (std::size_t button = 0;
			button < pressedMouseButtons_.size(); ++button)
		{
			if (pressedMouseButtons_[button])
			{
				InputInjectMouseEvent(releaseEvents[button],
					lastMousePosition_, 0, FALSE);
				pressedMouseButtons_[button] = false;
			}
		}
	}
	if (focusChangedHandler_ != nullptr)
	{
		focusChangedHandler_(active);
	}
}

}
