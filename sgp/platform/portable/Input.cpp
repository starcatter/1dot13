#include "platform/Input.h"

#include <array>

namespace Platform::Input
{
namespace
{
SGPPoint gCursorPosition{320, 240};
SGPRect gCursorClipRectangle{};
bool gCursorRestricted = false;
std::array<bool, 256> gLegacyKeyState{};
}

bool InitializeEventSource() noexcept
{
	return true;
}

void ShutdownEventSource() noexcept
{
}

SGPPoint GetCursorPosition() noexcept
{
	return gCursorPosition;
}

SGPPoint ScreenToLogicalPosition(SGPPoint position) noexcept
{
	return position;
}

void SetCursorPosition(SGPPoint position) noexcept
{
	gCursorPosition = position;
}

void RestrictCursor(const SGPRect& rectangle) noexcept
{
	gCursorClipRectangle = rectangle;
	gCursorRestricted = true;
}

void FreeCursor() noexcept
{
	gCursorRestricted = false;
}

void RestoreCursorRestriction() noexcept
{
}

bool IsCursorRestricted() noexcept
{
	return gCursorRestricted;
}

SGPRect GetCursorRestriction() noexcept
{
	return gCursorClipRectangle;
}

bool IsLegacyKeyPressed(UINT8 key) noexcept
{
	return gLegacyKeyState[key];
}

void SetLegacyKeyPressed(UINT8 key, bool pressed) noexcept
{
	gLegacyKeyState[key] = pressed;
}

void ClearLegacyKeyState() noexcept
{
	gLegacyKeyState.fill(false);
}

void FlushPendingKeyboardEvents() noexcept
{
}
}
