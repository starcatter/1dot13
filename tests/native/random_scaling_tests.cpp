#include "RandomScaling.h"

#include <cstdlib>
#include <iostream>

namespace
{

void Require(bool condition, const char* message)
{
	if (!condition)
	{
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

}

int main()
{
	using ja2::random::ScaleUint32ToRange;

	Require(ScaleUint32ToRange(0, 0) == 0, "zero range is safe");
	Require(ScaleUint32ToRange(0, 1000) == 0, "zero maps to the first bucket");
	Require(ScaleUint32ToRange(0x80000000u, 1000) == 500,
		"the midpoint maps to the middle bucket");
	Require(ScaleUint32ToRange(0xffffffffu, 1000) == 999,
		"the maximum maps inside the last bucket");

	UINT32 counts[16] = {};
	for (UINT32 highBits = 0; highBits < 65536; ++highBits)
	{
		const UINT32 bucket = ScaleUint32ToRange(highBits << 16, 16);
		Require(bucket < 16, "scaled value stays in range");
		++counts[bucket];
	}
	for (UINT32 count : counts)
		Require(count == 4096, "full-width samples distribute evenly");

	std::cout << "random scaling tests passed\n";
	return 0;
}
