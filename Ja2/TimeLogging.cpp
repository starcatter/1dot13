#include "TimeLogging.h"
#include "fileio/LogStore.h"
#include "time.h"

#include <cstring>
#include <sstream>
#include <string>


clock_t starttime;
clock_t t0;
clock_t t1;
static std::string timingLogName;

void TimingLogInitialize(const CHAR8* filename)
{
	starttime = clock();
	t1 = starttime;

	if (timingLogName.empty())
	{
		timingLogName = filename;
	}
}


void TimingLog(const CHAR8* logEvent, int n)
{
	if (!timingLogName.empty())
	{
		t0 = t1;
		t1 = clock();
		std::ostringstream line;
		line << logEvent << std::string(n, '\t') << ": "
			<< (static_cast<float>(t1 - t0) / CLOCKS_PER_SEC) << " s";
		(void)ja2::fileio::logStore().appendLine(timingLogName, line.str());
	}
}


void TimingLogTotalTime(const CHAR8* logEvent, int n)
{
	if (!timingLogName.empty())
	{
		t1 = clock();
		std::ostringstream line;
		line << logEvent << std::string(n, '\t') << ": "
			<< (static_cast<float>(t1 - starttime) / CLOCKS_PER_SEC) << " s";
		(void)ja2::fileio::logStore().appendLine(timingLogName, line.str());
	}
}


void TimingLogWrite(const CHAR8* text)
{
	if (!timingLogName.empty())
	{
		(void)ja2::fileio::logStore().appendBytes(timingLogName, text, strlen(text));
	}
}


void TimingLogStop()
{
	if (!timingLogName.empty())
	{
		static const char newline[] = "\r\n";
		(void)ja2::fileio::logStore().appendBytes(timingLogName, newline, sizeof(newline) - 1);
		timingLogName.clear();
	}
}
