#include <stdio.h>
#include <types.h>
#include <video.h>
#include <MemMan.h>
#include <Overhead Types.h>
#include <Soldier Control.h>
#include "renderworld.h"
#include "input.h"
#include "Font.h"
#include "screenids.h"
#include "Overhead.h"
#include "Font Control.h"
#include "Animation Control.h"
#include "Animation Data.h"
#include "Render Dirty.h"
#include "Sys Globals.h"
#include "english.h"
#include "MessageBoxScreen.h"
#include "fileio/FileIO.h"
#include "fileio/FileServices.h"
#include "fileio/StoreRouter.h"

#include <sstream>
#include <string>
#include <vector>

//forward declarations of common classes to eliminate includes
class OBJECTTYPE;
class SOLDIERTYPE;


void BuildListFile( );


BOOLEAN	gfAniEditMode = FALSE;
static UINT16		usStartAnim = 0;
static UINT8		ubStartHeight = 0;
static SOLDIERTYPE *pSoldier;

static BOOLEAN fOKFiles = FALSE;
static UINT8	 ubNumStates = 0;
static UINT16   *pusStates = NULL;
static INT8   ubCurLoadedState = 0;

static void CycleAnimations( )
{
	INT32 cnt;

	// FInd the next animation with start height the same...
	for ( cnt = usStartAnim + 1; cnt < NUMANIMATIONSTATES; cnt++ )
	{
		if ( gAnimControl[ cnt ].ubHeight == ubStartHeight )
		{
			usStartAnim = ( UINT8) cnt;
			pSoldier->EVENT_InitNewSoldierAnim( usStartAnim, 0 , TRUE );
			return;
		}
	}

	usStartAnim = 0;
	pSoldier->EVENT_InitNewSoldierAnim( usStartAnim, 0 , TRUE );
}


UINT32 AniEditScreenInit(void)
{

  return TRUE;
}

// The ShutdownGame function will free up/undo all things that were started in InitializeGame()
// It will also be responsible to making sure that all Gaming Engine tasks exit properly

UINT32 AniEditScreenShutdown(void)
{

	return TRUE;
}



UINT32  AniEditScreenHandle(void)
{
  InputAtom					InputEvent;
	static BOOLEAN		fFirstTime = TRUE;
	static UINT16			usOldState;
	static BOOLEAN		fToggle = FALSE;
	static BOOLEAN		fToggle2 = FALSE;

//	EV_S_SETPOSITION SSetPosition;

	// Make backups
	if ( fFirstTime )
	{
		gfAniEditMode = TRUE;

		usStartAnim   = 0;
		ubStartHeight = ANIM_STAND;

		fFirstTime = FALSE;
		fToggle		 = FALSE;
		fToggle2   = FALSE;
		ubCurLoadedState = 0;

		pSoldier = gusSelectedSoldier;

		gTacticalStatus.uiFlags |= LOADING_SAVED_GAME;

		pSoldier->EVENT_InitNewSoldierAnim( usStartAnim, 0 , TRUE );

		BuildListFile( );

	}



	/////////////////////////////////////////////////////
	StartFrameBufferRender( );

	RenderWorld( );

	ExecuteBaseDirtyRectQueue( );


	/////////////////////////////////////////////////////
	EndFrameBufferRender( );


	SetFont( LARGEFONT1 );
	mprintf( 0,0,JA2_TEXT("SOLDIER ANIMATION VIEWER") );
	gprintfdirty( (INT16)0,(INT16)0,JA2_TEXT("SOLDIER ANIMATION VIEWER") );


	mprintf( 0,20,JA2_TEXT("Current Animation: %S %S"), gAnimControl[ usStartAnim ].zAnimStr, gAnimSurfaceDatabase[ pSoldier->usAnimSurface ].Filename );
	gprintfdirty( (INT16)0,(INT16)20,JA2_TEXT("Current Animation: %S %S"), gAnimControl[ usStartAnim ].zAnimStr, gAnimSurfaceDatabase[ pSoldier->usAnimSurface ].Filename );


	switch( ubStartHeight )
	{
		case ANIM_STAND:

			mprintf( 0,40,JA2_TEXT("Current Stance: STAND") );
			break;

		case ANIM_CROUCH:

			mprintf( 0,40,JA2_TEXT("Current Stance: CROUCH") );
			break;

		case ANIM_PRONE:

			mprintf( 0,40,JA2_TEXT("Current Stance: PRONE") );
			break;
	}
	gprintfdirty( (INT16)0,(INT16)40,JA2_TEXT("Current Animation: %S"), gAnimControl[ usStartAnim ].zAnimStr );


	if ( fToggle )
	{
		mprintf( 0,60,JA2_TEXT("FORCE ON") );
		gprintfdirty( (INT16)0,(INT16)60,JA2_TEXT("FORCE OFF") );
	}

	if ( fToggle2 )
	{
		mprintf( 0,70,JA2_TEXT("LOADED ORDER ON") );
		gprintfdirty( (INT16)0,(INT16)70,JA2_TEXT("LOADED ORDER ON") );

		mprintf( 0,90,JA2_TEXT("LOADED ORDER : %S"), gAnimControl[ pusStates[ ubCurLoadedState ] ].zAnimStr );
		gprintfdirty( (INT16)0,(INT16)90,JA2_TEXT("LOADED ORDER : %S"), gAnimControl[ pusStates[ ubCurLoadedState ] ].zAnimStr );

	}

  if (DequeueSpecificEvent(&InputEvent, KEY_DOWN|KEY_UP|KEY_REPEAT))
  {
    if ((InputEvent.usEvent == KEY_DOWN)&&(InputEvent.usParam == ESC))
    {
			 fFirstTime = TRUE;

			 gfAniEditMode = FALSE;

	  	 fFirstTimeInGameScreen = TRUE;

			 gTacticalStatus.uiFlags &= (~LOADING_SAVED_GAME);

			 if ( fOKFiles )
			 {
					 MemFree( pusStates );
			 }

			 fOKFiles = FALSE;

			 return( GAME_SCREEN );
    }

		if ((InputEvent.usEvent == KEY_UP) && (InputEvent.usParam == SPACE ))
		{
			if ( !fToggle && !fToggle2 )
			{
				CycleAnimations( );
			}
		}

		if ((InputEvent.usEvent == KEY_UP) && (InputEvent.usParam == 's' ))
		{
			if ( !fToggle )
			{
				UINT16 usAnim=0;
				usOldState = usStartAnim;

				switch( ubStartHeight )
				{
					case ANIM_STAND:

						usAnim = STANDING;
						break;

					case ANIM_CROUCH:

						usAnim = CROUCHING;
						break;

					case ANIM_PRONE:

						usAnim = PRONE;
						break;
				}

				pSoldier->EVENT_InitNewSoldierAnim( usAnim, 0 , TRUE );
			}
			else
			{
				pSoldier->EVENT_InitNewSoldierAnim( usOldState, 0 , TRUE );
			}

			fToggle = !fToggle;
		}

		if ((InputEvent.usEvent == KEY_UP) && (InputEvent.usParam == 'l' ))
		{
			if ( !fToggle2 )
			{
				usOldState = usStartAnim;

				pSoldier->EVENT_InitNewSoldierAnim( pusStates[ ubCurLoadedState ], 0 , TRUE );
			}
			else
			{
				pSoldier->EVENT_InitNewSoldierAnim( usOldState, 0 , TRUE );
			}

			fToggle2 = !fToggle2;
		}


		if ((InputEvent.usEvent == KEY_UP) && (InputEvent.usParam == PGUP ))
		{
			 if ( fOKFiles && fToggle2 )
			 {
					ubCurLoadedState++;

					if ( ubCurLoadedState == ubNumStates )
					{
						ubCurLoadedState = 0;
					}

					pSoldier->EVENT_InitNewSoldierAnim( pusStates[ ubCurLoadedState ], 0 , TRUE );

			 }
		}


		if ((InputEvent.usEvent == KEY_UP) && (InputEvent.usParam == PGDN ))
		{
			 if ( fOKFiles && fToggle2 )
			 {
					ubCurLoadedState--;

					if ( ubCurLoadedState == 0 )
					{
						ubCurLoadedState = ubNumStates;
					}

					pSoldier->EVENT_InitNewSoldierAnim( pusStates[ ubCurLoadedState ], 0 , TRUE );
			 }
		}

		if ((InputEvent.usEvent == KEY_UP) && (InputEvent.usParam == 'c' ))
		{
			// CLEAR!
			usStartAnim = 0;
			pSoldier->EVENT_InitNewSoldierAnim( usStartAnim, 0 , TRUE );
		}

		if ((InputEvent.usEvent == KEY_UP) && (InputEvent.usParam == ENTER ))
		{
			if ( ubStartHeight == ANIM_STAND )
			{
				ubStartHeight = ANIM_CROUCH;
			}
			else if ( ubStartHeight == ANIM_CROUCH )
			{
				ubStartHeight = ANIM_PRONE;
			}
			else
			{
				ubStartHeight = ANIM_STAND;
			}
		}

  }


  return( ANIEDIT_SCREEN );

}


