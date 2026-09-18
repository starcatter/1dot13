	#include "Timer Control.h"
	#include "Overhead.h"
	#include "Handle Items.h"
	#include "worlddef.h"
	#include "renderworld.h"
	#include "Interface Control.h"
	#include "KeyMap.h"
	#include "platform/Clock.h"
	#include "timing/MainLoopScheduler.h"

#include "Soldier Control.h"
#include "connect.h"

// Fixed step of the synthetic game clock, in milliseconds.
static const INT32 BASETIMESLICE = 10;
static const UINT32 DEFAULT_FAST_FORWARD_TIMESLICE = 1000;
static std::uint64_t gNotifyIntervalMicroseconds = 16000;
static INT32 UPDATETIMESLICE = 10000;


INT32	giTimerDiag = 0;

UINT32	guiBaseJA2Clock = 0;
UINT32	guiBaseJA2NoPauseClock = 0;

BOOLEAN	gfPauseClock = FALSE;

const inline UINT32 TIME_MS_TO_US(UINT32 value) { return value * 1000; }

UINT32   giFastForwardPeriod = DEFAULT_FAST_FORWARD_TIMESLICE;
BOOLEAN giFastForwardMode = FALSE;
INT32   giFastForwardKey = 0;
static ja2::timing::MainLoopScheduler gMainLoopScheduler;


INT32		giTimerIntervals[ NUMTIMERS ] =
{
	5,					// Tactical Overhead
	20,					// NEXTSCROLL
	200,				// Start Scroll
	200,				// Animate tiles
	1000,				// FPS Counter
	80,					// PATH FIND COUNTER
	150,				// CURSOR TIMER
	250,				// RIGHT CLICK FOR MENU
	300,				// LEFT
	30,					// SLIDING TEXT
	200,				// TARGET REFINE TIMER
	150,					// CURSOR/AP FLASH
	60,					// FADE MERCS OUT
	160,				// PANEL SLIDE
	1000,				// CLOCK UPDATE DELAY
	20,					// PHYSICS UPDATE
	100,				// FADE ENEMYS
	20,					// STRATEGIC OVERHEAD
	40,
	500,				// NON GUN TARGET REFINE TIMER
	250,				// IMPROVED CURSOR FLASH
	500,				// 2nd CURSOR FLASH
	400,					// RADARMAP BLINK AND OVERHEAD MAP BLINK SHOUDL BE THE SAME
	400,
	10,					// Music Overhead
	100,				// Rubber band start delay
};

// TIMER COUNTERS
INT32		giTimerCounters[ NUMTIMERS ];

INT32		giTimerAirRaidQuote				= 0;
INT32		giTimerAirRaidDiveStarted = 0;
INT32		giTimerAirRaidUpdate			= 0;
INT32		giTimerCustomizable				= 0;
INT32		giTimerTeamTurnUpdate			= 0;

CUSTOMIZABLE_TIMER_CALLBACK gpCustomizableTimerCallback = NULL;

extern UINT32 guiCompressionStringBaseTime;
extern INT32 giFlashHighlightedItemBaseTime;
//extern INT32 giCompatibleItemBaseTime;//Moa:removed (see HandleMouseInCompatableItemForMapSectorInventory)
extern INT32 giAnimateRouteBaseTime;
extern INT32 giPotHeliPathBaseTime;
extern INT32 giPotMilitiaPathBaseTime;
extern INT32 giClickHeliIconBaseTime;
extern INT32 giExitToTactBaseTime;
extern UINT32 guiSectorLocatorBaseTime;
extern INT32 giCommonGlowBaseTime;
extern INT32 giFlashAssignBaseTime;
extern INT32 giFlashContractBaseTime;
extern UINT32 guiFlashCursorBaseTime;
extern INT32 giPotCharPathBaseTime;

// sevenfm: display overflow detection
extern void MapScreenMessage(UINT16 usColor, UINT8 ubPriority, STR16 pStringA, ...);

static BOOLEAN AdvanceTimeCounter(INT32& counter, std::uint64_t ticks)
{
	return ja2::timing::AdvanceLegacyCountdown(
		counter, ticks, BASETIMESLICE) ? TRUE : FALSE;
}

static void HandleJA2ClockOverflow()
{
	MapScreenMessage(162, 0, JA2_TEXT("guiBaseJA2Clock overflow detected!"));
	for (UINT32 index = 0; index < TOTAL_SOLDIERS; ++index)
	{
		if (MercPtrs[index]) MercPtrs[index]->ResetSoldierChangeStatTimer();
	}
}

