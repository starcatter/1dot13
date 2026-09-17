#ifndef JA2_WINDOWS_PRESENTER_FACTORY_H
#define JA2_WINDOWS_PRESENTER_FACTORY_H

#include "presentation/Presenter.h"

#include <windows.h>

#include <memory>

namespace ja2::presentation
{

std::unique_ptr<Presenter> createWindowsPresenter(HWND window,
	const RECT* windowRect, bool windowed, UINT16 width, UINT16 height,
	UINT8 pixelDepth, PresenterCreateResult& result);

}

#endif
