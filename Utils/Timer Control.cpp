	#include <windows.h>
	#include <string.h>
	#include "stdlib.h"
	#include "DEBUG.H"
	#include "Timer Control.h"
	#include "Overhead.h"
	#include "Handle Items.h"
	#include "worlddef.h"
	#include "renderworld.h"
	#include "Interface Control.h"
	#include "KeyMap.h"
	#include "platform/Clock.h"
	#include "platform/Sleep.h"
	#include "platform/TimerResolution.h"

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN
#endif

#include "Soldier Control.h"
#include "connect.h"

// Fixed step of the synthetic game clock, in milliseconds.
static INT32 BASETIMESLICE = 10;
const INT32 FASTFORWARDTIMESLICE = 1000;
static INT32 MIN_NOTIFY_TIME = 16000;
static INT32 UPDATETIMESLICE = 10000;


INT32	giClockTimer = -1;
INT32	giTimerDiag = 0;

UINT32	guiBaseJA2Clock = 0;
UINT32	guiBaseJA2NoPauseClock = 0;

BOOLEAN	gfPauseClock = FALSE;

const inline UINT32 TIME_US_TO_MS(UINT32 value) { return value / 1000; }
const inline UINT32 TIME_MS_TO_US(UINT32 value) { return value * 1000; }

UINT32   giFastForwardPeriod = FASTFORWARDTIMESLICE;
BOOLEAN giFastForwardMode = FALSE;
INT32   giFastForwardKey = 0;
static std::uint64_t guiClockCountMicroseconds = 0;
static std::uint64_t guiClockCountNextMicroseconds = 0;


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

HANDLE		ghClockThread;
DWORD		gdwClockThreadId;
HANDLE		ghClockThreadShutdown;

HANDLE		ghNotifyThread;
DWORD		gdwNotifyThreadId;
HANDLE		ghNotifyThreadEvent;
HANDLE		ghNotifyThreadShutdownComplete;

// Globals used while advancing soldier counters.
UINT32				gCNT;
SOLDIERTYPE		*gPSOLDIER;

// BOB: made global to help track freeze issue
std::int64_t gliTimestampDiff = 0;
std::int64_t gliWaitTime = 0;
std::int64_t giIncrement = 0;
UINT32 giSleepTime = 0;


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

typedef void (*TIMER_NOTIFY_CALLBACK) ( INT32 timer, PTR state );
struct TIMER_NOTIFY_ITEM
{
	TIMER_NOTIFY_CALLBACK callback;
	PTR state;
};
typedef std::list<TIMER_NOTIFY_ITEM> TIMER_NOTIFY_ITEM_LIST;
typedef TIMER_NOTIFY_ITEM_LIST::iterator TIMER_NOTIFY_ITEM_ITERATOR;
static TIMER_NOTIFY_ITEM_LIST glNotifyCallbacks;
static CRITICAL_SECTION gcsNotifyLock;

static bool HasTimerNotifyCallbacks( );
static void BroadcastTimerNotify(INT32 );
static BOOLEAN UpdateTimeCounter( INT32 &counter, INT32 &iTimeLeft );
static BOOLEAN UpdateCounter( INT32 counter, INT32 &iTimeLeft);
void ResetJA2ClockGlobalTimers(void);

