#include "platform/windows/ApplicationHost.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace Platform
{
HostPumpResult WindowsApplicationHost::waitAndDispatchOne(
	std::uint32_t timeoutMilliseconds)
{
	const DWORD waitResult = MsgWaitForMultipleObjectsEx(
		0, nullptr, timeoutMilliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
	if (waitResult == WAIT_FAILED)
	{
		return {HostPumpStatus::failed, 0};
	}
	if (waitResult != WAIT_OBJECT_0)
	{
		return {HostPumpStatus::deadlineReached, 0};
	}

	MSG message{};
	if (!PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
	{
		return {HostPumpStatus::deadlineReached, 0};
	}
	if (message.message == WM_QUIT)
	{
		return {HostPumpStatus::quitRequested,
			static_cast<int>(message.wParam)};
	}

	TranslateMessage(&message);
	DispatchMessage(&message);
	return {HostPumpStatus::eventDispatched, 0};
}
}
