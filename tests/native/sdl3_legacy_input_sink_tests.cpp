#include "input.h"
#include "platform/Input.h"
#include "platform/sdl/Sdl3LegacyInputSink.h"

#include <cassert>
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
	BOOLEAN updateWheelState;
};

std::vector<KeyRecord> gKeys;
std::vector<MouseRecord> gMouseEvents;
std::vector<bool> gFocusChanges;

void recordFocusChange(bool active)
{
	gFocusChanges.push_back(active);
}

}

extern "C"
{

BOOLEAN gfSGPInputReceived = FALSE;

void KeyDown(UINT32 virtualKey, UINT32 keyData)
{
	gKeys.push_back({true, virtualKey, keyData});
}

void KeyUp(UINT32 virtualKey, UINT32 keyData)
{
	gKeys.push_back({false, virtualKey, keyData});
}

void InputInjectMouseEvent(UINT16 event, SGPPoint position,
	INT16 wheelDelta, BOOLEAN updateWheelState)
{
	gMouseEvents.push_back(
		{event, position, wheelDelta, updateWheelState});
}

}

int main()
{
	Platform::Sdl3LegacyInputSink sink(recordFocusChange);

	sink.keyDown(0x41, 0x001E0000U);
	assert(gKeys.size() == 1);
	assert(gKeys.back().down);
	assert(gKeys.back().virtualKey == 0x41);
	assert(gKeys.back().keyData == 0x001E0000U);
	assert(Platform::Input::IsLegacyKeyPressed(0x41));
	assert(gfSGPInputReceived == TRUE);

	sink.keyUp(0x41, 0xC01E0000U);
	assert(gKeys.size() == 2);
	assert(!gKeys.back().down);
	assert(!Platform::Input::IsLegacyKeyPressed(0x41));

	const SGPPoint position{123, 234};
	sink.mouse(LEFT_BUTTON_DOWN, position, -2, true);
	assert(gMouseEvents.size() == 1);
	assert(gMouseEvents.back().event == LEFT_BUTTON_DOWN);
	assert(gMouseEvents.back().position.iX == position.iX);
	assert(gMouseEvents.back().position.iY == position.iY);
	assert(gMouseEvents.back().wheelDelta == -2);
	assert(gMouseEvents.back().updateWheelState == TRUE);
	const SGPPoint storedPosition = Platform::Input::GetCursorPosition();
	assert(storedPosition.iX == position.iX);
	assert(storedPosition.iY == position.iY);

	sink.keyDown(0x10, 0x002A0000U);
	assert(Platform::Input::IsLegacyKeyPressed(0x10));
	sink.focusChanged(false);
	assert(!Platform::Input::IsLegacyKeyPressed(0x10));
	assert(gKeys.size() == 4);
	assert(!gKeys.back().down);
	assert(gKeys.back().virtualKey == 0x10);
	assert((gKeys.back().keyData & TRANSITION_MASK) != 0);
	assert((gKeys.back().keyData & 0x40000000U) != 0);
	sink.focusChanged(true);
	assert(gFocusChanges.size() == 2);
	assert(!gFocusChanges[0]);
	assert(gFocusChanges[1]);
}
