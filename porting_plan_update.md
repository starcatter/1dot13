# JA2 1.13 Porting Plan Update

Date: 2026-09-15

## Executive Status

The portable file-I/O update at commit `cf8b80a7a` completes a major prerequisite for both a native Linux port and a maintainable modern-Windows build. It replaces the engine's most pervasive storage coupling with project-owned C++17 interfaces while retaining bfVFS as the compatibility-tested resource backend. It also removes the pointer-to-32-bit-file-handle design, makes writable paths explicit and confined, and gives save publication recoverable transaction semantics.

This is an important architectural milestone, but it is not yet a native Linux port. The production game target still accepts only `JA2_PLATFORM_BACKEND=WINDOWS`, requires a Windows target, uses a Windows GUI entry point, and links DirectDraw, WinMM, WinHTTP, and proprietary FMOD 3.75 components. The currently validated product remains a 32-bit Windows executable built with MSVC and exercised under Wine.

In roadmap terms, the update moves the project from "stabilize and reproduce the legacy Windows program" into "extract replaceable platform backends." File and resource access is now sufficiently isolated that subsequent work does not need to solve storage portability at the same time as windowing, rendering, input, or audio. That substantially reduces the risk and scope of each later migration.

## Platform-Core Progress: Monotonic Timing

The engine clock-source migration is now implemented as the first platform-core extraction. A project-owned C++17 monotonic clock wraps `std::chrono::steady_clock` and supplies process-relative microsecond, 64-bit millisecond, and legacy wrapping 32-bit millisecond readings. Runtime users of `GetTickCount`, QueryPerformanceCounter, the window `SetTimer` clock manager, and the dormant WinMM callback-timer path now use that clock. The fixed-step synthetic JA2 clocks, pause behavior, configurable clock speed, fast-forward cadence, and notification behavior remain engine policy rather than properties of the host clock.

Native timing tests cover monotonicity, unit coherence, elapsed-time accuracy, 32-bit deadline wraparound, and the legacy clock-manager/countdown contract. The same tests build for Windows x86 and compare the replacement directly with QueryPerformanceCounter and GetTickCount64. The Windows executable also builds with the replacement wired in.

Thread creation, events, waits, and critical sections in `Utils/Timer Control.cpp` deliberately remain for the next platform-core step. They are the delivery mechanism for timer ticks, not a clock source. Windows' short-sleep resolution hint is isolated behind a platform service so the current executable retains its pacing while the thread/wait loop is replaced.

Stracciatella provides a useful independent design check. Its current timer control also uses `std::chrono::steady_clock`; after first replacing decrementing counters with steady-clock deadlines, it removed its SDL timer as inaccurate overhead and advances the JA2 clock from the main loop. That validates the selected clock source and argues against mechanically translating the current Win32 timer and notify threads to `std::thread`. The next synchronization step should instead move tick delivery onto a portable main-loop scheduler, then delete those threads, events, callback lists, game-loop critical section, and the temporary Windows sleep-resolution hint together. This branch must retain the 1.13-specific fixed-step, clock-speed, fast-forward, pause, and notification semantics while doing so; Stracciatella's simpler elapsed-millisecond policy is not a drop-in replacement for them.

## Position Against the Portable-I/O Plan

### Completed Migration Boundary

The implementation satisfies the intended active-runtime scope of Stages 1, 2, 4, 5, and 6 in `PORTABLE_FILE_IO_PLAN.md`; Stage 3 save publication is complete while tactical scratch extraction remains compatibility-sensitive follow-up work:

- Stage 1 introduced project-owned file, metadata, enumeration, resource-store, and writable-store types. Legacy `HWFILE` values are generation-checked registry tokens rather than truncated pointers, and file ownership is explicit.
- Stage 2 separated read-only resource lookup from writable physical storage. `StoreRouter` and `FileServices` route saves, user data, logs, screenshots/exports, and scratch paths while retaining bfVFS profile and archive behavior for resources.
- Stage 3 added journaled, checksummed save transactions. Save files and optional `.IPQ` sidecars are published as a recoverable unit, stale sidecars are removed transactionally, and startup recovery runs before save discovery. Tactical embedded-file extraction retains its legacy operation order for compatibility and is not yet transactionally hardened.
- Stage 4 moved maintained runtime host-file operations behind portable services. Metadata, enumeration, standard-stream escapes, log output, path lookup, and active runtime file consumers no longer need to expose Win32 file APIs.
- Stage 5 moved active direct bfVFS file/profile consumers behind the resource adapter while preserving named profiles, merged INIs, overlays, SLF archives, and JPC/7z access.
- Stage 6 retained bfVFS behind the new abstraction. This is the conservative choice until the complete resource compatibility matrix justifies replacing any provider.

The implementation is broad rather than cosmetic: the commit changes 95 files, adds the portable service implementation and four native CTest executables, and migrates save/load, configuration, editor support, multiplayer profile paths, XML, Lua, image/resource iteration, logging, screenshots, and crash telemetry consumers.

### Validation Still Outstanding

Stage 0 remains an open release-quality validation program rather than an implementation blocker. The following evidence is still needed before treating the portable-I/O boundary as fully release-qualified:

- A fixture matrix covering JA2113, Vanilla, and UB profile precedence, named profiles, overlays, SLF, and supported JPC/7z behavior.
- A corpus of existing save files plus byte-identity checks for deterministic save, `.IPQ`, tactical scratch, laptop scratch, EDT, screenshot, and settings formats.
- Failure-injection coverage for all important save serialization, sync, journal, backup, and publication boundaries.
- Long runtime playthroughs covering new game, load, save, quicksave, quickload, tactical transitions, and clean shutdown.
- Native modern-Windows execution in addition to Wine execution.
- Product policy for independently configurable save, settings, log, screenshot/export, and scratch roots. They are logically distinct now but still share the active writable profile physically.

The distinction matters: the architecture is ready to support the next porting stages, but compatibility validation must continue in parallel so later backend work does not hide file/resource regressions.

