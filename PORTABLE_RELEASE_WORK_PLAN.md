# Portable release work plan

This is the execution plan for turning the currently playable native Linux
port into a release candidate.  It is deliberately divided into research
packets that can be delegated without overlapping production edits, followed
by implementation batches owned and reconciled in one place.

The current compatibility contracts are:

- the Win32/Wine build remains the behavior and save-format reference;
- the native Linux ASan build is the memory-safety playtest target;
- confirmed platform-independent bugs are tracked in `UPSTREAM_FIXES.md` and
  kept separable enough to submit to base JA2 1.13;
- save files retain the legacy Win32 format;
- the x86 assembly blitters remain a frozen rendering oracle until the real
  workload corpus is green.

## How to delegate research

Each research task must return a small packet and should not edit production
code unless its assignment explicitly changes from research to implementation.
The packet must contain:

1. a minimal reproducer or deterministic fixture;
2. the exact call graph and state owners involved;
3. the invariant that is currently violated;
4. whether the defect exists in baseline 1.13 or was introduced by the port;
5. a proposed patch boundary, including files it would touch;
6. focused automated tests and a short live validation script;
7. known collision points with the other packets.

Avoid broad audits with no exit condition.  Record uncertain claims as
hypotheses and name the observation that would prove or disprove each one.

## Research packets

| ID | Research scope | Primary evidence and files | Required output | Depends on |
|---|---|---|---|---|
| R1 | Map-screen dialogue lock | `Dialogue Control`, tactical/map face handling, pause locks, battle-roster suppression | Reproducer matrix for speech/subtitles and screen transitions; single state owner proposal; stale-lock invariant | none |
| R2 | Pre-battle lifecycle hardening | `Strategic/PreBattle Interface.cpp`, map battle locator, transition buffers | Explicit inactive/preparing/presented/closing model or equivalent invariants; failure-path cleanup table | R1 for dialogue interaction |
| R3 | Popup and inventory-popup safety | popup box classes, `Tactical/Interface Items.cpp`, strategic inventory, XML parser | One finding per suspected lifetime/bounds/type issue, focused fixture design, patch order that avoids a mega-change | none |
| R4 | Native crash reporting | `sgp/crash_report_portable.cpp`, build IDs, packaging/debug symbols | Async-signal-safe raw report format, core-dump interaction, symbolization workflow, deliberate-crash test | none |
| R5 | Blitter call recorder and corpus | `PortableBlitterCore.h`, thin wrappers in `vobject_blitters_portable.cpp`, `tests/blitters/` | Versioned capture schema, operation coverage map, bounded capture policy, replay integration design | none |
| R6 | Full-game blitter performance | current x86 benchmark results, native profiler support, representative game scenes | Repeatable optimized-build protocol, counters/timings to collect, acceptable regression budget proposal | R5 |
| R7 | Weather and input stress | weather renderer, `PixelSurface`, SDL event coalescing | Sandstorm/rain/snow test matrix, frame/input-latency measurement, remaining hot-path attribution | none |
| R8 | Strategic route and battle-state validation | strategic paths, groups, map arrival and encounter state | Deterministic squad/vehicle/helicopter/militia scenarios and assertions for route lifetime and encounter preservation | none |
| R9 | File-I/O release qualification | `PORTABLE_FILE_IO_PLAN.md`, save corpus, resource profiles, durable operations | Missing matrix cells, failure-injection plan, per-purpose root policy, tactical scratch-transaction proposal | none |
| R10 | Multiplayer refinement | `MULTIPLAYER_PORT_PLAN.md`, SDL3_net wrapper, lobby/combat state | Reconnect/timeout/3-4-peer/content-sync test plan and versioned fixed-width wire migration boundary | none |
| R11 | Base-1.13 extraction | `UPSTREAM_FIXES.md`, clean upstream-fix branch | Map every confirmed base fix to a minimal upstream commit or record why it cannot yet be separated | ongoing implementation evidence |

## Implementation batches

Implementation starts only after the relevant packet names its invariant and
test.  A batch may absorb reality-driven changes, but its exit gate stays
fixed unless the reason is written here.

### Batch A: finish the active P0 runtime work

1. Fix the map-screen dialogue/pause-lock lifecycle from R1.
2. Harden pre-battle initialization and recovery from R2 without regressing
   the already-fixed dirty rectangle and active-battle recovery.
3. Run the R7 and R8 validation matrices; fix only demonstrated failures.

Exit gate: dialogue can cross tactical/map/pre-battle boundaries without
leaving controls locked, and route/battle/weather stress is ASan-clean.

### Batch B: popup and inventory hardening

Apply R3 as reviewable slices in this order:

1. immediate initialization, bounds, type, and lookup checks;
2. stable inventory references and callback typing;
3. popup graphics/resource lifecycle;
4. XML ownership and depth enforcement.

