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
	}
	if (focusChangedHandler_ != nullptr)
	{
		focusChangedHandler_(active);
	}
}

}