## What This Unlocks

### Native Linux

The new boundary eliminates the need for a Linux port to reproduce Win32 file handles, Win32 directory enumeration, Windows timestamps, or bfVFS writes throughout gameplay code. A Linux backend can implement physical writable files, durable replacement, local-time conversion, and platform paths in a small set of files while resource consumers continue to use the same project interfaces.

It also provides a useful native-build foothold. File-I/O tests already compile and run as host-native C++17 programs even though the game cannot yet configure as a Linux target. This proves that part of the engine has escaped the global Windows build assumptions and establishes a pattern for extracting and testing the next platform-neutral layers.

### Modern Windows

The update removes several historical correctness and 64-bit hazards independently of Linux:

- No file-object pointer is packed into a 32-bit integer handle.
- Physical paths can use native wide Windows paths rather than depending on the process ANSI code page.
- Writable paths are confined against traversal, rooted/device paths, alternate data streams, and symlink or junction escape.
- Enumerations own their state instead of sharing one process-global iterator.
- Saves have explicit recovery after interrupted multi-file publication.
- Maintained runtime consumers no longer scatter Win32 file APIs through gameplay subsystems.

These changes make a future x64 Windows target more plausible, although they do not make the entire engine 64-bit clean. Rendering code, assembly blitters, serialization layouts, pointer/integer assumptions, third-party binary interfaces, and ABI-sensitive save structures still require separate treatment.

## Overall Porting Roadmap

### Foundation Already Established

- A pinned, source-built dependency baseline exists for Expat, zlib, libpng, libsmacker, LZMA SDK, Lua, utf8cpp, and bfVFS.
- Legacy Bink support and the obsolete prebuilt RakNet dependency have been removed; networking is deliberately disabled pending a new protocol/backend.
- A reproducible x86 MSVC build can be driven from Linux and validated under an isolated Wine runtime.
- Portable sleep and file-I/O seams demonstrate the intended extraction approach: narrow project interfaces, retained Windows behavior, native tests, then an alternate backend.

### Next Phase: Define the Platform Core

The next practical step is to inventory and extract the non-rendering platform services required before the game loop can compile natively. Expected boundaries include:

- Process entry, executable/base/user-directory discovery, command-line handling, restart, and single-instance behavior.
- Monotonic clocks, wall-clock time, timers, sleeping, and event-loop scheduling.
- Window creation, message/event pumping, cursor and focus state, keyboard, mouse, and text input.
- Dynamic-library loading and the narrow crash-reporting facilities that must remain platform-specific.
- Threading and synchronization calls that still expose Windows types or semantics.

Each boundary should retain the working Windows implementation and gain native unit or smoke tests before an SDL/Linux implementation is selected.

### Rendering and Presentation

DirectDraw and the current Windows presentation path remain central blockers. The lowest-risk route is to preserve software rendering and the game's existing 16-bit surfaces/blitters initially, then replace only presentation and window/event integration with SDL. This avoids combining a platform port with a rewrite of gameplay rendering semantics.

The project should keep `vobject_blitters.cpp` and other behavior-sensitive software blitters during this phase. SDL should first upload or present the existing framebuffer, handle scaling/fullscreen/window management, and translate input. cnc-ddraw remains useful for the Windows backend until that SDL path reaches behavioral parity.

### Audio

FMOD 3.75 is the largest remaining proprietary runtime dependency and is distributed as 32-bit Windows binaries. Audio needs a project-owned mixer/playback interface before Linux or x64 Windows can be first-class targets. The migration must characterize sample loading, channel allocation, looping, panning, volume, streaming music, callbacks, and timing assumptions before introducing SDL audio, OpenAL, miniaudio, or another maintained implementation.

This should be separated from rendering work. A temporary no-audio backend may help achieve an early native executable, but it is not a playtest-ready milestone because audio timing and callbacks can affect game behavior.

### Native Build Enablement

Once enough platform interfaces exist, CMake can add a `LINUX` backend instead of rejecting every non-Windows target. That step includes:

- Replacing unconditional MSVC flags and Windows libraries with compiler- and backend-specific target settings.
- Splitting Windows resource compilation and `WIN32` executable behavior from shared engine sources.
- Auditing public headers for `windows.h`, Windows scalar/handle types, calling conventions, and preprocessor-selected structures.
- Porting or conditionally isolating MSVC inline assembly and architecture-specific code.
- Building a headless or minimally presented native game target before demanding full feature parity.

### Architecture Modernization

Native Linux does not inherently require x64, but modern Linux distributions and long-term modern-Windows support make 64-bit a separate high-priority track. It should follow characterization of ABI-sensitive saves and network/data structures rather than being folded invisibly into the first native build.

The 64-bit track needs explicit checks for pointer truncation, `long`/`DWORD` assumptions, structure packing, format strings, inline assembly, SIMD/blitter assumptions, serialized raw structures, and third-party APIs. Existing saves must continue to load, which may require fixed-width disk structures and conversion at serialization boundaries rather than changing in-memory types wholesale.

## Recommended Milestones

1. Qualify the portable-I/O checkpoint with the missing profile/archive/save fixtures and native Windows runtime pass.
2. Produce a measured inventory of remaining Win32 API and Windows-type dependencies, grouped by platform service rather than raw occurrence count.
3. Extract platform core services while keeping the Windows/Wine baseline green.
4. Introduce SDL window, event, input, and software-framebuffer presentation behind selectable backends.
5. Replace FMOD through a characterized project audio interface.
6. Configure and link a native Linux game executable, initially preserving x86-sensitive engine behavior where practical.
7. Reach a Linux playtest gate: startup, menus, new game, tactical play, save/load, audio, video, input, and clean shutdown with real data.
8. Run the x64 modernization as an explicit compatibility project for Linux and Windows.
9. Restore optional products and services: map editor, UB variants, exporter, crash tooling, and eventually networking.

## Current Bottom Line

