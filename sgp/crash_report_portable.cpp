#include "crash_report.h"

#ifndef _WIN32

namespace sgp
{
void raiseAssertException(unsigned, const char*, const char*)
{
	// The assertion UI and log are still produced by DEBUG.cpp. Native crash
	// capture will be supplied by the platform crash service later.
}

void writeExceptionBacktrace(_EXCEPTION_POINTERS*) {}
void setCrashBuildId(const char*) {}
const wchar_t* crashReportMessage() { return nullptr; }
void setCrashUserHandle(const wchar_t*) {}
}

#endif
