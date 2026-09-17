#include "platform/Window.h"

#include "video_windows.h"

namespace Platform
{
void MinimizeMainWindow() noexcept
{
	if (ghWindow != nullptr)
	{
		ShowWindow(ghWindow, SW_MINIMIZE);
	}
}

void HideMainWindow() noexcept
{
	if (ghWindow != nullptr)
	{
		ShowWindow(ghWindow, SW_HIDE);
	}
}
}
