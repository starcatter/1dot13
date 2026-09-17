#ifndef JA2_VIDEO_INIT_H
#define JA2_VIDEO_INIT_H

#include "types.h"

#include <memory>

namespace ja2::presentation
{
class Presenter;
}

// Internal host-to-video-manager construction seam. The host creates and owns
// its native window; the common video manager assumes ownership of only the
// completed-frame presenter.
BOOLEAN InitializeVideoManagerWithPresenter(
	std::unique_ptr<ja2::presentation::Presenter> presenter);

#endif