static void AdvanceJA2BaseClock(std::uint64_t ticks)
{
	const std::uint64_t maximumClock = 0x7fffffffULL;
	while (ticks != 0)
	{
		if (guiBaseJA2Clock > maximumClock)
		{
			guiBaseJA2Clock = 0;
			--ticks;
			HandleJA2ClockOverflow();
			continue;
		}

		const std::uint64_t ticksUntilReset =
			(maximumClock - guiBaseJA2Clock) / BASETIMESLICE + 1;
		if (ticks < ticksUntilReset)
		{
			guiBaseJA2Clock += static_cast<UINT32>(ticks * BASETIMESLICE);
			return;
		}

		guiBaseJA2Clock = 0;
		ticks -= ticksUntilReset;
		HandleJA2ClockOverflow();
	}
}

static BOOLEAN AdvanceGameClock(std::uint64_t ticks)
{
	if (ticks == 0) return FALSE;

	guiBaseJA2NoPauseClock += static_cast<UINT32>(ticks * BASETIMESLICE);
	if (gfPauseClock) return FALSE;

	AdvanceJA2BaseClock(ticks);
	BOOLEAN timerDone = FALSE;
	for (UINT32 index = 0; index < NUMTIMERS; ++index)
		timerDone |= AdvanceTimeCounter(giTimerCounters[index], ticks);

	timerDone |= AdvanceTimeCounter(giTimerAirRaidQuote, ticks);
	timerDone |= AdvanceTimeCounter(giTimerAirRaidDiveStarted, ticks);
	timerDone |= AdvanceTimeCounter(giTimerAirRaidUpdate, ticks);
	timerDone |= AdvanceTimeCounter(giTimerTeamTurnUpdate, ticks);
	if (gpCustomizableTimerCallback)
		timerDone |= AdvanceTimeCounter(giTimerCustomizable, ticks);

	if (guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN)
	{
		for (UINT32 index = gTacticalStatus.Team[gbPlayerNum].bFirstID;
			index <= gTacticalStatus.Team[gbPlayerNum].bLastID; ++index)
		{
			SOLDIERTYPE* soldier = MercPtrs[index];
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.PortraitFlashCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.PanelAnimateCounter, ticks);
		}
	}
	else
	{
		for (UINT32 index = 0; index < guiNumMercSlots; ++index)
		{
			SOLDIERTYPE* soldier = MercSlots[index];
			if (!soldier) continue;
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.UpdateCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.DamageCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.ReloadCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.FlashSelCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.BlinkSelCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.PortraitFlashCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.AICounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.FadeCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.NextTileCounter, ticks);
			timerDone |= AdvanceTimeCounter(soldier->timeCounters.PanelAnimateCounter, ticks);
#ifdef JA2UB
			timerDone |= AdvanceTimeCounter(soldier->GetupFromJA25StartCounter, ticks);
#endif
		}
	}
	return timerDone;
}

BOOLEAN InitializeJA2Clock()
{
	// Init timer delays
	for (INT32 index = 0; index < NUMTIMERS; ++index)
	{
		giTimerCounters[index] = giTimerIntervals[index];
	}
	gMainLoopScheduler.reset(
		Platform::GetClockMicroseconds(), gNotifyIntervalMicroseconds);
	return TRUE;
}

BOOLEAN UpdateJA2Clock()
{
	const bool fastForward = IsFastForwardMode();
	const std::uint64_t now = Platform::GetClockMicroseconds();
	const std::uint64_t tickInterval = fastForward
		? giFastForwardPeriod : static_cast<std::uint64_t>(UPDATETIMESLICE);
	const ja2::timing::SchedulerUpdate update = gMainLoopScheduler.update(
		now, tickInterval, gNotifyIntervalMicroseconds, fastForward);
	return (update.notificationDue || AdvanceGameClock(update.ticksDue))
		? TRUE : FALSE;
}

UINT32 GetJA2ClockNextWakeMilliseconds()
{
	const std::uint64_t remaining = gMainLoopScheduler.timeUntilNext(
		Platform::GetClockMicroseconds(), IsFastForwardMode());
	if (remaining == 0) return 0;
	const std::uint64_t roundedUp = (remaining + 999) / 1000;
	return roundedUp < 0xffffffffULL
		? static_cast<UINT32>(roundedUp) : 0xfffffffeU;
}


void PauseTime( BOOLEAN fPaused )
{
	gfPauseClock = fPaused;
}

