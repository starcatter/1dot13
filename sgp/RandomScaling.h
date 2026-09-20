#ifndef JA2_RANDOM_SCALING_H
#define JA2_RANDOM_SCALING_H

#include "types.h"

namespace ja2::random
{

inline UINT32 ScaleUint32ToRange(UINT32 value, UINT32 range)
{
	if (range == 0)
		return 0;

	return static_cast<UINT32>((static_cast<UINT64>(value) * range) >> 32);
}

}

#endif