Portable I/O was a foundational dependency, not the final port. Its completion means storage, resources, saves, configuration, and most runtime-generated files no longer dictate a Windows-only architecture. The project can now attack platform lifecycle and multimedia backends in bounded pieces while retaining a working compatibility reference.

The nearest meaningful product milestone is therefore not "Linux complete" but "the shared engine configures natively with selectable platform services and reaches the existing software framebuffer without Win32." After that, SDL presentation/input and an FMOD replacement are the two principal gates to a usable native Linux build. For modern Windows, the immediate benefits are safer I/O and a more maintainable build; native x64 remains a later, explicit compatibility effort.

---

# Appendix A: Detailed Source-Level Portability Assessment

## Audit Scope and Interpretation

This appendix records a second-pass source audit performed after the initial roadmap assessment was saved. It examines the current `cf8b80a7a` tree rather than inferring progress solely from the file-I/O plan. The audit covers build gating, public type contamination, application hosting, timing and synchronization, rendering, input, audio, video, crash support, encoding, x64 hazards, serialization, dependencies, CI, and test evidence.

The terms below are used deliberately:

- **Portable subsystem** means the public contract can support another operating-system implementation. It does not necessarily mean that implementation exists yet.
- **Linux-ready** means code can compile and behave correctly as part of a native Linux game target, not merely as a standalone native test.
- **Modern Windows x86** means the current 32-bit executable running on a supported contemporary Windows system, potentially through cnc-ddraw.
- **Modern Windows x64** means a native 64-bit process. It is a separate compatibility project, not an automatic consequence of using a recent compiler.

## Capability Snapshot

| Capability | Current state | Meaning for the port |
|---|---|---|
| Dependency acquisition | Major open-source dependencies are pinned and source-built | Strong foundation for repeatable Windows and future Linux builds |
| Windows x86 compilation | Supported with MSVC; Wine-hosted toolchain documented | Working regression reference |
| Windows x86 runtime | Smoke-tested under Wine before the final commit boundary | Useful behavioral oracle, but exact-commit and native-Windows validation remain |
| Portable file contracts | Implemented | Callers no longer require Win32 file handles or writable bfVFS access |
| Native file tests | Four executables build and pass on Linux | Proven host-native island, not yet integrated into root CI |
| Linux game configuration | Explicitly rejected | No native game target yet |
| Window/event backend | Win32 only | Hard Linux blocker |
| Rendering/presentation | DirectDraw 2 plus cnc-ddraw | Works as a legacy compatibility path; no native Linux renderer |
| Input backend | Win32 mouse hook/messages and direct cursor polling | Hard Linux blocker, although the engine event queue is reusable |
| Audio backend | FMOD 3.75, 32-bit proprietary DLL | Hard Linux and Windows-x64 blocker |
| Cinematic decoder | Source-built libsmacker | Decoder is portable; final video/audio backends are not |
| Linux durable save backend | Not implemented | Save transactions cannot yet run correctly in a native game |
| Windows x64 | Unsupported | Assembly, pointers, serialization, and binary dependencies block it |
| Multiplayer | Deliberately disabled | Not a compile blocker; feature restoration is later work |

## Portable-I/O Implementation: Exact Position

### Proven Architecture

The new public file contract in `sgp/fileio/FileIO.h:13-111` uses project-owned errors, metadata, entries, streams, resource stores, and writable stores. It exposes neither Win32 handles nor bfVFS classes. `sgp/fileio/BfVfsResourceStore.h:16-67` hides bfVFS behind a PIMPL, while `sgp/FileMan.cpp:61-171` turns legacy 32-bit file handles into slot/generation tokens with stale-handle rejection.

`StoreRouter` provides the compatibility bridge between writable data and resource overlays. Its namespace and precedence handling is implemented in `sgp/fileio/StoreRouter.cpp:114-195` and `sgp/fileio/StoreRouter.cpp:255-350`. Default exclusive roots cover `Temp`, `ShadeTables`, `SavedGames`, and `MP_SavedGames`, with localized save paths supplied during initialization in `sgp/sgp.cpp:455-464`.

The physical store validates names and confinement in `sgp/fileio/PhysicalWritableStore.cpp:49-149`. It rejects rooted and traversing paths, control characters, Windows device names, alternate-data-stream syntax, and existing symlink/reparse components before checking canonical containment. This is valuable on both operating systems and is a direct modern-Windows hardening improvement.

Save transactions are materially stronger than simple temporary-file replacement:

- `sgp/fileio/DurableFileOperations.h:8-20` defines the durability boundary.
- `sgp/fileio/SaveTransaction.cpp:18-58` fixes journal versions, record kinds, and size limits.
- `sgp/fileio/SaveTransaction.cpp:180-205` length-frames and checksums records.
- `sgp/fileio/SaveTransaction.cpp:355-400` writes intent/completion records and assigns unique stage, backup, and journal names.
- `sgp/fileio/SaveTransaction.cpp:556-597` publishes the optional sidecar before the save, making the save the commit point.
- `sgp/fileio/SaveTransaction.cpp:421-529` performs idempotent commit-or-rollback recovery.
- `Ja2/SaveLoadGame.cpp:4480-4523` preserves the existing serializer through a borrowed transaction file handle.
- `Ja2/SaveLoadGame.cpp:7289-7359` restores embedded tactical and laptop scratch payloads through the legacy adapter. The router now accepts legacy doubled separators such as `Temp\\\\i_A9.DAT`, but transactional scratch replacement remains outstanding.

This means later platform work should implement `DurableFileOperations`, not redesign save publication or touch save serialization at the same time.

### Linux Implementations Inside the I/O Boundary

The first three Linux implementations below were completed immediately after the file-I/O
checkpoint; the remaining compatibility work is still open:

