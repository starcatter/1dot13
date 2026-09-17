#include "presentation/windows/WindowsPresenterFactory.h"

#include "presentation/windows/DirectDrawPresenter.h"

namespace ja2::presentation
{

std::unique_ptr<Presenter> createWindowsPresenter(HWND window,
	const RECT* windowRect, bool windowed, UINT16 width, UINT16 height,
	UINT8 pixelDepth, PresenterCreateResult& result)
{
	return DirectDrawPresenter::create(window, windowRect, windowed, width,
		height, pixelDepth, result);
}

}
