#include <stdio.h>
#include <stdlib.h>
#include "DEBUG.H"
#include "local.h"
#include "video.h"
#include "himage.h"
#include "vsurface.h"
#include "vsurface_private.h"
#include "WCheck.h"
#include "vobject_blitters.h"

extern void SetClippingRect(SGPRect *clip);
extern void GetClippingRect(SGPRect *clip);

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Video Surface SGP Module
//
// Second Revision: Dec 10, 1996, Andrew Emmons
//
///////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Defines
//
///////////////////////////////////////////////////////////////////////////////////////////////////

//
// This define is sent to CreateList SGP function. It dynamically re-sizes if
// the list gets larger
//

#define DEFAULT_NUM_REGIONS		5
#define DEFAULT_VIDEO_SURFACE_LIST_SIZE	10

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// LOCAL functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN FillSurface( HVSURFACE hDestVSurface, blt_vs_fx *pBltFx );
BOOLEAN FillSurfaceRect( HVSURFACE hDestVSurface, blt_vs_fx *pBltFx );
BOOLEAN BltVSurfaceUsingPixelSurface( HVSURFACE hDestVSurface, HVSURFACE hSrcVSurface, UINT32 fBltFlags, INT32 iDestX, INT32 iDestY, SGPRect *SrcRect );
BOOLEAN StretchVSurfaceUsingPixelSurface( HVSURFACE hDestVSurface, HVSURFACE hSrcVSurface, UINT32 fBltFlags, SGPRect *SrcRect, SGPRect *DestRect );
BOOLEAN GetVSurfaceRect( HVSURFACE hVSurface, SGPRect *pRect);

void DeletePrimaryVideoSurfaces( );

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// LOCAL global variables
//
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct VSURFACE_NODE
{
	HVSURFACE hVSurface;
	UINT32 uiIndex;
	struct VSURFACE_NODE *next, *prev;

#ifdef SGP_VIDEO_DEBUGGING
	STR8									pName;
	STR8									pCode;
#endif

}VSURFACE_NODE;

VSURFACE_NODE  *gpVSurfaceHead = NULL;
VSURFACE_NODE  *gpVSurfaceTail = NULL;
UINT32				guiVSurfaceIndex = 0;
UINT32				guiVSurfaceSize = 0;
UINT32				guiVSurfaceTotalAdded = 0;

#ifdef _DEBUG
enum
{
	DEBUGSTR_NONE,
	DEBUGSTR_SETVIDEOSURFACETRANSPARENCY,
	DEBUGSTR_ADDVIDEOSURFACEREGION,
	DEBUGSTR_GETVIDEOSURFACEDESCRIPTION,
	DEBUGSTR_BLTVIDEOSURFACE_DST,
	DEBUGSTR_BLTVIDEOSURFACE_SRC,
	DEBUGSTR_COLORFILLVIDEOSURFACEAREA,
	DEBUGSTR_SHADOWVIDEOSURFACERECT,
	DEBUGSTR_BLTSTRETCHVIDEOSURFACE_DST,
	DEBUGSTR_BLTSTRETCHVIDEOSURFACE_SRC,
	DEBUGSTR_DELETEVIDEOSURFACEFROMINDEX
};

UINT8 gubVSDebugCode = 0;

void CheckValidVSurfaceIndex( UINT32 uiIndex );
#endif

INT32				giMemUsedInSurfaces;

HVSURFACE		ghPrimary = NULL;
HVSURFACE		ghBackBuffer = NULL;
HVSURFACE   ghFrameBuffer = NULL;
HVSURFACE   ghMouseBuffer = NULL;

namespace
{
ja2::presentation::PixelFormat PixelFormatForSurface(const HVSURFACE surface)
{
	return surface->ubBitDepth == 8
		? ja2::presentation::PixelFormat::indexed8
		: ja2::presentation::PixelFormat::rgb565;
}
}

#include <map>
#include <vector>

extern std::map<UINT32,ClipRectangle> g_SurfaceRectangle;

namespace SurfaceData
{
	typedef void* tSurface;
	std::map<tID,tSurface>		_surfaceID;
	std::map<tSurface,BYTE*>	_surfaceData;
	std::map<BYTE*,tID>			_surfaceOfData;

	void RegisterSurface(tID surfaceID, HVSURFACE surface)
	{
		SurfaceData::_surfaceID[surfaceID] = surface;
		g_SurfaceRectangle[surfaceID].SetRect(surface->usWidth, surface->usHeight);
	}
	void UnRegisterSurface(tID surfaceID)
	{
		std::map<tID,tSurface>::iterator it = SurfaceData::_surfaceID.find(surfaceID);
		if(it != SurfaceData::_surfaceID.end())
		{
			g_SurfaceRectangle.erase(it->first);
			SurfaceData::_surfaceID.erase(it);
		}
	}
	void UnRegisterSurface(HVSURFACE surface)
	{
		std::map<tID,tSurface>::iterator it = SurfaceData::_surfaceID.begin();
		for(;it != SurfaceData::_surfaceID.end(); it++)
		{
			if(it->second == surface)
			{
				SurfaceData::_surfaceID.erase(it);
				return;
			}
		}
	}

	BYTE* SetSurfaceData(tID surfaceID, BYTE* data)
	{
		std::map<tID,tSurface>::iterator sit = SurfaceData::_surfaceID.find(surfaceID);
		if(sit != SurfaceData::_surfaceID.end())
		{
			SurfaceData::_surfaceData[sit->second] = data;
			SurfaceData::_surfaceOfData[data] = surfaceID;
			return data;
		}
		SGP_THROW(L"Unregistered surface ID");
	}
	BYTE* SetSurfaceData(HVSURFACE surface, BYTE* data)
	{
		std::map<tID,tSurface>::iterator sit = SurfaceData::_surfaceID.begin();
		for(;sit != SurfaceData::_surfaceID.end(); ++sit)
		{
			if(sit->second == surface)
			{
				SurfaceData::_surfaceData[sit->second] = data;
				SurfaceData::_surfaceOfData[data] = sit->first;
				return data;
			}
		}
		SGP_THROW(L"Unregistered surface");
	}
	void ReleaseSurfaceData(tID surfaceID)
	{
		// does a surface with this ID exist
		std::map<tID,tSurface>::iterator sit = SurfaceData::_surfaceID.find(surfaceID);
		if(sit != SurfaceData::_surfaceID.end())
		{
			// get saved data of this surface
			std::map<tSurface,BYTE*>::iterator dit = SurfaceData::_surfaceData.find(sit->second);
			if(dit != SurfaceData::_surfaceData.end())
			{
				// as data pointer is going to be removed, release its connection to a surface ID
				std::map<BYTE*,tID>::iterator it = SurfaceData::_surfaceOfData.find(dit->second);
				if(it != SurfaceData::_surfaceOfData.end())
				{
					SurfaceData::_surfaceOfData.erase(it);
				}
				// finally remove data pointer
				SurfaceData::_surfaceData.erase(dit);
			}
		}
	}
	void ReleaseSurfaceData(HVSURFACE surface)
	{
		// get saved data of this surface
		std::map<tSurface,BYTE*>::iterator dit = SurfaceData::_surfaceData.find(surface);
		if(dit != SurfaceData::_surfaceData.end())
		{
			// as data pointer is going to be removed, release its connection to a surface ID
			std::map<BYTE*,tID>::iterator it = SurfaceData::_surfaceOfData.find(dit->second);
			if(it != SurfaceData::_surfaceOfData.end())
			{
				_surfaceOfData.erase(it);
			}
			// finally remove data pointer
			SurfaceData::_surfaceData.erase(dit);
		}
	}

	BYTE* SetApplicationData(BYTE* data)
	{
		tID id = (tID)(data);
		SurfaceData::_surfaceOfData[data] = id;
		return data;
	}
	void ReleaseApplicationData(BYTE* data)
	{
		tID id = (tID)(data);
		std::map<BYTE*,tID>::iterator it = SurfaceData::_surfaceOfData.find(data);
		if(it != SurfaceData::_surfaceOfData.end())
		{
			g_SurfaceRectangle.erase(it->second);
			SurfaceData::_surfaceOfData.erase(it);
		}
		return ReleaseSurfaceData(id);
	}

