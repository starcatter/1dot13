	#include <stdio.h>
	#include "types.h"
	#include "Game Events.h"
	#include "Game Clock.h"
	#include "MemMan.h"
	#include "DEBUG.H"
	#include "Font Control.h"
	#include "message.h"
	#include "MiniEvents.h"
	#include "Text.h"

#ifdef JA2TESTVERSION

CHAR16 gEventName[NUMBER_OF_EVENT_TYPES_PLUS_ONE][40]={
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	JA2_TEXT("Null"),
	JA2_TEXT("ChangeLightValue"),
	JA2_TEXT("WeatherStart"),
	JA2_TEXT("WeatherEnd"),
	JA2_TEXT("CheckForQuests"),
	JA2_TEXT("Ambient"),
	JA2_TEXT("AIMResetMercAnnoyance"),
	JA2_TEXT("BobbyRayPurchase"),
	JA2_TEXT("DailyUpdateBobbyRayInventory"),
	JA2_TEXT("UpdateBobbyRayInventory"),
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	JA2_TEXT("DailyUpdateOfMercSite"),
	JA2_TEXT("Day3AddEMailFromSpeck"),
	JA2_TEXT("DelayedHiringOfMerc"),
	JA2_TEXT("HandleInsuredMercs"),
	JA2_TEXT("PayLifeInsuranceForDeadMerc"),
	JA2_TEXT("MercDailyUpdate"),
	JA2_TEXT("MercAboutToLeaveComment"),
	JA2_TEXT("MercContractOver"),
	JA2_TEXT("GroupArrival"),
	JA2_TEXT("Day2AddEMailFromIMP"),
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	JA2_TEXT("MercComplainEquipment"),
	JA2_TEXT("HourlyUpdate"),
	JA2_TEXT("HandleMineIncome"),
	JA2_TEXT("SetupMineIncome"),
	JA2_TEXT("QueuedBattle"),
	JA2_TEXT("LeavingMercArriveInDrassen"),
	JA2_TEXT("LeavingMercArriveInOmerta"),
	JA2_TEXT("SetByNPCSystem"),
	JA2_TEXT("SecondAirportAttendantArrived"),
	JA2_TEXT("HelicopterHoverTooLong"),
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	JA2_TEXT("HelicopterHoverWayTooLong"),
	JA2_TEXT("HelicopterDoneRefuelling"),
	JA2_TEXT("MercLeaveEquipInOmerta"),
	JA2_TEXT("MercLeaveEquipInDrassen"),
	JA2_TEXT("DailyEarlyMorningEvents"),
	JA2_TEXT("GroupAboutToArrive"),
	JA2_TEXT("ProcessTacticalSchedule"),
	JA2_TEXT("BeginRainStorm"),
	JA2_TEXT("EndRainStorm"),
	JA2_TEXT("HandleTownOpinion"),
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	JA2_TEXT("SetupTownOpinion"),
	JA2_TEXT("DelayedDeathHandling"),
	JA2_TEXT("BeginAirRaid"),
	JA2_TEXT("TownLoyaltyUpdate"),
	JA2_TEXT("Meanwhile"),
	JA2_TEXT("BeginCreatureQuest"),
	JA2_TEXT("CreatureSpread"),
	JA2_TEXT("DecayCreatures"),
	JA2_TEXT("CreatureNightPlanning"),
	JA2_TEXT("CreatureAttack"),
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	JA2_TEXT("EvaluateQueenSituation"),
	JA2_TEXT("CheckEnemyControlledSector"),
	JA2_TEXT("TurnOnNightLights"),
	JA2_TEXT("TurnOffNightLights"),
	JA2_TEXT("TurnOnPrimeLights"),
	JA2_TEXT("TurnOffPrimeLights"),
	JA2_TEXT("MercAboutToLeaveComment"),
	JA2_TEXT("ForceTimeInterupt"),
	JA2_TEXT("EnricoEmailEvent"),
	JA2_TEXT("InsuranceInvestigationStarted"),
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	JA2_TEXT("InsuranceInvestigationOver"),
	JA2_TEXT("HandleMinuteUpdate"),
	JA2_TEXT("TemperatureUpdate"),
	JA2_TEXT("Keith going out of business"),
	JA2_TEXT("MERC site back online"),
	JA2_TEXT("Investigate Sector"),
	JA2_TEXT("CheckIfMineCleared"),
	JA2_TEXT("RemoveAssassin"),
	JA2_TEXT("BandageBleedingMercs"),
	JA2_TEXT("ShowUpdateMenu"),
	//1234567890123456789012345678901234567890 (increase size of array if necessary)
	JA2_TEXT("SetMenuReason"),
	JA2_TEXT("AddSoldierToUpdateBox"),
	JA2_TEXT("BeginContractRenewalSequence"),
	JA2_TEXT("RPC_WHINE_ABOUT_PAY"),
	JA2_TEXT("HaventMadeImpCharacterEmail"),
	JA2_TEXT("Rainstorm"),
	JA2_TEXT("Quarter Hour Update"),
	JA2_TEXT("MERC Merc went up level email delay"),
	JA2_TEXT("CPostalService delivery"),
	JA2_TEXT("."),
#ifdef CRIPPLED_VERSION
	JA2_TEXT("Crippled version end game check"),
#endif
	JA2_TEXT("HelicopterHoverForAMinute"),
	JA2_TEXT("HelicopterRefuelForAMinute"),
	JA2_TEXT("MilitiaMovementOrder"),
	JA2_TEXT("PMCEmail"),
	JA2_TEXT("PMCReinforcementArrival"),
	JA2_TEXT("KingpinBounty1"),
	JA2_TEXT("KingpinBounty2"),
	JA2_TEXT("KingpinBounty3"),
	JA2_TEXT("ASDUpdate"),
	JA2_TEXT("ASDPurchaseFuel"),
	JA2_TEXT("ASDPurchaseJeep"),
	JA2_TEXT("ASDPurchaseTank"),
	JA2_TEXT("ASDPurchaseHeli"),
	JA2_TEXT("ASDPurchaseRobot"),
	JA2_TEXT("EnemyHeliUpdate"),
	JA2_TEXT("EnemyHeliRepair"),
	JA2_TEXT("EnemyHeliRefuel"),
	JA2_TEXT("SAMsiteRepaired"),
	JA2_TEXT("MilitiaWebsiteEmail"),
	JA2_TEXT("Weather Normal"),
	JA2_TEXT("Weather Rain"),
	JA2_TEXT("Weather Thunderstorm"),
	JA2_TEXT("Weather Sandstorm"),
	JA2_TEXT("Weather Snow"),
	JA2_TEXT("Intel Enrico Email"),
	JA2_TEXT("Intel Photofact verify"),
	JA2_TEXT("Daily raid events"),
	JA2_TEXT("bloodcat attack"),
	JA2_TEXT("zombie attack"),
	JA2_TEXT("bandit attack"),
	JA2_TEXT("ArmyFinishTraining"),
	JA2_TEXT("MiniEvent"),
	JA2_TEXT("ARC_Event"),
	JA2_TEXT("ReturnTransportGroup"),
};

