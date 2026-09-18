#ifndef JA2_APPLICATION_PLATFORM_H
#define JA2_APPLICATION_PLATFORM_H

#include "platform/ApplicationHost.h"
#include "types.h"

namespace ja2::application
{

// The narrow platform seam needed by the legacy game lifecycle. Runtime
// settings (including the logical resolution) are loaded before initializeVideo
// is called, so each host can create its native window and presenter correctly.
class ApplicationPlatform
{
public:
	virtual ~ApplicationPlatform() = default;
	virtual BOOLEAN initializeVideo() = 0;
	virtual Platform::ApplicationHost& applicationHost() = 0;
};

}

#endif