void SetCustomizableTimerCallbackAndDelay( INT32 iDelay, CUSTOMIZABLE_TIMER_CALLBACK pCallback, BOOLEAN fReplace )
{
	if ( gpCustomizableTimerCallback )
	{
		if ( !fReplace )
		{
			// replace callback but call the current callback first
			gpCustomizableTimerCallback();
		}
	}

	RESETTIMECOUNTER( giTimerCustomizable, iDelay );
	gpCustomizableTimerCallback = pCallback;
}

void CheckCustomizableTimer( void )
{
	if ( gpCustomizableTimerCallback )
	{
		if ( TIMECOUNTERDONE( giTimerCustomizable, 0 ) )
		{
			// set the callback to a temp variable so we can reset the global variable
			// before calling the callback, so that if the callback sets up another
			// instance of the timer, we don't reset it afterwards
			CUSTOMIZABLE_TIMER_CALLBACK pTempCallback;

			pTempCallback = gpCustomizableTimerCallback;
			gpCustomizableTimerCallback = NULL;
			pTempCallback();
		}
	}
}



void ResetJA2ClockGlobalTimers( void )
{
	UINT32 uiCurrentTime = GetJA2Clock();

	guiCompressionStringBaseTime = uiCurrentTime;
	giFlashHighlightedItemBaseTime = uiCurrentTime;
	//giCompatibleItemBaseTime = uiCurrentTime;//Moa: removed (see HandleMouseInCompatableItemForMapSectorInventory)
	giAnimateRouteBaseTime = uiCurrentTime;
	giPotHeliPathBaseTime = uiCurrentTime;
	giPotMilitiaPathBaseTime = uiCurrentTime;
	giClickHeliIconBaseTime = uiCurrentTime;
	giExitToTactBaseTime = uiCurrentTime;
	guiSectorLocatorBaseTime = uiCurrentTime;

	giCommonGlowBaseTime = uiCurrentTime;
	giFlashAssignBaseTime = uiCurrentTime;
	giFlashContractBaseTime = uiCurrentTime;
	guiFlashCursorBaseTime = uiCurrentTime;
	giPotCharPathBaseTime = uiCurrentTime;
}

void SetTileAnimCounter( INT32 iTime )
{
	giTimerIntervals[ ANIMATETILES ] = iTime;
}

void SetFastForwardPeriod(DOUBLE value)
{
	giFastForwardPeriod = (UINT32)(value);
	if (giFastForwardPeriod <= 1)
		giFastForwardPeriod = 1;
}

void SetFastForwardKey(INT32 key)
{
	giFastForwardKey = key;
}

BOOLEAN IsFastForwardKeyPressed()
{
	// WANNE: In a multiplayer game it is not allowed for the "pure" client to do fast forward
	// Only the server is allowed to do, because the AI is generated on the server
	if (is_networked)
	{
		if (!is_server)				// It is not allowed when we are not the server
			return false;
		else if (gTacticalStatus.ubCurrentTeam != 1)	// It is not allowed, when it is not the enemy turn!
			return false;
	}

	return giFastForwardKey && IsKeyPressed(giFastForwardKey);
}

void SetFastForwardMode(BOOLEAN enable)
{
	giFastForwardMode = enable;
}

BOOLEAN IsFastForwardMode()
{
	return giFastForwardMode || IsFastForwardKeyPressed();
}

void ResetCounter(INT32 counterIdx)
{
	giTimerCounters[ counterIdx ] = giTimerIntervals[ counterIdx ];
}

BOOLEAN CounterDone(INT32 counterIdx)
{
	return ( giTimerCounters[ counterIdx ] == 0 ) ? TRUE : FALSE;
}

void ResetTimerCounter(INT32 &timer, INT32 value)
{
	timer = value;
}

BOOLEAN TimeCounterDone(INT32 timer)
{
	return ( timer == 0 ) ? TRUE : FALSE;
}

void ZeroTimeCounter(INT32& timer)
{
	timer = 0;
}

#ifndef GetJA2Clock
UINT32	GetJA2Clock()
{
	return guiBaseJA2Clock;
}
#endif

#ifndef GetJA2NoPauseClock
UINT32	GetJA2NoPauseClock()
{
	return guiBaseJA2NoPauseClock;
}
#endif

void SetNotifyFrequencyKey(INT32 value)
{
	gNotifyIntervalMicroseconds = value > 0
		? static_cast<std::uint64_t>(value) : 1;
}

void SetClockSpeedPercent(FLOAT value)
{
	UPDATETIMESLICE = value > 0
		? static_cast<INT32>(TIME_MS_TO_US(BASETIMESLICE) * 100.0f / value)
		: TIME_MS_TO_US(BASETIMESLICE);
	if (UPDATETIMESLICE < 1) UPDATETIMESLICE = 1;
}

