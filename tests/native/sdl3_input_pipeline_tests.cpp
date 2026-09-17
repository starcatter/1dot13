#include "input.h"
#include "english.h"
#include "platform/Input.h"
#include "platform/sdl/Sdl3InputTranslator.h"
#include "platform/sdl/Sdl3LegacyInputSink.h"
#include "presentation/sdl/Sdl3Presenter.h"

#include <SDL3/SDL.h>

#include <cassert>
#include <string>

UINT16 SCREEN_WIDTH = 640;
UINT16 SCREEN_HEIGHT = 480;
BOOLEAN gfMouseLockedOnBorder = FALSE;
int iWindowedMode = 1;

void PrintScreen()
{
}

void VideoCaptureToggle()
{
}

int main()
{
	assert(InitializeInputManager());
	assert(SDL_Init(SDL_INIT_VIDEO));
	SDL_Window* window = SDL_CreateWindow(
		"JA2 SDL input pipeline test", 640, 480, SDL_WINDOW_HIDDEN);
	assert(window != nullptr);
	std::string error;
	auto presenter = ja2::presentation::Sdl3Presenter::create(
		window, 640, 480, error);
	assert(presenter != nullptr);

	Platform::Sdl3LegacyInputSink sink;
	Platform::Sdl3InputTranslator translator(
		*presenter, SDL_GetWindowID(window), sink);

	SDL_Event event{};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.windowID = SDL_GetWindowID(window);
	event.key.key = SDLK_A;
	event.key.scancode = SDL_SCANCODE_A;
	translator.dispatch(event);
	assert(Platform::Input::IsLegacyKeyPressed(0x41));

	InputAtom atom{};
	assert(DequeueEvent(&atom));
	assert(atom.usEvent == KEY_DOWN);
	assert(atom.usParam == static_cast<UINT32>('a'));

	event.type = SDL_EVENT_KEY_UP;
	translator.dispatch(event);
	assert(!Platform::Input::IsLegacyKeyPressed(0x41));
	assert(DequeueEvent(&atom));
	assert(atom.usEvent == KEY_UP);
	assert(atom.usParam == static_cast<UINT32>('a'));

	event = {};
	event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
	event.button.windowID = SDL_GetWindowID(window);
	event.button.button = SDL_BUTTON_LEFT;
	event.button.x = 123.0F;
	event.button.y = 234.0F;
	translator.dispatch(event);
	assert(DequeueEvent(&atom));
	assert(atom.usEvent == LEFT_BUTTON_DOWN);
	assert(GETXPOS(&atom) == 123);
	assert(GETYPOS(&atom) == 234);
	assert(gfLeftButtonState == TRUE);

	event = {};
	event.type = SDL_EVENT_KEY_DOWN;
	event.key.windowID = SDL_GetWindowID(window);
	event.key.key = SDLK_LSHIFT;
	event.key.scancode = SDL_SCANCODE_LSHIFT;
	translator.dispatch(event);
	assert(Platform::Input::IsLegacyKeyPressed(0x10));
	assert(_KeyDown(SHIFT) == TRUE);
	event = {};
	event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
	event.window.windowID = SDL_GetWindowID(window);
	translator.dispatch(event);
	assert(!Platform::Input::IsLegacyKeyPressed(0x10));
	assert(_KeyDown(SHIFT) == FALSE);
	assert(gfLeftButtonState == FALSE);
	assert(DequeueEvent(&atom));
	assert(atom.usEvent == LEFT_BUTTON_UP);
	assert(GETXPOS(&atom) == 123);
	assert(GETYPOS(&atom) == 234);

	presenter.reset();
	SDL_DestroyWindow(window);
	SDL_Quit();
	ShutdownInputManager();
}
