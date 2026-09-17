#ifndef __VSURFACE_PRIVATE_
#define __VSURFACE_PRIVATE_

// ***********************************************************************
// 
// PRIVATE, INTERNAL Header used by other SGP Internal modules
//
// Allows direct access to underlying Direct Draw Implementation
//
// ***********************************************************************

ja2::presentation::PixelSurface *GetVideoSurfacePixelSurface( HVSURFACE hVSurface );
void NotifyVideoSurfacePixelModified( HVSURFACE hVSurface );

HVSURFACE CreateVideoSurfaceFromDDSurface( LPDIRECTDRAWSURFACE2 lpDDSurface );

#endif
