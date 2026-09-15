#ifndef __TIMER_
#define __TIMER_

#include <cstdint>

typedef std::uint32_t TIMER;

#define MILLISECONDS(a) (a)
#define SECONDS(a)			((a) / 1000)
#define MINUTES(a)			(SECOND((a)) / 60)
#define HOURS(a)				(MINUTES((a)) / 60)
#define DAYS(a)					(HOURS((a)) / 24)

#ifdef __cplusplus
extern "C" {
#endif

bool InitializeClockManager(void);
void	ShutdownClockManager(void);
TIMER	GetClock(void);
TIMER	SetCountdownClock(std::uint32_t TimeToElapse);
std::uint32_t ClockIsTicking(TIMER uiTimer);

#ifdef __cplusplus
}
#endif

#endif
