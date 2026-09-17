# DirectDraw migration notes

This freezes the current renderer's ownership and behavior contracts before its
storage and presentation layers are replaced. It is an audit, not a claim that
the legacy COM lifetime is correct.

## Surface roles and ownership

`DirectDrawPresenter` now owns the DirectDraw device, cooperative/display mode,
primary surface, and windowed or fullscreen back buffer. The logical surface
manager owns only project-memory `PixelSurface` objects:

| Role | Creation/reference path | Current shutdown |
| --- | --- | --- |
| DirectDraw object | `DirectDrawPresenter::create` creates both interfaces | presenter releases both interfaces |
| Primary | presenter creates and retains both interfaces | presenter releases both interfaces |
| Back buffer, windowed | presenter creates and retains both interfaces | presenter releases both interfaces |
| Back buffer, fullscreen | presenter obtains the attached surface | presenter releases the attached reference |
| Logical primary/back/frame surfaces | project-owned `PixelSurface` objects | released with the logical surfaces |
| Frame buffer | project-owned `PixelSurface`; no eager DirectDraw allocation | released with the logical surface |
| Logical cursor | project-owned `PixelSurface`; no eager DirectDraw allocation | released with the logical surface |
| Cursor background | one shared project-owned `PixelSurface` used by both metadata slots | released automatically |
| Window clipper | local `clip`, attached to the windowed primary | no local release after attachment is visible |
| 8-bit presentation palette | created and attached by `DirectDrawPresenter` | presenter releases its reference; attached surfaces release theirs |

`SGPVSurface` no longer contains DirectDraw pointers, palettes, clippers, dirty
mirror state, or video-memory backup ownership. The obsolete shared DirectDraw
wrapper is no longer part of the build. DirectDraw ownership is confined to
the Windows presenter. The video manager owns only the neutral `Presenter`
interface; a Windows factory performs concrete DirectDraw construction, and
the shared palette contract no longer includes `ddraw.h`.

The remaining palette ownership should not be copied into the portable
renderer. Attached surfaces, palettes, clippers, queried interfaces, and
creator references have different COM ownership rules, and cnc-ddraw behavior
must remain testable while the replacement is developed.

## Behavioral invariants

- The engine draws into indexed-8 or RGB565 logical surfaces and obeys the lock
  pitch; width is never a row-stride substitute.
- Rectangle operations are half-open at the portable surface boundary.
- Source and destination color keys, clipped blits, overlap-safe self-blits,
  fills, and nearest-neighbor stretching are preserved by `PixelSurface` tests.
- The frame buffer is copied to the back buffer using either a full refresh or
  dirty-region lists. Cursor composition occurs on the back buffer, with the
  previous background restored before the new cursor is drawn.
- Lost-surface recovery restores presenter-owned surfaces, marks frame and
  cursor state dirty, and forces a full refresh. Engine-created logical
  surfaces retain their pixels independently and need no DirectDraw backup.
- Windows GDI WinFont rendering remains available only through the Windows
  backend. Portable builds deliberately select JA2 bitmap fonts.

## Conversion status

All engine-created `SGPVSurface` objects now use `PixelSurface` as their
canonical pixel storage, regardless of their legacy memory-placement request.
Lock/unlock, fills, same-format copies, color keys, palettes, and
nearest-neighbor stretch operate on that storage. They no longer allocate a
DirectDraw surface or video-memory backup at creation. Windows WinFont renders
through a GDI DIB and copies directly to canonical storage, so
it no longer requests a DirectDraw surface or exposes native drawing authority.

All generic and reserved logical surfaces, including primary, back, frame, and
cursor handles, are now PixelSurface-canonical. Frame
refresh, tactical scrolling in all eight
directions, overlays, fades, rain, and cursor save/compose/restore operate on
portable storage. Unkeyed copies use row memcpy and overlap-safe directional
memmove, avoiding per-pixel full-frame and scrolling costs. DirectDraw receives
the completed back buffer only through the Windows `DirectDrawPresenter`. That
adapter owns framebuffer upload, the windowed primary blit, fullscreen flip,
retry handling, and reserved presentation-surface recovery. Host submissions
are coalesced to the legacy 16 ms cadence without sleeping the game thread,
preserving blocking transition animations without flooding the wrapper. The
old fullscreen primary-to-back-buffer recovery copies are gone: the canonical
PixelSurface retains the complete frame and is uploaded before each flip.
Screenshot and optional movie-frame capture also read this canonical storage
instead of locking or reading back DirectDraw surfaces.
`sgp/video.cpp` and `sgp/WinFont.cpp` now contain no DirectDraw API calls:
device creation, mode and surface queries, palette attachment, upload, present,
suspend/resume, and shutdown are presenter operations. No native DirectDraw
surface accessor remains in the video or logical-surface interfaces.

## Replacement sequence

1. Completed: every generic and reserved logical surface has one project-owned
   storage representation while preserving numeric handles and lock/pitch
   behavior.
2. Completed: fills, cross-surface copies, color keys, palettes, and stretching
   use portable storage; the mixed DirectDraw mirror path is gone.
3. Keep the existing software blitters, including `vobject_blitters.cpp`, on
   their raw pixel-buffer interface.
4. Completed: cursor save/compose/restore operates on the project-owned back
   buffer and a shared portable background surface.
5. Completed: final upload, windowed blit, fullscreen flip, and presentation
   recovery live behind `Presenter`; Windows/cnc-ddraw remains the runnable
   comparison oracle until the SDL presenter is equivalent.
6. Started: the native `Sdl3Presenter` uploads the retained RGB565 framebuffer
   to a streaming texture, applies dirty-region updates, and uses SDL logical
   letterboxing with nearest-neighbor scaling. A dummy-video test covers full
   and partial upload, suspend/resume, and format rejection. The native SDL
   application host now owns subsystem/window lifetime and maps one waited
   event per scheduler iteration into dispatch, quit, or deadline results.
   SDL types remain confined to backend headers. A tested SDL input translator
   converts letterboxed pointer coordinates and maps keyboard, mouse, wheel,
   and focus events into the existing logical/Win32-compatible input
   vocabulary. A concrete native-tested sink now delivers that vocabulary to
   the existing input manager, maintains physical-key state, and synthesizes
   releases on focus loss. Selecting the SDL host for the production game loop
   is still pending.
7. Completed: Win32 class/window creation and DirectDraw presenter construction
   live in `platform/windows/VideoBootstrap.cpp`. The common `video.cpp` accepts
   an already-created presenter, contains no Win32 types or APIs, and is built
   by GCC and Clang as a strict native object target. SDL production bootstrap
   and full-game linkage remain pending.

The replacement shutdown invariant is strict: stop presentation, destroy
cursor/font consumers, destroy logical surfaces and palettes, then destroy the
presenter/window. Every resource has one owner and repeated shutdown is harmless.