static void AdvanceGameClock()
{
	static BOOLEAN fInFunction = FALSE;
	//SOLDIERTYPE		*pSoldier;

	if ( !fInFunction )
	{
		fInFunction = TRUE;

		BOOLEAN timerDone = FALSE;
		INT32 iTimeLeft = 0;

		// Advance the synthetic game clock once the real monotonic deadline passes.
		guiClockCountMicroseconds = Platform::GetClockMicroseconds();
		if (guiClockCountMicroseconds > guiClockCountNextMicroseconds)
		{
			INT32 iNext = IsFastForwardMode() ? giFastForwardPeriod : UPDATETIMESLICE;
			giIncrement = iNext;
			guiClockCountNextMicroseconds = guiClockCountMicroseconds +
				static_cast<std::uint64_t>(giIncrement);
			iTimeLeft = iNext;
			timerDone = IsFastForwardMode();
			guiBaseJA2NoPauseClock += BASETIMESLICE;

			if ( !gfPauseClock )
			{
				UINT32 uiOldClock = guiBaseJA2Clock;

				guiBaseJA2Clock += BASETIMESLICE;

				// Terapevt suggested fix
				if ((INT32)guiBaseJA2Clock < 0)
					guiBaseJA2Clock = 0;

				// detect overflow
				if (uiOldClock > guiBaseJA2Clock)
				{
					MapScreenMessage(162, 0, L"guiBaseJA2Clock overflow detected!");
					for (gCNT = 0; gCNT < TOTAL_SOLDIERS; gCNT++)
					{
						if (MercPtrs[gCNT])
							MercPtrs[gCNT]->ResetSoldierChangeStatTimer();
					}
				}

				for ( gCNT = 0; gCNT < NUMTIMERS; gCNT++ )
				{
					timerDone |= UpdateCounter( gCNT, iTimeLeft  );
				}

				// Update some specialized countdown timers...
				timerDone |= UpdateTimeCounter( giTimerAirRaidQuote, iTimeLeft );
				timerDone |= UpdateTimeCounter( giTimerAirRaidDiveStarted, iTimeLeft );
				timerDone |= UpdateTimeCounter( giTimerAirRaidUpdate, iTimeLeft );
				timerDone |= UpdateTimeCounter( giTimerTeamTurnUpdate, iTimeLeft );

				if ( gpCustomizableTimerCallback )
				{
					timerDone |= UpdateTimeCounter( giTimerCustomizable, iTimeLeft );
				}


				// If mapscreen...
				if( guiTacticalInterfaceFlags & INTERFACE_MAPSCREEN )
				{
					// IN Mapscreen, loop through player's team.....
					for ( gCNT = gTacticalStatus.Team[ gbPlayerNum ].bFirstID; gCNT <= gTacticalStatus.Team[ gbPlayerNum ].bLastID; gCNT++ )
					{
						gPSOLDIER = MercPtrs[ gCNT ];
						timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.PortraitFlashCounter, iTimeLeft );
						timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.PanelAnimateCounter, iTimeLeft );
					}
				}
				else
				{
					// Set update flags for soldiers
					////////////////////////////
					for ( gCNT = 0; gCNT < guiNumMercSlots; gCNT++ )
					{
						gPSOLDIER = MercSlots[ gCNT ];

						if ( gPSOLDIER != NULL )
						{
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.UpdateCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.DamageCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.ReloadCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.FlashSelCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.BlinkSelCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.PortraitFlashCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.AICounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.FadeCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.NextTileCounter, iTimeLeft );
							timerDone |= UpdateTimeCounter( gPSOLDIER->timeCounters.PanelAnimateCounter, iTimeLeft );
#ifdef JA2UB
							timerDone |= UpdateTimeCounter( gPSOLDIER->GetupFromJA25StartCounter, iTimeLeft );
#endif
						}
					}
				}
			}
		}

		if (timerDone)
			SetEvent(ghNotifyThreadEvent);
		fInFunction = FALSE;
	}
}

static UINT32 MIN_TIMER(UINT32 timer, UINT32 other)
{
	UINT32 value = TIME_MS_TO_US(timer);
	return ( value && value < other ? value : other );
}

// Returns the smallest time interval for a counter currently in use
UINT32 GetNextCounterDoneTime(void)
{
	guiClockCountMicroseconds = Platform::GetClockMicroseconds();
	if (guiClockCountNextMicroseconds >= guiClockCountMicroseconds)
		gliTimestampDiff = static_cast<std::int64_t>(
			guiClockCountNextMicroseconds - guiClockCountMicroseconds);
	else
		gliTimestampDiff = -static_cast<std::int64_t>(
			guiClockCountMicroseconds - guiClockCountNextMicroseconds);
	gliWaitTime = gliTimestampDiff;

	// If the wait time is implausible, restore the old 125-microsecond
	// recovery deadline rather than trusting a corrupt or stale value.
	if (gliWaitTime > 15000 || gliWaitTime < -15000) {
		gliWaitTime = 125;
		guiClockCountNextMicroseconds = guiClockCountMicroseconds + 125;
	}

	return (UINT32)((gliWaitTime > 0) ? gliWaitTime : 0);
}

// Function to test if there are any outstanding timers.  Used in fast forward routines
BOOLEAN IsTimerActive(void)
{
	return GetNextCounterDoneTime() <= FASTFORWARDTIMESLICE ? TRUE : FALSE;
}