#endif

void ValidateGameEvents();

STRATEGICEVENT									*gpEventList = NULL;

extern UINT32 guiGameClock;
extern BOOLEAN gfTimeInterruptPause;
BOOLEAN gfPreventDeletionOfAnyEvent = FALSE;
BOOLEAN gfEventDeletionPending = FALSE;

BOOLEAN gfProcessingGameEvents = FALSE;
UINT32	guiTimeStampOfCurrentlyExecutingEvent = 0;

//Determines if there are any events that will be processed between the current global time,
//and the beginning of the next global time.
BOOLEAN GameEventsPending( UINT32 uiAdjustment )
{
	#ifdef CRIPPLED_VERSION
	if( guiDay >= 8 )
	{
		return FALSE;
	}
	#endif
	if( !gpEventList )
		return FALSE;
	if( gpEventList->uiTimeStamp <= GetWorldTotalSeconds() + uiAdjustment )
		return TRUE;
	return FALSE;
}

//returns TRUE if any events were deleted
BOOLEAN DeleteEventsWithDeletionPending()
{
	STRATEGICEVENT *curr, *prev, *temp;
	BOOLEAN fEventDeleted = FALSE;
	//ValidateGameEvents();
	curr = gpEventList;
	prev = NULL;
	while( curr )
	{
		//ValidateGameEvents();
		if( curr->ubFlags & SEF_DELETION_PENDING )
		{
			if( prev )
			{ //deleting node in middle
				prev->next = curr->next;
				temp = curr;
				curr = curr->next;
				MemFree( temp );
				fEventDeleted = TRUE;
				//ValidateGameEvents();
				continue;
			}
			else
			{ //deleting head
				gpEventList = gpEventList->next;
				temp = curr;
				prev = NULL;
				curr = curr->next;
				MemFree( temp );
				fEventDeleted = TRUE;
				//ValidateGameEvents();
				continue;
			}
		}
		prev = curr;
		curr = curr->next;
	}
	gfEventDeletionPending = FALSE;
	return fEventDeleted;
}