1. `PosixDurableFileOperations` implements file `fsync`, atomic no-clobber same-directory publication, and containing-directory `fsync`. Save code selects it or the retained Windows backend through `makeDurableFileOperations`.
2. `PhysicalWritableStore::metadata` returns Unix-nanosecond modification timestamps on POSIX as well as Windows.
3. Linux `executableDirectory()` resolves `/proc/self/exe` independently of the working directory, retaining the previous current-directory behavior only as a restricted-environment fallback.
4. Physical writable lookup uses host-native case behavior while wildcard matching is explicitly ASCII case-insensitive (`sgp/fileio/PhysicalWritableStore.cpp:151-195`, `460-474`, and `516-522`). Case collisions and compatibility expectations need fixtures before Linux deployment on case-sensitive filesystems.
5. No native test currently links the real `BfVfsResourceStore`, SLF provider, or JPC/7z path. The native tests validate the physical/router half, not the complete resource stack.
6. Equipment-template filename conversion still calls `WideCharToMultiByte` in `Tactical/Handle Items.cpp:124-137`; it belongs in the wider text-encoding migration.

### Intentional Compatibility Debt

The following limitations are acceptable at this checkpoint but should remain visible:

- All logical writable purposes still share the active bfVFS writable-profile root (`sgp/fileio/FileServices.cpp:20-34`). Independent user/config/cache/state paths remain product policy work.
- bfVFS repeated opens intentionally preserve cached/shared-object behavior. Adapter accounting cannot see every direct bfVFS lease (`sgp/fileio/BfVfsResourceStore.h:39-42`).
- Profile mutation rejects live tracked leases rather than draining them (`sgp/fileio/BfVfsResourceStore.cpp:293-323` and `346-362`).
- Legacy `FileOpen` still preserves used legacy behavior instead of fully honoring every declared disposition flag (`sgp/FileMan.cpp:495-519`).
- Legacy position and size APIs retain signed-32-bit or `UINT32` limits (`sgp/FileMan.cpp:875-995` and `1160-1172`).
- The standard-library confinement sequence has a documented check/use race (`sgp/fileio/PhysicalWritableStore.h:10-14`), and generic exclusive creation/replacement is not guaranteed atomic (`sgp/fileio/PhysicalWritableStore.cpp:485-495` and `687-739`). Save transactions avoid depending on those generic guarantees by using their durability backend.
- Crash-safe output intentionally remains low-level Win32 (`sgp/crash_report.cpp:102-243`). That code needs a separate crash backend rather than conversion to ordinary file services.

## Native Linux Blocker Map

### Build System

The root build stops native work before compilation:

- `CMakeLists.txt:11-19` accepts only `JA2_PLATFORM_BACKEND=WINDOWS` and requires `WIN32`.
- `CMakeLists.txt:208-213` creates a Windows GUI executable and compiles `Ja2/Res/ja2.rc` unconditionally.
- `CMakeLists.txt:142-149` links WinMM and WinHTTP unconditionally.
- `CMakeLists.txt:232-234` links DirectDraw and `fmodvc.lib` unconditionally.
- `/Oy-` at `CMakeLists.txt:54-55` and `/w14062` at `CMakeLists.txt:221` are not conditioned as generic compiler-independent settings.
- `cmake/Warnings.cmake:15-19` and `cmake/AddressSanitizer.cmake:28-32` treat generic Clang as though it were always clang-cl and select Windows-style options/libraries.
- `CMakePresets.json:14-78` contains Windows x86 MSVC/clang-cl configurations only.

The first native-build patch should not try to make all engine sources compile. It should introduce backend-specific source groups, libraries, flags, executable resources, and tools, then create a controlled Linux target that can expose the next class of errors.

### Fundamental Types and Header Fan-Out

The most consequential type problems are concentrated in `sgp/types.h`:

- `sgp/types.h:33-35` uses MSVC-specific `__int64`.
- `sgp/types.h:48` defines `CHAR16` as `wchar_t`. Windows `wchar_t` is 16-bit; normal Linux `wchar_t` is 32-bit. This affects memory layout, text behavior, Lua string storage (`lua/lwstring.h:22-25`), and serialized data such as `Ja2/SaveLoadGame.cpp:1231`.
- `sgp/types.h:55` defines `FLAGS32` as `unsigned long`, which is 32-bit on Windows LLP64 and normally 64-bit on Linux LP64.
- `sgp/FileMan.h:120` exposes `_cdecl`.

`-fshort-wchar` would be a poor shortcut because it makes the program disagree with the platform C/C++ wide-character ABI. The durable solution is an explicit 16-bit engine/serialized character type plus conversion at OS, library, and UI boundaries.

Windows headers still spread through the engine. `sgp/video.h:4-6` includes `windows.h`, DirectDraw, and `process.h`, then `sgp/sgp.h:9` includes `video.h`. `sgp.h` is included at roughly 163 sites, so one presentation header acts as broad Windows contamination. Representative public leaks include `HWND`/`HINSTANCE` in `sgp/video.h:24-35`, `LOGFONT` in `sgp/WinFont.h:3-11`, `POINT` in `Strategic/Map Screen Interface Map.h:423`, and `BOOL` in `sgp/line.h:64-66`.

The next header cleanup should split common engine declarations from platform/window/video implementation declarations. Existing project types such as `SGPPoint`, `SGPRect`, `BOOLEAN`, and fixed-width integer aliases provide replacement vocabulary.

### Application Host and Event Loop

`sgp/sgp.cpp` is still the complete Windows application host:

- `sgp/sgp.cpp:705` defines `WinMain`.
- `sgp/sgp.cpp:176-369` handles focus, activation, input, rendering restoration, timers, shutdown, and other window messages in one WndProc.
- `sgp/sgp.cpp:849-864` runs a blocking `GetMessage` loop.
- `sgp/sgp.cpp:254-261` triggers game execution through `WM_TIMER`.
- `sgp/sgp.cpp:542-559` also registers timer-thread notifications.

This coupling means SDL cannot be introduced cleanly by replacing only DirectDraw. A neutral application host should own initialization, event polling, focus transitions, game ticks, quit requests, and exactly-once teardown. Win32 and SDL should implement that contract separately.

