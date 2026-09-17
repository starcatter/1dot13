#ifndef JA2_PRESENTER_H
#define JA2_PRESENTER_H

#include "presentation/Palette.h"
#include "presentation/PresentationTypes.h"

namespace ja2::presentation
{

enum class PresenterCreateResult
{
	success,
	backendFailure,
	displayModeFailure,
};

// Presents an already composed engine framebuffer. Window creation, event
// translation, and logical-surface rendering deliberately live elsewhere.
class Presenter
{
public:
	virtual ~Presenter() = default;

	virtual bool present(const PresentFrame& frame) = 0;
	virtual void suspend() = 0;
	virtual bool resume() = 0;
	virtual bool getRgbMasks(
		UINT16& red, UINT16& green, UINT16& blue) const = 0;
	virtual bool setPalette(const SGPPaletteEntry* entries) = 0;
	virtual void leaveDisplayMode() = 0;
	virtual void shutdown() = 0;
};

}

#endif