static void AdjustClockToEventStamp( STRATEGICEVENT *pEvent, UINT32 *puiAdjustment )
{
	UINT32 uiDiff;

	uiDiff = pEvent->uiTimeStamp - guiGameClock;
	guiGameClock += uiDiff;
	*puiAdjustment -= uiDiff;

	//Calculate the day, hour, and minutes.
	guiDay = ( guiGameClock / NUM_SEC_IN_DAY );
	guiHour = ( guiGameClock - ( guiDay * NUM_SEC_IN_DAY ) ) / NUM_SEC_IN_HOUR;
	guiMin	= ( guiGameClock - ( ( guiDay * NUM_SEC_IN_DAY ) + ( guiHour * NUM_SEC_IN_HOUR ) ) ) / NUM_SEC_IN_MIN;

	#ifdef CRIPPLED_VERSION
	if( guiDay >= 8 )
	{
		guiDay = 8;
		guiHour = 0;
		guiMin = 0;
		return;
	}

	#endif

	swprintf( WORLDTIMESTR, JA2_TEXT("%s %d, %02d:%02d"), gpGameClockString[ STR_GAMECLOCK_DAY_NAME ], guiDay, guiHour, guiMin );
}

//If there are any events pending, they are processed, until the time limit is reached, or
//a major event is processed (one that requires the player's attention).
void ProcessPendingGameEvents( UINT32 uiAdjustment, UINT8 ubWarpCode )
{
	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"ProcessPendingGameEvents");
	STRATEGICEVENT *curr, *pEvent, *prev, *temp;
	BOOLEAN fDeleteEvent = FALSE, fDeleteQueuedEvent = FALSE;

	#ifdef CRIPPLED_VERSION
	if( guiDay >= 8 )
	{
		return;
	}
	#endif

	gfTimeInterrupt = FALSE;
	gfProcessingGameEvents = TRUE;

	//While we have events inside the time range to be updated, process them...
	curr = gpEventList;
	prev = NULL; //prev only used when warping time to target time.
	while( !gfTimeInterrupt && curr && curr->uiTimeStamp <= guiGameClock + uiAdjustment )
	{
		fDeleteEvent = FALSE;
		//Update the time by the difference, but ONLY if the event comes after the current time.
		//In the beginning of the game, series of events are created that are placed in the list
		//BEFORE the start time.	Those events will be processed without influencing the actual time.
		if( curr->uiTimeStamp > guiGameClock && ubWarpCode != WARPTIME_PROCESS_TARGET_TIME_FIRST )
		{
			AdjustClockToEventStamp( curr, &uiAdjustment );
		}
		//Process the event
		if( ubWarpCode != WARPTIME_PROCESS_TARGET_TIME_FIRST )
		{
			fDeleteEvent = ExecuteStrategicEvent( curr );
		}
		else if( curr->uiTimeStamp == guiGameClock + uiAdjustment )
		{ //if we are warping to the target time to process that event first,
			if( !curr->next || curr->next->uiTimeStamp > guiGameClock + uiAdjustment )
			{ //make sure that we are processing the last event for that second
				AdjustClockToEventStamp( curr, &uiAdjustment );

				fDeleteEvent = ExecuteStrategicEvent( curr );

				if( curr && prev && fDeleteQueuedEvent )
				{ //The only case where we are deleting a node in the middle of the list
					prev->next = curr->next;
				}
			}
			else
			{ //We are at the current target warp time however, there are still other events following in this time cycle.
				//We will only target the final event in this time.	NOTE:	Events are posted using a FIFO method
				prev = curr;
				curr = curr->next;
				continue;
			}
		}
		else
		{ //We are warping time to the target time.	We haven't found the event yet,
			//so continuing will keep processing the list until we find it.	NOTE:	Events are posted using a FIFO method
			prev = curr;
			curr = curr->next;
			continue;
		}
		if( fDeleteEvent )
		{
			//Determine if event node is a special event requiring reposting
			switch( curr->ubEventType )
			{
				case RANGED_EVENT:
					AddAdvancedStrategicEvent( ENDRANGED_EVENT, curr->ubCallbackID, curr->uiTimeStamp+curr->uiTimeOffset, curr->uiParam );
					break;
				case PERIODIC_EVENT:
					pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, curr->ubCallbackID, curr->uiTimeStamp+curr->uiTimeOffset, curr->uiParam );
					if( pEvent )
						pEvent->uiTimeOffset = curr->uiTimeOffset;
					break;
				case EVERYDAY_EVENT:
					AddAdvancedStrategicEvent( EVERYDAY_EVENT, curr->ubCallbackID, curr->uiTimeStamp+NUM_SEC_IN_DAY, curr->uiParam );
					break;
			}
			if( curr == gpEventList )
			{
				gpEventList = gpEventList->next;
				MemFree( curr );
				curr = gpEventList;
				prev = NULL;
				//ValidateGameEvents();
			}
			else
			{
				temp = curr;
				prev->next = curr->next;
				curr = curr->next;
				MemFree( temp );
				//ValidateGameEvents();
			}
		}
		else
		{
			prev = curr;
			curr = curr->next;
		}
	}

	gfProcessingGameEvents = FALSE;

	if( gfEventDeletionPending )
	{
		DeleteEventsWithDeletionPending();
	}

	if( uiAdjustment && !gfTimeInterrupt )
		guiGameClock += uiAdjustment;

	DebugMsg (TOPIC_JA2,DBG_LEVEL_3,"ProcessPendingGameEvents done");
}