	tID GetSurfaceID(BYTE* data)
	{
		std::map<BYTE*,tID>::iterator it = SurfaceData::_surfaceOfData.find(data);
		if(it != SurfaceData::_surfaceOfData.end())
		{
			return it->second;
		}
		//SGP_THROW(L"Pointer to surface invalid");
		return 0;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Video Surface Manager functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN InitializeVideoSurfaceManager( )
{
	//Shouldn't be calling this if the video surface manager already exists.
	//Call shutdown first...
	Assert( !gpVSurfaceHead );
	Assert( !gpVSurfaceTail );
	RegisterDebugTopic(TOPIC_VIDEOSURFACE, "Video Surface Manager");
	gpVSurfaceHead = gpVSurfaceTail = NULL;

	giMemUsedInSurfaces = 0;

	// Create primary and backbuffer from globals
	if ( !SetPrimaryVideoSurfaces( ) )
	{
		DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_1, String( "Could not create primary surfaces" ) );
		return FALSE;
	}

	return TRUE ;
}

BOOLEAN ShutdownVideoSurfaceManager( )
{
	VSURFACE_NODE *curr;

	DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_0, "Shutting down the Video Surface manager");

	// Delete primary viedeo surfaces
	DeletePrimaryVideoSurfaces( );

	while( gpVSurfaceHead )
	{
		curr = gpVSurfaceHead;
		gpVSurfaceHead = gpVSurfaceHead->next;
		DeleteVideoSurface( curr->hVSurface );
#ifdef SGP_VIDEO_DEBUGGING
		if( curr->pName )
			MemFree( curr->pName );
		if( curr->pCode )
			MemFree( curr->pCode );
#endif
		MemFree( curr );
	}
	gpVSurfaceHead = NULL;
	gpVSurfaceTail = NULL;
	guiVSurfaceIndex = 0;
	guiVSurfaceSize = 0;
	guiVSurfaceTotalAdded = 0;
	UnRegisterDebugTopic(TOPIC_VIDEOSURFACE, "Video Objects");
	return TRUE;
}


BOOLEAN RestoreVideoSurfaces( )
{
	VSURFACE_NODE *curr;

	//
	// Loop through Video Surfaces and Restore
	//
	curr = gpVSurfaceTail;
	while( curr )
	{
		if( !RestoreVideoSurface( curr->hVSurface ) )
		{
			return FALSE;
		}
		curr = curr->prev;
	}
	return TRUE;
}


BOOLEAN AddStandardVideoSurface( VSURFACE_DESC *pVSurfaceDesc, UINT32 *puiIndex )
{

	HVSURFACE hVSurface;

	// Assertions
	Assert( puiIndex );
	Assert( pVSurfaceDesc );

	// Create video object
	hVSurface = CreateVideoSurface( pVSurfaceDesc );

	if( !hVSurface )
	{
		// Video Object will set error condition.
		return FALSE ;
	}

	// Set transparency to default
	SetVideoSurfaceTransparencyColor( hVSurface, FROMRGB( 0, 0, 0 ) );

	// Set into video object list
	if( gpVSurfaceHead )
	{ //Add node after tail
		gpVSurfaceTail->next = (VSURFACE_NODE*)MemAlloc( sizeof( VSURFACE_NODE ) );
		Assert( gpVSurfaceTail->next ); //out of memory?
		gpVSurfaceTail->next->prev = gpVSurfaceTail;
		gpVSurfaceTail->next->next = NULL;
		gpVSurfaceTail = gpVSurfaceTail->next;
	}
	else
	{ //new list
		gpVSurfaceHead = (VSURFACE_NODE*)MemAlloc( sizeof( VSURFACE_NODE ) );
		Assert( gpVSurfaceHead ); //out of memory?
		gpVSurfaceHead->prev = gpVSurfaceHead->next = NULL;
		gpVSurfaceTail = gpVSurfaceHead;
	}
#ifdef SGP_VIDEO_DEBUGGING
	gpVSurfaceTail->pName = NULL;
	gpVSurfaceTail->pCode = NULL;
#endif
	//Set the hVSurface into the node.
	gpVSurfaceTail->hVSurface = hVSurface;
	gpVSurfaceTail->uiIndex = guiVSurfaceIndex+=2;
	*puiIndex = gpVSurfaceTail->uiIndex;
	SurfaceData::RegisterSurface(*puiIndex, hVSurface);
	Assert( guiVSurfaceIndex < 0xfffffff0 ); //unlikely that we will ever use 2 billion VSurfaces!
	//We would have to create about 70 VSurfaces per second for 1 year straight to achieve this...
	guiVSurfaceSize++;
	guiVSurfaceTotalAdded++;

	return TRUE ;
}


BYTE *LockVideoSurface( UINT32 uiVSurface, UINT32 *puiPitch )
{
	VSURFACE_NODE *curr;

    if ( iUseWinFonts ) {
	    CurrentSurface = uiVSurface;
    }
	//
	// Check if given backbuffer or primary buffer
	//
	if ( uiVSurface == PRIMARY_SURFACE )
	{
		return SurfaceData::SetSurfaceData(uiVSurface,
			LockVideoSurfaceBuffer(ghPrimary, puiPitch));
	}

	if ( uiVSurface == BACKBUFFER )
	{
		return SurfaceData::SetSurfaceData(uiVSurface,
			LockVideoSurfaceBuffer(ghBackBuffer, puiPitch));
	}

	if ( uiVSurface == FRAME_BUFFER )
	{
		return SurfaceData::SetSurfaceData(uiVSurface,
			LockVideoSurfaceBuffer(ghFrameBuffer, puiPitch));
	}

	if ( uiVSurface == MOUSE_BUFFER )
	{
		return SurfaceData::SetSurfaceData(uiVSurface,
			LockVideoSurfaceBuffer(ghMouseBuffer, puiPitch));
	}

	//
	// Otherwise, use list
	//

	curr = gpVSurfaceHead;
	while( curr )
	{
		if( curr->uiIndex == uiVSurface )
		{
			break;
		}
		curr = curr->next;
	}
	if( !curr )
	{
		return FALSE;
	}

	//
	// Lock buffer
	//

	return SurfaceData::SetSurfaceData(uiVSurface, LockVideoSurfaceBuffer( curr->hVSurface, puiPitch ));

}

void UnLockVideoSurface( UINT32 uiVSurface )
{
	VSURFACE_NODE *curr;

	SurfaceData::ReleaseSurfaceData(uiVSurface);
	//
	// Check if given backbuffer or primary buffer
	//
	if ( uiVSurface == PRIMARY_SURFACE )
	{
		UnLockVideoSurfaceBuffer(ghPrimary);
		return;
	}

	if ( uiVSurface == BACKBUFFER )
	{
		UnLockVideoSurfaceBuffer(ghBackBuffer);
		return;
	}

	if ( uiVSurface == FRAME_BUFFER )
	{
		UnLockVideoSurfaceBuffer(ghFrameBuffer);
		return;
	}

	if ( uiVSurface == MOUSE_BUFFER )
	{
		UnLockVideoSurfaceBuffer(ghMouseBuffer);
		return;
	}

	curr = gpVSurfaceHead;
	while( curr )
	{
		if( curr->uiIndex == uiVSurface )
		{
			break;
		}
		curr = curr->next;
	}
	if( !curr )
	{
		return;
	}

	//
	// unlock buffer
	//

	UnLockVideoSurfaceBuffer( curr->hVSurface );
}

BOOLEAN SetVideoSurfaceTransparency( UINT32 uiIndex, COLORVAL TransColor )
{
	HVSURFACE hVSurface;

	//
	// Get Video Surface
	//

#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_SETVIDEOSURFACETRANSPARENCY;
#endif
	CHECKF( GetVideoSurface( &hVSurface, uiIndex ) );

	//
	// Set transparency
	//

	SetVideoSurfaceTransparencyColor( hVSurface, TransColor );

	return( TRUE );
}

BOOLEAN AddVideoSurfaceRegion( UINT32 uiIndex, VSURFACE_REGION *pNewRegion )
{
	HVSURFACE hVSurface;

	//
	// Get Video Surface
	//

#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_ADDVIDEOSURFACEREGION;
#endif
	CHECKF( GetVideoSurface( &hVSurface, uiIndex ) );

	//
	// Add Region
	//

	CHECKF( AddVSurfaceRegion( hVSurface, pNewRegion ) );

	return( TRUE );

}

BOOLEAN GetVideoSurfaceDescription( UINT32 uiIndex, UINT16 *usWidth, UINT16 *usHeight, UINT8 *ubBitDepth )
{
	HVSURFACE hVSurface;

	Assert( usWidth != NULL );
	Assert( usHeight != NULL );
	Assert( ubBitDepth != NULL );

	//
	// Get Video Surface
	//

#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_GETVIDEOSURFACEDESCRIPTION;
#endif
	CHECKF( GetVideoSurface( &hVSurface, uiIndex ) );

	*usWidth = hVSurface->usWidth;
	*usHeight = hVSurface->usHeight;
	*ubBitDepth = hVSurface->ubBitDepth;

	return TRUE;
}

