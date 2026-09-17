#ifndef JA2_DIRECT_DRAW_PRESENTER_H
#define JA2_DIRECT_DRAW_PRESENTER_H

#include "presentation/Presenter.h"

#include <ddraw.h>
#include <memory>

namespace ja2::presentation
{

// Windows compatibility presenter used while DirectDraw remains the host
// output backend. It owns the DirectDraw device, cooperative/display mode,
// primary surface, flip chain, final framebuffer upload, and presentation.
enum class DirectDrawPresenterCreateResult
{
	success,
	directDrawFailure,
	displayModeFailure,
};

class DirectDrawPresenter final : public Presenter
{
public:
	static std::unique_ptr<DirectDrawPresenter> create(HWND window,
		const RECT* windowRect, bool windowed, UINT16 width, UINT16 height,
		UINT8 pixelDepth, DirectDrawPresenterCreateResult& result);
	~DirectDrawPresenter() override;

	bool present(const PresentFrame& frame) override;
	void suspend() override;
	bool resume() override;
	void leaveDisplayMode();
	void shutdown();

	LPDIRECTDRAW2 directDrawObject() const noexcept { return directDrawObject2_; }
	LPDIRECTDRAWSURFACE2 primarySurface() const noexcept { return primarySurface2_; }
	LPDIRECTDRAWSURFACE2 backBuffer() const noexcept { return backBuffer2_; }

private:
	DirectDrawPresenter() = default;
	bool initialize(HWND window, const RECT* windowRect, bool windowed,
		UINT16 width, UINT16 height, UINT8 pixelDepth,
		DirectDrawPresenterCreateResult& result);
	bool upload(const ConstPixelBuffer& buffer);
	bool presentWindowed();
	bool presentFullscreen(bool verticalSync);
	bool restoreSurface(LPDIRECTDRAWSURFACE2 surface);

	LPDIRECTDRAW directDrawObject1_ = nullptr;
	LPDIRECTDRAW2 directDrawObject2_ = nullptr;
	LPDIRECTDRAWSURFACE primarySurface1_ = nullptr;
	LPDIRECTDRAWSURFACE2 primarySurface2_ = nullptr;
	LPDIRECTDRAWSURFACE backBuffer1_ = nullptr;
	LPDIRECTDRAWSURFACE2 backBuffer2_ = nullptr;
	HWND window_ = nullptr;
	const RECT* windowRect_ = nullptr;
	bool windowed_ = false;
	bool suspended_ = false;
};

}

#endif