DWORD WINAPI JA2ClockThread( LPVOID lpParam ) 
{
	__try
	{
		for(;;) 
		{
			AdvanceGameClock();

			DWORD dwResult = WaitForSingleObject(ghClockThreadShutdown, 0);
			if (dwResult == WAIT_OBJECT_0 || dwResult == WAIT_ABANDONED)
				break;
			YieldProcessor();

			// Sleep for a couple of milliseconds if not in fast forward mode
			if (!IsFastForwardMode()) {
				
				giSleepTime = TIME_US_TO_MS(GetNextCounterDoneTime());

				// monitor the returned sleep times, if we try to sleep for more than 2 secs then somehing must be very wrong.
				if (giSleepTime > 2000) {
					giSleepTime = 250;
				}

				Platform::Sleep(giSleepTime);
			}
		} 
	}
	__except( EXCEPTION_EXECUTE_HANDLER  )
	{
		// Unhandled exception just exit
		__debugbreak();
	}
	return 0L;
}

DWORD WINAPI JA2NotifyThread( LPVOID lpParam )
{
	HANDLE waitHandles[] = {ghClockThreadShutdown, ghNotifyThreadEvent};
	for(;;) 
	{
		DWORD dwResult = WaitForSingleObject(ghClockThreadShutdown, 0);
		if (dwResult == WAIT_OBJECT_0 || dwResult == WAIT_ABANDONED)
			break;

		DWORD waitTime = (!IsFastForwardMode()) ? max(TIME_US_TO_MS(MIN_NOTIFY_TIME), TIME_US_TO_MS( GetNextCounterDoneTime() ) ) : 0;
		dwResult = WaitForMultipleObjectsEx(_countof(waitHandles), waitHandles, FALSE, waitTime, FALSE);
		if (dwResult == WAIT_OBJECT_0)
			break;
		if (dwResult >= WAIT_ABANDONED_0 && dwResult <= (WAIT_ABANDONED_0 + _countof(waitHandles)))
			break;
		if ( dwResult == WAIT_FAILED || dwResult == WAIT_TIMEOUT || (dwResult-WAIT_OBJECT_0) == 1)
		{
			if (HasTimerNotifyCallbacks())
			{
				BroadcastTimerNotify(-1);
			}
		}
		else
		{
			// unexpected failure
			DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "JA2NotifyThread failed!");
			__debugbreak();
		}
	} 
	SetEvent(ghNotifyThreadShutdownComplete);
	return 0L;
}



BOOLEAN InitializeJA2Clock()
{
	INT32			cnt;

	// Init timer delays
	for ( cnt = 0; cnt < NUMTIMERS; cnt++ )
	{
		giTimerCounters[ cnt ] = giTimerIntervals[ cnt ];
	}


	guiClockCountMicroseconds = Platform::GetClockMicroseconds();
	guiClockCountNextMicroseconds = guiClockCountMicroseconds;

	Platform::EnableHighResolutionSleep();

	InitializeCriticalSection(&gcsNotifyLock);

	ghClockThreadShutdown = CreateEvent(NULL, TRUE, FALSE, NULL);
	ghNotifyThreadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	ghNotifyThreadShutdownComplete = CreateEvent(NULL, TRUE, FALSE, NULL);
	ghClockThread = CreateThread(
		NULL, 0, JA2ClockThread, NULL, 0, &gdwClockThreadId);
	ghNotifyThread = CreateThread(
		NULL, 0, JA2NotifyThread, NULL, 0, &gdwNotifyThreadId);

	return TRUE;
}