BOOLEAN AddSameDayStrategicEvent( UINT8 ubCallbackID, UINT32 uiMinStamp, UINT32 uiParam )
{
	return( AddStrategicEvent( ubCallbackID, uiMinStamp + GetWorldDayInMinutes(), uiParam ) );
}

BOOLEAN AddSameDayStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiSecondStamp, UINT32 uiParam )
{
	return( AddStrategicEventUsingSeconds( ubCallbackID, uiSecondStamp + GetWorldDayInSeconds(), uiParam ) );
}

BOOLEAN AddFutureDayStrategicEvent( UINT8 ubCallbackID, UINT32 uiMinStamp, UINT32 uiParam, UINT32 uiNumDaysFromPresent )
{
	UINT32 uiDay;
	uiDay = GetWorldDay();
	return( AddStrategicEvent( ubCallbackID, uiMinStamp + GetFutureDayInMinutes( uiDay + uiNumDaysFromPresent ), uiParam ) );
}

BOOLEAN AddFutureDayStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiSecondStamp, UINT32 uiParam, UINT32 uiNumDaysFromPresent )
{
	UINT32 uiDay;
	uiDay = GetWorldDay();
	return( AddStrategicEventUsingSeconds( ubCallbackID, uiSecondStamp + GetFutureDayInMinutes( uiDay + uiNumDaysFromPresent ) * 60, uiParam ) );
}