The same extraction should address lifecycle hazards already visible in the reference backend: `atexit(SafeSGPExit)` is registered at `sgp/sgp.cpp:372-377`, while `WM_DESTROY` invokes shutdown at `sgp/sgp.cpp:302-305`; only part of the shutdown chain is explicitly idempotent (`sgp/sgp.cpp:909-923`). Backend work should define one owner for shutdown rather than reproduce this ambiguity.

### Process, Paths, and Operating-System Services

Remaining process-level assumptions include:

- Restart through `GetModuleFileNameA` and `ShellExecuteA` (`sgp/sgp.cpp:729-737`).
- Single-instance detection by window title (`sgp/sgp.cpp:757-766`).
- Windows command-line acquisition and `_alloca` (`sgp/sgp.cpp:1317-1324`).
- A second fixed 99-byte command-line representation (`sgp/sgp.cpp:799-804`).
- Wine registry detection/override exposed by `wine/include/wine.h:4-13` and implemented in `wine/wine.cpp:14-64`.
- Win32 dialogs in lifecycle and telemetry (`sgp/sgp.cpp:925-934`, `sgp/crash_telemetry.cpp:175-203`).
- WinHTTP telemetry in `sgp/crash_telemetry.cpp:71-103`.

These are narrow services and should not delay early rendering experiments. Native Linux can initially use no-op single-instance/restart/telemetry implementations, provided the behavior is explicit and the core interfaces do not expose Windows types.

### Timing, Threads, and Synchronization

The timer layer has a reusable API but a deeply Windows-specific implementation:

- `sgp/timer.h:20-24` is already platform-neutral.
- `sgp/timer.cpp:13-41` uses `GetTickCount`, `SetTimer`, and `KillTimer`.
- `Utils/Timer Control.cpp:22-50` stores QueryPerformanceCounter state.
- `Utils/Timer Control.cpp:322-444` creates Win32 threads/events and uses WinMM timing.
- `Utils/Timer Control.cpp:456-483` performs timed shutdown waits and closes handles without a conventional RAII join model.

Three important critical sections cover the game loop (`sgp/sgp.cpp:121`), timer notifications (`Utils/Timer Control.cpp:150`), and input queue (`sgp/input.cpp:97`). `Platform::Sleep` already has a neutral header (`sgp/platform/Sleep.h:1-8`), but only the Windows implementation is selected in `sgp/CMakeLists.txt:45-47`.

A portable timing phase should preserve observable tick cadence and the legacy 32-bit millisecond behavior while moving implementation to `steady_clock`, RAII synchronization, events/condition variables, and joinable threads. Timing changes affect AI, input repetition, sound callbacks, and animation, so they require trace-based tests rather than only successful compilation.

### Text and Encoding

The audit found approximately 124 direct `MultiByteToWideChar`/`WideCharToMultiByte` call lines across roughly 40 modules. Representative sites include `lua/lwstring.cpp:24-35`, `Utils/XML_Items.cpp:467-500`, and `Strategic/XML_SectorNames.cpp:299-324`.

This is a hidden Linux blocker because it combines Windows conversion APIs, active code-page assumptions, and the `CHAR16` width problem. Text modernization should define:

- The fixed-width internal/serialized UTF-16 representation that must remain compatible.
- The encoding of logical resource names, already treated as UTF-8 bytes by the file layer.
- Explicit UTF-8/UTF-16 conversion helpers.
- Boundaries for localized XML, Lua, command-line text, filesystem paths, and rendering fonts.

This should be its own characterized phase. Blind global replacement with standard wide strings would change behavior and binary layouts.

### Crash Handling and Diagnostics

Crash handling is intentionally platform-specific but currently leaks Windows and x86 assumptions:

- Vectored exception setup occurs around `sgp/sgp.cpp:680-708`.
- Main-loop and shutdown recovery use MSVC SEH at `sgp/sgp.cpp:1246-1257` and `1397-1415`.
- Timer threads use SEH in `Utils/Timer Control.cpp:322-353` and `724-736`.
- `sgp/crash_report.h:7-30` exposes `_EXCEPTION_POINTERS`.
- `sgp/crash_report.cpp:119-229` assumes 32-bit registers/addresses and walks Windows process internals.

Linux needs a signal/backtrace backend, but should not imitate the current practice of catching an access violation and retrying execution. A portable contract should capture minimal diagnostics and terminate predictably. Normal diagnostics should route to the logger or stderr; crash-safe output should remain a separate allocation-minimal implementation.

## Rendering and Input Assessment

### What Must Be Replaced

The renderer still creates DirectDraw, requests cooperative/display modes, and owns primary/back/frame/cursor surfaces in `sgp/video.cpp:324-500`. Windowed presentation uses blits in `sgp/video.cpp:2262-2298`, and fullscreen presentation flips surfaces in `sgp/video.cpp:2299-2323`.

On contemporary Windows, cnc-ddraw supplies much of the behavior users perceive as modern compatibility. Detection in `sgp/sgp.cpp:939-945` and `1221-1226` alters the engine path, while `gamedir/ddraw.ini:8-68` configures scaling, borderless/windowed behavior, presentation, and mouse adjustment. The native engine itself has not yet been modernized beyond this wrapper strategy.

DirectDraw also leaks into nominally generic structures: `sgp/video.h:24-59` exposes window and DirectDraw objects, and `sgp/vsurface.h:88-110` stores DirectDraw pointers in `SGPVSurface`. `sgp/WinFont.cpp:420-460` obtains a GDI device context from a private DirectDraw surface, making WinFont a special rendering migration item.

### What Can Be Preserved

The engine's CPU rendering architecture provides a strong seam:

- The game deliberately uses 16-bit rendering (`Ja2/local.h:21-23` and `76`).
- `sgp/vobject.cpp:335-368` locks a destination, runs existing software blitters, and unlocks it.
- Logical surface handles are defined in `sgp/vsurface.h:19-22`.
- Lock/unlock and blit contracts are in `sgp/vsurface.h:145-176`.
- Surface registry and special-surface routing are in `sgp/vsurface.cpp:382-543`.

