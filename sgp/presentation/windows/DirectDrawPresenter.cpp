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

DirectDrawPresenter::DirectDrawPresenter(
	LPDIRECTDRAWSURFACE primarySurface,
	LPDIRECTDRAWSURFACE2 primarySurface2,
	LPDIRECTDRAWSURFACE2 backBuffer, const RECT* windowRect,
	bool windowed) noexcept
	: primarySurface_(primarySurface), primarySurface2_(primarySurface2),
	  backBuffer_(backBuffer), windowRect_(windowRect), windowed_(windowed)
{
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
			backBuffer_, nullptr, &description, 0, nullptr);
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

	result = IDirectDrawSurface2_Unlock(backBuffer_, nullptr);
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
			const_cast<RECT*>(windowRect_), backBuffer_, nullptr,
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
		result = IDirectDrawSurface_Flip(primarySurface_, nullptr,
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
	if (!restoreSurface(primarySurface2_) || !restoreSurface(backBuffer_))
	{
		return false;
	}
	suspended_ = false;
	return true;
}

}
