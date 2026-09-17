#ifndef JA2_PRESENTATION_TYPES_H
#define JA2_PRESENTATION_TYPES_H

#include "types.h"

#include <cstddef>

namespace ja2::presentation
{

enum class PixelFormat
{
	indexed8,
	rgb565,
};

struct ConstPixelBuffer
{
	const BYTE* pixels = nullptr;
	UINT32 pitchBytes = 0;
	UINT16 width = 0;
	UINT16 height = 0;
	PixelFormat format = PixelFormat::rgb565;
};

struct PresentFrame
{
	ConstPixelBuffer buffer;
	const SGPRect* dirtyRegions = nullptr;
	std::size_t dirtyRegionCount = 0;
	bool fullRefresh = false;
	bool verticalSync = false;
};

}

#endif
