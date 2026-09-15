#include "fileio/LocalTime.h"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
using ja2::fileio::LocalCalendarTime;
using ja2::fileio::localCalendarTimeFromUnixNanoseconds;

class ScopedTimezone
{
public:
	explicit ScopedTimezone(const char* timezone)
	{
		const char* previous = std::getenv("TZ");
		if(previous != nullptr)
		{
			hadPrevious_ = true;
			previous_ = previous;
		}
		setenv("TZ", timezone, 1);
		tzset();
	}

	~ScopedTimezone()
	{
		if(hadPrevious_)
			setenv("TZ", previous_.c_str(), 1);
		else
			unsetenv("TZ");
		tzset();
	}

private:
	bool hadPrevious_ = false;
	std::string previous_;
};

void requireTime(const LocalCalendarTime& actual, std::uint16_t year,
	std::uint16_t month, std::uint16_t day, std::uint16_t hour,
	std::uint16_t minute, const char* message)
{
	if(actual.year != year || actual.month != month || actual.day != day ||
		actual.hour != hour || actual.minute != minute)
	{
		throw std::runtime_error(message);
	}
}

void testUtcConversion()
{
	ScopedTimezone timezone("UTC0");
	requireTime(localCalendarTimeFromUnixNanoseconds(0),
		1970, 1, 1, 0, 0, "Unix epoch UTC conversion");
	requireTime(localCalendarTimeFromUnixNanoseconds(59999999999LL),
		1970, 1, 1, 0, 0, "sub-minute nanoseconds do not round up");
	requireTime(localCalendarTimeFromUnixNanoseconds(60000000000LL),
		1970, 1, 1, 0, 1, "minute boundary conversion");
	requireTime(localCalendarTimeFromUnixNanoseconds(-1),
		1969, 12, 31, 23, 59, "negative fractional second floors correctly");
}

void testLocalConversion()
{
	ScopedTimezone timezone("EST5");
	requireTime(localCalendarTimeFromUnixNanoseconds(0),
		1969, 12, 31, 19, 0, "UTC timestamp uses the local timezone");
}
}

int main()
{
	try
	{
		testUtcConversion();
		testLocalConversion();
		std::cout << "local time tests passed\n";
		return 0;
	}
	catch(const std::exception& error)
	{
		std::cerr << "local time tests failed: " << error.what() << '\n';
		return 1;
	}
}
