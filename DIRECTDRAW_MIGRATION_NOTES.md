# DirectDraw migration notes

This freezes the current renderer's ownership and behavior contracts before its
storage and presentation layers are replaced. It is an audit, not a claim that
the legacy COM lifetime is correct.

## Surface roles and ownership

`DirectDrawPresenter` now owns the DirectDraw device, cooperative/display mode,
primary surface, and windowed or fullscreen back buffer. The surface manager
owns any remaining logical-surface compatibility mirrors:

| Role | Creation/reference path | Current shutdown |
| --- | --- | --- |
| DirectDraw object | `DirectDrawPresenter::create` creates both interfaces | presenter releases both interfaces |
| Primary | presenter creates and retains both interfaces | presenter releases both interfaces |
| Back buffer, windowed | presenter creates and retains both interfaces | presenter releases both interfaces |
| Back buffer, fullscreen | presenter obtains the attached surface | presenter releases the attached reference |
| Frame buffer | its canonical PixelSurface has one ordinary system-memory compatibility mirror | released with the logical surface |
| Logical cursor | its canonical PixelSurface has one ordinary system-memory compatibility mirror | released with the logical surface |
| Cursor background | one shared project-owned `PixelSurface` used by both metadata slots | released automatically |
| Window clipper | local `clip`, attached to the windowed primary | no local release after attachment is visible |
| 8-bit palette | global `gpDirectDrawPalette` | no explicit release is visible |

Generic `SGPVSurface` objects use the same two-reference convention in
`pSurfaceData1`/`pSurfaceData`. Video-memory surfaces also own a system-memory
backup in `pSavedSurfaceData1`/`pSavedSurfaceData`. Unlike reserved surfaces,
`DeleteVideoSurface` releases both interfaces and the backup pair through
`DDReleaseSurface`.

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
- `UpdateBackupSurface` copies primary surface data into its backup.
  `RestoreVideoSurface` restores the allocation and copies backup data into the
  primary. The previously reversed arguments were corrected and the direction
  is locked down by the golden composition/restore test.
- Lost-surface recovery restores reserved presentation/cursor surfaces, marks
  frame and cursor state dirty, and forces a full refresh. Generic video-memory
  surfaces restore their pixels from system-memory backups.
- Windows GDI WinFont rendering remains available only through the Windows
  backend. Portable builds deliberately select JA2 bitmap fonts.

## Conversion status

Explicit system-memory `SGPVSurface` objects now use `PixelSurface` as their
canonical pixel storage. Lock/unlock, fills, same-format copies, color keys,
palettes, and nearest-neighbor stretch operate on that storage. Their existing
DirectDraw allocation is retained temporarily as a lazy compatibility mirror:
portable-to-portable operations never cross it, while mixed blits synchronize
only the participant that changed. Windows WinFont similarly synchronizes before
acquiring a GDI context and marks the mirror authoritative after drawing.

This makes a useful generic-surface slice portable without breaking the many
copies between generic and reserved surfaces. Default/video-memory generic
surfaces and the reserved primary surface remain DirectDraw owned. The reserved
frame buffer, back buffer, and logical cursor buffer are now
PixelSurface-canonical. Frame refresh, tactical scrolling in all eight
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
The compatibility mirrors can be deleted after WinFont, remaining mixed paths,
and default/video-memory generic surfaces have portable owners.

## Replacement sequence

1. Give every generic and reserved logical surface one project-owned storage
   representation; preserve existing numeric handles and lock/pitch behavior.
   Explicit system-memory generic surfaces plus the reserved frame, back, and
   cursor buffers are complete behind a lazy mirror.
2. Route fills and cross-surface copies through it. The lazy mirror keeps mixed
   generic/reserved copies valid while the reserved surfaces are converted.
3. Keep the existing software blitters, including `vobject_blitters.cpp`, on
   their raw pixel-buffer interface.
4. Completed: cursor save/compose/restore operates on the project-owned back
   buffer and a shared portable background surface.
5. Completed: final upload, windowed blit, fullscreen flip, and presentation
   recovery live behind `Presenter`; Windows/cnc-ddraw remains the runnable
   comparison oracle until the SDL presenter is equivalent.
6. Add an SDL host/event adapter only after surface ownership no longer depends
   on DirectDraw. SDL types stay out of engine-facing headers.

The replacement shutdown invariant is strict: stop presentation, destroy
cursor/font consumers, destroy logical surfaces and palettes, then destroy the
presenter/window. Every resource has one owner and repeated shutdown is harmless.
