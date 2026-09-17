#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0500
#endif
#include <windows.h>

#include "input.h"
#include "platform/Input.h"
#include "video_windows.h"

#include <cstring>

namespace Platform::Input
{
namespace
{
bool gCursorRestricted = false;
RECT gCursorClipRectangle{};
HHOOK gMouseHook = nullptr;

LRESULT CALLBACK MouseHandler(int code, WPARAM message, LPARAM data)
{
	if (code < 0)
		return CallNextHookEx(gMouseHook, code, message, data);

	const auto* mouse = reinterpret_cast<const MOUSEHOOKSTRUCTEX*>(data);
	const SGPPoint position = ScreenToLogicalPosition(
		{static_cast<INT32>(mouse->pt.x), static_cast<INT32>(mouse->pt.y)});

	UINT16 event = 0;
	INT16 wheelDelta = 0;
	switch (message)
	{
		case WM_XBUTTONDOWN:
			if (mouse->mouseData == (static_cast<DWORD>(XBUTTON1) << 16))
				event = X1_BUTTON_DOWN;
			else if (mouse->mouseData == (static_cast<DWORD>(XBUTTON2) << 16))
				event = X2_BUTTON_DOWN;
			break;
		case WM_XBUTTONUP:
			if (mouse->mouseData == (static_cast<DWORD>(XBUTTON1) << 16))
				event = X1_BUTTON_UP;
			else if (mouse->mouseData == (static_cast<DWORD>(XBUTTON2) << 16))
				event = X2_BUTTON_UP;
			break;
		case WM_MOUSEWHEEL:
		{
			const INT16 rawDelta = static_cast<INT16>(HIWORD(mouse->mouseData));
			wheelDelta = static_cast<INT16>(rawDelta / WHEEL_DELTA);
			if (rawDelta == WHEEL_DELTA)
				event = MOUSE_WHEEL_UP;
			else if (rawDelta == -WHEEL_DELTA)
				event = MOUSE_WHEEL_DOWN;
			break;
		}
		case WM_MBUTTONDOWN: event = MIDDLE_BUTTON_DOWN; break;
		case WM_MBUTTONUP: event = MIDDLE_BUTTON_UP; break;
		case WM_LBUTTONDOWN: event = LEFT_BUTTON_DOWN; break;
		case WM_LBUTTONUP: event = LEFT_BUTTON_UP; break;
		case WM_RBUTTONDOWN: event = RIGHT_BUTTON_DOWN; break;
		case WM_RBUTTONUP: event = RIGHT_BUTTON_UP; break;
		case WM_MOUSEMOVE: event = MOUSE_POS; break;
		default:
			// The legacy hook still refreshed the cached cursor position for
			// otherwise-unhandled mouse messages.
			InputInjectMouseEvent(0, position, 0, FALSE);
			return CallNextHookEx(gMouseHook, code, message, data);
	}

	InputInjectMouseEvent(event, position, wheelDelta,
		message == WM_MOUSEWHEEL ? TRUE : FALSE);
	// Preserve the legacy hook's unconditional pass-through after observing the
	// event. Window processing and cnc-ddraw still see the original message.
	return CallNextHookEx(gMouseHook, code, message, data);
}
}

bool InitializeEventSource() noexcept
{
	// cnc-ddraw's Sir-Tech compatibility path intercepts SetWindowsHookExA and
	// rewrites the hook's desktop coordinates into the game's logical surface
	// coordinates.  This is an intentional ABI contract: using the W entry
	// point bypasses that adapter when the output window is scaled.
	gMouseHook = SetWindowsHookExA(
		WH_MOUSE, MouseHandler, nullptr, GetCurrentThreadId());
	return gMouseHook != nullptr;
}

void ShutdownEventSource() noexcept
{
	if (gMouseHook != nullptr)
	{
		UnhookWindowsHookEx(gMouseHook);
		gMouseHook = nullptr;
	}
}

SGPPoint GetCursorPosition() noexcept
{
	POINT position{};
	GetCursorPos(&position);
	ScreenToClient(ghWindow, &position);
	return {static_cast<INT32>(position.x), static_cast<INT32>(position.y)};
}

SGPPoint ScreenToLogicalPosition(SGPPoint position) noexcept
{
	POINT nativePosition{position.iX, position.iY};
	ScreenToClient(ghWindow, &nativePosition);
	return {static_cast<INT32>(nativePosition.x), static_cast<INT32>(nativePosition.y)};
}

void SetCursorPosition(SGPPoint position) noexcept
{
	POINT nativePosition{position.iX, position.iY};
	ClientToScreen(ghWindow, &nativePosition);
	SetCursorPos(nativePosition.x, nativePosition.y);
}

void RestrictCursor(const SGPRect& rectangle) noexcept
{
	static_assert(sizeof(SGPRect) == sizeof(RECT));
	std::memcpy(&gCursorClipRectangle, &rectangle, sizeof(gCursorClipRectangle));
	ClientToScreen(ghWindow, reinterpret_cast<LPPOINT>(&gCursorClipRectangle));
	ClientToScreen(ghWindow, reinterpret_cast<LPPOINT>(&gCursorClipRectangle) + 1);
	ClipCursor(&gCursorClipRectangle);
	gCursorRestricted = true;
}

void FreeCursor() noexcept
{
	ClipCursor(nullptr);
	gCursorRestricted = false;
}

void RestoreCursorRestriction() noexcept
{
	if (gCursorRestricted)
		ClipCursor(&gCursorClipRectangle);
}

bool IsCursorRestricted() noexcept
{
	return gCursorRestricted;
}

SGPRect GetCursorRestriction() noexcept
{
	RECT rectangle{};
	GetClipCursor(&rectangle);
	ScreenToClient(ghWindow, reinterpret_cast<LPPOINT>(&rectangle));
	ScreenToClient(ghWindow, reinterpret_cast<LPPOINT>(&rectangle) + 1);
	return {rectangle.left, rectangle.top, rectangle.right, rectangle.bottom};
}

bool IsLegacyKeyPressed(UINT8 key) noexcept
{
	return GetAsyncKeyState(static_cast<int>(key)) != 0;
}

void SetLegacyKeyPressed(UINT8, bool) noexcept
{
	// The Windows backend polls the operating system's live key state.
}

void ClearLegacyKeyState() noexcept
{
	// The Windows backend polls the operating system's live key state.
}

void FlushPendingKeyboardEvents() noexcept
{
	MSG message{};
	while (PeekMessage(&message, ghWindow, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
}
}