BOOLEAN GetVideoSurface( HVSURFACE *hVSurface, UINT32 uiIndex )
{
	VSURFACE_NODE *curr;

#ifdef _DEBUG
	CheckValidVSurfaceIndex( uiIndex );
#endif

	if ( uiIndex == PRIMARY_SURFACE )
	{
		*hVSurface = ghPrimary;
		return TRUE;
	}

	if ( uiIndex == BACKBUFFER )
	{
		*hVSurface = ghBackBuffer;
		return TRUE;
	}

	if ( uiIndex == FRAME_BUFFER )
	{
		*hVSurface = ghFrameBuffer;
		return TRUE;
	}

	if ( uiIndex == MOUSE_BUFFER )
	{
		*hVSurface = ghMouseBuffer;
		return TRUE;
	}

	curr = gpVSurfaceHead;
	while( curr )
	{
		if( curr->uiIndex == uiIndex )
		{
			*hVSurface = curr->hVSurface;
			return TRUE;
		}
		curr = curr->next;
	}
	return FALSE;
}

BOOLEAN SetPrimaryVideoSurfaces( )
{
	VSURFACE_DESC surfaceDescription{};

	DeletePrimaryVideoSurfaces( );

	surfaceDescription.fCreateFlags = VSURFACE_SYSTEM_MEM_USAGE;
	surfaceDescription.usWidth = SCREEN_WIDTH;
	surfaceDescription.usHeight = SCREEN_HEIGHT;
	surfaceDescription.ubBitDepth = 16;
	ghPrimary = CreateVideoSurface(&surfaceDescription);
	CHECKF( ghPrimary != NULL );
	ghPrimary->fFlags |= VSURFACE_RESERVED_SURFACE;
	SurfaceData::RegisterSurface(PRIMARY_SURFACE, ghPrimary);

	ghBackBuffer = CreateVideoSurface(&surfaceDescription);
	CHECKF( ghBackBuffer != NULL );
	ghBackBuffer->fFlags |= VSURFACE_RESERVED_SURFACE;
	SurfaceData::RegisterSurface(BACKBUFFER, ghBackBuffer);

	// Frame and cursor pixels are portable canonical storage. Native mirrors are
	// no longer allocated during logical-surface creation.
	surfaceDescription.usWidth = MAX_CURSOR_WIDTH;
	surfaceDescription.usHeight = MAX_CURSOR_HEIGHT;
	ghMouseBuffer = CreateVideoSurface(&surfaceDescription);
	CHECKF( ghMouseBuffer != NULL );
	ghMouseBuffer->fFlags |= VSURFACE_RESERVED_SURFACE;
	ghMouseBuffer->pixelSurface->setColorKey(0);
	if (!iUseWinFonts)
	{
		CHECKF(SetVideoSurfaceTransparencyColor(ghMouseBuffer, 0));
	}
	SurfaceData::RegisterSurface(MOUSE_BUFFER, ghMouseBuffer);

	surfaceDescription.usWidth = SCREEN_WIDTH;
	surfaceDescription.usHeight = SCREEN_HEIGHT;
	ghFrameBuffer = CreateVideoSurface(&surfaceDescription);
	CHECKF( ghFrameBuffer != NULL );
	ghFrameBuffer->fFlags |= VSURFACE_RESERVED_SURFACE;
	SurfaceData::RegisterSurface(FRAME_BUFFER, ghFrameBuffer);

	return TRUE;
}

