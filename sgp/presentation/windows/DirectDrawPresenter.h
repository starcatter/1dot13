#ifndef JA2_DIRECT_DRAW_PRESENTER_H
#define JA2_DIRECT_DRAW_PRESENTER_H

#include "presentation/Presenter.h"

#include <ddraw.h>

namespace ja2::presentation
{

// Windows compatibility presenter used while DirectDraw remains the host
// output backend. The engine owns the supplied objects; this adapter owns only
// final framebuffer upload and presentation.
class DirectDrawPresenter final : public Presenter
{
public:
	DirectDrawPresenter(LPDIRECTDRAWSURFACE primarySurface,
		LPDIRECTDRAWSURFACE2 primarySurface2,
		LPDIRECTDRAWSURFACE2 backBuffer, const RECT* windowRect,
		bool windowed) noexcept;

	bool present(const PresentFrame& frame) override;
	void suspend() override;
	bool resume() override;

private:
	bool upload(const ConstPixelBuffer& buffer);
	bool presentWindowed();
	bool presentFullscreen(bool verticalSync);
	bool restoreSurface(LPDIRECTDRAWSURFACE2 surface);

	LPDIRECTDRAWSURFACE primarySurface_;
	LPDIRECTDRAWSURFACE2 primarySurface2_;
	LPDIRECTDRAWSURFACE2 backBuffer_;
	const RECT* windowRect_;
	bool windowed_;
	bool suspended_ = false;
};

}

#endif
