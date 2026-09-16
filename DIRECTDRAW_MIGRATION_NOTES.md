# DirectDraw migration notes

This freezes the current renderer's ownership and behavior contracts before its
storage and presentation layers are replaced. It is an audit, not a claim that
the legacy COM lifetime is correct.

## Surface roles and ownership

`sgp/video.cpp` creates the reserved renderer surfaces directly:

| Role | Creation/reference path | Current shutdown |
| --- | --- | --- |
| DirectDraw object | `DirectDrawCreate` gives `_gpDirectDrawObject`; `QueryInterface` gives `gpDirectDrawObject` | releases only `gpDirectDrawObject` |
| Primary | `CreateSurface` gives `_gpPrimarySurface`; `QueryInterface` gives `gpPrimarySurface` | releases only `gpPrimarySurface` |
| Back buffer, windowed | `CreateSurface` gives `_gpBackBuffer`; `QueryInterface` gives `gpBackBuffer` | releases only `gpBackBuffer` |
| Back buffer, fullscreen | `GetAttachedSurface` gives `gpBackBuffer` | releases `gpBackBuffer` |
| Frame buffer | `CreateSurface` gives `_gpFrameBuffer`; `QueryInterface` gives `gpFrameBuffer` | neither reference is visibly released |
| Cursor and cursor original | each has a creator-side `_gp*` reference and a queried `gp*` reference | releases only queried references |
| Cursor background 0 | `_pSurface` creator reference plus queried `pSurface` | releases only queried reference |
| Window clipper | local `clip`, attached to the windowed primary | no local release after attachment is visible |
| 8-bit palette | global `gpDirectDrawPalette` | no explicit release is visible |

Generic `SGPVSurface` objects use the same two-reference convention in
`pSurfaceData1`/`pSurfaceData`. Video-memory surfaces also own a system-memory
backup in `pSavedSurfaceData1`/`pSavedSurfaceData`. Unlike reserved surfaces,
`DeleteVideoSurface` releases both interfaces and the backup pair through
`DDReleaseSurface`.

The missing releases above should not be copied into the portable renderer.
They should also not be repaired casually in the DirectDraw oracle: attached
surfaces, palettes, clippers, queried interfaces, and creator references have
different COM ownership rules, and cnc-ddraw behavior must remain testable while
the replacement is developed.

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

## Replacement sequence

1. Give every generic and reserved logical surface one project-owned storage
   representation; preserve existing numeric handles and lock/pitch behavior.
2. Route fills and cross-surface copies through it. Convert generic and reserved
   surfaces together because today they blit between each other.
3. Keep the existing software blitters, including `vobject_blitters.cpp`, on
   their raw pixel-buffer interface.
4. Make cursor save/compose/restore operate on the project-owned back buffer.
5. Implement final upload behind `Presenter`; keep Windows/cnc-ddraw as the
   runnable comparison oracle until the SDL presenter is equivalent.
6. Add an SDL host/event adapter only after surface ownership no longer depends
   on DirectDraw. SDL types stay out of engine-facing headers.

The replacement shutdown invariant is strict: stop presentation, destroy
cursor/font consumers, destroy logical surfaces and palettes, then destroy the
presenter/window. Every resource has one owner and repeated shutdown is harmless.
