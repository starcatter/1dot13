#include "LocalTime.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <ctime>
#include <limits>
#endif

namespace ja2::fileio
{
LocalCalendarTime localCalendarTimeFromUnixNanoseconds(
	std::int64_t unixNanoseconds) noexcept
{
	LocalCalendarTime result{};
#ifdef _WIN32
	constexpr std::int64_t nanosecondsPerWindowsTick = 100;
	constexpr std::int64_t unixEpochWindowsTicks = 116444736000000000LL;
	std::int64_t unixTicks = unixNanoseconds / nanosecondsPerWindowsTick;
	if (unixNanoseconds < 0 && unixNanoseconds % nanosecondsPerWindowsTick != 0)
		--unixTicks;
	const std::int64_t windowsTicks = unixEpochWindowsTicks +
		unixTicks;
	const std::uint64_t rawWindowsTicks = static_cast<std::uint64_t>(windowsTicks);

	FILETIME fileTime{};
	fileTime.dwLowDateTime = static_cast<DWORD>(rawWindowsTicks);
	fileTime.dwHighDateTime = static_cast<DWORD>(rawWindowsTicks >> 32);
	SYSTEMTIME utcTime{};
	SYSTEMTIME localTime{};
	if (!FileTimeToSystemTime(&fileTime, &utcTime) ||
		!SystemTimeToTzSpecificLocalTime(nullptr, &utcTime, &localTime))
	{
		return result;
	}

	result.year = localTime.wYear;
	result.month = localTime.wMonth;
	result.day = localTime.wDay;
	result.hour = localTime.wHour;
	result.minute = localTime.wMinute;
#else
	constexpr std::int64_t nanosecondsPerSecond = 1000000000LL;
	std::int64_t unixSeconds = unixNanoseconds / nanosecondsPerSecond;
	if (unixNanoseconds < 0 && unixNanoseconds % nanosecondsPerSecond != 0)
		--unixSeconds;

	if constexpr (std::numeric_limits<std::time_t>::is_signed)
	{
		if constexpr (sizeof(std::time_t) < sizeof(std::int64_t))
		{
			if (unixSeconds < static_cast<std::int64_t>((std::numeric_limits<std::time_t>::min)()) ||
				unixSeconds > static_cast<std::int64_t>((std::numeric_limits<std::time_t>::max)()))
			{
				return result;
			}
		}
	}
	else
	{
		if (unixSeconds < 0) return result;
		if constexpr (sizeof(std::time_t) < sizeof(std::uint64_t))
		{
			if (static_cast<std::uint64_t>(unixSeconds) >
				static_cast<std::uint64_t>((std::numeric_limits<std::time_t>::max)()))
			{
				return result;
			}
		}
	}

	const std::time_t time = static_cast<std::time_t>(unixSeconds);
	std::tm localTime{};
	if (localtime_r(&time, &localTime) == nullptr) return result;

	result.year = static_cast<std::uint16_t>(localTime.tm_year + 1900);
	result.month = static_cast<std::uint16_t>(localTime.tm_mon + 1);
	result.day = static_cast<std::uint16_t>(localTime.tm_mday);
	result.hour = static_cast<std::uint16_t>(localTime.tm_hour);
	result.minute = static_cast<std::uint16_t>(localTime.tm_min);
#endif
	return result;
}
}
