#include "presentation/windows/DirectDrawPresenter.h"

#include "DirectX Common.h"

#include <cstring>

namespace ja2::presentation
{

namespace
{

constexpr INT32 maxDirectDrawErrors = 10;

std::size_t bytesPerPixel(PixelFormat format) noexcept
{
	return format == PixelFormat::indexed8 ? 1U : 2U;
}

bool shouldStopRetrying(HRESULT result, INT32& errorCount) noexcept
{
	return result == DDERR_SURFACELOST ||
		(FAILED(result) && ++errorCount > maxDirectDrawErrors);
}

}

std::unique_ptr<DirectDrawPresenter> DirectDrawPresenter::create(HWND window,
	const RECT* windowRect, bool windowed, UINT16 width, UINT16 height,
	UINT8 pixelDepth, DirectDrawPresenterCreateResult& result)
{
	std::unique_ptr<DirectDrawPresenter> presenter(new DirectDrawPresenter);
	if (!presenter->initialize(window, windowRect, windowed, width, height,
		pixelDepth, result))
	{
		return nullptr;
	}
	return presenter;
}

bool DirectDrawPresenter::initialize(HWND window, const RECT* windowRect,
	bool windowed, UINT16 width, UINT16 height, UINT8 pixelDepth,
	DirectDrawPresenterCreateResult& result)
{
	result = DirectDrawPresenterCreateResult::directDrawFailure;
	window_ = window;
	windowRect_ = windowRect;
	windowed_ = windowed;

	HRESULT code = DirectDrawCreate(nullptr, &directDrawObject1_, nullptr);
	if (code != DD_OK)
	{
		DirectXAttempt(code, __LINE__, __FILE__);
		return false;
	}
	code = IDirectDraw_QueryInterface(directDrawObject1_, IID_IDirectDraw2,
		reinterpret_cast<void**>(&directDrawObject2_));
	if (code != DD_OK)
	{
		DirectXAttempt(code, __LINE__, __FILE__);
		return false;
	}

	code = IDirectDraw2_SetCooperativeLevel(directDrawObject2_, window_,
		windowed_ ? DDSCL_NORMAL : DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
	if (code != DD_OK)
	{
		DirectXAttempt(code, __LINE__, __FILE__);
		return false;
	}
	if (!windowed_)
	{
		code = IDirectDraw2_SetDisplayMode(directDrawObject2_, width, height,
			pixelDepth, 0, 0);
		if (code != DD_OK)
		{
			IDirectDraw2_SetCooperativeLevel(
				directDrawObject2_, window_, DDSCL_NORMAL);
			result = DirectDrawPresenterCreateResult::displayModeFailure;
			DirectXAttempt(code, __LINE__, __FILE__);
			return false;
		}
	}

	DDSURFACEDESC description{};
	description.dwSize = sizeof(description);
	if (windowed_)
	{
		description.dwFlags = DDSD_CAPS;
		description.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		code = IDirectDraw2_CreateSurface(directDrawObject2_, &description,
			&primarySurface1_, nullptr);
		if (code != DD_OK)
		{
			DirectXAttempt(code, __LINE__, __FILE__);
			return false;
		}

		LPDIRECTDRAWCLIPPER clipper = nullptr;
		code = DirectDrawCreateClipper(0, &clipper, nullptr);
		if (code == DD_OK)
		{
			code = IDirectDrawClipper_SetHWnd(clipper, 0, window_);
		}
		if (code == DD_OK)
		{
			code = IDirectDrawSurface_SetClipper(primarySurface1_, clipper);
		}
		if (clipper != nullptr)
		{
			IDirectDrawClipper_Release(clipper);
		}
		if (code != DD_OK)
		{
			DirectXAttempt(code, __LINE__, __FILE__);
			return false;
		}
		code = IDirectDrawSurface_QueryInterface(primarySurface1_,
			IID_IDirectDrawSurface2,
			reinterpret_cast<void**>(&primarySurface2_));
		if (code != DD_OK)
		{
			DirectXAttempt(code, __LINE__, __FILE__);
			return false;
		}

		description = {};
		description.dwSize = sizeof(description);
		description.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
		description.ddsCaps.dwCaps =
			DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
		description.dwWidth = width;
		description.dwHeight = height;
		code = IDirectDraw2_CreateSurface(directDrawObject2_, &description,
			&backBuffer1_, nullptr);
		if (code != DD_OK)
		{
			DirectXAttempt(code, __LINE__, __FILE__);
			return false;
		}
		code = IDirectDrawSurface_QueryInterface(backBuffer1_,
			IID_IDirectDrawSurface2,
			reinterpret_cast<void**>(&backBuffer2_));
	}
	else
	{
		description.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
		description.ddsCaps.dwCaps =
			DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
		description.dwBackBufferCount = 1;
		code = IDirectDraw2_CreateSurface(directDrawObject2_, &description,
			&primarySurface1_, nullptr);
		if (code != DD_OK)
		{
			DirectXAttempt(code, __LINE__, __FILE__);
			return false;
		}
		code = IDirectDrawSurface_QueryInterface(primarySurface1_,
			IID_IDirectDrawSurface2,
			reinterpret_cast<void**>(&primarySurface2_));
		if (code == DD_OK)
		{
			DDSCAPS caps{};
			caps.dwCaps = DDSCAPS_BACKBUFFER;
			code = IDirectDrawSurface2_GetAttachedSurface(
				primarySurface2_, &caps, &backBuffer2_);
		}
	}

	if (code != DD_OK)
	{
		DirectXAttempt(code, __LINE__, __FILE__);
		return false;
	}
	result = DirectDrawPresenterCreateResult::success;
	return true;
}

DirectDrawPresenter::~DirectDrawPresenter()
{
	shutdown();
}

bool DirectDrawPresenter::upload(const ConstPixelBuffer& buffer)
{
	if (buffer.pixels == nullptr || buffer.width == 0 || buffer.height == 0)
	{
		return false;
	}

	DDSURFACEDESC description{};
	description.dwSize = sizeof(description);
	HRESULT result;
	do
	{
		result = IDirectDrawSurface2_Lock(
			backBuffer2_, nullptr, &description, 0, nullptr);
	} while (result == DDERR_WASSTILLDRAWING);

	if (result != DD_OK)
	{
		DirectXAttempt(result, __LINE__, __FILE__);
		return false;
	}

	const std::size_t rowBytes =
		static_cast<std::size_t>(buffer.width) * bytesPerPixel(buffer.format);
	const bool validDestination = description.lpSurface != nullptr &&
		description.lPitch > 0 &&
		static_cast<std::size_t>(description.lPitch) >= rowBytes &&
		buffer.pitchBytes >= rowBytes &&
		(description.dwWidth == 0 || buffer.width <= description.dwWidth) &&
		(description.dwHeight == 0 || buffer.height <= description.dwHeight);

	if (validDestination)
	{
		BYTE* destination = static_cast<BYTE*>(description.lpSurface);
		for (UINT16 y = 0; y < buffer.height; ++y)
		{
			std::memcpy(destination +
					static_cast<std::size_t>(y) * description.lPitch,
				buffer.pixels + static_cast<std::size_t>(y) * buffer.pitchBytes,
				rowBytes);
		}
	}

	result = IDirectDrawSurface2_Unlock(backBuffer2_, nullptr);
	if (result != DD_OK)
	{
		DirectXAttempt(result, __LINE__, __FILE__);
		return false;
	}
	return validDestination;
}

bool DirectDrawPresenter::presentWindowed()
{
	if (windowRect_ == nullptr)
	{
		return false;
	}

	HRESULT result;
	INT32 errorCount = 0;
	do
	{
		result = IDirectDrawSurface2_Blt(primarySurface2_,
			const_cast<RECT*>(windowRect_), backBuffer2_, nullptr,
			DDBLT_WAIT, nullptr);
		if (result != DD_OK && result != DDERR_WASSTILLDRAWING)
		{
			if (result == DDERR_INVALIDRECT)
			{
				return false;
			}
			DirectXAttempt(result, __LINE__, __FILE__);
			if (shouldStopRetrying(result, errorCount))
			{
				return false;
			}
		}
	} while (result != DD_OK);
	return true;
}

bool DirectDrawPresenter::presentFullscreen(bool verticalSync)
{
	HRESULT result;
	INT32 errorCount = 0;
	do
	{
		result = IDirectDrawSurface_Flip(primarySurface1_, nullptr,
			verticalSync ? DDFLIP_WAIT : 0x00000008L);
		if (result != DD_OK && result != DDERR_WASSTILLDRAWING)
		{
			if (result == DDERR_INVALIDRECT)
			{
				return false;
			}
			DirectXAttempt(result, __LINE__, __FILE__);
			if (shouldStopRetrying(result, errorCount))
			{
				return false;
			}
		}
	} while (result != DD_OK);
	return true;
}

bool DirectDrawPresenter::present(const PresentFrame& frame)
{
	if (suspended_ || !upload(frame.buffer))
	{
		return false;
	}
	return windowed_ ? presentWindowed() :
		presentFullscreen(frame.verticalSync);
}

void DirectDrawPresenter::suspend()
{
	suspended_ = true;
}

bool DirectDrawPresenter::restoreSurface(LPDIRECTDRAWSURFACE2 surface)
{
	const HRESULT result = IDirectDrawSurface2_Restore(surface);
	if (result != DD_OK)
	{
		DirectXAttempt(result, __LINE__, __FILE__);
		return false;
	}
	return true;
}

bool DirectDrawPresenter::resume()
{
	if (!restoreSurface(primarySurface2_) || !restoreSurface(backBuffer2_))
	{
		return false;
	}
	suspended_ = false;
	return true;
}

bool DirectDrawPresenter::getRgbMasks(
	UINT16& red, UINT16& green, UINT16& blue) const
{
	if (primarySurface2_ == nullptr)
	{
		return false;
	}
	DDSURFACEDESC description{};
	description.dwSize = sizeof(description);
	description.dwFlags = DDSD_PIXELFORMAT;
	const HRESULT result = IDirectDrawSurface2_GetSurfaceDesc(
		primarySurface2_, &description);
	if (result != DD_OK)
	{
		DirectXAttempt(result, __LINE__, __FILE__);
		return false;
	}
	red = static_cast<UINT16>(description.ddpfPixelFormat.dwRBitMask);
	green = static_cast<UINT16>(description.ddpfPixelFormat.dwGBitMask);
	blue = static_cast<UINT16>(description.ddpfPixelFormat.dwBBitMask);
	return true;
}

bool DirectDrawPresenter::setPalette(const SGPPaletteEntry* entries,
	LPDIRECTDRAWSURFACE2 auxiliarySurface)
{
	if (directDrawObject2_ == nullptr || primarySurface2_ == nullptr ||
		backBuffer2_ == nullptr || entries == nullptr)
	{
		return false;
	}
	if (palette_ != nullptr)
	{
		IDirectDrawPalette_Release(palette_);
		palette_ = nullptr;
	}
	HRESULT result = IDirectDraw2_CreatePalette(directDrawObject2_,
		DDPCAPS_8BIT | DDPCAPS_ALLOW256,
		reinterpret_cast<PALETTEENTRY*>(
			const_cast<SGPPaletteEntry*>(entries)),
		&palette_, nullptr);
	if (result == DD_OK)
	{
		result = IDirectDrawSurface2_SetPalette(primarySurface2_, palette_);
	}
	if (result == DD_OK)
	{
		result = IDirectDrawSurface2_SetPalette(backBuffer2_, palette_);
	}
	if (result == DD_OK && auxiliarySurface != nullptr)
	{
		result = IDirectDrawSurface2_SetPalette(auxiliarySurface, palette_);
	}
	if (result != DD_OK)
	{
		DirectXAttempt(result, __LINE__, __FILE__);
		return false;
	}
	return true;
}

void DirectDrawPresenter::leaveDisplayMode()
{
	if (directDrawObject2_ != nullptr)
	{
		IDirectDraw2_RestoreDisplayMode(directDrawObject2_);
		if (window_ != nullptr)
		{
			IDirectDraw2_SetCooperativeLevel(
				directDrawObject2_, window_, DDSCL_NORMAL);
		}
	}
}

void DirectDrawPresenter::shutdown()
{
	if (palette_ != nullptr)
	{
		IDirectDrawPalette_Release(palette_);
		palette_ = nullptr;
	}
	if (backBuffer2_ != nullptr)
	{
		IDirectDrawSurface2_Release(backBuffer2_);
		backBuffer2_ = nullptr;
	}
	if (backBuffer1_ != nullptr)
	{
		IDirectDrawSurface_Release(backBuffer1_);
		backBuffer1_ = nullptr;
	}
	if (primarySurface2_ != nullptr)
	{
		IDirectDrawSurface2_Release(primarySurface2_);
		primarySurface2_ = nullptr;
	}
	if (primarySurface1_ != nullptr)
	{
		IDirectDrawSurface_Release(primarySurface1_);
		primarySurface1_ = nullptr;
	}
	if (directDrawObject2_ != nullptr)
	{
		leaveDisplayMode();
		IDirectDraw2_Release(directDrawObject2_);
		directDrawObject2_ = nullptr;
	}
	if (directDrawObject1_ != nullptr)
	{
		IDirectDraw_Release(directDrawObject1_);
		directDrawObject1_ = nullptr;
	}
	window_ = nullptr;
	windowRect_ = nullptr;
}

}
