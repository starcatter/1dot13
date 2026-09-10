#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "platform/Sleep.h"

void Platform::Sleep(std::uint32_t milliseconds)
{
	::Sleep(milliseconds);
}
