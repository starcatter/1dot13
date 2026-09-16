#ifndef JA2_LEGACY_SGP_H
#define JA2_LEGACY_SGP_H

// Compatibility umbrella for source files that still depend on the historic
// SGP header fan-out. New platform-neutral code should include sgp.h and the
// specific subsystem headers it uses instead.
#include "sgp.h"
#include "local.h"
#include "video.h"
// Temporary Windows compatibility for legacy translation units. Portable
// code must not obtain native presentation handles through this umbrella.
#include "video_windows.h"

#endif
