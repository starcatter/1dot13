#include "types.h"
#include "video.h"
#include "video_windows.h"
#include "vsurface_private.h"
#include "vobject_blitters.h"
#include "LegacySGP.h"
#include <stdio.h>
#include <io.h>
#include "renderworld.h"
#include "Render Dirty.h"
#include "Fade Screen.h"
#include "impTGA.h"
#include "Timer Control.h"
#include "FileMan.h"
#include "input.h"
#include "GameSettings.h"
#include "sgp_logger.h"
#include "fileio/FileIO.h"
#include "fileio/FileServices.h"
#include "fileio/StoreRouter.h"
#include "platform/Clock.h"
#include "platform/Dialog.h"
#include "presentation/DirtyRegionTracker.h"
#include "presentation/PixelSurface.h"
#include "presentation/Presenter.h"
#include "presentation/windows/DirectDrawPresenter.h"
#include "UtfConversion.h"

#include <memory>
#include <vector>

#include "resource.h"
#include <vfs/Core/vfs_string.h>

#include "local.h"
#include "Text.h"


extern int iScreenMode;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Defines
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#define VIDEO_OFF			 0x00
#define VIDEO_ON				0x01
#define VIDEO_SHUTTING_DOWN	0x02
#define VIDEO_SUSPENDED		0x04

#define THREAD_OFF			0x00
#define THREAD_ON			 0x01
#define THREAD_SUSPENDED		0x02

#define CURRENT_MOUSE_DATA		0
#define PREVIOUS_MOUSE_DATA		1


///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Typedefs
//
///////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct
{
	BOOLEAN				 fRestore;
	INT16					usMouseXPos, usMouseYPos;
	INT16					usLeft, usTop, usRight, usBottom;
	RECT										Region;

} MouseCursorBackground;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// LOCAL globals
//
///////////////////////////////////////////////////////////////////////////////////////////////////

//
// Video state variables
//

static UINT16				 gusScreenWidth;
static UINT16				 gusScreenHeight;
static UINT8					gubScreenPixelDepth;

static RECT	gScrollRegion;

#define			MAX_NUM_FRAMES			25

BOOLEAN												gfVideoCapture=FALSE;
UINT32												guiFramePeriod = (1000 / 15 );
UINT32												guiLastFrame;
UINT16													*gpFrameData[ MAX_NUM_FRAMES ];
INT32													giNumFrames = 0;

//
// Direct Draw objects for both the Primary and Backbuffer surfaces
//



//
extern RECT									rcWindow;
extern POINT									ptWindowSize;

UINT32 CurrentSurface = BACKBUFFER;

//
// Globals for mouse cursor
//

static UINT16				 gusMouseCursorWidth;
static UINT16				 gusMouseCursorHeight;
static INT16					gsMouseCursorXOffset;
static INT16					gsMouseCursorYOffset;

static MouseCursorBackground	gMouseCursorBackground[2];
static std::unique_ptr<ja2::presentation::PixelSurface>
	gMouseCursorBackgroundSurface;
static std::unique_ptr<ja2::presentation::DirectDrawPresenter> gPresenter;
static std::uint64_t gNextPresentationMicroseconds = 0;

static HVOBJECT				gpCursorStore;

BOOLEAN			gfFatalError = FALSE;
char				gFatalErrorString[ 512 ];

// 8-bit palette stuff

SGPPaletteEntry								gSgpPalette[256];

//
// Make sure we record the value of the hWindow (main window frame for the application)
//

HWND							ghWindow;

//
// Refresh thread based variables
//

UINT32						guiFrameBufferState;	// BUFFER_READY, BUFFER_DIRTY
UINT32						guiMouseBufferState;	// BUFFER_READY, BUFFER_DIRTY, BUFFER_DISABLED
UINT32									 guiVideoManagerState;	// VIDEO_ON, VIDEO_OFF, VIDEO_SUSPENDED, VIDEO_SHUTTING_DOWN
UINT32						guiRefreshThreadState;	// THREAD_ON, THREAD_OFF, THREAD_SUSPENDED

//
// Dirty rectangle management variables
//

void							(*gpFrameBufferRefreshOverride)(void);
// The real dimensions are installed by InitializeVideoManager after runtime
// resolution selection. Avoid reading the zero-initialized screen globals
// during static initialization.
static ja2::presentation::DirtyRegionTracker gDirtyRegionTracker(1, 1);

//
// Screen output stuff
//

BOOLEAN						gfPrintFrameBuffer;
UINT32						guiPrintFrameBufferIndex;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// External Variables
//
///////////////////////////////////////////////////////////////////////////////////////////////////

extern UINT16 gusRedMask;
extern UINT16 gusGreenMask;
extern UINT16 gusBlueMask;
extern INT16	gusRedShift;
extern INT16	gusBlueShift;
extern INT16	gusGreenShift;

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Local Function Prototypes
//
///////////////////////////////////////////////////////////////////////////////////////////////////

void SnapshotSmall( void );
void VideoMovieCapture( BOOLEAN fEnable );
void RefreshMovieCache( );
static BOOLEAN BlitSurfaceRegion(UINT32 destination, UINT32 source,
	INT32 destinationX, INT32 destinationY, const RECT& sourceRegion);
static BOOLEAN RestoreMouseBackground(const MouseCursorBackground& background);
static BOOLEAN SaveMouseBackground(const MouseCursorBackground& background);
static BOOLEAN DrawMouseCursor(const MouseCursorBackground& background);
static BOOLEAN IsPresentationDue(void);
static BOOLEAN PresentBackBuffer(void);