The first SDL renderer should therefore preserve 16-bit CPU surfaces, existing pitch/color-key behavior, logical handles, dirty regions, and the current software blitters. It should replace surface storage where necessary and convert/upload only at final presentation. Rewriting gameplay drawing or `vobject_blitters.cpp` at the same time would greatly increase regression risk.

Before backend parity work, two legacy details deserve characterization:

- DirectDraw cleanup in `sgp/video.cpp:641-684` does not visibly release every creator-side object/reference established in `sgp/video.cpp:324-500`, making backend reinitialization less reliable than process-exit cleanup.
- Backup/restore calls around `sgp/vsurface.cpp:1328-1341` and `2065-2083` appear to pass source/destination opposite to the `DDBltFastSurface` declaration in `sgp/DirectDraw Calls.cpp:222-233`. Golden tests should determine intended behavior rather than copying a possible latent bug blindly.

### Input Migration Seam

`InputAtom`, engine event values, and the queue API in `sgp/input.h:10-115` are suitable as the backend-neutral destination for SDL events. The implementation remains tied to a Win32 mouse hook (`sgp/input.cpp:125-283`), cursor clipping/warping (`sgp/input.cpp:1539-1607`), keyboard message flushing (`sgp/input.cpp:1622-1642`), and VK/scancode translation (`sgp/input.cpp:830-928`).

Gameplay also polls the OS cursor directly, including `Ja2/gameloop.cpp:220-253`, `Laptop/laptop.cpp:702-703`, `Strategic/mapscreen.cpp:8978-8979`, and `Tactical/Interface.cpp:2625-2626`. These bypasses must move through a logical cursor service before SDL can own window-to-game coordinate conversion. The conversion must reproduce cnc-ddraw's present mouse adjustment when output scaling or letterboxing is active.

## Audio and Cinematic Assessment

FMOD coupling is narrower than the rendering coupling. `CMakeLists.txt:232-234` links it only into SGP, and direct calls are concentrated in `sgp/soundman.cpp:1470-1906`. Most gameplay uses the existing `soundman.h`/`Sound Control` facade, which is an excellent replacement boundary. `sgp/soundman.h:4-5` still includes FMOD unnecessarily even though its public declarations do not expose FMOD types.

The replacement backend must characterize and preserve more than basic sample playback:

- 128 cache/channel slots (`sgp/soundman.cpp:59-69`).
- Stable sound instance IDs and `SOUND_ERROR` behavior.
- Volume 0-127 and pan 0-255 conversion.
- Finite and infinite loops.
- Caller-owned in-memory samples and streaming music.
- Randomly scheduled playback.
- Main-loop-polled fades and end-of-sound callbacks (`sgp/soundman.cpp:967-1022`).
- Callback behavior when callers explicitly stop sounds (`sgp/soundman.cpp:1888-1905`).
- The current `SoundGetPosition` wall-clock semantics (`sgp/soundman.cpp:1027-1055`), which are not true device playback position.

libsmacker is already source-built and is not the primary blocker. Dependency setup is in `cmake/dependencies/Libsmacker.cmake:4-53`; decoded frames use the generic 16-bit framebuffer path in `Utils/Cinematics.cpp:346-385`, and decoded audio uses `SoundPlayFromBuffer` in `Utils/Cinematics.cpp:167-187` and `296-309`. Once framebuffer presentation and memory-audio playback are portable, the existing cinematic decoder should largely follow them.

## Modern Windows and 64-Bit Assessment

### Modern Windows x86

The project has a credible 32-bit compatibility baseline: current MSVC, pinned source dependencies, strict warnings, Wine-hosted build tooling, cnc-ddraw, and a previously confirmed interactive runtime. Portable I/O improves this target directly through Unicode-capable physical paths, safer handles, confined writable locations, independent enumeration, and recoverable saves.

What remains before calling modern-Windows x86 release-qualified is mostly validation and legacy-backend hardening:

- Build and run the exact committed portable-I/O tree on native Windows.
- Exercise all shipped applications, not only JA2: map editor, UB, and UB map editor.
- Validate resolution/fullscreen/alt-tab/focus behavior across current Windows versions and graphics drivers.
- Verify save compatibility and interrupted-save recovery using real datasets.
- Resolve or document DirectDraw resource-lifetime and restore semantics.
- Decide whether cnc-ddraw remains a supported distribution component after an SDL backend exists.

### Why Windows x64 Is a Separate Project

Four independent classes of blocker prevent a native x64 switch.

#### 1. x86 assembly

The audit found about 104 textual assembly sites across ten files, concentrated in `sgp/vobject_blitters.cpp:25-14690` and `TileEngine/renderworld.cpp:4947-8896`, with additional sites in `Ja2/profiler.cpp` and `sgp/shading.cpp`. MSVC x64 does not support the existing inline-assembly form. Portable C++ reference implementations plus golden pixel tests are needed before architecture-specific optimization can be reconsidered.

#### 2. Live pointer truncation

The file-handle pointer truncation is fixed, but other pointer-as-`UINT32` channels remain active:

- Dialogue item pointers: `Tactical/Interface Dialogue.cpp:1379-1382` and `Tactical/Dialogue Control.cpp:1208-1214`.
- Tactical UI pointers in button userdata: `sgp/Button System.h:97-108` and representative uses in `Tactical/Interface.cpp:845-1088`.
- Soldier pointers passed through positional sound data: `Tactical/Soldier Control.cpp:2442-2445` and `Utils/Sound Control.cpp:1081-1085`.
- Strategic group pointers passed through dialogue data: `Strategic/Strategic Movement.cpp:1016-1021` and `Tactical/Dialogue Control.cpp:1016-1026`.
- Opaque FMOD/callback values: `sgp/soundman.cpp:1625-1825`.
- Address-derived rain randomness: `Tactical/Rain.cpp:239-248`.

These paths can corrupt live state on x64 even if the program compiles. They require typed payloads, object IDs, or `uintptr_t` according to ownership and lifetime semantics.

