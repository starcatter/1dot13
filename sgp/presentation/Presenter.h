#ifndef JA2_PRESENTER_H
#define JA2_PRESENTER_H

#include "presentation/PresentationTypes.h"

namespace ja2::presentation
{

// Presents an already composed engine framebuffer. Window creation, event
// translation, and logical-surface rendering deliberately live elsewhere.
class Presenter
{
public:
	virtual ~Presenter() = default;

	virtual bool present(const PresentFrame& frame) = 0;
	virtual void suspend() = 0;
	virtual bool resume() = 0;
};

}

#endif