///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN InitializeVideoManager(HINSTANCE hInstance, UINT16 usCommandShow, void *WindowProc)
{
	HWND			hWindow;
	WNDCLASS		WindowClass;
	UINT8		 ClassName[] = APPLICATION_NAME;

	//
	// Register debug topics
	//

	RegisterDebugTopic(TOPIC_VIDEO, "Video");
	DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "Initializing the video manager");

	/////////////////////////////////////////////////////////////////////////////////////////////////
	//
	// Register and Realize our display window. The DirectX surface will eventually overlay on top
	// of this surface.
	//
	// <<<<<<<<< Don't change this >>>>>>>>
	//
	/////////////////////////////////////////////////////////////////////////////////////////////////

	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = (WNDPROC) WindowProc;
	WindowClass.cbClsExtra = 0;
	WindowClass.cbWndExtra = 0;
	WindowClass.hInstance = hInstance;
	WindowClass.hIcon = LoadIcon(hInstance,	MAKEINTRESOURCE( IDI_ICON1 ) );
	WindowClass.hCursor = NULL;
	WindowClass.hbrBackground = NULL;
	WindowClass.lpszMenuName = NULL;
	WindowClass.lpszClassName = (LPCSTR) ClassName;
	RegisterClass(&WindowClass);

	//
	// Get a window handle for our application (gotta have on of those)
	// Don't change this
	//
	if( 1==iScreenMode )	// windowed mode
	{
		RECT window;
		DWORD style;
		DWORD exstyle;

		window.top = 0;
		window.left = 0;
		window.right = SCREEN_WIDTH;
		window.bottom = SCREEN_HEIGHT;

		exstyle = WS_EX_APPWINDOW;
		style = WS_OVERLAPPEDWINDOW & (~(WS_MAXIMIZEBOX | WS_SYSMENU));

		AdjustWindowRectEx( &window, style, FALSE, exstyle);
		OffsetRect( &window, -window.left, -window.top);

		ptWindowSize.x = window.right;
		ptWindowSize.y = window.bottom;

		hWindow = CreateWindowEx(exstyle, (LPCSTR) ClassName, "Jagged Alliance 2", style, window.left, window.top, window.right, window.bottom, NULL, NULL, hInstance, NULL);
		GetClientRect( hWindow, &window);
		window.top = window.top;
	}
	else	// fullscreen mode
	{
		hWindow = CreateWindowEx(WS_EX_TOPMOST, (LPCSTR) ClassName, "Jagged Alliance 2", WS_POPUP | WS_VISIBLE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, NULL, NULL, hInstance, NULL);
	}
	if (hWindow == NULL)
	{
		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "Failed to create window frame for Direct Draw");
		return FALSE;
	}

	//
	// Okay, now hide the cursor for the window.
	//
	SetCursor( NULL);

	//
	// Excellent. Now we record the hWindow variable for posterity (not)
	//

	memset( gpFrameData, 0, sizeof( gpFrameData ) );


	ghWindow = hWindow;

	//
	// Display our full screen window
	//

	//	ShowCursor(FALSE);
	ShowWindow(hWindow, usCommandShow);
	UpdateWindow(hWindow);
	SetFocus(hWindow);

	ja2::presentation::DirectDrawPresenterCreateResult presenterResult;
	gPresenter = ja2::presentation::DirectDrawPresenter::create(
		ghWindow, &rcWindow, iScreenMode == 1, SCREEN_WIDTH, SCREEN_HEIGHT,
		PIXEL_DEPTH, presenterResult);
	if (!gPresenter)
	{
		if (presenterResult ==
			ja2::presentation::DirectDrawPresenterCreateResult::displayModeFailure)
		{
			CHAR16 message[256];
			swprintf(message, Additional113Text[ADDTEXT_DIFFRES_REQUIRED],
				SCREEN_WIDTH, SCREEN_HEIGHT);
			Platform::ShowDialog(APPLICATION_NAME,
				ja2::text::utf16ToUtf8ReplacingInvalid(message),
				Platform::DialogKind::warning);
			PostQuitMessage(1);
		}
		return FALSE;
	}

	gusScreenWidth = SCREEN_WIDTH;
	gusScreenHeight = SCREEN_HEIGHT;
	gubScreenPixelDepth = PIXEL_DEPTH;


	//
	// The two cursor metadata slots intentionally share one saved-background
	// surface. Only their coordinates differ between refreshes.
	//
	gMouseCursorBackground[CURRENT_MOUSE_DATA].fRestore = FALSE;
	gMouseCursorBackground[PREVIOUS_MOUSE_DATA].fRestore = FALSE;
	gMouseCursorBackgroundSurface =
		std::make_unique<ja2::presentation::PixelSurface>(
			MAX_CURSOR_WIDTH, MAX_CURSOR_HEIGHT,
			ja2::presentation::PixelFormat::rgb565, 4);

	//
	// Initialize state variables
	//

	guiFrameBufferState			= BUFFER_DIRTY;
	guiMouseBufferState			= BUFFER_DISABLED;
	guiVideoManagerState		 = VIDEO_ON;
	guiRefreshThreadState		= THREAD_OFF;
	// Runtime settings may have changed the resolution since static startup.
	gDirtyRegionTracker = ja2::presentation::DirtyRegionTracker(
		SCREEN_WIDTH, SCREEN_HEIGHT);
	gDirtyRegionTracker.invalidateScreen();
	gNextPresentationMicroseconds = 0;
	gpFrameBufferRefreshOverride = NULL;
	gpCursorStore				= NULL;
	gfPrintFrameBuffer			= FALSE;
	guiPrintFrameBufferIndex	 = 0;

	//
	// This function must be called to setup RGB information
	//

	if (GetRGBDistribution() == FALSE)
		return FALSE;

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
void ShutdownVideoManager(void)
{
	//UINT32	uiRefreshThreadState;

	DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "Shutting down the video manager");

	//
	// Toggle the state of the video manager to indicate to the refresh thread that it needs to shut itself
	// down
	//

	gMouseCursorBackgroundSurface.reset();
	gPresenter.reset();

	// destroy the window
	// DestroyWindow( ghWindow );

	guiVideoManagerState = VIDEO_OFF;

	if (gpCursorStore != NULL)
	{
		DeleteVideoObject(gpCursorStore);
		gpCursorStore = NULL;
	}

	// ATE: Release mouse cursor!
	FreeMouseCursor( FALSE );

	UnRegisterDebugTopic(TOPIC_VIDEO, "Video");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void SuspendVideoManager(void)
{
	if (gPresenter)
	{
		gPresenter->suspend();
	}
	guiVideoManagerState = VIDEO_SUSPENDED;

}

