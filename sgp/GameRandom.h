#ifndef JA2_GAME_RANDOM_H
#define JA2_GAME_RANDOM_H

#include "types.h"

// Narrow callable boundary for modules that do not need random.h's legacy
// inline implementation and game-settings dependency.
UINT32 GameRandom(UINT32 range);

#endif
