#ifndef JA2_PIXEL_SURFACE_H
#define JA2_PIXEL_SURFACE_H

#include "himage.h"
#include "presentation/PresentationTypes.h"

#include <array>
#include <cstddef>
#include <vector>

namespace ja2::presentation
{

struct MutablePixelBuffer
{
	BYTE* pixels = nullptr;
	UINT32 pitchBytes = 0;
	UINT16 width = 0;
	UINT16 height = 0;
	PixelFormat format = PixelFormat::rgb565;
};

struct BlitOptions
{
	bool useSourceColorKey = false;
	bool useDestinationColorKey = false;
};

// Backend-neutral storage for the legacy engine's indexed and RGB565 logical
// surfaces. Rectangles passed to the operations below are half-open.
class PixelSurface
{
public:
	PixelSurface(UINT16 width, UINT16 height, PixelFormat format,
		UINT32 pitchAlignment = 1);

	UINT16 width() const noexcept { return width_; }
	UINT16 height() const noexcept { return height_; }
	PixelFormat format() const noexcept { return format_; }
	UINT32 pitchBytes() const noexcept { return pitchBytes_; }

	MutablePixelBuffer lock() noexcept;
	void unlock() noexcept {}
	ConstPixelBuffer pixels() const noexcept;

	void setPalette(const SGPPaletteEntry* entries, std::size_t count);
	const SGPPaletteEntry* palette() const noexcept;

	void setColorKey(UINT16 pixel) noexcept;
	void clearColorKey() noexcept;
	bool hasColorKey() const noexcept { return hasColorKey_; }
	UINT16 colorKey() const noexcept { return colorKey_; }

	void fill(UINT16 pixel);
	void fillRect(const SGPRect& rect, UINT16 pixel);

	bool blitFrom(const PixelSurface& source, const SGPRect& sourceRect,
		INT32 destinationX, INT32 destinationY,
		const BlitOptions& options = {});
	bool stretchFrom(const PixelSurface& source, const SGPRect& sourceRect,
		const SGPRect& destinationRect, const BlitOptions& options = {});

private:
	std::size_t bytesPerPixel() const noexcept;
	UINT16 readPixel(const std::vector<BYTE>& data, INT32 x, INT32 y) const;
	void writePixel(INT32 x, INT32 y, UINT16 pixel);
	bool shouldCopy(UINT16 sourcePixel, UINT16 destinationPixel,
		const PixelSurface& source, const BlitOptions& options) const noexcept;

	UINT16 width_;
	UINT16 height_;
	PixelFormat format_;
	UINT32 pitchBytes_;
	std::vector<BYTE> pixels_;
	std::array<SGPPaletteEntry, 256> palette_{};
	bool hasPalette_ = false;
	bool hasColorKey_ = false;
	UINT16 colorKey_ = 0;
};

}

#endif