STRATEGICEVENT* AddAdvancedStrategicEvent( UINT8 ubEventType, UINT8 ubCallbackID, UINT32 uiTimeStamp, UINT32 uiParam )
{
	STRATEGICEVENT		*pNode, *pNewNode, *pPrevNode;

	if( gfProcessingGameEvents && uiTimeStamp <= guiTimeStampOfCurrentlyExecutingEvent )
	{ //Prevents infinite loops of posting events that are the same time or earlier than the event
		//currently being processed.
		#ifdef JA2TESTVERSION
			//if( ubCallbackID == EVENT_PROCESS_TACTICAL_SCHEDULE )
			{
				ScreenMsg( FONT_RED, MSG_DEBUG, JA2_TEXT("%s Event Rejected:	Can't post events <= time while inside an event callback.	This is a special case situation that isn't a bug."), gEventName[ ubCallbackID ] );
			}
			//else
			//{
			//	AssertMsg( 0, String( "%S Event Rejected:	Can't post events <= time while inside an event callback.", gEventName[ ubCallbackID ] ) );
			//}
		#endif
		return NULL;
	}

	pNewNode = (STRATEGICEVENT *) MemAlloc( sizeof( STRATEGICEVENT ) );
	Assert( pNewNode );
	memset( pNewNode, 0, sizeof( STRATEGICEVENT ) );
	pNewNode->ubCallbackID		= ubCallbackID;
	pNewNode->uiParam					= uiParam;
	pNewNode->ubEventType			= ubEventType;
	pNewNode->uiTimeStamp			= uiTimeStamp;
	pNewNode->uiTimeOffset			= 0;

	// Search list for a place to insert
	pNode = gpEventList;

	// If it's the first head, do this!
	if( !pNode )
	{
		gpEventList = pNewNode;
		pNewNode->next = NULL;
	}
	else
	{
		pPrevNode = NULL;
		while( pNode )
		{
			if( uiTimeStamp < pNode->uiTimeStamp )
			{
				break;
			}
			pPrevNode = pNode;
			pNode = pNode->next;
		}

		// If we are at the end, set at the end!
		if ( !pNode )
		{
			pPrevNode->next = pNewNode;
			pNewNode->next	= NULL;
		}
		else
		{
			// We have a previous node here
			// Insert IN FRONT!
			if ( pPrevNode )
			{
				pNewNode->next = pPrevNode->next;
				pPrevNode->next = pNewNode;
			}
			else
			{	// It's the head
				pNewNode->next = gpEventList;
				gpEventList = pNewNode;
			}
		}
	}

	return pNewNode ;
}

BOOLEAN AddStrategicEvent( UINT8 ubCallbackID, UINT32 uiMinStamp, UINT32 uiParam )
{
	if( AddAdvancedStrategicEvent( ONETIME_EVENT, ubCallbackID, uiMinStamp*60, uiParam ) )
		return TRUE;
	return FALSE;
}

BOOLEAN AddStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiSecondStamp, UINT32 uiParam )
{
	if( AddAdvancedStrategicEvent( ONETIME_EVENT, ubCallbackID, uiSecondStamp, uiParam ) )
		return TRUE;
	return FALSE;
}


