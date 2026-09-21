#include "presentation/PixelSurface.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace ja2::presentation
{

namespace
{

INT32 clampToRange(INT32 value, INT32 minimum, INT32 maximum)
{
	return std::max(minimum, std::min(value, maximum));
}

}

PixelSurface::PixelSurface(
	UINT16 width, UINT16 height, PixelFormat format, UINT32 pitchAlignment)
	: width_(width), height_(height), format_(format), pitchBytes_(0)
{
	if (width == 0 || height == 0 || pitchAlignment == 0)
	{
		throw std::invalid_argument("Invalid pixel-surface dimensions");
	}

	const std::size_t rowBytes =
		static_cast<std::size_t>(width) * bytesPerPixel();
	const std::size_t alignedPitch =
		((rowBytes + pitchAlignment - 1) / pitchAlignment) * pitchAlignment;
	if (alignedPitch > std::numeric_limits<UINT32>::max() ||
		alignedPitch > std::numeric_limits<std::size_t>::max() / height)
	{
		throw std::overflow_error("Pixel-surface allocation is too large");
	}

	pitchBytes_ = static_cast<UINT32>(alignedPitch);
	pixels_.resize(alignedPitch * height);
}

std::size_t PixelSurface::bytesPerPixel() const noexcept
{
	return format_ == PixelFormat::indexed8 ? 1U : 2U;
}

MutablePixelBuffer PixelSurface::lock() noexcept
{
	return {pixels_.data(), pitchBytes_, width_, height_, format_};
}

ConstPixelBuffer PixelSurface::pixels() const noexcept
{
	return {pixels_.data(), pitchBytes_, width_, height_, format_};
}

bool PixelSurface::replacePixelsFrom(const ConstPixelBuffer& source) noexcept
{
	const std::size_t rowBytes = static_cast<std::size_t>(width_) * bytesPerPixel();
	if (source.pixels == nullptr || source.width != width_ ||
		source.height != height_ || source.format != format_ ||
		source.pitchBytes < rowBytes)
	{
		return false;
	}
	for (UINT16 y = 0; y < height_; ++y)
	{
		std::memcpy(pixels_.data() + static_cast<std::size_t>(y) * pitchBytes_,
			source.pixels + static_cast<std::size_t>(y) * source.pitchBytes, rowBytes);
	}
	return true;
}

bool PixelSurface::copyPixelsTo(const MutablePixelBuffer& destination) const noexcept
{
	const std::size_t rowBytes = static_cast<std::size_t>(width_) * bytesPerPixel();
	if (destination.pixels == nullptr || destination.width != width_ ||
		destination.height != height_ || destination.format != format_ ||
		destination.pitchBytes < rowBytes)
	{
		return false;
	}
	for (UINT16 y = 0; y < height_; ++y)
	{
		std::memcpy(destination.pixels +
				static_cast<std::size_t>(y) * destination.pitchBytes,
			pixels_.data() + static_cast<std::size_t>(y) * pitchBytes_, rowBytes);
	}
	return true;
}

void PixelSurface::setPalette(
	const SGPPaletteEntry* entries, std::size_t count)
{
	if (format_ != PixelFormat::indexed8 || entries == nullptr ||
		count != palette_.size())
	{
		throw std::invalid_argument("Indexed surfaces require a 256-entry palette");
	}
	std::copy_n(entries, count, palette_.begin());
	hasPalette_ = true;
}

const SGPPaletteEntry* PixelSurface::palette() const noexcept
{
	return hasPalette_ ? palette_.data() : nullptr;
}

void PixelSurface::setColorKey(UINT16 pixel) noexcept
{
	hasColorKey_ = true;
	colorKey_ = format_ == PixelFormat::indexed8
		? static_cast<UINT16>(pixel & 0xffU)
		: pixel;
}

void PixelSurface::clearColorKey() noexcept
{
	hasColorKey_ = false;
}

UINT16 PixelSurface::readPixel(
	const std::vector<BYTE>& data, INT32 x, INT32 y) const
{
	const std::size_t offset = static_cast<std::size_t>(y) * pitchBytes_ +
		static_cast<std::size_t>(x) * bytesPerPixel();
	if (format_ == PixelFormat::indexed8)
	{
		return data[offset];
	}
	UINT16 pixel;
	std::memcpy(&pixel, data.data() + offset, sizeof(pixel));
	return pixel;
}

void PixelSurface::writePixel(INT32 x, INT32 y, UINT16 pixel)
{
	const std::size_t offset = static_cast<std::size_t>(y) * pitchBytes_ +
		static_cast<std::size_t>(x) * bytesPerPixel();
	if (format_ == PixelFormat::indexed8)
	{
		pixels_[offset] = static_cast<BYTE>(pixel);
		return;
	}
	std::memcpy(pixels_.data() + offset, &pixel, sizeof(pixel));
}

void PixelSurface::fill(UINT16 pixel)
{
	fillRect({0, 0, width_, height_}, pixel);
}

void PixelSurface::fillRect(const SGPRect& rect, UINT16 pixel)
{
	const INT32 left = clampToRange(rect.iLeft, 0, width_);
	const INT32 top = clampToRange(rect.iTop, 0, height_);
	const INT32 right = clampToRange(rect.iRight, 0, width_);
	const INT32 bottom = clampToRange(rect.iBottom, 0, height_);
	if (left >= right || top >= bottom)
	{
		return;
	}

	const std::size_t pixelBytes = bytesPerPixel();
	const std::size_t rowBytes =
		static_cast<std::size_t>(right - left) * pixelBytes;
	BYTE* firstRow = pixels_.data() +
		static_cast<std::size_t>(top) * pitchBytes_ +
		static_cast<std::size_t>(left) * pixelBytes;
	if (format_ == PixelFormat::indexed8)
	{
		std::memset(firstRow, static_cast<BYTE>(pixel), rowBytes);
	}
	else
	{
		// Seed one RGB565 pixel and duplicate the initialized prefix.  This
		// avoids millions of tiny writePixel()/memcpy calls for full-screen
		// clears while remaining safe for surfaces with unusual pitch alignment.
		std::memcpy(firstRow, &pixel, sizeof(pixel));
		std::size_t initialized = sizeof(pixel);
		while (initialized < rowBytes)
		{
			const std::size_t amount =
				std::min(initialized, rowBytes - initialized);
			std::memcpy(firstRow + initialized, firstRow, amount);
			initialized += amount;
		}
	}

	for (INT32 y = top + 1; y < bottom; ++y)
	{
		BYTE* row = pixels_.data() +
			static_cast<std::size_t>(y) * pitchBytes_ +
			static_cast<std::size_t>(left) * pixelBytes;
		std::memcpy(row, firstRow, rowBytes);
	}
}

bool PixelSurface::shouldCopy(
	UINT16 sourcePixel, UINT16 destinationPixel, const PixelSurface& source,
	const BlitOptions& options) const noexcept
{
	if (options.useSourceColorKey && source.hasColorKey_ &&
		sourcePixel == source.colorKey_)
	{
		return false;
	}
	if (options.useDestinationColorKey && hasColorKey_ &&
		destinationPixel != colorKey_)
	{
		return false;
	}
	return true;
}

bool PixelSurface::blitFrom(
	const PixelSurface& source, const SGPRect& requestedSourceRect,
	INT32 destinationX, INT32 destinationY, const BlitOptions& options)
{
	if (format_ != source.format_)
	{
		return false;
	}

	SGPRect src = requestedSourceRect;
	if (src.iLeft < 0)
	{
		destinationX -= src.iLeft;
		src.iLeft = 0;
	}
	if (src.iTop < 0)
	{
		destinationY -= src.iTop;
		src.iTop = 0;
	}
	src.iRight = std::min<INT32>(src.iRight, source.width_);
	src.iBottom = std::min<INT32>(src.iBottom, source.height_);
	if (destinationX < 0)
	{
		src.iLeft -= destinationX;
		destinationX = 0;
	}
	if (destinationY < 0)
	{
		src.iTop -= destinationY;
		destinationY = 0;
	}

	const INT32 copyWidth = std::min<INT32>(
		src.iRight - src.iLeft, static_cast<INT32>(width_) - destinationX);
	const INT32 copyHeight = std::min<INT32>(
		src.iBottom - src.iTop, static_cast<INT32>(height_) - destinationY);
	if (copyWidth <= 0 || copyHeight <= 0)
	{
		return true;
	}

	const bool useSourceColorKey =
		options.useSourceColorKey && source.hasColorKey_;
	const bool useDestinationColorKey =
		options.useDestinationColorKey && hasColorKey_;
	if (!useSourceColorKey && !useDestinationColorKey)
	{
		const std::size_t pixelBytes = bytesPerPixel();
		const std::size_t copyBytes =
			static_cast<std::size_t>(copyWidth) * pixelBytes;
		const auto copyRow = [&](INT32 row)
		{
			BYTE* destination = pixels_.data() +
				static_cast<std::size_t>(destinationY + row) * pitchBytes_ +
				static_cast<std::size_t>(destinationX) * pixelBytes;
			const BYTE* sourceRow = source.pixels_.data() +
				static_cast<std::size_t>(src.iTop + row) * source.pitchBytes_ +
				static_cast<std::size_t>(src.iLeft) * pixelBytes;
			if (this == &source)
			{
				std::memmove(destination, sourceRow, copyBytes);
			}
			else
			{
				std::memcpy(destination, sourceRow, copyBytes);
			}
		};

		if (this == &source && destinationY > src.iTop)
		{
			for (INT32 y = copyHeight; y-- > 0;)
			{
				copyRow(y);
			}
		}
		else
		{
			for (INT32 y = 0; y < copyHeight; ++y)
			{
				copyRow(y);
			}
		}
		return true;
	}

	const std::vector<BYTE>* sourcePixels = &source.pixels_;
	std::vector<BYTE> snapshot;
	if (this == &source)
	{
		snapshot = source.pixels_;
		sourcePixels = &snapshot;
	}
	if (format_ == PixelFormat::indexed8)
	{
		for (INT32 y = 0; y < copyHeight; ++y)
		{
			const BYTE* sourceRow = sourcePixels->data() +
				static_cast<std::size_t>(src.iTop + y) * source.pitchBytes_ +
				static_cast<std::size_t>(src.iLeft);
			BYTE* destinationRow = pixels_.data() +
				static_cast<std::size_t>(destinationY + y) * pitchBytes_ +
				static_cast<std::size_t>(destinationX);
			for (INT32 x = 0; x < copyWidth; ++x)
			{
				const BYTE sourcePixel = sourceRow[x];
				if ((!useSourceColorKey || sourcePixel != source.colorKey_) &&
					(!useDestinationColorKey ||
						destinationRow[x] == colorKey_))
				{
					destinationRow[x] = sourcePixel;
				}
			}
		}
		return true;
	}

	const bool alignedRgb565 =
		(source.pitchBytes_ % alignof(UINT16)) == 0 &&
		(pitchBytes_ % alignof(UINT16)) == 0 &&
		(reinterpret_cast<std::uintptr_t>(sourcePixels->data()) %
			alignof(UINT16)) == 0 &&
		(reinterpret_cast<std::uintptr_t>(pixels_.data()) %
			alignof(UINT16)) == 0;
	for (INT32 y = 0; y < copyHeight; ++y)
	{
		const BYTE* sourceBytes = sourcePixels->data() +
			static_cast<std::size_t>(src.iTop + y) * source.pitchBytes_ +
			static_cast<std::size_t>(src.iLeft) * sizeof(UINT16);
		BYTE* destinationBytes = pixels_.data() +
			static_cast<std::size_t>(destinationY + y) * pitchBytes_ +
			static_cast<std::size_t>(destinationX) * sizeof(UINT16);
		if (alignedRgb565)
		{
			const auto* sourceRow =
				reinterpret_cast<const UINT16*>(sourceBytes);
			auto* destinationRow = reinterpret_cast<UINT16*>(destinationBytes);
			for (INT32 x = 0; x < copyWidth; ++x)
			{
				const UINT16 sourcePixel = sourceRow[x];
				if ((!useSourceColorKey || sourcePixel != source.colorKey_) &&
					(!useDestinationColorKey ||
						destinationRow[x] == colorKey_))
				{
					destinationRow[x] = sourcePixel;
				}
			}
			continue;
		}

		for (INT32 x = 0; x < copyWidth; ++x)
		{
			UINT16 sourcePixel = 0;
			UINT16 destinationPixel = 0;
			std::memcpy(&sourcePixel,
				sourceBytes + static_cast<std::size_t>(x) * sizeof(UINT16),
				sizeof(sourcePixel));
			if (useDestinationColorKey)
			{
				std::memcpy(&destinationPixel,
					destinationBytes +
						static_cast<std::size_t>(x) * sizeof(UINT16),
					sizeof(destinationPixel));
			}
			if ((!useSourceColorKey || sourcePixel != source.colorKey_) &&
				(!useDestinationColorKey || destinationPixel == colorKey_))
			{
				std::memcpy(destinationBytes +
						static_cast<std::size_t>(x) * sizeof(UINT16),
					&sourcePixel, sizeof(sourcePixel));
			}
		}
	}
	return true;
}

bool PixelSurface::stretchFrom(
	const PixelSurface& source, const SGPRect& sourceRect,
	const SGPRect& destinationRect, const BlitOptions& options)
{
	if (format_ != source.format_ ||
		sourceRect.iRight <= sourceRect.iLeft ||
		sourceRect.iBottom <= sourceRect.iTop ||
		destinationRect.iRight <= destinationRect.iLeft ||
		destinationRect.iBottom <= destinationRect.iTop ||
		sourceRect.iLeft < 0 || sourceRect.iTop < 0 ||
		sourceRect.iRight > source.width_ || sourceRect.iBottom > source.height_)
	{
		return false;
	}

	const INT32 left = clampToRange(destinationRect.iLeft, 0, width_);
	const INT32 top = clampToRange(destinationRect.iTop, 0, height_);
	const INT32 right = clampToRange(destinationRect.iRight, 0, width_);
	const INT32 bottom = clampToRange(destinationRect.iBottom, 0, height_);
	const INT32 sourceWidth = sourceRect.iRight - sourceRect.iLeft;
	const INT32 sourceHeight = sourceRect.iBottom - sourceRect.iTop;
	const INT32 destinationWidth = destinationRect.iRight - destinationRect.iLeft;
	const INT32 destinationHeight = destinationRect.iBottom - destinationRect.iTop;

	const std::vector<BYTE>* sourcePixels = &source.pixels_;
	std::vector<BYTE> snapshot;
	if (this == &source)
	{
		snapshot = source.pixels_;
		sourcePixels = &snapshot;
	}
	for (INT32 y = top; y < bottom; ++y)
	{
		const INT32 sourceY = sourceRect.iTop +
			static_cast<INT32>((static_cast<INT64>(y - destinationRect.iTop) *
				sourceHeight) / destinationHeight);
		for (INT32 x = left; x < right; ++x)
		{
			const INT32 sourceX = sourceRect.iLeft +
				static_cast<INT32>((static_cast<INT64>(x - destinationRect.iLeft) *
					sourceWidth) / destinationWidth);
			const UINT16 sourcePixel =
				source.readPixel(*sourcePixels, sourceX, sourceY);
			const UINT16 destinationPixel = readPixel(pixels_, x, y);
			if (shouldCopy(sourcePixel, destinationPixel, source, options))
			{
				writePixel(x, y, sourcePixel);
			}
		}
	}
	return true;
}

}