void DoTester( )
{
	if (gPresenter)
	{
		gPresenter->leaveDisplayMode();
	}
	//	ShowCursor(TRUE);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN RestoreVideoManager(void)
{
	//
	// Make sure the video manager is indeed suspended before moving on
	//

	if (guiVideoManagerState == VIDEO_SUSPENDED)
	{
		//
		// Restore the Primary and Backbuffer
		//

		if (!gPresenter || !gPresenter->resume())
		{
			return FALSE;
		}

		//
		// Set the video state to VIDEO_ON
		//

		guiFrameBufferState = BUFFER_DIRTY;
		guiMouseBufferState = BUFFER_DIRTY;
		gDirtyRegionTracker.invalidateScreen();
		guiVideoManagerState = VIDEO_ON;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void GetCurrentVideoSettings( UINT16 *usWidth, UINT16 *usHeight, UINT8 *ubBitDepth )
{
	*usWidth = (UINT16) gusScreenWidth;
	*usHeight = (UINT16) gusScreenHeight;
	*ubBitDepth = (UINT8) gubScreenPixelDepth;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN CanBlitToFrameBuffer(void)
{
	BOOLEAN fCanBlit;

	//
	// W A R N I N G ---- W A R N I N G ---- W A R N I N G ---- W A R N I N G ---- W A R N I N G ----
	//
	// This function is intended to be called by a thread which has already locked the
	// FRAME_BUFFER_MUTEX mutual exclusion section. Anything else will cause the application to
	// yack
	//

	fCanBlit = (guiFrameBufferState == BUFFER_READY);

	return fCanBlit;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN CanBlitToMouseBuffer(void)
{
	BOOLEAN fCanBlit;

	//
	// W A R N I N G ---- W A R N I N G ---- W A R N I N G ---- W A R N I N G ---- W A R N I N G ----
	//
	// This function is intended to be called by a thread which has already locked the
	// MOUSE_BUFFER_MUTEX mutual exclusion section. Anything else will cause the application to
	// yack
	//

	fCanBlit = (guiMouseBufferState == BUFFER_READY);

	return fCanBlit;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void InvalidateRegion(INT32 iLeft, INT32 iTop, INT32 iRight, INT32 iBottom)
{
	gDirtyRegionTracker.invalidate(iLeft, iTop, iRight, iBottom);
}


void InvalidateRegionEx(INT32 iLeft, INT32 iTop, INT32 iRight, INT32 iBottom, UINT32 uiFlags )
{
	gDirtyRegionTracker.invalidateExtended(iLeft, iTop, iRight, iBottom,
		uiFlags, gsVIEWPORT_WINDOW_END_Y);
}


///////////////////////////////////////////////////////////////////////////////////////////////////

void InvalidateRegions(SGPRect *pArrayOfRegions, UINT32 uiRegionCount)
{
	gDirtyRegionTracker.invalidateMany(pArrayOfRegions, uiRegionCount);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void InvalidateScreen(void)
{
	//
	// W A R N I N G ---- W A R N I N G ---- W A R N I N G ---- W A R N I N G ---- W A R N I N G ----
	//
	// This function is intended to be called by a thread which has already locked the
	// FRAME_BUFFER_MUTEX mutual exclusion section. Anything else will cause the application to
	// yack
	//

	gDirtyRegionTracker.invalidateScreen();
	guiFrameBufferState = BUFFER_DIRTY;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void InvalidateFrameBuffer(void)
{
	//
	// W A R N I N G ---- W A R N I N G ---- W A R N I N G ---- W A R N I N G ---- W A R N I N G ----
	//
	// This function is intended to be called by a thread which has already locked the
	// FRAME_BUFFER_MUTEX mutual exclusion section. Anything else will cause the application to
	// yack
	//

	guiFrameBufferState = BUFFER_DIRTY;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void SetFrameBufferRefreshOverride(PTR pFrameBufferRefreshOverride)
{

	gpFrameBufferRefreshOverride = (void (__cdecl *)(void))pFrameBufferRefreshOverride;
}

//#define SCROLL_TEST

///////////////////////////////////////////////////////////////////////////////////////////////////
void ScrollJA2Background(UINT32 uiDirection, INT16 sScrollXIncrement,
	INT16 sScrollYIncrement, BOOLEAN fRenderStrip,
	UINT32 uiCurrentMouseBackbuffer )
{
	UINT16 usWidth, usHeight;
	UINT8	ubBitDepth;
	static RECT	Region;
	static UINT16	usMouseXPos, usMouseYPos;
	static RECT		StripRegions[ 2 ], MouseRegion;
	UINT16				usNumStrips = 0;
	INT32					cnt;
	INT16					sShiftX, sShiftY;
	INT32					uiCountY;
	UINT32					uiDestPitchBYTES;


	GetCurrentVideoSettings( &usWidth, &usHeight, &ubBitDepth );
	usHeight=(gsVIEWPORT_WINDOW_END_Y - gsVIEWPORT_WINDOW_START_Y );
	uiDestPitchBYTES = (usWidth * ubBitDepth) / 8;

	///zmiany
	StripRegions[ 0 ].left	= gsVIEWPORT_START_X ;
	StripRegions[ 0 ].right	= gsVIEWPORT_END_X	;
	StripRegions[ 0 ].top	= gsVIEWPORT_WINDOW_START_Y ;
	StripRegions[ 0 ].bottom = gsVIEWPORT_WINDOW_END_Y ;
	StripRegions[ 1 ].left	= gsVIEWPORT_START_X ;
	StripRegions[ 1 ].right	= gsVIEWPORT_END_X;
	StripRegions[ 1 ].top	= gsVIEWPORT_WINDOW_START_Y;
	StripRegions[ 1 ].bottom = gsVIEWPORT_WINDOW_END_Y;

	MouseRegion.left		= gMouseCursorBackground[ uiCurrentMouseBackbuffer ].usLeft;
	MouseRegion.top			= gMouseCursorBackground[ uiCurrentMouseBackbuffer ].usTop;
	MouseRegion.right		= gMouseCursorBackground[ uiCurrentMouseBackbuffer ].usRight;
	MouseRegion.bottom	= gMouseCursorBackground[ uiCurrentMouseBackbuffer ].usBottom;

	usMouseXPos					= gMouseCursorBackground[ uiCurrentMouseBackbuffer ].usMouseXPos;
	usMouseYPos					= gMouseCursorBackground[ uiCurrentMouseBackbuffer ].usMouseYPos;

	switch (uiDirection)
	{
	case SCROLL_LEFT:

		Region.left = 0;
		Region.top = gsVIEWPORT_WINDOW_START_Y;
		Region.right = usWidth-(sScrollXIncrement);
		Region.bottom = gsVIEWPORT_WINDOW_START_Y + usHeight;

		BlitSurfaceRegion(BACKBUFFER, BACKBUFFER, sScrollXIncrement,
			gsVIEWPORT_WINDOW_START_Y, Region);

		// memset z-buffer
		for(uiCountY = gsVIEWPORT_WINDOW_START_Y; uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0, sScrollXIncrement*2);
		}

		StripRegions[ 0 ].right =(INT16)(gsVIEWPORT_START_X+sScrollXIncrement);
		usMouseXPos += sScrollXIncrement;

		usNumStrips = 1;
		break;

	case SCROLL_RIGHT:


		Region.left = sScrollXIncrement ;
		Region.top = gsVIEWPORT_WINDOW_START_Y;
		Region.right = usWidth;
		Region.bottom = gsVIEWPORT_WINDOW_START_Y + usHeight;

		if (Region.left >= Region.right)
		{
			break;
		}

		BlitSurfaceRegion(BACKBUFFER, BACKBUFFER, 0,
			gsVIEWPORT_WINDOW_START_Y, Region);

		// memset z-buffer
		for(uiCountY= gsVIEWPORT_WINDOW_START_Y; uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES) + ( ( gsVIEWPORT_END_X - sScrollXIncrement ) * 2 ), 0,
				sScrollXIncrement*2);
		}


		//for(uiCountY=0; uiCountY < usHeight; uiCountY++)
		//{
		//	memcpy(pDestBuf+(uiCountY*uiDestPitchBYTES),
		//					pSrcBuf+(uiCountY*uiDestPitchBYTES)+sScrollXIncrement*uiBPP,
		//					uiDestPitchBYTES-sScrollXIncrement*uiBPP);
		//}

		StripRegions[ 0 ].left =(INT16)(gsVIEWPORT_END_X-sScrollXIncrement);
		usMouseXPos -= sScrollXIncrement;

		usNumStrips = 1;
		break;

	case SCROLL_UP:

		Region.left = 0;
		Region.top = gsVIEWPORT_WINDOW_START_Y;
		Region.right = usWidth;
		Region.bottom = gsVIEWPORT_WINDOW_START_Y + usHeight - sScrollYIncrement;

		BlitSurfaceRegion(BACKBUFFER, BACKBUFFER, 0,
			gsVIEWPORT_WINDOW_START_Y + sScrollYIncrement, Region);


		for(uiCountY=sScrollYIncrement-1+gsVIEWPORT_WINDOW_START_Y; uiCountY >= gsVIEWPORT_WINDOW_START_Y; uiCountY--)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0,
				uiDestPitchBYTES);
		}

		//for(uiCountY=usHeight-1; uiCountY >= sScrollYIncrement; uiCountY--)
		//{
		//	memcpy(pDestBuf+(uiCountY*uiDestPitchBYTES),
		//					pSrcBuf+((uiCountY-sScrollYIncrement)*uiDestPitchBYTES),
		//					uiDestPitchBYTES);
		//}
		StripRegions[ 0 ].bottom =(INT16)(gsVIEWPORT_WINDOW_START_Y+sScrollYIncrement);
		usNumStrips = 1;

		usMouseYPos += sScrollYIncrement;

		break;

	case SCROLL_DOWN:

		Region.left = 0;
		Region.top = gsVIEWPORT_WINDOW_START_Y + sScrollYIncrement;
		Region.right = usWidth;
		Region.bottom = gsVIEWPORT_WINDOW_START_Y + usHeight;

		BlitSurfaceRegion(BACKBUFFER, BACKBUFFER, 0,
			gsVIEWPORT_WINDOW_START_Y, Region);

		// Zero out z
		for(uiCountY=(gsVIEWPORT_WINDOW_END_Y - sScrollYIncrement ); uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0,
				uiDestPitchBYTES);
		}

		//for(uiCountY=0; uiCountY < (usHeight-sScrollYIncrement); uiCountY++)
		//{
		//	memcpy(pDestBuf+(uiCountY*uiDestPitchBYTES),
		//					pSrcBuf+((uiCountY+sScrollYIncrement)*uiDestPitchBYTES),
		//					uiDestPitchBYTES);
		//}

		StripRegions[ 0 ].top = (INT16)(gsVIEWPORT_WINDOW_END_Y-sScrollYIncrement);
		usNumStrips = 1;

		usMouseYPos -= sScrollYIncrement;

		break;

	case SCROLL_UPLEFT:

		Region.left = 0;
		Region.top = gsVIEWPORT_WINDOW_START_Y;
		Region.right = usWidth-(sScrollXIncrement);
		Region.bottom = gsVIEWPORT_WINDOW_START_Y + usHeight - sScrollYIncrement;

		BlitSurfaceRegion(BACKBUFFER, BACKBUFFER, sScrollXIncrement,
			gsVIEWPORT_WINDOW_START_Y + sScrollYIncrement, Region);

		// memset z-buffer
		for(uiCountY=gsVIEWPORT_WINDOW_START_Y; uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0, sScrollXIncrement*2);

		}
		for(uiCountY=gsVIEWPORT_WINDOW_START_Y + sScrollYIncrement-1; uiCountY >= gsVIEWPORT_WINDOW_START_Y; uiCountY--)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0, uiDestPitchBYTES);
		}


		StripRegions[ 0 ].right =	(INT16)(gsVIEWPORT_START_X+sScrollXIncrement);
		StripRegions[ 1 ].bottom = (INT16)(gsVIEWPORT_WINDOW_START_Y+sScrollYIncrement);
		StripRegions[ 1 ].left	= (INT16)(gsVIEWPORT_START_X+sScrollXIncrement);
		usNumStrips = 2;

		usMouseYPos += sScrollYIncrement;
		usMouseXPos += sScrollXIncrement;

		break;

	case SCROLL_UPRIGHT:

		Region.left = sScrollXIncrement;
		Region.top = gsVIEWPORT_WINDOW_START_Y;
		Region.right = usWidth;
		Region.bottom = gsVIEWPORT_WINDOW_START_Y + usHeight - sScrollYIncrement;

		BlitSurfaceRegion(BACKBUFFER, BACKBUFFER, 0,
			gsVIEWPORT_WINDOW_START_Y + sScrollYIncrement, Region);

		// memset z-buffer
		for(uiCountY=gsVIEWPORT_WINDOW_START_Y; uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES) + ( ( gsVIEWPORT_END_X - sScrollXIncrement ) * 2 ), 0,
				sScrollXIncrement*2);
		}
		for(uiCountY=gsVIEWPORT_WINDOW_START_Y + sScrollYIncrement-1; uiCountY >= gsVIEWPORT_WINDOW_START_Y; uiCountY--)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0, uiDestPitchBYTES);
		}


		StripRegions[ 0 ].left =	(INT16)(gsVIEWPORT_END_X-sScrollXIncrement);
		StripRegions[ 1 ].bottom = (INT16)(gsVIEWPORT_WINDOW_START_Y+sScrollYIncrement);
		StripRegions[ 1 ].right	= (INT16)(gsVIEWPORT_END_X-sScrollXIncrement);
		usNumStrips = 2;

		usMouseYPos += sScrollYIncrement;
		usMouseXPos -= sScrollXIncrement;

		break;

	case SCROLL_DOWNLEFT:

		Region.left = 0;
		Region.top = gsVIEWPORT_WINDOW_START_Y + sScrollYIncrement;
		Region.right = usWidth-(sScrollXIncrement);
		Region.bottom = gsVIEWPORT_WINDOW_START_Y + usHeight;

		BlitSurfaceRegion(BACKBUFFER, BACKBUFFER, sScrollXIncrement,
			gsVIEWPORT_WINDOW_START_Y, Region);

		// memset z-buffer
		for(uiCountY=gsVIEWPORT_WINDOW_START_Y; uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0, sScrollXIncrement*2);
		}
		for(uiCountY=(gsVIEWPORT_WINDOW_END_Y - sScrollYIncrement); uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0, uiDestPitchBYTES);
		}


		StripRegions[ 0 ].right =(INT16)(gsVIEWPORT_START_X+sScrollXIncrement);


		StripRegions[ 1 ].top		= (INT16)(gsVIEWPORT_WINDOW_END_Y-sScrollYIncrement);
		StripRegions[ 1 ].left	= (INT16)(gsVIEWPORT_START_X+sScrollXIncrement);
		usNumStrips = 2;

		usMouseYPos -= sScrollYIncrement;
		usMouseXPos += sScrollXIncrement;

		break;

	case SCROLL_DOWNRIGHT:

		Region.left = sScrollXIncrement;
		Region.top = gsVIEWPORT_WINDOW_START_Y + sScrollYIncrement;
		Region.right = usWidth;
		Region.bottom = gsVIEWPORT_WINDOW_START_Y + usHeight;

		BlitSurfaceRegion(BACKBUFFER, BACKBUFFER, 0,
			gsVIEWPORT_WINDOW_START_Y, Region);

		// memset z-buffer
		for(uiCountY=gsVIEWPORT_WINDOW_START_Y; uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES) + ( ( gsVIEWPORT_END_X - sScrollXIncrement ) * 2 ), 0,
				sScrollXIncrement*2);
		}
		for(uiCountY=(gsVIEWPORT_WINDOW_END_Y - sScrollYIncrement); uiCountY < gsVIEWPORT_WINDOW_END_Y; uiCountY++)
		{
			memset((UINT8 *)gpZBuffer+(uiCountY*uiDestPitchBYTES), 0, uiDestPitchBYTES);
		}


		StripRegions[ 0 ].left =(INT16)(gsVIEWPORT_END_X-sScrollXIncrement);
		StripRegions[ 1 ].top = (INT16)(gsVIEWPORT_WINDOW_END_Y-sScrollYIncrement);
		StripRegions[ 1 ].right = (INT16)(gsVIEWPORT_END_X-sScrollXIncrement);
		usNumStrips = 2;

		usMouseYPos -= sScrollYIncrement;
		usMouseXPos -= sScrollXIncrement;

		break;

	}

	if ( fRenderStrip )
	{

		// Memset to 0
#ifdef SCROLL_TEST
		ColorFillVideoSurfaceArea(BACKBUFFER, 0, 0,
			SCREEN_WIDTH, SCREEN_HEIGHT, 0);
#endif



		for ( cnt = 0; cnt < usNumStrips; cnt++ )
		{
			//RenderStaticWorld();
			//RenderDynamicWorld();
			RenderStaticWorldRect( (INT16)StripRegions[ cnt ].left , (INT16)StripRegions[ cnt ].top , (INT16)StripRegions[ cnt ].right, (INT16)StripRegions[ cnt ].bottom , TRUE );
			// Optimize Redundent tiles too!
			//ExamineZBufferRect( (INT16)StripRegions[ cnt ].left, (INT16)StripRegions[ cnt ].top, (INT16)StripRegions[ cnt ].right, (INT16)StripRegions[ cnt ].bottom );
			BlitSurfaceRegion(BACKBUFFER, FRAME_BUFFER,
				StripRegions[cnt].left, StripRegions[cnt].top,
				StripRegions[cnt]);
		}

		sShiftX = 0;
		sShiftY = 0;

		switch (uiDirection)
		{
		case SCROLL_LEFT:

			sShiftX = sScrollXIncrement;
			sShiftY = 0;
			break;

		case SCROLL_RIGHT:

			sShiftX = -sScrollXIncrement;
			sShiftY = 0;
			break;

		case SCROLL_UP:

			sShiftX = 0;
			sShiftY = sScrollYIncrement;
			break;

		case SCROLL_DOWN:

			sShiftX = 0;
			sShiftY = -sScrollYIncrement;
			break;

		case SCROLL_UPLEFT:

			sShiftX = sScrollXIncrement;
			sShiftY = sScrollYIncrement;
			break;

		case SCROLL_UPRIGHT:

			sShiftX = -sScrollXIncrement;
			sShiftY = sScrollYIncrement;
			break;

		case SCROLL_DOWNLEFT:

			sShiftX = sScrollXIncrement;
			sShiftY = -sScrollYIncrement;
			break;

		case SCROLL_DOWNRIGHT:

			sShiftX = -sScrollXIncrement;
			sShiftY = -sScrollYIncrement;
			break;


		}

		// RESTORE SHIFTED
		RestoreShiftedVideoOverlays( sShiftX, sShiftY );

		// SAVE NEW
		SaveVideoOverlaysArea( BACKBUFFER );

		// BLIT NEW
		ExecuteVideoOverlaysToAlternateBuffer( BACKBUFFER );



	}


	//InvalidateRegion( sLeftDraw, sTopDraw, sRightDraw, sBottomDraw );

	//UpdateSaveBuffer();
	//SaveBackgroundRects();
}


