#ifndef JA2_FILEIO_LOCALTIME_H
#define JA2_FILEIO_LOCALTIME_H

#include <cstdint>

namespace ja2::fileio
{
struct LocalCalendarTime
{
	std::uint16_t year = 0;
	std::uint16_t month = 0;
	std::uint16_t day = 0;
	std::uint16_t hour = 0;
	std::uint16_t minute = 0;
};

LocalCalendarTime localCalendarTimeFromUnixNanoseconds(
	std::int64_t unixNanoseconds) noexcept;
}

#endif
