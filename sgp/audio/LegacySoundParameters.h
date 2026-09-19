#ifndef JA2_LEGACY_SOUND_PARAMETERS_H
#define JA2_LEGACY_SOUND_PARAMETERS_H

#include <cstdint>
#include <limits>

namespace Audio
{
using EndOfStreamCallback = void (*)(void*);

inline bool IsSpecifiedEndOfStreamCallback(EndOfStreamCallback callback) noexcept
{
	// Legacy callers use memset(..., 0xff, sizeof(SOUNDPARMS)) to mark every
	// optional field as unspecified. On x64 the resulting function pointer is
	// UINTPTR_MAX, not the 32-bit integer sentinel 0xffffffff.
	return callback != nullptr &&
		reinterpret_cast<std::uintptr_t>(callback) !=
			std::numeric_limits<std::uintptr_t>::max();
}
}

#endif