#### 3. Raw native-structure serialization

The save system writes many in-memory structures directly. Pointer-bearing examples include:

- `STRATEGICEVENT::next` in `Strategic/Game Events.h:10-20`, serialized in `Strategic/Game Events.cpp:719-782`.
- `UNDERGROUND_SECTORINFO::next` in `Strategic/Campaign Types.h:597-608`, serialized in `Strategic/Queen Command.cpp:2633-2689`.
- Laptop save pointers in `Laptop/LaptopSave.h:108-117`, serialized in `Laptop/laptop.cpp:7144-7232`.
- Vehicle path/passenger pointers in `Tactical/Vehicles.h:281-308`, serialized in `Tactical/Vehicles.cpp:2294-2345`.

On x64, pointer width and padding change record sizes and shift subsequent bytes even when pointer fields are later repaired. The safe migration is to capture the x86 on-disk layout, define fixed-width disk DTOs, and convert to/from native runtime structures. Changing typedefs globally without this layer would break existing saves.

#### 4. 32-bit binary dependencies

`gamedir/fmod.dll` and `gamedir/ddraw.dll` are 32-bit PE components. A 64-bit process cannot load them. FMOD must be replaced, and the x64 target must use SDL/native presentation rather than the bundled x86 DirectDraw wrapper.

### Recommended Relationship Between Linux and x64 Work

Linux-native and Windows-x64 share some prerequisites but should not be conflated:

- Shared work: fixed-width types, encoding, header cleanup, portable blitters, pointer-safe payloads, audio replacement, SDL presentation/input, and disk DTOs.
- Linux-specific work: POSIX durability/path/time backends, native application host, signals, compiler/build conditioning, and case-sensitive filesystem validation.
- Windows-x64-specific work: x64 presets/CI, Windows crash/context support, native-Windows runtime qualification, and replacement of every x86-only binary/assembly path.

If the immediate product goal is a playable Linux build, it is reasonable to preserve 32-bit-compatible save and gameplay semantics while adding Linux backends. If the goal is a long-lived modern architecture, pointer transport and disk-layout characterization should begin early because they affect interface design. A full Windows-x64 release does not need to block the first Linux executable, but ignoring x64 constraints while defining new interfaces would create avoidable rework.

## Networking Status

The obsolete RakNet dependency has been removed and the current compatibility facade reports networking unavailable (`Multiplayer/network_stub.cpp:5-128`). No active Winsock API dependency was found in the multiplayer path. This means multiplayer is not a native compile blocker, but it also means multiplayer is not part of the feature-parity target.

Crash telemetry remains a separate WinHTTP client and can initially be disabled or moved behind a portable HTTP service. Any future multiplayer implementation should be planned as a new portable protocol/transport project rather than coupled to the platform-port critical path.

## Test and Build Evidence

### Verified During This Assessment

The standalone native suite in `tests/fileio/CMakeLists.txt` was rebuilt on Linux on 2026-09-15. All six tests passed:

1. `portable_fileio_tests`
2. `save_transaction_tests`
3. `local_time_tests`
4. `log_store_tests`
5. `posix_durable_file_operations_tests`
6. `platform_paths_tests`

The suite covers physical modes, traversal/confinement, replacement, routing, writable-root refresh, save transaction publication/recovery/failure injection, real POSIX durability and publication, local-time boundaries, platform paths, CRLF log handling, and concurrent log lines. It is deliberately standalone and is not currently added by the root CMake project.

The Wine/MSVC x86 runtime was also playtested after compatibility fixes on 2026-09-15. Startup completed in roughly two seconds in the isolated runtime, strategic saves loaded, and tactical saves containing embedded `Temp` files loaded successfully. A regression test now covers doubled legacy path separators at the router boundary.

### Evidence Gaps

- Root CMake does not enable/add the file-I/O tests, and CI runs Windows x86 compilation without CTest (`.github/workflows/build.yml:87-158`).
- The tests do not exercise the legacy `FileMan` registry/disposition behavior or real bfVFS profiles/archives.
- There are no fixtures for SLF, JPC/7z, named profile precedence, profile mutation with live leases, or repeated archive-member opens.
- Save transaction tests use `FakeDurableFileOperations`; the real Windows durability backend has no fault-injection integration test.
- There is no Windows timestamp/DST test, junction test, case-collision test, greater-than-4-GiB test, or real save compatibility corpus.
- There are no automated renderer pixel-golden tests, input traces, audio callback traces, x64 layout manifests, or full-game Linux compilation tests.
- Exact-commit build provenance should be refreshed after the file-I/O checkpoint is squashed; the currently playtested executable includes the final compatibility fixes but was produced from a dirty tree based on `cf8b80a7a`.

## Revised Critical Path

### Phase 1: Qualify and Freeze the Compatibility Reference

Deliverables:

- Exact-commit Wine/MSVC x86 build provenance and direct runtime smoke test.
- Native modern-Windows x86 build/run.
- Save corpus and deterministic format hashes.
- Resource/profile/archive fixtures.
- Renderer screenshots/framebuffer hashes, input traces, and audio callback traces.
- Root/CI integration of the four native I/O tests.

Exit gate: the current backend is measurable enough to identify whether later differences are intentional.

### Phase 2: Make the Shared Core Compilable

Deliverables:

- Backend-conditioned CMake structure and a `LINUX` skeleton.
- Fixed-width fundamental types, especially `FLAGS32`, plus a deliberate `CHAR16` design.
- Common headers separated from `windows.h`/DirectDraw declarations.
- Central UTF-8/UTF-16 conversion service.
- Pointer-safe callback/userdata payload designs for interfaces touched by platform work.

Exit gate: a meaningful shared source subset compiles under a native GCC/Clang toolchain without pretending to be Windows.

### Phase 3: Implement Platform Core Backends

Deliverables:

- Application host and event-loop interface with Win32 and SDL/Linux implementations.
- Monotonic time, scheduling, sleep, synchronization, and thread lifecycle backends.
- Linux executable/base/user path policy.
- POSIX durable save operations and Linux timestamps.
- Platform diagnostics, dialogs/no-op policy, dynamic-library handling, and crash boundaries.