//rain
//extern BOOLEAN gfVSync;

BOOLEAN IsItAllowedToRenderRain();
extern UINT32 guiRainRenderSurface;

BOOLEAN gfNextRefreshFullScreen = FALSE;
//end rain

static BOOLEAN BlitSurfaceRegion(UINT32 destination, UINT32 source,
	INT32 destinationX, INT32 destinationY, const RECT& sourceRegion)
{
	blt_vs_fx effects;
	effects.SrcRect = {sourceRegion.left, sourceRegion.top,
		sourceRegion.right, sourceRegion.bottom};
	return BltVideoSurface(destination, source, 0, destinationX, destinationY,
		VS_BLT_SRCSUBRECT, &effects);
}

static ja2::presentation::PixelSurface *GetPixelSurface(
	UINT32 handle, HVSURFACE *owner)
{
	HVSURFACE surface;
	if (!GetVideoSurface(&surface, handle))
	{
		return NULL;
	}
	if (owner != NULL)
	{
		*owner = surface;
	}
	return GetVideoSurfacePixelSurface(surface);
}

static BOOLEAN RestoreMouseBackground(const MouseCursorBackground& background)
{
	HVSURFACE backBuffer;
	ja2::presentation::PixelSurface *destination =
		GetPixelSurface(BACKBUFFER, &backBuffer);
	if (destination == NULL || !gMouseCursorBackgroundSurface)
	{
		return FALSE;
	}
	const SGPRect source = {background.usLeft, background.usTop,
		background.usRight, background.usBottom};
	if (!destination->blitFrom(*gMouseCursorBackgroundSurface, source,
		background.usMouseXPos, background.usMouseYPos))
	{
		return FALSE;
	}
	return TRUE;
}

static BOOLEAN SaveMouseBackground(const MouseCursorBackground& background)
{
	ja2::presentation::PixelSurface *source =
		GetPixelSurface(BACKBUFFER, NULL);
	if (source == NULL || !gMouseCursorBackgroundSurface)
	{
		return FALSE;
	}
	const SGPRect sourceRegion = {background.Region.left,
		background.Region.top, background.Region.right, background.Region.bottom};
	return gMouseCursorBackgroundSurface->blitFrom(*source, sourceRegion,
		background.usLeft, background.usTop) ? TRUE : FALSE;
}

