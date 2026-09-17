#ifndef JA2_SDL3_PRESENTER_H
#define JA2_SDL3_PRESENTER_H

#include "presentation/Presenter.h"

#include <SDL3/SDL.h>

#include <memory>
#include <string>

namespace ja2::presentation
{

// Final-frame SDL presenter. The application host owns SDL initialization and
// the window; this object owns only the renderer and streaming texture.
class Sdl3Presenter final : public Presenter
{
public:
	static std::unique_ptr<Sdl3Presenter> create(
		SDL_Window* window, UINT16 width, UINT16 height, std::string& error);
	~Sdl3Presenter() override;

	bool present(const PresentFrame& frame) override;
	void suspend() override;
	bool resume() override;
	bool getRgbMasks(
		UINT16& red, UINT16& green, UINT16& blue) const override;
	bool setPalette(const SGPPaletteEntry* entries) override;
	void leaveDisplayMode() override;
	void shutdown() override;

private:
	Sdl3Presenter() = default;
	bool initialize(SDL_Window* window, UINT16 width, UINT16 height,
		std::string& error);
	bool updateTexture(const PresentFrame& frame);

	SDL_Renderer* renderer_ = nullptr;
	SDL_Texture* texture_ = nullptr;
	UINT16 width_ = 0;
	UINT16 height_ = 0;
	int verticalSync_ = -1;
	bool textureInitialized_ = false;
	bool suspended_ = false;
};

}

#endif
