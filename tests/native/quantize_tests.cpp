#include "Quantize.h"

#include <algorithm>
#include <array>
#include <cassert>

namespace
{
bool contains(const std::array<SGPPaletteEntry, 3>& palette,
	UINT8 red, UINT8 green, UINT8 blue)
{
	return std::any_of(palette.begin(), palette.end(),
		[=](const SGPPaletteEntry& entry)
		{
			return entry.peRed == red && entry.peGreen == green &&
				entry.peBlue == blue && entry.peFlags == 0;
		});
}
}

int main()
{
	std::array<UINT8, 9> bgrPixels{
		0, 0, 255,
		0, 255, 0,
		255, 0, 0,
	};
	CQuantizer quantizer(3, 6);
	assert(quantizer.ProcessImage(bgrPixels.data(), 3, 1));
	assert(quantizer.GetColorCount() == 3);

	std::array<SGPPaletteEntry, 3> palette{};
	quantizer.GetColorTable(palette.data());
	assert(contains(palette, 255, 0, 0));
	assert(contains(palette, 0, 255, 0));
	assert(contains(palette, 0, 0, 255));

	std::array<UINT8, 12> reductionPixels{
		0, 0, 255,
		0, 255, 0,
		255, 0, 0,
		255, 255, 255,
	};
	CQuantizer reduced(2, 6);
	assert(reduced.ProcessImage(reductionPixels.data(), 4, 1));
	assert(reduced.GetColorCount() <= 2);
	return 0;
}