static BOOLEAN DrawMouseCursor(const MouseCursorBackground& background)
{
	HVSURFACE backBuffer;
	ja2::presentation::PixelSurface *destination =
		GetPixelSurface(BACKBUFFER, &backBuffer);
	ja2::presentation::PixelSurface *cursor =
		GetPixelSurface(MOUSE_BUFFER, NULL);
	if (destination == NULL || cursor == NULL)
	{
		return FALSE;
	}
	const SGPRect source = {background.usLeft, background.usTop,
		background.usRight, background.usBottom};
	if (!destination->blitFrom(*cursor, source, background.usMouseXPos,
		background.usMouseYPos, {true, false}))
	{
		return FALSE;
	}
	return TRUE;
}

static BOOLEAN IsPresentationDue(void)
{
	// Game logic and blocking transitions may compose more often than the host
	// can display. Limit only the final submission; never sleep the game thread.
	// This is the same separation used by Stracciatella's capped game refresh.
	static const std::uint64_t frameIntervalMicroseconds = 16000;
	const std::uint64_t now = Platform::GetClockMicroseconds();
	if (gNextPresentationMicroseconds != 0 &&
		now < gNextPresentationMicroseconds)
	{
		return FALSE;
	}
	gNextPresentationMicroseconds = now + frameIntervalMicroseconds;
	return TRUE;
}

static BOOLEAN PresentBackBuffer(void)
{
	HVSURFACE backBuffer;
	if (!gPresenter || !GetVideoSurface(&backBuffer, BACKBUFFER))
	{
		return FALSE;
	}
	ja2::presentation::PixelSurface* pixels =
		GetVideoSurfacePixelSurface(backBuffer);
	if (pixels == NULL)
	{
		return FALSE;
	}

	ja2::presentation::PresentFrame frame;
	frame.buffer = pixels->pixels();
	frame.dirtyRegions = gDirtyRegionTracker.regions().data();
	frame.dirtyRegionCount = gDirtyRegionTracker.regions().size();
	frame.fullRefresh = gDirtyRegionTracker.fullRefresh();
	frame.verticalSync = gGameExternalOptions.gfVSync;
	return gPresenter->present(frame) ? TRUE : FALSE;
}