void DeletePrimaryVideoSurfaces( )
{
	//
	// If globals are not null, delete them
	//

	if ( ghPrimary != NULL )
	{
		SurfaceData::UnRegisterSurface(ghPrimary);
		DeleteVideoSurface( ghPrimary );
		ghPrimary = NULL;
	}

	if ( ghBackBuffer != NULL )
	{
		SurfaceData::UnRegisterSurface(ghBackBuffer);
		DeleteVideoSurface( ghBackBuffer );
		ghBackBuffer = NULL;
	}

	if ( ghFrameBuffer != NULL )
	{
		SurfaceData::UnRegisterSurface(ghFrameBuffer);
		DeleteVideoSurface( ghFrameBuffer );
		ghFrameBuffer = NULL;
	}

	if ( ghMouseBuffer != NULL )
	{
		SurfaceData::UnRegisterSurface(ghMouseBuffer);
		DeleteVideoSurface( ghMouseBuffer );
		ghMouseBuffer = NULL;
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Given an index to the dest and src vobject contained in our private VSurface list
// Based on flags, blit accordingly
// There are two types, a BltFast and a Blt. BltFast is 10% faster, uses no
// clipping lists
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN BltVideoSurface(UINT32 uiDestVSurface, UINT32 uiSrcVSurface, UINT16 usRegionIndex, INT32 iDestX, INT32 iDestY, UINT32 fBltFlags, blt_vs_fx *pBltFx )
{

	HVSURFACE	hDestVSurface;
	HVSURFACE	hSrcVSurface;

#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_BLTVIDEOSURFACE_DST;
#endif
	if( !GetVideoSurface( &hDestVSurface, uiDestVSurface ) )
	{
		return FALSE;
	}
#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_BLTVIDEOSURFACE_SRC;
#endif
	if( !GetVideoSurface( &hSrcVSurface, uiSrcVSurface ) )
	{
		return FALSE;
	}
	if ( !BltVideoSurfaceToVideoSurface( hDestVSurface, hSrcVSurface, usRegionIndex, iDestX, iDestY, fBltFlags, pBltFx ) )
	{ // VO Blitter will set debug messages for error conditions
		return FALSE ;
	}
	return TRUE ;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Fills an rectangular area with a specified color value.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN ColorFillVideoSurfaceArea(UINT32 uiDestVSurface, INT32 iDestX1, INT32 iDestY1, INT32 iDestX2, INT32 iDestY2, UINT16 Color16BPP)
{
	blt_vs_fx BltFx;
	HVSURFACE	hDestVSurface;
	SGPRect Clip;

#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_COLORFILLVIDEOSURFACEAREA;
#endif
	if( !GetVideoSurface( &hDestVSurface, uiDestVSurface ) )
	{
		return FALSE;
	}

	BltFx.ColorFill = Color16BPP;
	BltFx.DestRegion = 0;

	//
	// Clip fill region coords
	//

	GetClippingRect(&Clip);

	if(iDestX1 < Clip.iLeft)
		iDestX1 = Clip.iLeft;

	if(iDestX1 > Clip.iRight)
		return(FALSE);

	if(iDestX2 > Clip.iRight)
		iDestX2 = Clip.iRight;

	if(iDestX2 < Clip.iLeft)
		return(FALSE);

	if(iDestY1 < Clip.iTop)
		iDestY1 = Clip.iTop;

	if(iDestY1 > Clip.iBottom)
		return(FALSE);

	if(iDestY2 > Clip.iBottom)
		iDestY2 = Clip.iBottom;

	if(iDestY2 < Clip.iTop)
		return(FALSE);

	if((iDestX2 <= iDestX1) || (iDestY2 <= iDestY1))
		return(FALSE);

	BltFx.SrcRect.iLeft =	BltFx.FillRect.iLeft = iDestX1;
	BltFx.SrcRect.iTop = BltFx.FillRect.iTop = iDestY1;
	BltFx.SrcRect.iRight = BltFx.FillRect.iRight = iDestX2;
	BltFx.SrcRect.iBottom = BltFx.FillRect.iBottom = iDestY2;

	return( FillSurfaceRect( hDestVSurface, &BltFx ) );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Fills an rectangular area with a specified image value.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN ImageFillVideoSurfaceArea(UINT32 uiDestVSurface, INT32 iDestX1, INT32 iDestY1, INT32 iDestX2, INT32 iDestY2, HVOBJECT BkgrndImg, UINT16 Index, INT16 Ox, INT16 Oy)
{
	INT16 xc,yc,hblits,wblits,aw,pw,ah,ph,w,h,xo,yo;
	ETRLEObject		*pTrav;
	SGPRect NewClip,OldClip;

	pTrav = &(BkgrndImg->pETRLEObject[Index]);
	ph = (INT16)(pTrav->usHeight+pTrav->sOffsetY);
	pw = (INT16)(pTrav->usWidth+pTrav->sOffsetX);

	ah = (INT16)(iDestY2-iDestY1);
	aw = (INT16)(iDestX2-iDestX1);

	Ox %= pw;
	Oy %= ph;

	if(Ox>0)
		Ox -= pw;
	xo = (-Ox)%pw;

	if(Oy>0)
		Oy -= ph;
	yo = (-Oy)%ph;

	if(Ox<0)
		xo = (-Ox)%pw;
	else
	{
		xo = pw-(Ox%pw);
		Ox -= pw;
	}

	if(Oy<0)
		yo = (-Oy)%ph;
	else
	{
		yo = ph-(Oy%pw);
		Oy -= ph;
	}

	hblits = ((ah+yo)/ph) + (((ah+yo)%ph)?1:0);
	wblits = ((aw+xo)/pw) + (((aw+xo)%pw)?1:0);

	if((hblits==0) || (wblits==0))
		return(FALSE);

	//
	// Clip fill region coords
	//

	GetClippingRect(&OldClip);

	NewClip.iLeft=iDestX1;
	NewClip.iTop=iDestY1;
	NewClip.iRight=iDestX2;
	NewClip.iBottom=iDestY2;

	if(NewClip.iLeft < OldClip.iLeft)
		NewClip.iLeft = OldClip.iLeft;

	if(NewClip.iLeft > OldClip.iRight)
		return(FALSE);

	if(NewClip.iRight > OldClip.iRight)
		NewClip.iRight = OldClip.iRight;

	if(NewClip.iRight < OldClip.iLeft)
		return(FALSE);

	if(NewClip.iTop < OldClip.iTop)
		NewClip.iTop = OldClip.iTop;

	if(NewClip.iTop > OldClip.iBottom)
		return(FALSE);

	if(NewClip.iBottom > OldClip.iBottom)
		NewClip.iBottom = OldClip.iBottom;

	if(NewClip.iBottom < OldClip.iTop)
		return(FALSE);

	if((NewClip.iRight <= NewClip.iLeft) || (NewClip.iBottom <= NewClip.iTop))
		return(FALSE);

	SetClippingRect(&NewClip);

	yc = (INT16)iDestY1;
	for(h=0;h<hblits;h++)
	{
		xc = (INT16)iDestX1;
		for(w=0;w<wblits;w++)
		{
			BltVideoObject(uiDestVSurface, BkgrndImg, Index, xc+Ox, yc+Oy, VO_BLT_SRCTRANSPARENCY, NULL);
			xc += pw;
		}
		yc += ph;
	}

	SetClippingRect(&OldClip);
	return(TRUE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Video Surface Manipulation Functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

HVSURFACE CreateVideoSurface( VSURFACE_DESC *VSurfaceDesc )
{
	HVSURFACE						hVSurface;
	HIMAGE							hImage = NULL;
	SGPRect							tempRect;
	UINT16							usHeight;
	UINT16							usWidth;
	UINT8								ubBitDepth;

	//
	// Check creation options
	//

	//do
	//{
	//
	// Check if creating from file
	//

	if ( VSurfaceDesc->fCreateFlags & VSURFACE_CREATE_FROMFILE )
	{
		//
		// Create himage object from file
		//
		ImageFileType::TestOrder order = ImageFileType::JPC_FALLBACK;
		if(VSurfaceDesc->fCreateFlags & VSURFACE_CREATE_FROMJPC) {
			order = ImageFileType::JPC;
		} if(VSurfaceDesc->fCreateFlags & VSURFACE_CREATE_FROMJPC_FALLBACK) {
			order = ImageFileType::JPC_FALLBACK;
		} if(VSurfaceDesc->fCreateFlags & VSURFACE_CREATE_FROMPNG) {
			order = ImageFileType::PNG;
		} if(VSurfaceDesc->fCreateFlags & VSURFACE_CREATE_FROMPNG_FALLBACK) {
			order = ImageFileType::PNG_FALLBACK;
		}

		SGP_THROW_IFFALSE(hImage = CreateImage( VSurfaceDesc->ImageFile, IMAGE_ALLIMAGEDATA, order ),
			_BS(L"Could not create video surface from file : ") << vfs::String(VSurfaceDesc->ImageFile) << _BS::wget);
		
		if ( hImage == NULL )
		{
			DbgMessage( TOPIC_VIDEOSURFACE, DBG_LEVEL_2, "Invalid Image Filename given" );
			return( NULL );
		}

		//
		// Set values from himage
		//
		usHeight = hImage->usHeight;
		usWidth = hImage->usWidth;
		ubBitDepth = hImage->ubBitDepth;
		//			break;
	}
	else
	{

		//
		// If here, no special options given,
		// Set values from given description structure
		//

		usHeight = VSurfaceDesc->usHeight;
		usWidth = VSurfaceDesc->usWidth;
		ubBitDepth = VSurfaceDesc->ubBitDepth;

	}
	//while( FALSE );

	//
	// Assertions
	//

	Assert ( usHeight > 0 );
	Assert ( usWidth > 0 );

	switch( ubBitDepth )
	{
	case 8:
		break;

	case 16:
	// BF: handle 24 bpp and 32 bpp images as 16 bpp ones = convert larger color space to the smaller one
	case 24:
	case 32:
		break;

	default:

		//
		// If Here, an invalid format was given
		//

		DbgMessage( TOPIC_VIDEOSURFACE, DBG_LEVEL_2, "Invalid BPP value, can only be 8 or 16." );
		return( FALSE );
	}

	//
	// Allocate memory for Video Surface data and initialize
	//
	hVSurface = new SGPVSurface{};
	hVSurface->usHeight				= usHeight;
	hVSurface->usWidth				= usWidth;
	// BF : since we use a 16bpp framebuffer and images are converted to that format, 
	//      there is no need to set the surface bit depth to a higher value than 16
	hVSurface->ubBitDepth			= ubBitDepth > 16 ? 16 : ubBitDepth;
	hVSurface->p16BPPPalette		= NULL;
	hVSurface->TransparentColor		= FROMRGB( 0, 0, 0 );
	// Legacy memory-placement flags no longer select the canonical storage.
	// Engine-created surfaces live in project-owned heap memory.
	hVSurface->fFlags				= VSURFACE_SYSTEM_MEM_USAGE;

	// Every engine-created logical surface has portable canonical storage.
	hVSurface->pixelSurface =
		std::make_unique<ja2::presentation::PixelSurface>(
			hVSurface->usWidth, hVSurface->usHeight,
			PixelFormatForSurface(hVSurface), 4);


	//
	// Initialize surface with hImage , if given
	//

	if ( VSurfaceDesc->fCreateFlags & VSURFACE_CREATE_FROMFILE )
	{
		tempRect.iLeft = 0;
		tempRect.iTop = 0;
		tempRect.iRight = hImage->usWidth-1;
		tempRect.iBottom = hImage->usHeight-1;
		SetVideoSurfaceDataFromHImage( hVSurface, hImage, 0, 0, &tempRect );

		//
		// Set palette from himage
		//

		if ( hImage->ubBitDepth == 8 )
		{
			SetVideoSurfacePalette( hVSurface, hImage->pPalette );
		}

		//
		// Delete himage object
		//

		DestroyImage( hImage );
	}

	//
	// All is well
	//

	hVSurface->usHeight					= usHeight;
	hVSurface->usWidth					= usWidth;
	// BF: re-set bit depth (see above)
	hVSurface->ubBitDepth				= ubBitDepth > 16 ? 16 : ubBitDepth;

	giMemUsedInSurfaces += ( hVSurface->usHeight * hVSurface->usWidth * ( hVSurface->ubBitDepth / 8 ) );

	DbgMessage( TOPIC_VIDEOSURFACE, DBG_LEVEL_3, String("Success in Creating Video Surface" ) );

	return( hVSurface );
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Called when surface is lost, for the most part called by utility functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN RestoreVideoSurface( HVSURFACE hVSurface )
{
	Assert( hVSurface != NULL );
	// Logical surfaces retain their contents independently of presenter loss.
	return hVSurface->pixelSurface ? TRUE : FALSE;
}


// Lock must be followed by release
// Pitch MUST be used for all width calculations ( Pitch is in bytes )
// The time between Locking and unlocking must be minimal
BYTE *LockVideoSurfaceBuffer( HVSURFACE hVSurface, UINT32 *pPitch )
{
	Assert( hVSurface != NULL );
	Assert( pPitch != NULL );
	Assert( hVSurface->pixelSurface != NULL );
	const ja2::presentation::MutablePixelBuffer pixels =
		hVSurface->pixelSurface->lock();
	*pPitch = pixels.pitchBytes;
	return pixels.pixels;
}

void UnLockVideoSurfaceBuffer( HVSURFACE hVSurface )
{
	Assert( hVSurface != NULL );
	Assert( hVSurface->pixelSurface != NULL );
	hVSurface->pixelSurface->unlock();
}

ja2::presentation::PixelSurface *GetVideoSurfacePixelSurface(
	HVSURFACE hVSurface )
{
	Assert(hVSurface != NULL);
	return hVSurface->pixelSurface.get();
}

// Given an HIMAGE object, blit imagery into existing Video Surface. Can be from 8->16 BPP
BOOLEAN SetVideoSurfaceDataFromHImage( HVSURFACE hVSurface, HIMAGE hImage, UINT16 usX, UINT16 usY, SGPRect *pSrcRect )
{
	BYTE		*pDest;
	UINT32	fBufferBPP = 0;
	UINT32  uiPitch;
	UINT16  usPixelWidth;
	SGPRect	aRect;

	// Assertions
	Assert( hVSurface != NULL );
	Assert( hImage != NULL );

	// Get Size of hImage and determine if it can fit
	CHECKF( hImage->usWidth  >= hVSurface->usWidth );
	CHECKF( hImage->usHeight >= hVSurface->usHeight );

	// Check BPP and see if they are the same
	if ( hImage->ubBitDepth != hVSurface->ubBitDepth )
	{
		// They are not the same, but we can go from 8->16 without much cost
		if ( hImage->ubBitDepth == 8 && hVSurface->ubBitDepth == 16 )
		{
			fBufferBPP = BUFFER_16BPP;
		}
		else if ( (hImage->ubBitDepth == 24 || hImage->ubBitDepth == 32) && hVSurface->ubBitDepth == 16 )
		{
			// convert image to 16bpp
			fBufferBPP = BUFFER_16BPP;
		}
	}
	else
	{
		// Set buffer BPP
		switch ( hImage->ubBitDepth )
		{
		case 8:

			fBufferBPP = BUFFER_8BPP;
			break;

		case 16:

			fBufferBPP = BUFFER_16BPP;
			break;
		}

	}

	Assert( fBufferBPP != 0 );

	// Get surface buffer data
	pDest = LockVideoSurfaceBuffer( hVSurface, &uiPitch );

	// Effective width ( in PIXELS ) is Pitch ( in bytes ) converted to pitch ( IN PIXELS )
	usPixelWidth = (UINT16)( uiPitch / ( hVSurface->ubBitDepth / 8 ) );

	CHECKF( pDest != NULL );

	// Blit Surface
	// If rect is NULL, use entrie image size
	if ( pSrcRect == NULL )
	{
		aRect.iLeft = 0;
		aRect.iTop = 0;
		aRect.iRight = hImage->usWidth;
		aRect.iBottom = hImage->usHeight;
	}
	else
	{
		aRect.iLeft = pSrcRect->iLeft;
		aRect.iTop = pSrcRect->iTop;
		aRect.iRight = pSrcRect->iRight;
		aRect.iBottom = pSrcRect->iBottom;
	}

	// This HIMAGE function will transparently copy buffer
	if ( !CopyImageToBuffer( hImage, fBufferBPP, pDest, usPixelWidth, hVSurface->usHeight, usX, usY, &aRect ) )
	{
		DbgMessage( TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Error Occured Copying HIMAGE to HVSURFACE" ));
		UnLockVideoSurfaceBuffer( hVSurface );
		return( FALSE );
	}

	// All is OK
	UnLockVideoSurfaceBuffer( hVSurface );

	return( TRUE );

}

// Store the indexed palette and its RGB565 conversion with the logical surface.
BOOLEAN SetVideoSurfacePalette( HVSURFACE hVSurface, SGPPaletteEntry *pSrcPalette )
{

	Assert( hVSurface != NULL );
	if (hVSurface->pixelSurface && hVSurface->ubBitDepth == 8)
	{
		hVSurface->pixelSurface->setPalette(pSrcPalette, 256);
	}

	// Delete 16BPP Palette if one exists
	if ( hVSurface->p16BPPPalette != NULL )
	{
		MemFree( hVSurface->p16BPPPalette );
		hVSurface->p16BPPPalette = NULL;
	}

	// Create 16BPP Palette
	hVSurface->p16BPPPalette = Create16BPPPalette( pSrcPalette );

	DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_3, String("Video Surface Palette change successfull" ));
	return( TRUE );
}

// Convert the requested RGB value to the logical surface's pixel format.
BOOLEAN SetVideoSurfaceTransparencyColor( HVSURFACE hVSurface, COLORVAL TransColor )
{
	// Assertions
	Assert( hVSurface != NULL );

	//Set trans color into Video Surface
	hVSurface->TransparentColor = TransColor;
	if (hVSurface->pixelSurface)
	{
		hVSurface->pixelSurface->setColorKey(hVSurface->ubBitDepth == 8
			? static_cast<UINT16>(TransColor)
			: Get16BPPColor(TransColor));
	}

	return( TRUE );
}


BOOLEAN GetVSurfacePaletteEntries( HVSURFACE hVSurface, SGPPaletteEntry *pPalette )
{
	if (hVSurface->pixelSurface && hVSurface->pixelSurface->palette())
	{
		memcpy(pPalette, hVSurface->pixelSurface->palette(),
			256 * sizeof(SGPPaletteEntry));
		return TRUE;
	}
	return FALSE;
}


BOOLEAN DeleteVideoSurfaceFromIndex( UINT32 uiIndex )
{
	VSURFACE_NODE *curr;

#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_DELETEVIDEOSURFACEFROMINDEX;
	CheckValidVSurfaceIndex( uiIndex );
#endif

	curr = gpVSurfaceHead;
	while( curr )
	{
		if( curr->uiIndex == uiIndex )
		{ //Found the node, so detach it and delete it.

			//Deallocate the memory for the video surface
			DeleteVideoSurface( curr->hVSurface );

			if( curr == gpVSurfaceHead )
			{ //Advance the head, because we are going to remove the head node.
				gpVSurfaceHead = gpVSurfaceHead->next;
			}
			if( curr == gpVSurfaceTail )
			{ //Back up the tail, because we are going to remove the tail node.
				gpVSurfaceTail = gpVSurfaceTail->prev;
			}
			//Detach the node from the vsurface list
			if( curr->next )
			{ //Make the prev node point to the next
				curr->next->prev = curr->prev;
			}
			if( curr->prev )
			{ //Make the next node point to the prev
				curr->prev->next = curr->next;
			}
			//The node is now detached.  Now deallocate it.

#ifdef SGP_VIDEO_DEBUGGING
			if( curr->pName )
			{
				MemFree( curr->pName );
			}
			if( curr->pCode )
			{
				MemFree( curr->pCode );
			}
#endif

			MemFree( curr );
			guiVSurfaceSize--;
			return TRUE;
		}
		curr = curr->next;
	}
	return FALSE;
}


// Deletes all palettes, surfaces and region data
BOOLEAN DeleteVideoSurface( HVSURFACE hVSurface )
{
	// Assertions
	CHECKF( hVSurface != NULL );

	// Release region data
	hVSurface->RegionList.clear();

	//If there is a 16bpp palette, free it
	if( hVSurface->p16BPPPalette != NULL )
	{
		MemFree( hVSurface->p16BPPPalette );
		hVSurface->p16BPPPalette = NULL;
	}

	giMemUsedInSurfaces -= ( hVSurface->usHeight * hVSurface->usWidth * ( hVSurface->ubBitDepth / 8 ) );

	// Release object
	delete hVSurface;
	return( TRUE );
}

// ********************************************************
//
// Region manipulation functions
//
// ********************************************************

BOOLEAN GetNumRegions( HVSURFACE hVSurface , UINT32 *puiNumRegions )
{
	Assert( hVSurface );

	*puiNumRegions = hVSurface->RegionList.size();

	return( TRUE );

}

BOOLEAN AddVSurfaceRegion( HVSURFACE hVSurface, VSURFACE_REGION *pNewRegion )
{
	Assert( hVSurface != NULL );
	Assert( pNewRegion != NULL );

	// Add new region to list
	hVSurface->RegionList.push_back(*pNewRegion);

	return( TRUE );
}

// Add a group of regions
BOOLEAN AddVSurfaceRegions( HVSURFACE hVSurface, VSURFACE_REGION **ppNewRegions, UINT16 uiNumRegions )
{
	UINT16 cnt;

	Assert( hVSurface != NULL );
	Assert( ppNewRegions != NULL );

	for ( cnt = 0; cnt < uiNumRegions; cnt++ )
	{
		AddVSurfaceRegion( hVSurface, ppNewRegions[ cnt ] );
	}

	return( TRUE );
}

BOOLEAN ClearAllVSurfaceRegions( HVSURFACE hVSurface )
{
	Assert( hVSurface != NULL );
	hVSurface->RegionList.clear();
	return( TRUE );
}

BOOLEAN GetVSurfaceRegion( HVSURFACE hVSurface, UINT16 usIndex, VSURFACE_REGION *aRegion )
{
	Assert( hVSurface != NULL );

	if (usIndex >= hVSurface->RegionList.size())
	{
		return( FALSE );
	}

	*aRegion = hVSurface->RegionList[usIndex];
	return( TRUE );
}

BOOLEAN GetVSurfaceRect( HVSURFACE hVSurface, SGPRect *pRect)
{
	Assert( hVSurface != NULL );
	Assert( pRect != NULL );

	pRect->iLeft=0;
	pRect->iTop=0;
	pRect->iRight=hVSurface->usWidth;
	pRect->iBottom=hVSurface->usHeight;

	return( TRUE );
}

BOOLEAN ReplaceVSurfaceRegion( HVSURFACE hVSurface , UINT16 usIndex, VSURFACE_REGION *aRegion )
{
	VSURFACE_REGION OldRegion;

	Assert( hVSurface != NULL );

	if (usIndex >= hVSurface->RegionList.size())
	{
		return( FALSE );
	}

	hVSurface->RegionList[usIndex] = *aRegion;
	return( TRUE );
}

BOOLEAN AddVSurfaceRegionAtIndex( HVSURFACE hVSurface, UINT16 usIndex, VSURFACE_REGION *pNewRegion )
{
	Assert( hVSurface != NULL );
	Assert( pNewRegion != NULL );

	if (usIndex >= hVSurface->RegionList.size())
	{
		return(FALSE);
	}

	auto pos = hVSurface->RegionList.begin() + usIndex;
	hVSurface->RegionList.insert(pos, *pNewRegion);
	return(TRUE);
}

// *******************************************************************
//
// Blitting Functions
//
// *******************************************************************

// Blt  will use DD Blt or BltFast depending on flags.
// Will drop down into user-defined blitter if 8->16 BPP blitting is being done

BOOLEAN BltVideoSurfaceToVideoSurface( HVSURFACE hDestVSurface, HVSURFACE hSrcVSurface, UINT16 usIndex, INT32 iDestX, INT32 iDestY, INT32 fBltFlags, blt_vs_fx *pBltFx )
{
	VSURFACE_REGION aRegion;
	SGPRect				 SrcRect, DestRect;
	UINT8					*pSrcSurface8, *pDestSurface8;
	UINT16				*pDestSurface16, *pSrcSurface16;
	UINT32				uiSrcPitch, uiDestPitch, uiWidth, uiHeight;

	// Assertions
	Assert( hDestVSurface != NULL );

	// Check that both region and subrect are not given
	if ((fBltFlags&VS_BLT_SRCREGION) && (fBltFlags&VS_BLT_SRCSUBRECT))
	{
		DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Inconsistant blit flags given" ));
		return( FALSE );
	}

	// Check for dest src options
	if ( fBltFlags & VS_BLT_DESTREGION )
	{
		CHECKF( pBltFx != NULL );
		CHECKF( GetVSurfaceRegion( hDestVSurface, pBltFx->DestRegion, &aRegion ) );

		// Set starting coordinates from destination region
		iDestY = aRegion.RegionCoords.iTop;
		iDestX = aRegion.RegionCoords.iLeft;
	}

	// Check for fill, if true, fill entire region with color
	if ( fBltFlags & VS_BLT_COLORFILL )
	{
		return( FillSurface( hDestVSurface, pBltFx ) );
	}

	// Check for colorfill rectangle
	if ( fBltFlags & VS_BLT_COLORFILLRECT )
	{
		return( FillSurfaceRect( hDestVSurface, pBltFx ) );
	}

	// Check for source coordinate options - from region, specific rect or full src dimensions
	do
	{
		// Get Region from index, if specified
		if ( fBltFlags & VS_BLT_SRCREGION )
		{
			CHECKF( GetVSurfaceRegion( hSrcVSurface, usIndex, &aRegion ) )

			SrcRect = aRegion.RegionCoords;
			break;
		}

		// Use SUBRECT if specified
		if ( fBltFlags & VS_BLT_SRCSUBRECT )
		{
			SGPRect aSubRect;

			CHECKF( pBltFx != NULL );

			aSubRect = pBltFx->SrcRect;

			SrcRect = aSubRect;

			break;
		}

		// Here, use default, which is entire Video Surface
		// Check Sizes, SRC size MUST be <= DEST size
		if ( hDestVSurface->usHeight < hSrcVSurface->usHeight )
		{
			DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Incompatible height size given in Video Surface blit" ));
			return( FALSE );
		}
		if ( hDestVSurface->usWidth < hSrcVSurface->usWidth )
		{
			DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Incompatible height size given in Video Surface blit" ));
			return( FALSE );
		}

		SrcRect.iTop = 0;
		SrcRect.iLeft = 0;
		SrcRect.iBottom = hSrcVSurface->usHeight;
		SrcRect.iRight = hSrcVSurface->usWidth;

	} while( FALSE );

	// Once here, assert valid Src
	Assert( hSrcVSurface != NULL );

	// clipping -- added by DB
	GetVSurfaceRect( hDestVSurface, &DestRect);
	uiWidth=SrcRect.iRight-SrcRect.iLeft;
	uiHeight=SrcRect.iBottom-SrcRect.iTop;

	// check for position entirely off the screen
	if(iDestX >= DestRect.iRight)
		return(FALSE);
	if(iDestY >= DestRect.iBottom)
		return(FALSE);
	if((iDestX+(INT32)uiWidth) < DestRect.iLeft)
		return(FALSE);
	if((iDestY+(INT32)uiHeight) < DestRect.iTop)
		return(FALSE);

	// DB The mirroring stuff has to do it's own clipping because
	// it needs to invert some of the numbers
	if(!(fBltFlags & VS_BLT_MIRROR_Y))
	{
		if((iDestX+(INT32)uiWidth) >= DestRect.iRight)
		{
			SrcRect.iRight-=((iDestX+uiWidth)-DestRect.iRight);
			uiWidth-=((iDestX+uiWidth)-DestRect.iRight);
		}
		if((iDestY+(INT32)uiHeight) >= DestRect.iBottom)
		{
			SrcRect.iBottom-=((iDestY+uiHeight)-DestRect.iBottom);
			uiHeight-=((iDestY+uiHeight)-DestRect.iBottom);
		}
		if(iDestX < DestRect.iLeft)
		{
			SrcRect.iLeft+=(DestRect.iLeft-iDestX);
			uiWidth-=(DestRect.iLeft-iDestX);
			iDestX=DestRect.iLeft;
		}
		if(iDestY < DestRect.iTop)
		{
			SrcRect.iTop+=(DestRect.iTop-iDestY);
			uiHeight-=(DestRect.iTop-iDestY);
			iDestY=DestRect.iTop;
		}
	}

	// Send dest position, rectangle, etc to DD bltfast function
	// First check BPP values for compatibility
	if ( hDestVSurface->ubBitDepth == 16 && hSrcVSurface->ubBitDepth == 16 )
	{
		if(fBltFlags & VS_BLT_MIRROR_Y)
		{
			if((pSrcSurface16=(UINT16 *)LockVideoSurfaceBuffer(hSrcVSurface, &uiSrcPitch))==NULL)
			{
				DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Failed on lock of 16BPP surface for blitting" ));
				return(FALSE);
			}

			if((pDestSurface16=(UINT16 *)LockVideoSurfaceBuffer(hDestVSurface, &uiDestPitch))==NULL)
			{
				UnLockVideoSurfaceBuffer(hSrcVSurface);
				DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Failed on lock of 16BPP dest surface for blitting" ));
				return(FALSE);
			}

			Blt16BPPTo16BPPMirror(pDestSurface16, uiDestPitch, pSrcSurface16, uiSrcPitch, iDestX, iDestY, SrcRect.iLeft, SrcRect.iTop, uiWidth, uiHeight);
			UnLockVideoSurfaceBuffer(hSrcVSurface);
			UnLockVideoSurfaceBuffer(hDestVSurface);
			return(TRUE);
		}
		// For testing with non-DDraw blitting, uncomment to test -- DB

		CHECKF( BltVSurfaceUsingPixelSurface( hDestVSurface, hSrcVSurface,
			fBltFlags, iDestX, iDestY,
			&SrcRect ) );

	}
	else if ( hDestVSurface->ubBitDepth == 8 && hSrcVSurface->ubBitDepth == 8 )
	{
		if((pSrcSurface8=(UINT8 *)LockVideoSurfaceBuffer(hSrcVSurface, &uiSrcPitch))==NULL)
		{
			DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Failed on lock of 8BPP surface for blitting" ));
			return(FALSE);
		}

		if((pDestSurface8=(UINT8 *)LockVideoSurfaceBuffer(hDestVSurface, &uiDestPitch))==NULL)
		{
			UnLockVideoSurfaceBuffer(hSrcVSurface);
			DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Failed on lock of 8BPP dest surface for blitting" ));
			return(FALSE);
		}

		//Blt8BPPDataTo8BPPBuffer( UINT8 *pBuffer, UINT32 uiDestPitchBYTES, HVOBJECT hSrcVObject, INT32 iX, INT32 iY, UINT16 usIndex );
		Blt8BPPTo8BPP(pDestSurface8, uiDestPitch, pSrcSurface8, uiSrcPitch, iDestX, iDestY, SrcRect.iLeft, SrcRect.iTop, uiWidth, uiHeight);
		UnLockVideoSurfaceBuffer(hSrcVSurface);
		UnLockVideoSurfaceBuffer(hDestVSurface);
		return(TRUE);
	}
	else
	{
		DbgMessage(TOPIC_VIDEOSURFACE, DBG_LEVEL_2, String( "Incompatible BPP values with src and dest Video Surfaces for blitting" ));
		return( FALSE );
	}

	return( TRUE );
}

HVSURFACE GetPrimaryVideoSurface( )
{
	return( ghPrimary );
}

HVSURFACE GetBackBufferVideoSurface( )
{
	return( ghBackBuffer );
}

HVSURFACE GetFrameBufferVideoSurface( )
{
	return( ghFrameBuffer );
}

HVSURFACE GetMouseBufferVideoSurface( )
{
	return( ghMouseBuffer );
}


// UTILITY FUNCTIONS FOR BLITTING

BOOLEAN FillSurface( HVSURFACE hDestVSurface, blt_vs_fx *pBltFx )
{
	Assert( hDestVSurface != NULL );
	CHECKF( pBltFx != NULL );
	CHECKF(hDestVSurface->pixelSurface != NULL);
	hDestVSurface->pixelSurface->fill(
		static_cast<UINT16>(pBltFx->ColorFill));
	return( TRUE );
}

BOOLEAN FillSurfaceRect( HVSURFACE hDestVSurface, blt_vs_fx *pBltFx )
{
	Assert( hDestVSurface != NULL );
	CHECKF( pBltFx != NULL );
	CHECKF(hDestVSurface->pixelSurface != NULL);
	hDestVSurface->pixelSurface->fillRect(pBltFx->FillRect,
		static_cast<UINT16>(pBltFx->ColorFill));
	return( TRUE );
}


BOOLEAN BltVSurfaceUsingPixelSurface( HVSURFACE hDestVSurface,
	HVSURFACE hSrcVSurface, UINT32 fBltFlags, INT32 iDestX,
	INT32 iDestY, SGPRect *SrcRect )
{
	if (fBltFlags & VS_BLT_FAST)
	{
		CHECKF(iDestX >= 0);
		CHECKF(iDestY >= 0);
	}

	CHECKF(hDestVSurface->pixelSurface != NULL);
	CHECKF(hSrcVSurface->pixelSurface != NULL);
	CHECKF(hDestVSurface->pixelSurface->format() ==
		hSrcVSurface->pixelSurface->format());
	const ja2::presentation::BlitOptions options{
		(fBltFlags & VS_BLT_USECOLORKEY) != 0,
		(fBltFlags & VS_BLT_USEDESTCOLORKEY) != 0};
	return hDestVSurface->pixelSurface->blitFrom(
		*hSrcVSurface->pixelSurface, *SrcRect, iDestX, iDestY, options)
		? TRUE : FALSE;
}

BOOLEAN Blt16BPPBufferShadowRectAlternateTable(UINT16 *pBuffer, UINT32 uiDestPitchBYTES, SGPRect *area);



BOOLEAN InternalShadowVideoSurfaceRect(  UINT32	uiDestVSurface, INT32 X1, INT32 Y1, INT32 X2, INT32 Y2, BOOLEAN fLowPercentShadeTable )
{
	UINT16 *pBuffer;
	UINT32 uiPitch;
	SGPRect   area;
	HVSURFACE hVSurface;


	// CLIP IT!
	// FIRST GET SURFACE

	//
	// Get Video Surface
	//
#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_SHADOWVIDEOSURFACERECT;
#endif
	CHECKF( GetVideoSurface( &hVSurface, uiDestVSurface ) );

	if ( X1 < 0 )
		X1 = 0;

	if ( X2 < 0 )
		return( FALSE );

	if ( Y2 < 0 )
		return( FALSE );

	if ( Y1 < 0 )
		Y1 = 0;

	if ( X2 >= hVSurface->usWidth )
		X2 = hVSurface->usWidth-1;

	if ( Y2 >= hVSurface->usHeight )
		Y2 = hVSurface->usHeight-1;

	if ( X1 >= hVSurface->usWidth )
		return( FALSE );

	if ( Y1 >= hVSurface->usHeight )
		return( FALSE );

	if (  ( X2 - X1 ) <= 0 )
		return( FALSE );

	if (  ( Y2 - Y1 ) <= 0 )
		return( FALSE );


	area.iTop=Y1;
	area.iBottom=Y2;
	area.iLeft=X1;
	area.iRight=X2;


	// Lock video surface
	pBuffer = (UINT16*)LockVideoSurface( uiDestVSurface, &uiPitch );
	//UnLockVideoSurface( uiDestVSurface );

	if ( pBuffer == NULL )
	{
		return( FALSE );
	}

	if ( !fLowPercentShadeTable )
	{
		// Now we have the video object and surface, call the shadow function
		if(!Blt16BPPBufferShadowRect(pBuffer, uiPitch, &area))
		{
			// Blit has failed if false returned
			return( FALSE );
		}
	}
	else
	{
		// Now we have the video object and surface, call the shadow function
		if(!Blt16BPPBufferShadowRectAlternateTable(pBuffer, uiPitch, &area))
		{
			// Blit has failed if false returned
			return( FALSE );
		}
	}

	// Mark as dirty if it's the backbuffer
	//if ( uiDestVSurface == BACKBUFFER )
	//{
	//	InvalidateBackbuffer( );
	//}

	UnLockVideoSurface( uiDestVSurface );
	return( TRUE );
}


BOOLEAN ShadowVideoSurfaceRect(  UINT32	uiDestVSurface, INT32 X1, INT32 Y1, INT32 X2, INT32 Y2)
{
	return( InternalShadowVideoSurfaceRect( uiDestVSurface, X1, Y1, X2, Y2, FALSE ) );
}


BOOLEAN ShadowVideoSurfaceRectUsingLowPercentTable(  UINT32	uiDestVSurface, INT32 X1, INT32 Y1, INT32 X2, INT32 Y2)
{
	return( InternalShadowVideoSurfaceRect( uiDestVSurface, X1, Y1, X2, Y2, TRUE ) );
}


BOOLEAN StretchVSurfaceUsingPixelSurface( HVSURFACE hDestVSurface,
	HVSURFACE hSrcVSurface, UINT32 fBltFlags, SGPRect *SrcRect,
	SGPRect *DestRect )
{
	CHECKF(hDestVSurface->pixelSurface != NULL);
	CHECKF(hSrcVSurface->pixelSurface != NULL);
	CHECKF(hDestVSurface->pixelSurface->format() ==
		hSrcVSurface->pixelSurface->format());
	const ja2::presentation::BlitOptions options{
		(fBltFlags & VS_BLT_USECOLORKEY) != 0, false};
	return hDestVSurface->pixelSurface->stretchFrom(
		*hSrcVSurface->pixelSurface, *SrcRect, *DestRect, options)
		? TRUE : FALSE;
}


//
// This function will stretch the source image to the size of the dest rect.
//
// If the 2 images are not 16 Bpp, it returns false.
//
BOOLEAN BltStretchVideoSurface(UINT32 uiDestVSurface, UINT32 uiSrcVSurface, INT32 iDestX, INT32 iDestY, UINT32 fBltFlags, SGPRect *SrcRect, SGPRect *DestRect )
{
	HVSURFACE	hDestVSurface;
	HVSURFACE	hSrcVSurface;

#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_BLTSTRETCHVIDEOSURFACE_DST;
#endif
	if( !GetVideoSurface( &hDestVSurface, uiDestVSurface ) )
	{
		return FALSE;
	}
#ifdef _DEBUG
	gubVSDebugCode = DEBUGSTR_BLTSTRETCHVIDEOSURFACE_SRC;
#endif
	if( !GetVideoSurface( &hSrcVSurface, uiSrcVSurface ) )
	{
		return FALSE;
	}

	//if the 2 images are not both 16bpp, return FALSE
	if( (hDestVSurface->ubBitDepth != 16) && (hSrcVSurface->ubBitDepth != 16) )
		return(FALSE);

	if(!StretchVSurfaceUsingPixelSurface( hDestVSurface, hSrcVSurface,
		fBltFlags, SrcRect, DestRect ) )
	{
		//
		// VO Blitter will set debug messages for error conditions
		//

		return( FALSE );
	}

	return( TRUE );
}


BOOLEAN ShadowVideoSurfaceImage( UINT32	uiDestVSurface, HVOBJECT hImageHandle, INT32 iPosX, INT32 iPosY)
{
	//Horizontal shadow
	ShadowVideoSurfaceRect( uiDestVSurface, iPosX+3, iPosY+hImageHandle->pETRLEObject->usHeight, iPosX+hImageHandle->pETRLEObject->usWidth, iPosY+	hImageHandle->pETRLEObject->usHeight+3);

	//vertical shadow
	ShadowVideoSurfaceRect( uiDestVSurface, iPosX+hImageHandle->pETRLEObject->usWidth, iPosY+3, iPosX+hImageHandle->pETRLEObject->usWidth+3, iPosY+	hImageHandle->pETRLEObject->usHeight);
	return( TRUE );
}

BOOLEAN MakeVSurfaceFromVObject(UINT32 uiVObject, UINT16 usSubIndex, UINT32 *puiVSurface)
{
	HVOBJECT hSrcVObject;
	UINT32 uiVSurface;
	VSURFACE_DESC hDesc;

	if(GetVideoObject(&hSrcVObject, uiVObject))
	{
		// ATE: Memset
		memset( &hDesc, 0, sizeof( VSURFACE_DESC ) );
		hDesc.fCreateFlags=VSURFACE_CREATE_DEFAULT;
		hDesc.usWidth=hSrcVObject->pETRLEObject[ usSubIndex ].usWidth;
		hDesc.usHeight=hSrcVObject->pETRLEObject[ usSubIndex ].usHeight;
		hDesc.ubBitDepth=PIXEL_DEPTH;

		if(AddVideoSurface(&hDesc, &uiVSurface))
		{
			if(BltVideoObjectFromIndex(uiVSurface, uiVObject, usSubIndex, 0, 0, VO_BLT_SRCTRANSPARENCY, NULL))
			{
				*puiVSurface=uiVSurface;
				return(TRUE);
			}
			else
				DeleteVideoSurfaceFromIndex(uiVSurface);
		}
	}		

	return(FALSE);
}

#ifdef _DEBUG
void CheckValidVSurfaceIndex( UINT32 uiIndex )
{
	BOOLEAN fAssertError = FALSE;
	if( uiIndex == 0xffffffff )
	{ //-1 index -- deleted
		fAssertError = TRUE;
	}
	else if( uiIndex % 2 && uiIndex < 0xfffffff0  )
	{ //odd numbers are reserved for vobjects
		fAssertError = TRUE;
	}

	if( fAssertError )
	{
		CHAR8 str[60];
		switch( gubVSDebugCode )
		{
		case DEBUGSTR_SETVIDEOSURFACETRANSPARENCY:
			sprintf( str, "SetVideoSurfaceTransparency" );
			break;
		case DEBUGSTR_ADDVIDEOSURFACEREGION:
			sprintf( str, "AddVideoSurfaceRegion" );
			break;
		case DEBUGSTR_GETVIDEOSURFACEDESCRIPTION:
			sprintf( str, "GetVideoSurfaceDescription" );
			break;
		case DEBUGSTR_BLTVIDEOSURFACE_DST:
			sprintf( str, "BltVideoSurface (dest)" );
			break;
		case DEBUGSTR_BLTVIDEOSURFACE_SRC:
			sprintf( str, "BltVideoSurface (src)" );
			break;
		case DEBUGSTR_COLORFILLVIDEOSURFACEAREA:
			sprintf( str, "ColorFillVideoSurfaceArea" );
			break;
		case DEBUGSTR_SHADOWVIDEOSURFACERECT:
			sprintf( str, "ShadowVideoSurfaceRect" );
			break;
		case DEBUGSTR_BLTSTRETCHVIDEOSURFACE_DST:
			sprintf( str, "BltStretchVideoSurface (dest)" );
			break;
		case DEBUGSTR_BLTSTRETCHVIDEOSURFACE_SRC:
			sprintf( str, "BltStretchVideoSurface (src)" );
			break;
		case DEBUGSTR_DELETEVIDEOSURFACEFROMINDEX:
			sprintf( str, "DeleteVideoSurfaceFromIndex" );
			break;
		case DEBUGSTR_NONE:
		default:
			sprintf( str, "GetVideoSurface" );
			break;
		}
		if( uiIndex == 0xffffffff )
		{
			AssertMsg( 0, String( "Trying to %s with deleted index -1." , str ) );
		}
		else
		{
			AssertMsg( 0, String( "Trying to %s using a VOBJECT ID %d!", str, uiIndex ) );
		}
	}
}
#endif

#ifdef SGP_VIDEO_DEBUGGING
typedef struct DUMPFILENAME
{
	CHAR8 str[256];
}DUMPFILENAME;
void DumpVSurfaceInfoIntoFile( const STR8 filename, BOOLEAN fAppend )
{
	VSURFACE_NODE *curr;
	FILE *fp;
	DUMPFILENAME *pName, *pCode;
	UINT32 *puiCounter;
	CHAR8 tempName[ 256 ];
	CHAR8 tempCode[ 256 ];
	UINT32 i, uiUniqueID;
	BOOLEAN fFound;
	if( !guiVSurfaceSize )
	{
		return;
	}

	if( fAppend )
	{
		fp = fopen( filename, "a" );
	}
	else
	{
		fp = fopen( filename, "w" );
	}
	Assert( fp );

	//Allocate enough strings and counters for each node.
	pName = (DUMPFILENAME*)MemAlloc( sizeof( DUMPFILENAME ) * guiVSurfaceSize );
	pCode = (DUMPFILENAME*)MemAlloc( sizeof( DUMPFILENAME ) * guiVSurfaceSize );
	memset( pName, 0, sizeof( DUMPFILENAME ) * guiVSurfaceSize );
	memset( pCode, 0, sizeof( DUMPFILENAME ) * guiVSurfaceSize );
	puiCounter = (UINT32*)MemAlloc( 4 * guiVSurfaceSize );
	memset( puiCounter, 0, 4 * guiVSurfaceSize );

	//Loop through the list and record every unique filename and count them
	uiUniqueID = 0;
	curr = gpVSurfaceHead;
	while( curr )
	{
		strcpy( tempName, curr->pName );
		strcpy( tempCode, curr->pCode );
		fFound = FALSE;
		for( i = 0; i < uiUniqueID; i++ )
		{
			if( !_stricmp( tempName, pName[i].str ) && !_stricmp( tempCode, pCode[i].str ) )
			{ //same string
				fFound = TRUE;
				(puiCounter[ i ])++;
				break;
			}
		}
		if( !fFound )
		{
			strcpy( pName[i].str, tempName );
			strcpy( pCode[i].str, tempCode );
			(puiCounter[ i ])++;
			uiUniqueID++;
		}
		curr = curr->next;
	}

	//Now dump the info.
	fprintf( fp, "-----------------------------------------------\n" );
	fprintf( fp, "%d unique vSurface names exist in %d VSurfaces\n", uiUniqueID, guiVSurfaceSize );
	fprintf( fp, "-----------------------------------------------\n\n" );
	for( i = 0; i < uiUniqueID; i++ )
	{
		fprintf( fp, "%d occurrences of %s\n", puiCounter[i], pName[i].str );
		fprintf( fp, "%s\n\n", pCode[i].str );
	}
	fprintf( fp, "\n-----------------------------------------------\n\n" );

	//Free all memory associated with this operation.
	MemFree( pName );
	MemFree( pCode );
	MemFree( puiCounter );
	fclose( fp );
}

//Debug wrapper for adding vsurfaces
BOOLEAN _AddAndRecordVSurface( VSURFACE_DESC *VSurfaceDesc, UINT32 *uiIndex, UINT32 uiLineNum, const STR8 pSourceFile )
{
	UINT16 usLength;
	CHAR8 str[256];
	if( !AddStandardVideoSurface( VSurfaceDesc, uiIndex ) )
	{
		return FALSE;
	}

	//record the filename of the vsurface (some are created via memory though)
	usLength = strlen( VSurfaceDesc->ImageFile ) + 1;
	gpVSurfaceTail->pName = (STR8)MemAlloc( usLength );
	memset( gpVSurfaceTail->pName, 0, usLength );
	strcpy( gpVSurfaceTail->pName, VSurfaceDesc->ImageFile );

	//record the code location of the calling creating function.
	sprintf( str, "%s -- line(%d)", pSourceFile, uiLineNum );
	usLength = strlen( str ) + 1;
	gpVSurfaceTail->pCode = (STR8)MemAlloc( usLength );
	memset( gpVSurfaceTail->pCode, 0, usLength );
	strcpy( gpVSurfaceTail->pCode, str );

	return TRUE;
}

#endif

