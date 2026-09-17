#include "video_windows.h"

#include "LegacySGP.h"
#include "Text.h"
#include "fileio/FileIO.h"
#include "platform/Dialog.h"
#include "presentation/Presenter.h"
#include "presentation/windows/WindowsPresenterFactory.h"
#include "resource.h"
#include "UtfConversion.h"
#include "video_init.h"

#include <cstring>
#include <memory>

extern int iScreenMode;
extern RECT rcWindow;
extern POINT ptWindowSize;

HWND ghWindow;

BOOLEAN InitializeVideoManager(
	HINSTANCE instance, UINT16 commandShow, void* windowProcedure)
{
	WNDCLASS windowClass{};
	UINT8 className[] = APPLICATION_NAME;
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = reinterpret_cast<WNDPROC>(windowProcedure);
	windowClass.hInstance = instance;
	windowClass.hIcon = LoadIcon(instance, MAKEINTRESOURCE(IDI_ICON1));
	windowClass.lpszClassName = reinterpret_cast<LPCSTR>(className);
	RegisterClass(&windowClass);

	HWND windowHandle = nullptr;
	if (iScreenMode == 1)
	{
		RECT window{0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
		const DWORD extendedStyle = WS_EX_APPWINDOW;
		const DWORD style =
			WS_OVERLAPPEDWINDOW & (~(WS_MAXIMIZEBOX | WS_SYSMENU));
		AdjustWindowRectEx(&window, style, FALSE, extendedStyle);
		OffsetRect(&window, -window.left, -window.top);
		ptWindowSize.x = window.right;
		ptWindowSize.y = window.bottom;
		windowHandle = CreateWindowEx(extendedStyle,
			reinterpret_cast<LPCSTR>(className), "Jagged Alliance 2", style,
			window.left, window.top, window.right, window.bottom, nullptr,
			nullptr, instance, nullptr);
	}
	else
	{
		windowHandle = CreateWindowEx(WS_EX_TOPMOST,
			reinterpret_cast<LPCSTR>(className), "Jagged Alliance 2",
			WS_POPUP | WS_VISIBLE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
			nullptr, nullptr, instance, nullptr);
	}
	if (windowHandle == nullptr)
	{
		return FALSE;
	}

	SetCursor(nullptr);
	ghWindow = windowHandle;
	ShowWindow(windowHandle, commandShow);
	UpdateWindow(windowHandle);
	SetFocus(windowHandle);

	ja2::presentation::PresenterCreateResult result;
	std::unique_ptr<ja2::presentation::Presenter> presenter =
		ja2::presentation::createWindowsPresenter(ghWindow, &rcWindow,
			iScreenMode == 1, SCREEN_WIDTH, SCREEN_HEIGHT, PIXEL_DEPTH, result);
	if (!presenter)
	{
		if (result ==
			ja2::presentation::PresenterCreateResult::displayModeFailure)
		{
			CHAR16 message[256];
			swprintf(message,
				Additional113Text[ADDTEXT_DIFFRES_REQUIRED], SCREEN_WIDTH,
				SCREEN_HEIGHT);
			Platform::ShowDialog(APPLICATION_NAME,
				ja2::text::utf16ToUtf8ReplacingInvalid(message),
				Platform::DialogKind::warning);
			PostQuitMessage(1);
		}
		return FALSE;
	}

	return InitializeVideoManagerWithPresenter(std::move(presenter));
}
