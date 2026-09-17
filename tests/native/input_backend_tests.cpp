#include "platform/Input.h"

#include <cassert>

int main()
{
	assert(Platform::Input::InitializeEventSource());

	const SGPPoint initial = Platform::Input::GetCursorPosition();
	assert(initial.iX == 320);
	assert(initial.iY == 240);

	const SGPPoint position{-17, 731};
	Platform::Input::SetCursorPosition(position);
	const SGPPoint moved = Platform::Input::GetCursorPosition();
	assert(moved.iX == position.iX);
	assert(moved.iY == position.iY);
	const SGPPoint converted = Platform::Input::ScreenToLogicalPosition(position);
	assert(converted.iX == position.iX);
	assert(converted.iY == position.iY);

	const SGPRect restriction{11, 22, 333, 444};
	Platform::Input::RestrictCursor(restriction);
	assert(Platform::Input::IsCursorRestricted());
	const SGPRect stored = Platform::Input::GetCursorRestriction();
	assert(stored.iLeft == restriction.iLeft);
	assert(stored.iTop == restriction.iTop);
	assert(stored.iRight == restriction.iRight);
	assert(stored.iBottom == restriction.iBottom);

	Platform::Input::RestoreCursorRestriction();
	assert(Platform::Input::IsCursorRestricted());
	Platform::Input::FreeCursor();
	assert(!Platform::Input::IsCursorRestricted());

	assert(!Platform::Input::IsLegacyKeyPressed(0x41));
	Platform::Input::SetLegacyKeyPressed(0x41, true);
	Platform::Input::SetLegacyKeyPressed(0x10, true);
	assert(Platform::Input::IsLegacyKeyPressed(0x41));
	assert(Platform::Input::IsLegacyKeyPressed(0x10));
	Platform::Input::SetLegacyKeyPressed(0x41, false);
	assert(!Platform::Input::IsLegacyKeyPressed(0x41));
	assert(Platform::Input::IsLegacyKeyPressed(0x10));
	Platform::Input::ClearLegacyKeyState();
	assert(!Platform::Input::IsLegacyKeyPressed(0x10));
	Platform::Input::FlushPendingKeyboardEvents();
	Platform::Input::ShutdownEventSource();
}