void RefreshScreen(void *DummyVariable)
{
	static UINT32	uiRefreshThreadState, uiIndex;
	UINT16	usScreenWidth, usScreenHeight;
	static BOOLEAN fShowMouse;
	static RECT	Region;
	static INT16	sx, sy;
	static SGPPoint MousePos;
	static BOOLEAN fFirstTime = TRUE;
	UINT32						uiTime;

	usScreenWidth = usScreenHeight = 0;

	if ( fFirstTime )
	{
		fShowMouse = FALSE;
	}

	if( gfNextRefreshFullScreen )
	{
		if( guiCurrentScreen == GAME_SCREEN )
		{
			InvalidateScreen();
			gfRenderScroll = FALSE;
			//			gfForceFullScreenRefresh = TRUE;
			//			guiFrameBufferState == BUFFER_DIRTY;
		}
		gfNextRefreshFullScreen = FALSE;
	}

	//DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "Looping in refresh");

	///////////////////////////////////////////////////////////////////////////////////////////////
	//
	// REFRESH_THREAD_MUTEX
	//
	///////////////////////////////////////////////////////////////////////////////////////////////

	switch (guiVideoManagerState)
	{
		case VIDEO_ON
			: //
				// Excellent, everything is cosher, we continue on
				//
				uiRefreshThreadState = guiRefreshThreadState = THREAD_ON;
				usScreenWidth = gusScreenWidth;
				usScreenHeight = gusScreenHeight;
				break;
				case VIDEO_OFF
					: //
						// Hot damn, the video manager is suddenly off. We have to bugger out of here. Don't forget to
						// leave the critical section
						//
						guiRefreshThreadState = THREAD_OFF;
						return;
						case VIDEO_SUSPENDED
							: //
								// This are suspended. Make sure the refresh function does try to access any of the direct
								// draw surfaces
								//
								uiRefreshThreadState = guiRefreshThreadState = THREAD_SUSPENDED;
								break;
								case VIDEO_SHUTTING_DOWN
									: //
										// Well things are shutting down. So we need to bugger out of there. Don't forget to leave the
										// critical section before returning
										//
										guiRefreshThreadState = THREAD_OFF;
										return;
	}


	//
	// Get the current mouse position
	//

	GetMousePos(&MousePos);

	/////////////////////////////////////////////////////////////////////////////////////////////
	//
	// FRAME_BUFFER_MUTEX
	//
	/////////////////////////////////////////////////////////////////////////////////////////////


	// RESTORE OLD POSITION OF MOUSE
	if (gMouseCursorBackground[CURRENT_MOUSE_DATA].fRestore == TRUE )
	{
		Region.left = gMouseCursorBackground[CURRENT_MOUSE_DATA].usLeft;
		Region.top = gMouseCursorBackground[CURRENT_MOUSE_DATA].usTop;
		Region.right = gMouseCursorBackground[CURRENT_MOUSE_DATA].usRight;
		Region.bottom = gMouseCursorBackground[CURRENT_MOUSE_DATA].usBottom;

		if (!RestoreMouseBackground(
			gMouseCursorBackground[CURRENT_MOUSE_DATA]))
		{
			goto ENDOFLOOP;
		}

		// Save position into other background region
		memcpy( &(gMouseCursorBackground[PREVIOUS_MOUSE_DATA] ), &(gMouseCursorBackground[CURRENT_MOUSE_DATA] ), sizeof( MouseCursorBackground ) );

	}


	//
	// Ok we were able to get a hold of the frame buffer stuff. Check to see if it needs updating
	// if not, release the frame buffer stuff right away
	//
	if (guiFrameBufferState == BUFFER_DIRTY)
	{

		// Well the frame buffer is dirty.
		//

		if (gpFrameBufferRefreshOverride != NULL)
		{
			//
			// Method (3) - We are using a function override to refresh the frame buffer. First we
			// call the override function then we must set the override pointer to NULL
			//

			(*gpFrameBufferRefreshOverride)();
			gpFrameBufferRefreshOverride = NULL;

		}


		if ( gfFadeInitialized && gfFadeInVideo )
		{
			gFadeFunction( );
		}
		else
			//
			// Either Method (1) or (2)
			//
		{
			if (gDirtyRegionTracker.fullRefresh())
			{
				//
				// Method (1) - We will be refreshing the entire screen
				//

				Region.left = 0;
				Region.top = 0;
				Region.right = usScreenWidth;
				Region.bottom = usScreenHeight;

				if (!BlitSurfaceRegion(BACKBUFFER, FRAME_BUFFER, 0, 0, Region))
				{
					goto ENDOFLOOP;
				}

			}
			else
			{
				const std::vector<SGPRect>& dirtyRegions =
					gDirtyRegionTracker.regions();
				for (uiIndex = 0; uiIndex < dirtyRegions.size(); uiIndex++)
				{
					Region.left	= dirtyRegions[uiIndex].iLeft;
					Region.top	= dirtyRegions[uiIndex].iTop;
					Region.right	= dirtyRegions[uiIndex].iRight;
					Region.bottom = dirtyRegions[uiIndex].iBottom;

					if (!BlitSurfaceRegion(BACKBUFFER, FRAME_BUFFER,
						Region.left, Region.top, Region))
					{
						goto ENDOFLOOP;
					}

				}

				// Now do new, extended dirty regions
				const std::vector<ja2::presentation::ExtendedDirtyRegion>&
					extendedDirtyRegions = gDirtyRegionTracker.extendedRegions();
				for (uiIndex = 0; uiIndex < extendedDirtyRegions.size(); uiIndex++)
				{
					Region.left	= extendedDirtyRegions[uiIndex].bounds.iLeft;
					Region.top	= extendedDirtyRegions[uiIndex].bounds.iTop;
					Region.right	= extendedDirtyRegions[uiIndex].bounds.iRight;
					Region.bottom = extendedDirtyRegions[uiIndex].bounds.iBottom;

					// Do some checks if we are in the process of scrolling!
					if ( gfRenderScroll )
					{

						// Check if we are completely out of bounds
						if ( Region.top <= gsVIEWPORT_WINDOW_END_Y	&& Region.bottom <= gsVIEWPORT_WINDOW_END_Y )
						{
							continue;
						}

					}

					if (!BlitSurfaceRegion(BACKBUFFER, FRAME_BUFFER,
						Region.left, Region.top, Region))
					{
						goto ENDOFLOOP;
					}
				}
			}

		}
		if ( gfRenderScroll )
		{
			ScrollJA2Background(guiScrollDirection, gsScrollXIncrement,
				gsScrollYIncrement, TRUE, PREVIOUS_MOUSE_DATA);
		}

		gfIgnoreScrollDueToCenterAdjust = FALSE;




		//
		// Update the guiFrameBufferState variable to reflect that the frame buffer can now be handled
		//

		guiFrameBufferState = BUFFER_READY;
	}

	//
	// Do we want to print the frame stuff ??
	//

	if( gfVideoCapture )
	{
		uiTime=Platform::GetClockMilliseconds();
		if((uiTime < guiLastFrame) || (uiTime > (guiLastFrame+guiFramePeriod)))
		{
			SnapshotSmall( );
			guiLastFrame=uiTime;
		}
	}


	if (gfPrintFrameBuffer == TRUE)
	{
		HVSURFACE backBuffer;
		ja2::presentation::PixelSurface* surface =
			GetVideoSurface(&backBuffer, BACKBUFFER) ?
			GetVideoSurfacePixelSurface(backBuffer) : NULL;
		if (surface != NULL &&
			surface->format() == ja2::presentation::PixelFormat::rgb565)
		{
			CHAR8 fileName[64];
			do
			{
				sprintf(fileName, "SCREEN%03d.TGA", guiPrintFrameBufferIndex++);
			}
			while (FileExists(fileName));

			try
			{
				const ja2::presentation::ConstPixelBuffer pixels =
					surface->pixels();
				std::unique_ptr<ja2::fileio::File> output =
					ja2::fileio::storeRouter().create(fileName);
				char head[] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,
					static_cast<char>(LOBYTE(pixels.width)),
					static_cast<char>(HIBYTE(pixels.width)),
					static_cast<char>(LOBYTE(pixels.height)),
					static_cast<char>(HIBYTE(pixels.height)), 0x10, 0};
				output->writeExact(head, sizeof(head));

				const std::size_t rowBytes =
					static_cast<std::size_t>(pixels.width) * sizeof(UINT16);
				const bool convert565 = gusRedMask == 0xF800 &&
					gusGreenMask == 0x07E0 && gusBlueMask == 0x001F;
				std::vector<UINT16> convertedRow(
					convert565 ? pixels.width : 0);
				for (INT32 y = pixels.height - 1; y >= 0; --y)
				{
					const BYTE* row = pixels.pixels +
						static_cast<std::size_t>(y) * pixels.pitchBytes;
					if (convert565)
					{
						memcpy(convertedRow.data(), row, rowBytes);
						ConvertRGBDistribution565To555(
							convertedRow.data(), pixels.width);
						output->writeExact(convertedRow.data(), rowBytes);
					}
					else
					{
						output->writeExact(row, rowBytes);
					}
				}
			}
			catch(std::exception& ex)
			{
				SGP_RETHROW(L"", ex);
			}
		}
		gfPrintFrameBuffer = FALSE;
	}

	//
	// Ok we were able to get a hold of the frame buffer stuff. Check to see if it needs updating
	// if not, release the frame buffer stuff right away
	//

	if (guiMouseBufferState == BUFFER_DIRTY)
	{
		// Cursor writers already update the canonical PixelSurface. The old
		// gpMouseCursor upload existed only for DirectDraw composition.
		guiMouseBufferState = BUFFER_READY;
	}

	//
	// Check current state of the mouse cursor
	//

	if (fShowMouse == FALSE)
	{
		if (guiMouseBufferState == BUFFER_READY)
		{
			fShowMouse = TRUE;
		}
		else
		{
			fShowMouse = FALSE;
		}
	}
	else
	{
		if (guiMouseBufferState == BUFFER_DISABLED)
		{
			fShowMouse = FALSE;
		}
	}

	///////////////////////////////////////////////////////////////////////////////////////////////
	//
	// End of MOUSE_BUFFER_MUTEX
	//
	///////////////////////////////////////////////////////////////////////////////////////////////


	///////////////////////////////////////////////////////////////////////////////////////////////
	//
	// If fMouseState == TRUE
	//
	// (1) Save the mouse background from the back buffer.
	// (2) If step (1) succeeds, blit the mouse cursor onto the back buffer.
	//
	///////////////////////////////////////////////////////////////////////////////////////////////

	if (fShowMouse == TRUE )
	{
		//
		// Step (1) - Save mouse background
		//

		Region.left	= MousePos.iX - gsMouseCursorXOffset;
		Region.top	= MousePos.iY - gsMouseCursorYOffset;
		Region.right	= Region.left + gusMouseCursorWidth;
		Region.bottom = Region.top + gusMouseCursorHeight;

		if (Region.right > usScreenWidth)
		{
			Region.right = usScreenWidth;
		}

		if (Region.bottom > usScreenHeight)
		{
			Region.bottom = usScreenHeight;
		}

		if ((Region.right > Region.left)&&(Region.bottom > Region.top))
		{
			//
			// Make sure the mouse background is marked for restore and coordinates are saved for the
			// future restore
			//

			gMouseCursorBackground[CURRENT_MOUSE_DATA].fRestore	= TRUE;
			gMouseCursorBackground[CURRENT_MOUSE_DATA].usRight	 = (INT16)Region.right - (INT16) Region.left;
			gMouseCursorBackground[CURRENT_MOUSE_DATA].usBottom	= (INT16)Region.bottom - (INT16) Region.top;
			if (Region.left < 0)
			{
				gMouseCursorBackground[CURRENT_MOUSE_DATA].usLeft = (INT16) (0 - Region.left);
				gMouseCursorBackground[CURRENT_MOUSE_DATA].usMouseXPos = 0;
				Region.left = 0;
			}
			else
			{
				gMouseCursorBackground[CURRENT_MOUSE_DATA].usMouseXPos = (UINT16) MousePos.iX - gsMouseCursorXOffset;
				gMouseCursorBackground[CURRENT_MOUSE_DATA].usLeft = 0;
			}
			if (Region.top < 0)
			{
				gMouseCursorBackground[CURRENT_MOUSE_DATA].usMouseYPos = 0;
				gMouseCursorBackground[CURRENT_MOUSE_DATA].usTop = (UINT16) (0 - Region.top);
				Region.top = 0;
			}
			else
			{
				gMouseCursorBackground[CURRENT_MOUSE_DATA].usMouseYPos = (UINT16) MousePos.iY - gsMouseCursorYOffset;
				gMouseCursorBackground[CURRENT_MOUSE_DATA].usTop = 0;
			}

			if ((Region.right > Region.left)&&(Region.bottom > Region.top))
			{
				// Save clipped region
				gMouseCursorBackground[CURRENT_MOUSE_DATA].Region = Region;

				//
				// Ok, do the actual data save to the mouse background
				//

				if (!SaveMouseBackground(
					gMouseCursorBackground[CURRENT_MOUSE_DATA]))
				{
					goto ENDOFLOOP;
				}

				//
				// Step (2) - Blit mouse cursor to back buffer
				//

				Region.left = gMouseCursorBackground[CURRENT_MOUSE_DATA].usLeft;
				Region.top = gMouseCursorBackground[CURRENT_MOUSE_DATA].usTop;
				Region.right = gMouseCursorBackground[CURRENT_MOUSE_DATA].usRight;
				Region.bottom = gMouseCursorBackground[CURRENT_MOUSE_DATA].usBottom;

				if (!DrawMouseCursor(
					gMouseCursorBackground[CURRENT_MOUSE_DATA]))
				{
					goto ENDOFLOOP;
				}
			}
			else
			{
				//
				// Hum, the mouse was not blitted this round. Henceforth we will flag fRestore as FALSE
				//

				gMouseCursorBackground[CURRENT_MOUSE_DATA].fRestore = FALSE;
			}

		}
		else
		{
			//
			// Hum, the mouse was not blitted this round. Henceforth we will flag fRestore as FALSE
			//

			gMouseCursorBackground[CURRENT_MOUSE_DATA].fRestore = FALSE;

		}
	}
	else
	{
		//
		// Well since there was no mouse handling this round, we disable the mouse restore
		//

		gMouseCursorBackground[CURRENT_MOUSE_DATA].fRestore = FALSE;

	}



	///////////////////////////////////////////////////////////////////////////////////////////////
	// Rain																						//
	///////////////////////////////////////////////////////////////////////////////////////////////

	if( IsItAllowedToRenderRain() && gfProgramIsRunning )
	{
		BltVideoSurface( BACKBUFFER, guiRainRenderSurface, 0, 0, 0, VS_BLT_FAST | VS_BLT_USECOLORKEY, NULL );
		gfNextRefreshFullScreen = TRUE;
	}

	if (!IsPresentationDue())
	{
		// Composition is already retained in the canonical back buffer. Consume
		// this engine frame's bookkeeping even when its host submission is
		// coalesced with a later frame.
		gfRenderScroll = FALSE;
		gfScrollStart = FALSE;
		gDirtyRegionTracker.clearAfterPresent();
		goto ENDOFLOOP;
	}

	// The engine hands the completed portable framebuffer to the host adapter.
	// DirectDraw upload, windowed blit, fullscreen flip, and retry handling stay
	// behind Presenter.
	if (!PresentBackBuffer())
	{
		goto ENDOFLOOP;
	}
	if (iScreenMode != 1)
	{
		// A flip changes which allocation DirectDraw calls the back buffer. Keep
		// the compatibility mirror dirty; the retained PixelSurface is canonical
		// and the presenter uploads it again before the next flip.
		HVSURFACE backBuffer;
		if (GetVideoSurface(&backBuffer, BACKBUFFER))
		{
		}
	}
	gfRenderScroll = FALSE;
	gfScrollStart = FALSE;
	gDirtyRegionTracker.clearAfterPresent();


