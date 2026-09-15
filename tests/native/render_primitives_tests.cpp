#include "english.h"
#include "line.h"

#include <array>
#include <cstddef>

extern UINT16 gsKeyTranslationTable[1024];

namespace
{
constexpr int kWidth = 8;
constexpr int kHeight = 6;
constexpr int kPitch = kWidth * 2;

UINT16 pixel(const std::array<UINT8, kPitch * kHeight>& buffer, int x, int y)
{
	const std::size_t offset = static_cast<std::size_t>(y * kPitch + x * 2);
	return static_cast<UINT16>(buffer[offset] | (buffer[offset + 1] << 8));
}
}

int main()
{
	if (gsKeyTranslationTable['A'] != 'a' ||
		gsKeyTranslationTable['A' + 256] != 'A' ||
		gsKeyTranslationTable[8] != BACKSPACE)
	{
		return 1;
	}

	std::array<UINT8, kPitch * kHeight> buffer{};
	SetClippingRegionAndImageWidth(kPitch, 1, 1, 5, 3);
	LineDraw(TRUE, -2, 2, 7, 2, static_cast<INT16>(0x1234), buffer.data());
	if (pixel(buffer, 0, 2) != 0 || pixel(buffer, 1, 2) != 0x1234 ||
		pixel(buffer, 5, 2) != 0x1234 || pixel(buffer, 6, 2) != 0)
	{
		return 2;
	}

	PixelAlterColour(TRUE, 3, 2, static_cast<INT16>(0x00c0), buffer.data());
	if (pixel(buffer, 3, 2) != 0x12f4)
	{
		return 3;
	}

	PixelDraw(TRUE, 0, 0, static_cast<INT16>(0xffff), buffer.data());
	return pixel(buffer, 0, 0) == 0 ? 0 : 4;
}