Exit gate: native startup reaches game initialization and can run deterministic game ticks without DirectDraw or FMOD.

### Phase 4: Portable Presentation and Input

Deliverables:

- Backend-neutral CPU surface ownership retaining 16-bit rendering contracts.
- SDL window and framebuffer presentation, scaling, letterboxing, fullscreen, focus, and dirty-region handling.
- SDL-to-`InputAtom` translation with logical cursor coordinates.
- Replacement/isolation of WinFont GDI rendering.
- Pixel-golden and input-trace comparison against the Windows reference.

Exit gate: menus and tactical rendering are interactive on Linux with behaviorally compatible input.

### Phase 5: Portable Audio and Cinematics

Deliverables:

- Backend-neutral sound implementation behind the existing facade.
- Compatible IDs, channel/cache policy, loops, pan/volume, streams, fades, positions, and callback order.
- Smacker frame and memory-audio integration validated through the new backends.

Exit gate: playable Linux sessions have music, effects, speech, and cinematics without FMOD.

### Phase 6: Linux Playtest Release

Deliverables:

- JA2113, Vanilla, and selected mod-profile validation on case-sensitive filesystems.
- New game, strategic/tactical transitions, combat, save/load/quicksave/quickload, cinematics, and clean shutdown.
- Packaging policy for game resources and XDG-appropriate writable roots.
- Crash/log collection and user-facing diagnostics.
- Performance and long-session stability checks.

Exit gate: a native Linux executable is usable with legitimate JA2 data without Wine.

### Phase 7: Native x64 and Product Breadth

Deliverables:

- Fixed-width disk DTOs and legacy-save conversion.
- Removal/replacement of pointer-valued 32-bit payloads.
- Portable reference blitters replacing x86 inline assembly, with optional optimized implementations later.
- Windows x64 and Linux x64 presets and CI.
- Map editor, UB targets, exporter, and tooling qualification.
- Networking considered independently after single-player parity.

Exit gate: 64-bit Windows and Linux builds load legacy saves and pass the same behavior suites.

## Priority Risk Register

| Risk | Impact | Mitigation |
|---|---|---|
| `CHAR16` changes width on Linux | Corrupt layouts and text behavior | Define fixed-width UTF-16 representation before broad native compilation |
| Raw structure saves change on x64 | Existing saves become unreadable | Capture x86 layouts; add disk DTOs and corpus tests |
| Pointer-to-`UINT32` channels survive | Runtime corruption on x64 | Replace with typed payloads/IDs before enabling x64 |
| SDL rewrite changes 16-bit rendering | Visual/gameplay regressions | Preserve software blitters and compare framebuffer goldens |
| Timer rewrite changes cadence | AI, input, sound, animation drift | Record tick/input/audio traces and retain legacy millisecond semantics |
| Linux case sensitivity changes lookup | Missing or shadowed resources | Add profile/archive/case-collision fixture matrix |
| Save durability differs on POSIX | Mixed or lost save generations | Implement and fault-test file/directory `fsync` plus atomic rename |
| FMOD behavior is under-characterized | Audio callbacks or timing alter gameplay | Trace IDs, loops, fades, stop/EOS callbacks, and positions before replacement |
| cnc-ddraw masks DirectDraw assumptions | Native backend behaves differently | Treat wrapper behavior as explicit compatibility input, not engine behavior |
| Port work outruns regression evidence | Failures become difficult to localize | Keep Windows/Wine backend green and add one validation gate per extracted service |

## Final Audit Conclusion

The portable-I/O update moves the project across a genuine architectural boundary: storage and resource behavior is no longer inseparable from Windows gameplay code. It supplies reusable contracts, host-native tests, and safer modern-Windows behavior, and it reduces the number of concerns that must change simultaneously when native platform work starts.

The broader audit also shows that the remaining effort is dominated by three coherent projects rather than thousands of unrelated API substitutions:

1. Extract the application host, timing, text, and OS services so the shared engine can compile natively.
2. Replace DirectDraw/Win32 input with SDL while preserving the existing 16-bit software-rendering model.
3. Replace FMOD and then address explicit x64 hazards in pointers, assembly, and serialization.

That is a substantially better position than the original tree, but still before the first native Linux game executable. The portable-I/O completion should be treated as the end of the storage-foundation phase and the start of platform-core extraction, with Windows x86 retained as the compatibility oracle and x64 handled as a deliberate parallel modernization track.

## Deferred Near-Term Porting Sequence

Once the current Windows/Wine file-I/O behavior is again a trustworthy baseline, the next low-risk target is the POSIX/Linux half of the file-platform boundary rather than the SDL window:

Completed immediately after the file-I/O checkpoint:

1. Added `PosixDurableFileOperations` with file `fsync`, parent-directory `fsync`, and atomic no-clobber rename/replace behavior.
2. Selected durable-file operations through a platform backend factory instead of constructing the Windows implementation directly.
3. Added Linux file timestamps and executable-directory discovery through `/proc/self/exe`.
4. Covered durability, replacement, timestamps, and executable-path discovery with native integration tests while keeping the Windows x86 build green.

Then proceed through these independently verifiable seams:

1. Condition CMake and establish a small native shared-core target.
2. Extract process services: command-line handling, restart/single-instance behavior, dialogs, and telemetry.
3. Put an interface in front of FMOD while retaining the existing Windows implementation.
4. Extract monotonic time, timers, synchronization, and thread lifecycle, validating behavior with traces.
5. Remove DirectDraw and Win32 presentation/input types from platform-neutral headers.
6. Introduce the SDL window, input translation, and framebuffer presentation only after those dependencies are isolated.

Deferring SDL preserves a runnable Windows compatibility oracle while the current `WndProc` responsibilities--lifecycle, timers, input, focus, and rendering--are separated and DirectDraw types are removed from shared interfaces.