ENDOFLOOP:


	fFirstTime = FALSE;

}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Direct X object access functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

// Buffer access functions
///////////////////////////////////////////////////////////////////////////////////////////////////

PTR LockFrameBuffer(UINT32 *uiPitch)
{
	HVSURFACE frameBuffer;
	if (!GetVideoSurface(&frameBuffer, FRAME_BUFFER))
	{
		return NULL;
	}
	return LockVideoSurfaceBuffer(frameBuffer, uiPitch);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void UnlockFrameBuffer(void)
{
	HVSURFACE frameBuffer;
	if (GetVideoSurface(&frameBuffer, FRAME_BUFFER))
	{
		UnLockVideoSurfaceBuffer(frameBuffer);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

PTR LockMouseBuffer(UINT32 *uiPitch)
{
	HVSURFACE mouseBuffer;
	if (!GetVideoSurface(&mouseBuffer, MOUSE_BUFFER))
	{
		return NULL;
	}
	return LockVideoSurfaceBuffer(mouseBuffer, uiPitch);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void UnlockMouseBuffer(void)
{
	HVSURFACE mouseBuffer;
	if (GetVideoSurface(&mouseBuffer, MOUSE_BUFFER))
	{
		UnLockVideoSurfaceBuffer(mouseBuffer);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// RGB color management functions
//
///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN GetRGBDistribution(void)
{
	UINT16		usBit;

	if (!gPresenter || !gPresenter->getRgbMasks(
		gusRedMask, gusGreenMask, gusBlueMask))
	{
		return FALSE;
	}

	//
	// Ok we now have the surface description, we now can get the information that we need
	//

	if (!gusRedMask)
	{
		Platform::ShowDialog(APPLICATION_NAME,
			ja2::text::utf16ToUtf8ReplacingInvalid(Additional113Text[ADDTEXT_16BPP_REQUIRED]),
			Platform::DialogKind::warning);
		PostQuitMessage(1);
		return FALSE;
	}

	// RGB 5,5,5
	if((gusRedMask==0x7c00) && (gusGreenMask==0x03e0) && (gusBlueMask==0x1f))
		guiTranslucentMask=0x3def;
	// RGB 5,6,5
	else// if((gusRedMask==0xf800) && (gusGreenMask==0x03e0) && (gusBlueMask==0x1f))
		guiTranslucentMask=0x7bef;


	usBit = 0x8000;
	gusRedShift = 8;
	while(!(gusRedMask & usBit))
	{
		usBit >>= 1;
		gusRedShift--;
	}

	usBit = 0x8000;
	gusGreenShift = 8;
	while(!(gusGreenMask & usBit))
	{
		usBit >>= 1;
		gusGreenShift--;
	}

	usBit = 0x8000;
	gusBlueShift = 8;
	while(!(gusBlueMask & usBit))
	{
		usBit >>= 1;
		gusBlueShift--;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN GetPrimaryRGBDistributionMasks(UINT32 *RedBitMask, UINT32 *GreenBitMask, UINT32 *BlueBitMask)
{
	*RedBitMask	= gusRedMask;
	*GreenBitMask = gusGreenMask;
	*BlueBitMask	= gusBlueMask;

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN SetMouseCursorFromObject(UINT32 uiVideoObjectHandle, UINT16 usVideoObjectSubIndex, UINT16 usOffsetX, UINT16 usOffsetY )
{
	BOOLEAN		ReturnValue;
	PTR			pTmpPointer;
	UINT32		uiPitch;
	ETRLEObject	pETRLEPointer;

	//
	// Erase cursor background
	//

	pTmpPointer = LockMouseBuffer(&uiPitch);
	memset(pTmpPointer, 0, MAX_CURSOR_HEIGHT * uiPitch);
	UnlockMouseBuffer();

	//
	// Get new cursor data
	//

	ReturnValue = BltVideoObjectFromIndex(MOUSE_BUFFER, uiVideoObjectHandle, usVideoObjectSubIndex, 0, 0, VO_BLT_SRCTRANSPARENCY, NULL);
	guiMouseBufferState = BUFFER_DIRTY;

	if (GetVideoObjectETRLEPropertiesFromIndex(uiVideoObjectHandle, &pETRLEPointer, usVideoObjectSubIndex))
	{
		gsMouseCursorXOffset = usOffsetX;
		gsMouseCursorYOffset = usOffsetY;
		gusMouseCursorWidth = pETRLEPointer.usWidth + pETRLEPointer.sOffsetX;
		gusMouseCursorHeight = pETRLEPointer.usHeight + pETRLEPointer.sOffsetY;

		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "=================================================");
		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, String("Mouse Create with [ %d. %d ] [ %d, %d]", pETRLEPointer.sOffsetX, pETRLEPointer.sOffsetY, pETRLEPointer.usWidth, pETRLEPointer.usHeight));
		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "=================================================");

	}
	else
	{
		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "Failed to get mouse info");
	}

	return ReturnValue;
}

BOOLEAN EraseMouseCursor( )
{
	PTR			pTmpPointer;
	UINT32		uiPitch;

	//
	// Erase cursor background
	//

	pTmpPointer = LockMouseBuffer(&uiPitch);
	// when there is no mouse buffer the game can run into an infinite loop (some DirectX stuff)
	if(pTmpPointer)
	{
		memset(pTmpPointer, 0, MAX_CURSOR_HEIGHT * uiPitch);
	}
	UnlockMouseBuffer();

	// Don't set dirty
	return( TRUE );
}

BOOLEAN SetMouseCursorProperties( INT16 sOffsetX, INT16 sOffsetY, UINT16 usCursorHeight, UINT16 usCursorWidth )
{
	gsMouseCursorXOffset = sOffsetX;
	gsMouseCursorYOffset = sOffsetY;
	gusMouseCursorWidth	= usCursorWidth;
	gusMouseCursorHeight = usCursorHeight;
	return( TRUE );
}

BOOLEAN BltToMouseCursor(UINT32 uiVideoObjectHandle, UINT16 usVideoObjectSubIndex, UINT16 usXPos, UINT16 usYPos )
{
	BOOLEAN		ReturnValue;

	ReturnValue = BltVideoObjectFromIndex(MOUSE_BUFFER, uiVideoObjectHandle, usVideoObjectSubIndex, usXPos, usYPos, VO_BLT_SRCTRANSPARENCY, NULL);

	return ReturnValue;
}

void DirtyCursor( )
{
	guiMouseBufferState = BUFFER_DIRTY;
}

void EnableCursor( BOOLEAN fEnable )
{
	if ( fEnable )
	{
		guiMouseBufferState = BUFFER_DISABLED;
	}
	else
	{
		guiMouseBufferState = BUFFER_READY;
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN HideMouseCursor(void)
{
	guiMouseBufferState = BUFFER_DISABLED;

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN LoadCursorFile(STR8 pFilename)
{
	VOBJECT_DESC VideoObjectDescription;

	//
	// Make sure the old cursor store is destroyed
	//

	if (gpCursorStore != NULL)
	{
		DeleteVideoObject(gpCursorStore);
		gpCursorStore = NULL;
	}

	//
	// Get the source file with all the cursors inside
	//

	VideoObjectDescription.fCreateFlags = VOBJECT_CREATE_FROMFILE;
	strcpy(VideoObjectDescription.ImageFile, pFilename);
	gpCursorStore = CreateVideoObject(&VideoObjectDescription);

	//
	// Were we successful in creating the cursor store ?
	//

	if (gpCursorStore == NULL)
	{
		return FALSE;
	}

	return TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BOOLEAN SetCurrentCursor(UINT16 usVideoObjectSubIndex,	UINT16 usOffsetX, UINT16 usOffsetY )
{
	BOOLEAN		ReturnValue;
	PTR			pTmpPointer;
	UINT32		uiPitch;
	ETRLEObject	pETRLEPointer;

	//
	// Make sure we have a cursor store
	//

	if (gpCursorStore == NULL)
	{
		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "ERROR : Cursor store is not loaded");
		return FALSE;
	}

	//
	// Ok, then blit the mouse cursor to the MOUSE_BUFFER (which is really gpMouseBufferOriginal)
	//
	//
	// Erase cursor background
	//

	pTmpPointer = LockMouseBuffer(&uiPitch);
	memset(pTmpPointer, 0, MAX_CURSOR_HEIGHT * uiPitch);
	UnlockMouseBuffer();

	//
	// Get new cursor data
	//

	ReturnValue = BltVideoObject(MOUSE_BUFFER, gpCursorStore, usVideoObjectSubIndex, 0, 0, VO_BLT_SRCTRANSPARENCY, NULL);
	guiMouseBufferState = BUFFER_DIRTY;

	if (GetVideoObjectETRLEProperties(gpCursorStore, &pETRLEPointer, usVideoObjectSubIndex))
	{
		gsMouseCursorXOffset = usOffsetX;
		gsMouseCursorYOffset = usOffsetY;
		gusMouseCursorWidth = pETRLEPointer.usWidth + pETRLEPointer.sOffsetX;
		gusMouseCursorHeight = pETRLEPointer.usHeight + pETRLEPointer.sOffsetY;

		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "=================================================");
		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, String("Mouse Create with [ %d. %d ] [ %d, %d]", pETRLEPointer.sOffsetX, pETRLEPointer.sOffsetY, pETRLEPointer.usWidth, pETRLEPointer.usHeight));
		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "=================================================");
	}
	else
	{
		DebugMsg(TOPIC_VIDEO, DBG_LEVEL_0, "Failed to get mouse info");
	}

	return ReturnValue;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void StartFrameBufferRender(void)
{
	return;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void EndFrameBufferRender(void)
{

	guiFrameBufferState = BUFFER_DIRTY;

	return;

}

///////////////////////////////////////////////////////////////////////////////////////////////////

void PrintScreen(void)
{
	gfPrintFrameBuffer = TRUE;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
BOOLEAN Set8BPPPalette(SGPPaletteEntry *pPalette)
{
	// If we are in 256 colors, then we have to initialize the palette system to 0 (faded out)
	memcpy(gSgpPalette, pPalette, sizeof(SGPPaletteEntry)*256);

	if (!gPresenter || !gPresenter->setPalette(gSgpPalette))
	{
		return(FALSE);
	}

	return(TRUE);
}


void FatalError( const STR8 pError, ...)
{
	va_list argptr;

	va_start(argptr, pError);			// Set up variable argument pointer
	vsprintf(gFatalErrorString, pError, argptr);
	va_end(argptr);


	gfFatalError = TRUE;

	// Release the active presentation backend.
	if(gPresenter)
	{
		gPresenter->shutdown();
	}
	ShowWindow( ghWindow, SW_HIDE );

	// destroy the window
	// DestroyWindow( ghWindow );

	gfProgramIsRunning = FALSE;

	//MessageBox( ghWindow, gFatalErrorString, "JA2 Fatal Error", MB_OK | MB_TASKMODAL );
	Platform::ShowDialog("JA2 Fatal Error", gFatalErrorString, Platform::DialogKind::error);
}


/*********************************************************************************
* SnapshotSmall
*
*		Grabs the canonical back buffer and stores it in the movie-frame cache.
*
*********************************************************************************/

#pragma pack (push, 1)

typedef struct {

	UINT8		ubIDLength;
	UINT8		ubColorMapType;
	UINT8		ubTargaType;
	UINT16	usColorMapOrigin;
	UINT16	usColorMapLength;
	UINT8		ubColorMapEntrySize;
	UINT16	usOriginX;
	UINT16	usOriginY;
	UINT16	usImageWidth;
	UINT16	usImageHeight;
	UINT8		ubBitsPerPixel;
	UINT8		ubImageDescriptor;

} TARGA_HEADER;

#pragma pack (pop)


void SnapshotSmall(void)
{
	HVSURFACE backBuffer;
	ja2::presentation::PixelSurface* surface =
		GetVideoSurface(&backBuffer, BACKBUFFER) ?
		GetVideoSurfacePixelSurface(backBuffer) : NULL;
	if (surface == NULL ||
		surface->format() != ja2::presentation::PixelFormat::rgb565 ||
		surface->width() != SCREEN_WIDTH || surface->height() != SCREEN_HEIGHT ||
		giNumFrames < 0 || giNumFrames >= MAX_NUM_FRAMES ||
		gpFrameData[giNumFrames] == NULL)
	{
		return;
	}

	const ja2::presentation::ConstPixelBuffer pixels = surface->pixels();
	const std::size_t rowBytes =
		static_cast<std::size_t>(pixels.width) * sizeof(UINT16);
	UINT16* destination = gpFrameData[giNumFrames];
	for (UINT16 y = 0; y < pixels.height; ++y)
	{
		memcpy(destination + static_cast<std::size_t>(y) * pixels.width,
			pixels.pixels + static_cast<std::size_t>(y) * pixels.pitchBytes,
			rowBytes);
	}

	giNumFrames++;

	if ( giNumFrames == MAX_NUM_FRAMES )
	{
		RefreshMovieCache( );
	}
}


void VideoCaptureToggle(void)
{
#ifdef JA2TESTVERSION
	VideoMovieCapture( (BOOLEAN)!gfVideoCapture);
#endif
}

void VideoMovieCapture( BOOLEAN fEnable )
{
	INT32 cnt;

	gfVideoCapture=fEnable;
	if(fEnable)
	{
		for ( cnt = 0; cnt < MAX_NUM_FRAMES; cnt++ )
		{
			gpFrameData[ cnt ] = (UINT16 *)MemAlloc( SCREEN_WIDTH * SCREEN_HEIGHT * 2 );
		}

		giNumFrames = 0;

		guiLastFrame=Platform::GetClockMilliseconds();
	}
	else
	{
		RefreshMovieCache( );

		for ( cnt = 0; cnt < MAX_NUM_FRAMES; cnt++ )
		{
			if ( gpFrameData[ cnt ] != NULL )
			{
				MemFree( gpFrameData[ cnt ] );
			}
		}
		giNumFrames = 0;
	}
}

void RefreshMovieCache( )
{
	TARGA_HEADER Header;
	INT32 iCountX, iCountY;
	CHAR8 cFilename[_MAX_PATH];
	static UINT32 uiPicNum=0;
	UINT16 *pDest;
	INT32	cnt;
	PauseTime( TRUE );
	try
	{
	for ( cnt = 0; cnt < giNumFrames; cnt++ )
	{
		sprintf( cFilename, "JA%5.5d.TGA", uiPicNum++ );
		std::unique_ptr<ja2::fileio::File> output =
			ja2::fileio::storeRouter().create(cFilename);
		memset(&Header, 0, sizeof(TARGA_HEADER));

		Header.ubTargaType=2;			// Uncompressed 16/24/32 bit
		Header.usImageWidth=SCREEN_WIDTH;
		Header.usImageHeight=SCREEN_HEIGHT;
		Header.ubBitsPerPixel=16;
		output->writeExact(&Header, sizeof(TARGA_HEADER));
		pDest = gpFrameData[ cnt ];

		for(iCountY=SCREEN_HEIGHT-1; iCountY >=0 ; iCountY-=1)
		{
			for(iCountX=0; iCountX < SCREEN_WIDTH; iCountX ++ )
			{
				output->writeExact(pDest + (iCountY * SCREEN_WIDTH) + iCountX, sizeof(UINT16));
			}

		}
	}

	PauseTime( FALSE );

	giNumFrames = 0;
	}
	catch(std::exception& ex)
	{
		SGP_ERROR(ex.what());
	}
}