static UINT16 GetAnimStateFromName( STR8 zName )
{
	INT32 cnt;

	// FInd the next animation with start height the same...
	for ( cnt = 0; cnt < NUMANIMATIONSTATES; cnt++ )
	{
		if ( _stricmp( gAnimControl[ cnt ].zAnimStr, zName ) == 0 )
		{
			return( (UINT16) cnt );
		}
	}

	return( 5555 );
}


void BuildListFile( )
{
	std::unique_ptr<ja2::fileio::File> infoFile;
	try
	{
		infoFile = ja2::fileio::storeRouter().openRead("ANITEST.DAT");
	}
	catch(const ja2::fileio::Error&)
	{
		return;
	}

	std::string contents(static_cast<std::size_t>(infoFile->size()), '\0');
	infoFile->readExact(contents.data(), contents.size());
	std::istringstream input(contents);
	std::vector<std::string> filenames;
	std::string filename;
	while(std::getline(input, filename))
	{
		if(!filename.empty() && filename.back() == '\r') filename.pop_back();
		if(!filename.empty()) filenames.push_back(filename);
	}

	// Allocate array
	pusStates = (UINT16 *) MemAlloc( sizeof( UINT16 ) * filenames.size() );

	fOKFiles = TRUE;
	int cnt = 0;
	for(const std::string& current : filenames)
	{
		const UINT16 usState = GetAnimStateFromName(const_cast<CHAR8*>(current.c_str()));

		if ( usState != 5555 )
		{
			// Bob: swapped places, there's numEntries things to be put in pusStates, I'm guessing supposed to start at 0
			pusStates[cnt] = usState;
			cnt++;
			ubNumStates	= (UINT8)cnt;			
			// pusStates[ cnt ] = usState;
		}
		else
		{
			CHAR16 zError[128];
			swprintf( zError, JA2_TEXT("Animation str %S is not known: "), current.c_str() );
			DoMessageBox( MSG_BOX_BASIC_STYLE, zError, ANIEDIT_SCREEN, ( UINT8 )MSG_BOX_FLAG_YESNO, NULL, NULL );
			return;
		}
	}
}
