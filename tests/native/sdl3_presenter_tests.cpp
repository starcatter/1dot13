#include "presentation/PixelSurface.h"
#include "presentation/sdl/Sdl3Presenter.h"

#include <SDL3/SDL.h>

#include <cassert>
#include <string>

using namespace ja2::presentation;

int main()
{
	assert(SDL_Init(SDL_INIT_VIDEO));
	SDL_Window* window = SDL_CreateWindow(
		"JA2 SDL presenter test", 64, 48, SDL_WINDOW_HIDDEN);
	assert(window != nullptr);

	std::string error;
	auto presenter = Sdl3Presenter::create(window, 4, 3, error);
	assert(presenter != nullptr);
	assert(error.empty());

	PixelSurface frame(4, 3, PixelFormat::rgb565);
	frame.fill(0x1234);
	assert(presenter->present({frame.pixels(), nullptr, 0, true, false}));

	frame.fillRect({1, 1, 3, 2}, 0xabcd);
	const SGPRect dirty{1, 1, 3, 2};
	assert(presenter->present({frame.pixels(), &dirty, 1, false, false}));

	presenter->suspend();
	assert(!presenter->present({frame.pixels(), nullptr, 0, true, false}));
	assert(presenter->resume());
	assert(presenter->present({frame.pixels(), nullptr, 0, true, false}));

	PixelSurface indexed(4, 3, PixelFormat::indexed8);
	assert(!presenter->present({indexed.pixels(), nullptr, 0, true, false}));

	presenter.reset();
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