Exit gate: focused malformed-input, maximum-size, mutation, and repeated
open/close tests pass, followed by strategic and tactical inventory playtests.

### Batch C: native diagnostics

Implement R4 with a minimal fatal path and a separate normal-process
symbolizer/helper where useful.  Never allocate through the game allocator,
take engine locks, invoke UI, or perform network activity from a signal
handler.

Exit gate: deliberate crashes produce a local report with build identity,
signal, fault address, thread, and symbolizable frames in packaged and
developer builds.

### Batch D: real-game blitter corpus

Implement R5 before making further blitter optimizations.  The first version
is a developer-only, bounded recorder around the normalized portable blitter
dispatch.  It records replayable call state rather than merely taking screen
shots.

The capture format must be fixed-width, little-endian, versioned, and contain:

- operation/policy identity and all scalar arguments;
- selected ETRLE metadata and its exact compressed byte range;
- the active 256-entry palette and any alpha/Z-strip inputs;
- clip geometry, destination/Z pitch, coordinate parity, and initial touched
  buffer windows including canary margins;
- destination and Z results as capture-integrity oracles;
- current game screen, frame number, and a user-supplied capture label.

Recording is toggled by a global developer key command that works on the main
menu/options, laptop, strategic, and tactical screens.  A session records a
small number of completed frames, deduplicates equivalent call shapes, obeys
strict call/byte limits, buffers in memory, and writes atomically only after
capture stops.  The disabled path must be either compiled out or a predictable
single cheap branch; there must be no per-call file I/O.

The replay harness reconstructs pointer relationships from the serialized
data and runs the same fixture through generic reference, optimized portable,
and Win32 x86 assembly backends.  It compares return values, full captured
destination/Z windows, and canaries byte-for-byte.  Native post-state stored
in the capture verifies serializer integrity, but does not replace assembly as
the compatibility oracle.

Start with one representative capture from each of:

- main menu plus new-game/options controls;
- laptop AIM pages, inventory icons, and video/conference overlays;
- strategic map, merc/inventory panels, routes, weather, and pre-battle UI;
- tactical terrain with roofs, lighting, actors, items, cursors, smoke,
  explosion, and weather effects.

Exit gate: every exported blitter observed in captures agrees with assembly,
or has an individually documented and approved legacy defect exception.  The
corpus is deterministic, bounded in repository size, and replayable without
game data.

### Batch E: profiler-backed blitter gate

Use R6 and the captured corpus to report call counts, total cost, tail cost,
and frame times for stationary tactical rendering, scrolling, roofs/lights,
weather, smoke, and explosions.  Optimize only measured hot paths and rerun
the byte-equivalence gate after every path change.

Exit gate: the portable renderer is within the agreed full-frame budget and
has no correctness disagreement hidden by timing exclusions.

### Batch F: compatibility and product breadth

Run R9 before closing file I/O, then R10 for multiplayer.  These touch broad
state and are intentionally after the single-player runtime and renderer are
stable.  Extract R11 continuously so confirmed base fixes do not become tied
to the port.

Exit gate: the profile/resource/save matrix and fault tests pass; multiplayer
meets its explicitly chosen peer/version scope; upstream fixes are independently
reviewable.

## Collision map

The coordinating implementation owner should serialize edits in these areas:

- dialogue, map-screen input, and pre-battle state (R1/R2);
- popup classes, tactical item UI, and strategic inventory (R3);
- `PortableBlitterCore.h`, `vobject_blitters_portable.cpp`, and
  `tests/blitters/` (R5/R6);
- SDL global input commands and recorder hotkeys (R5/R7);
- root `CMakeLists.txt`, test registration, and build presets (R4/R5/R9/R10);
- save/serialization DTOs and multiplayer wire records (R9/R10).

Research agents should not reserve these files by making speculative cleanup
commits.  During implementation, rebase or cherry-pick one coherent slice at
a time, rerun the narrow tests, then run the shared native suite.

## Merge and playtest gate for every batch

1. Preserve unrelated work and start from a clean tracked worktree.
2. Add a focused regression test or a bounded debug invariant.
3. Build with 16 jobs and run the relevant focused tests.
4. Run the complete native suite; rerun network tests outside a restricted
   sandbox when local bind permissions are the only failure.
5. Run the Win32/Wine compatibility build when shared gameplay code changed.
6. Playtest the exact reproducer under native ASan.
7. Update `RUNTIME_HARDENING_TODO.md`, `UPSTREAM_FIXES.md`, and the applicable
   subsystem plan with evidence rather than only changing status words.
8. Commit a snapshot before the next collision-prone batch.

## Immediate order

The next implementation work after research packets land is R1/R2, then R3.
R4 and R5 can be prepared independently and integrated afterward.  R7/R8 are
available now as playtest work.  R6 waits for real captures, while R9/R10 are
release-qualification tracks rather than blockers for the current ASan bug
hunt.
