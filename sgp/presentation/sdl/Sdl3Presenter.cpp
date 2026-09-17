#include "presentation/sdl/Sdl3Presenter.h"

#include <algorithm>
#include <cstddef>

namespace ja2::presentation
{

namespace
{

void setSdlError(std::string& error, const char* operation)
{
	error = operation;
	error += ": ";
	error += SDL_GetError();
}

}

std::unique_ptr<Sdl3Presenter> Sdl3Presenter::create(
	SDL_Window* window, UINT16 width, UINT16 height, std::string& error)
{
	std::unique_ptr<Sdl3Presenter> presenter(new Sdl3Presenter);
	if (!presenter->initialize(window, width, height, error))
	{
		return nullptr;
	}
	return presenter;
}

bool Sdl3Presenter::initialize(
	SDL_Window* window, UINT16 width, UINT16 height, std::string& error)
{
	if (window == nullptr || width == 0 || height == 0)
	{
		error = "SDL presenter requires a window and non-zero dimensions";
		return false;
	}

	renderer_ = SDL_CreateRenderer(window, nullptr);
	if (renderer_ == nullptr)
	{
		setSdlError(error, "SDL_CreateRenderer failed");
		return false;
	}
	if (!SDL_SetRenderLogicalPresentation(renderer_, width, height,
		SDL_LOGICAL_PRESENTATION_LETTERBOX))
	{
		setSdlError(error, "SDL_SetRenderLogicalPresentation failed");
		return false;
	}

	texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_RGB565,
		SDL_TEXTUREACCESS_STREAMING, width, height);
	if (texture_ == nullptr)
	{
		setSdlError(error, "SDL_CreateTexture failed");
		return false;
	}
	if (!SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST))
	{
		setSdlError(error, "SDL_SetTextureScaleMode failed");
		return false;
	}
	if (!SDL_SetRenderDrawColor(renderer_, 0, 0, 0, SDL_ALPHA_OPAQUE))
	{
		setSdlError(error, "SDL_SetRenderDrawColor failed");
		return false;
	}

	width_ = width;
	height_ = height;
	error.clear();
	return true;
}

Sdl3Presenter::~Sdl3Presenter()
{
	shutdown();
}

void Sdl3Presenter::shutdown()
{
	if (texture_ != nullptr)
	{
		SDL_DestroyTexture(texture_);
		texture_ = nullptr;
	}
	if (renderer_ != nullptr)
	{
		SDL_DestroyRenderer(renderer_);
		renderer_ = nullptr;
	}
}

bool Sdl3Presenter::updateTexture(const PresentFrame& frame)
{
	const ConstPixelBuffer& buffer = frame.buffer;
	const std::size_t rowBytes = static_cast<std::size_t>(width_) * 2U;
	if (buffer.pixels == nullptr || buffer.format != PixelFormat::rgb565 ||
		buffer.width != width_ || buffer.height != height_ ||
		buffer.pitchBytes < rowBytes ||
		(frame.dirtyRegionCount != 0 && frame.dirtyRegions == nullptr))
	{
		return false;
	}

	if (!textureInitialized_ || frame.fullRefresh)
	{
		if (!SDL_UpdateTexture(texture_, nullptr, buffer.pixels,
			static_cast<int>(buffer.pitchBytes)))
		{
			return false;
		}
		textureInitialized_ = true;
		return true;
	}

	for (std::size_t index = 0; index < frame.dirtyRegionCount; ++index)
	{
		const SGPRect& dirty = frame.dirtyRegions[index];
		const INT32 left = std::max<INT32>(dirty.iLeft, 0);
		const INT32 top = std::max<INT32>(dirty.iTop, 0);
		const INT32 right = std::min<INT32>(dirty.iRight, width_);
		const INT32 bottom = std::min<INT32>(dirty.iBottom, height_);
		if (right <= left || bottom <= top)
		{
			continue;
		}
		const SDL_Rect destination{
			left, top, right - left, bottom - top};
		const BYTE* source = buffer.pixels +
			static_cast<std::size_t>(top) * buffer.pitchBytes +
			static_cast<std::size_t>(left) * 2U;
		if (!SDL_UpdateTexture(texture_, &destination, source,
			static_cast<int>(buffer.pitchBytes)))
		{
			return false;
		}
	}
	return true;
}

bool Sdl3Presenter::present(const PresentFrame& frame)
{
	if (suspended_ || renderer_ == nullptr || texture_ == nullptr ||
		!updateTexture(frame))
	{
		return false;
	}

	const int requestedVerticalSync = frame.verticalSync ? 1 : 0;
	if (requestedVerticalSync != verticalSync_)
	{
		if (!SDL_SetRenderVSync(renderer_, requestedVerticalSync))
		{
			return false;
		}
		verticalSync_ = requestedVerticalSync;
	}
	return SDL_RenderClear(renderer_) &&
		SDL_RenderTexture(renderer_, texture_, nullptr, nullptr) &&
		SDL_RenderPresent(renderer_);
}

void Sdl3Presenter::suspend()
{
	suspended_ = true;
}

bool Sdl3Presenter::resume()
{
	if (renderer_ == nullptr || texture_ == nullptr)
	{
		return false;
	}
	suspended_ = false;
	return true;
}

bool Sdl3Presenter::getRgbMasks(
	UINT16& red, UINT16& green, UINT16& blue) const
{
	red = 0xf800;
	green = 0x07e0;
	blue = 0x001f;
	return true;
}

bool Sdl3Presenter::setPalette(const SGPPaletteEntry* entries)
{
	// The final framebuffer is RGB565. Indexed palettes are consumed while the
	// engine converts indexed art into that framebuffer, not by SDL.
	return entries != nullptr;
}

void Sdl3Presenter::leaveDisplayMode()
{
}

}