BOOLEAN AddRangedStrategicEvent( UINT8 ubCallbackID, UINT32 uiStartMin, UINT32 uiLengthMin, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( RANGED_EVENT, ubCallbackID, uiStartMin*60, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiLengthMin * 60;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddSameDayRangedStrategicEvent( UINT8 ubCallbackID, UINT32 uiStartMin, UINT32 uiLengthMin, UINT32 uiParam)
{
	return AddRangedStrategicEvent( ubCallbackID, uiStartMin + GetWorldDayInMinutes(), uiLengthMin, uiParam );
}

BOOLEAN AddFutureDayRangedStrategicEvent( UINT8 ubCallbackID, UINT32 uiStartMin, UINT32 uiLengthMin, UINT32 uiParam, UINT32 uiNumDaysFromPresent )
{
	return AddRangedStrategicEvent( ubCallbackID, uiStartMin + GetFutureDayInMinutes( GetWorldDay() + uiNumDaysFromPresent ), uiLengthMin, uiParam );
}

BOOLEAN AddRangedStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiStartSeconds, UINT32 uiLengthSeconds, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( RANGED_EVENT, ubCallbackID, uiStartSeconds, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiLengthSeconds;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddSameDayRangedStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiStartSeconds, UINT32 uiLengthSeconds, UINT32 uiParam)
{
	return AddRangedStrategicEventUsingSeconds( ubCallbackID, uiStartSeconds + GetWorldDayInSeconds(), uiLengthSeconds, uiParam );
}

BOOLEAN AddFutureDayRangedStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiStartSeconds, UINT32 uiLengthSeconds, UINT32 uiParam, UINT32 uiNumDaysFromPresent )
{
	return AddRangedStrategicEventUsingSeconds( ubCallbackID, uiStartSeconds + GetFutureDayInMinutes( GetWorldDay() + uiNumDaysFromPresent ) * 60, uiLengthSeconds, uiParam );
}

BOOLEAN AddEveryDayStrategicEvent( UINT8 ubCallbackID, UINT32 uiStartMin, UINT32 uiParam )
{
	if( AddAdvancedStrategicEvent( EVERYDAY_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiStartMin * 60, uiParam ) )
		return TRUE;
	return FALSE;
}

BOOLEAN AddEveryDayStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiStartSeconds, UINT32 uiParam )
{
	if( AddAdvancedStrategicEvent( EVERYDAY_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiStartSeconds, uiParam ) )
		return TRUE;
	return FALSE;
}