void	ShutdownJA2Clock(void)
{
	SetEvent(ghClockThreadShutdown);
	WaitForSingleObject(ghNotifyThreadShutdownComplete, 2000);
	HANDLE waitHandles[] = {ghClockThread, ghNotifyThread};
	WaitForMultipleObjects(_countof(waitHandles), waitHandles, TRUE, 1000);
	CloseHandle(ghClockThreadShutdown);
	CloseHandle(ghClockThread);
	CloseHandle(ghNotifyThreadEvent);
	CloseHandle(ghNotifyThread);
	// During ungraceful shutdowns notify lock may be in use in notify thread
	if (TryEnterCriticalSection(&gcsNotifyLock))
	{
		LeaveCriticalSection(&gcsNotifyLock);
		DeleteCriticalSection(&gcsNotifyLock);
	}
	Platform::DisableHighResolutionSleep();
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

void AddTimerNotifyCallback( TIMER_NOTIFY_CALLBACK callback, PTR state )
{
	EnterCriticalSection(&gcsNotifyLock);
	BOOL addItem = TRUE;
	for (TIMER_NOTIFY_ITEM_ITERATOR itr = glNotifyCallbacks.begin(); itr != glNotifyCallbacks.end(); ++itr) {
		if ( callback == (*itr).callback && state == (*itr).state ){
			addItem = FALSE;
			break;
		}
	}
	if (addItem)
	{
		TIMER_NOTIFY_ITEM item;
		item.callback = callback;
		item.state = state;
		glNotifyCallbacks.push_back(item);
	}
	LeaveCriticalSection(&gcsNotifyLock);
}

void RemoveTimerNotifyCallback( TIMER_NOTIFY_CALLBACK callback, PTR state )
{
	EnterCriticalSection(&gcsNotifyLock);
	for ( TIMER_NOTIFY_ITEM_ITERATOR itr = glNotifyCallbacks.begin(); itr != glNotifyCallbacks.end(); ) 
	{
		if ( callback == (*itr).callback && state == (*itr).state )
			itr = glNotifyCallbacks.erase(itr);
		else
			++itr;
	}
	LeaveCriticalSection(&gcsNotifyLock);
}

void ClearTimerNotifyCallbacks()
{
	// If we cannot get the lock it is likely due to exception while handling notification and we are shutting down
	if ( TryEnterCriticalSection(&gcsNotifyLock) )
	{
		glNotifyCallbacks.clear();
		LeaveCriticalSection(&gcsNotifyLock);
	}
}

static bool HasTimerNotifyCallbacks( )
{
	return !glNotifyCallbacks.empty();
}


// Call timer notify routine
//   Separate the callback notifies with normal try/catch
//   as SEH __try/__except are incompatible with C++ exceptions
static void InnerTimerNotify(INT32 timer)
{
   try
   {
	  for (TIMER_NOTIFY_ITEM_ITERATOR itr = glNotifyCallbacks.begin(); itr != glNotifyCallbacks.end(); ++itr)
	  {
		 if ( NULL != (*itr).callback)
			(*itr).callback( timer, (*itr).state );
	  }
   }
   catch (...)   {
	   __debugbreak();
	   DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "InnerTimerNotify Failed!");
   }
}
static void BroadcastTimerNotify(INT32 timer)
{
	EnterCriticalSection(&gcsNotifyLock);
	__try
	{
		__try { InnerTimerNotify(timer); }
		__except( EXCEPTION_EXECUTE_HANDLER  )
		{ /*  Not sure.  exit? */ 
			__debugbreak();
			DebugMsg(TOPIC_JA2, DBG_LEVEL_3, "BroadcastTimerNotify Failed!");
		}
	}
	__finally
	{
		LeaveCriticalSection(&gcsNotifyLock);
	}
}

BOOLEAN UpdateTimeCounter( INT32 &counter, INT32 &iTimeLeft)
{
	if (counter == 0) {
		return FALSE;
	} else if ( ( counter - BASETIMESLICE ) < 0 ) {
		counter = 0;
		return TRUE;
	} else {
		counter -= BASETIMESLICE;
		if ( counter < iTimeLeft )
			iTimeLeft = counter;
		return FALSE;
	}
	return FALSE;
}

BOOLEAN UpdateCounter( INT32 counterIdx, INT32 &iTimeLeft )
{
	INT32& counter = giTimerCounters[ counterIdx ];
	return UpdateTimeCounter(counter, iTimeLeft);
}

BOOLEAN UpdateCounter( INT32 counterIdx )
{
	INT32 iDummy = 0;
	return UpdateCounter(counterIdx, iDummy);
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

BOOLEAN IsJA2TimerThread()
{
	return (GetCurrentThreadId() == gdwClockThreadId);
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
	MIN_NOTIFY_TIME = value;
}

void SetClockSpeedPercent(FLOAT value)
{
	UPDATETIMESLICE = (UINT32)((FLOAT)TIME_MS_TO_US(BASETIMESLICE) * 100.0f / value);
}