//NEW:	Period Events
//Event will get processed automatically once every X minutes.
BOOLEAN AddPeriodStrategicEvent( UINT8 ubCallbackID, UINT32 uiOnceEveryXMinutes, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiOnceEveryXMinutes * 60, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiOnceEveryXMinutes * 60;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddPeriodStrategicEventUsingSeconds( UINT8 ubCallbackID, UINT32 uiOnceEveryXSeconds, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiOnceEveryXSeconds, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiOnceEveryXSeconds;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddPeriodStrategicEventWithOffset( UINT8 ubCallbackID, UINT32 uiOnceEveryXMinutes, UINT32 uiOffsetFromCurrent, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiOffsetFromCurrent * 60, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiOnceEveryXMinutes * 60;
		return TRUE;
	}
	return FALSE;
}

BOOLEAN AddPeriodStrategicEventUsingSecondsWithOffset( UINT8 ubCallbackID, UINT32 uiOnceEveryXSeconds, UINT32 uiOffsetFromCurrent, UINT32 uiParam )
{
	STRATEGICEVENT *pEvent;
	pEvent = AddAdvancedStrategicEvent( PERIODIC_EVENT, ubCallbackID, GetWorldDayInSeconds() + uiOffsetFromCurrent, uiParam );
	if( pEvent )
	{
		pEvent->uiTimeOffset = uiOnceEveryXSeconds;
		return TRUE;
	}
	return FALSE;
}

void DeleteAllStrategicEventsOfType( UINT8 ubCallbackID )
{
	STRATEGICEVENT	*curr, *prev, *temp;
	prev = NULL;
	curr = gpEventList;
	while( curr )
	{
		if( curr->ubCallbackID == ubCallbackID && !(curr->ubFlags & SEF_DELETION_PENDING) )
		{
			if( gfPreventDeletionOfAnyEvent )
			{
				curr->ubFlags |= SEF_DELETION_PENDING;
				gfEventDeletionPending = TRUE;
				prev = curr;
				curr = curr->next;
				continue;
			}
			//Detach the node
			if( prev )
				prev->next = curr->next;
			else
				gpEventList = curr->next;

			//isolate and remove curr
			temp = curr;
			curr = curr->next;
			MemFree( temp );
			//ValidateGameEvents();
		}
		else
		{	//Advance all the nodes
			prev = curr;
			curr = curr->next;
		}
	}
}

void DeleteAllStrategicEvents()
{
	STRATEGICEVENT *temp;
	while( gpEventList )
	{
		temp = gpEventList;
		gpEventList = gpEventList->next;
		MemFree( temp );
		//ValidateGameEvents();
		temp = NULL;
	}
	gpEventList = NULL;
}

//Searches for and removes the first event matching the supplied information.	There may very well be a need
//for more specific event removal, so let me know (Kris), of any support needs.	Function returns FALSE if
//no events were found or if the event wasn't deleted due to delete lock,
BOOLEAN DeleteStrategicEvent( UINT8 ubCallbackID, UINT32 uiParam )
{
	STRATEGICEVENT *curr, *prev;
	curr = gpEventList;
	prev = NULL;
	while( curr )
	{ //deleting middle
		if( curr->ubCallbackID == ubCallbackID && curr->uiParam == uiParam )
		{
			if( !(curr->ubFlags & SEF_DELETION_PENDING) )
			{
				if( gfPreventDeletionOfAnyEvent )
				{
					curr->ubFlags |= SEF_DELETION_PENDING;
					gfEventDeletionPending = TRUE;
					return FALSE;
				}
				if( prev )
				{
					prev->next = curr->next;
				}
				else
				{
					gpEventList = gpEventList->next;
				}
				MemFree( curr );
				//ValidateGameEvents();
				return TRUE;
			}
		}
		prev = curr;
		curr = curr->next;
	}
	return FALSE;
}

std::vector< std::pair<UINT32, UINT32> > GetAllStrategicEventsOfType( UINT8 ubCallbackID )
{
	std::vector< std::pair<UINT32, UINT32> > vec;

	STRATEGICEVENT* curr = gpEventList;
	while ( curr )
	{
		if ( curr->ubCallbackID == ubCallbackID )
		{
			vec.push_back( std::pair<UINT32, UINT32>( curr->uiTimeStamp, curr->uiParam ) );
		}

		curr = curr->next;
	}

	return vec;
}

//part of the game.sav files (not map files)
namespace
{
	// The legacy save format stored the linked-list pointer as a 32-bit value.
	// It was never meaningful on load, but it is part of the established 28-byte
	// Windows record layout.
	struct SavedStrategicEvent
	{
		UINT32 legacyNext;
		UINT32 uiTimeStamp;
		UINT32 uiParam;
		UINT32 uiTimeOffset;
		UINT8 ubEventType;
		UINT8 ubCallbackID;
		UINT8 ubFlags;
		INT8 bPadding[6];
	};

	static_assert( sizeof(SavedStrategicEvent) == 28 );
}

BOOLEAN SaveStrategicEventsToSavedGame( HWFILE hFile )
{
	UINT32	uiNumBytesWritten=0;

	UINT32	uiNumGameEvents=0;
	STRATEGICEVENT *pTempEvent = gpEventList;

	//Go through the list and determine the number of events
	while( pTempEvent )
	{
		pTempEvent = pTempEvent->next;
		uiNumGameEvents++;
	}


	//write the number of strategic events
	FileWrite( hFile, &uiNumGameEvents, sizeof( UINT32 ), &uiNumBytesWritten );
	if( uiNumBytesWritten != sizeof( UINT32 ) )
	{
		return(FALSE);
	}


	//loop through all the events and save them.
	pTempEvent = gpEventList;
	while( pTempEvent )
	{
		SavedStrategicEvent savedEvent{};
		savedEvent.uiTimeStamp = pTempEvent->uiTimeStamp;
		savedEvent.uiParam = pTempEvent->uiParam;
		savedEvent.uiTimeOffset = pTempEvent->uiTimeOffset;
		savedEvent.ubEventType = pTempEvent->ubEventType;
		savedEvent.ubCallbackID = pTempEvent->ubCallbackID;
		savedEvent.ubFlags = pTempEvent->ubFlags;
		memcpy( savedEvent.bPadding, pTempEvent->bPadding, sizeof(savedEvent.bPadding) );

		//write the current strategic event
		FileWrite( hFile, &savedEvent, sizeof(savedEvent), &uiNumBytesWritten );
		if( uiNumBytesWritten != sizeof(savedEvent) )
		{
			return(FALSE);
		}

		pTempEvent = pTempEvent->next;
	}


	return( TRUE );
}


BOOLEAN LoadStrategicEventsFromSavedGame( HWFILE hFile )
{
	UINT32		uiNumGameEvents;
	UINT32		cnt;
	UINT32		uiNumBytesRead=0;
	STRATEGICEVENT *pTemp = NULL;


	//erase the old Game Event queue
	DeleteAllStrategicEvents();


	//Read the number of strategic events
	FileRead( hFile, &uiNumGameEvents, sizeof( UINT32 ), &uiNumBytesRead );
	if( uiNumBytesRead != sizeof( UINT32 ) )
	{
		return(FALSE);
	}


	pTemp = NULL;

	//loop through all the events and save them.
	for( cnt=0; cnt<uiNumGameEvents; cnt++ )
	{
		STRATEGICEVENT *pTempEvent = NULL;

		// allocate memory for the event
		pTempEvent = (STRATEGICEVENT *) MemAlloc( sizeof( STRATEGICEVENT ) );
		if( pTempEvent == NULL )
			return( FALSE );

		SavedStrategicEvent savedEvent{};
		FileRead( hFile, &savedEvent, sizeof(savedEvent), &uiNumBytesRead );
		if( uiNumBytesRead != sizeof(savedEvent) )
		{
			return(FALSE);
		}

		pTempEvent->next = NULL;
		pTempEvent->uiTimeStamp = savedEvent.uiTimeStamp;
		pTempEvent->uiParam = savedEvent.uiParam;
		pTempEvent->uiTimeOffset = savedEvent.uiTimeOffset;
		pTempEvent->ubEventType = savedEvent.ubEventType;
		pTempEvent->ubCallbackID = savedEvent.ubCallbackID;
		pTempEvent->ubFlags = savedEvent.ubFlags;
		memcpy( pTempEvent->bPadding, savedEvent.bPadding, sizeof(savedEvent.bPadding) );

		// Add the new node to the list

		//if its the first node,
		if( cnt == 0 )
		{
			// assign it as the head node
			gpEventList = pTempEvent;

			//assign the 'current node' to the head node
			pTemp = gpEventList;
		}
		else
		{
			// add the new node to the next field of the current node
			pTemp->next = pTempEvent;

			//advance the current node to the next node
			pTemp = pTemp->next;
		}

		// NULL out the next field ( cause there is no next field yet )
		pTempEvent->next = NULL;
	}

	InitMiniEvents();

	return( TRUE );
}

void LockStrategicEventFromDeletion( STRATEGICEVENT *pEvent )
{
	pEvent->ubFlags |= SEF_PREVENT_DELETION;
}

void UnlockStrategicEventFromDeletion( STRATEGICEVENT *pEvent )
{
	pEvent->ubFlags &= ~SEF_PREVENT_DELETION;
}

void ValidateGameEvents()
{
	STRATEGICEVENT *curr;
	curr = gpEventList;
	while( curr )
	{
		curr = curr->next;
		if( curr == (STRATEGICEVENT*)0xdddddddd )
		{
			return;
		}
	}
}
