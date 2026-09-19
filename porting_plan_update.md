# JA2 1.13 Porting Plan Update

Date: 2026-09-16

## Executive Status

The portable file-I/O update at commit `cf8b80a7a` completes a major prerequisite for both a native Linux port and a maintainable modern-Windows build. It replaces the engine's most pervasive storage coupling with project-owned C++17 interfaces while retaining bfVFS as the compatibility-tested resource backend. It also removes the pointer-to-32-bit-file-handle design, makes writable paths explicit and confined, and gives save publication recoverable transaction semantics.

This is an important architectural milestone, but it is not yet a native Linux port. CMake now accepts a `LINUX` backend and builds a bounded shared-core target, but the production game executable still requires the `WINDOWS` backend, uses a Windows GUI entry point, and links DirectDraw, WinMM, WinHTTP, and proprietary FMOD 3.75 components. The currently playtestable product remains a 32-bit Windows executable built with MSVC and exercised under Wine.

In roadmap terms, the update moves the project from "stabilize and reproduce the legacy Windows program" into "extract replaceable platform backends." File and resource access is now sufficiently isolated that subsequent work does not need to solve storage portability at the same time as windowing, rendering, input, or audio. That substantially reduces the risk and scope of each later migration.

## Platform-Core Progress: Monotonic Timing and Scheduling

The engine clock-source migration is now implemented as the first platform-core extraction. A project-owned C++17 monotonic clock wraps `std::chrono::steady_clock` and supplies process-relative microsecond, 64-bit millisecond, and legacy wrapping 32-bit millisecond readings. Former runtime uses of `GetTickCount`, QueryPerformanceCounter, window timers, and the dormant WinMM callback-timer path have been replaced or removed. The fixed-step synthetic JA2 clocks, pause behavior, configurable clock speed, fast-forward cadence, and notification behavior remain engine policy rather than properties of the host clock.

Native timing tests cover monotonicity, unit coherence, elapsed-time accuracy, 32-bit deadline wraparound, fixed-step scheduling, delayed catch-up, fast-forward behavior, and the legacy countdown contract. The same tests build and run for Windows x86; the clock test compares the replacement directly with QueryPerformanceCounter and GetTickCount64. The Windows executable also builds with the replacement wired in.

Tick delivery is now single-threaded. A portable `MainLoopScheduler` calculates fixed ticks, periodic game-loop notifications, delayed catch-up, and the next host wake deadline. `Utils/Timer Control.cpp` advances the legacy synthetic clocks and countdowns in bulk while preserving pause, configurable clock speed, fast-forward, and exact-boundary countdown behavior. The Windows host waits for either queued messages or that portable deadline and runs the game loop on its main thread.

The two Win32 timer threads, their events and shutdown waits, timer callback list and lock, game-loop critical section, and temporary Windows sleep-resolution hint have therefore been deleted. Stracciatella provided a useful independent design check: it likewise removed timer-thread delivery and advances its clock from the main loop. The 1.13 implementation remains deliberately distinct because it retains the branch's fixed-step, clock-speed, fast-forward, pause, and timer-notification semantics rather than adopting Stracciatella's simpler elapsed-millisecond policy.

## Platform-Core Progress: Portable Threading and Synchronization

The remaining maintained engine uses of Windows thread and synchronization APIs have now been removed. The input queue uses a C++17 recursive mutex with RAII locking, preserving the recursive behavior of a Windows critical section because dequeue processing may synthesize mouse-repeat events and re-enter the queue. Initialization, destruction, and MSVC structured-exception lock cleanup are no longer necessary.

Crash telemetry now starts its best-effort upload through a small project-owned detached-task service implemented with `std::thread`. Thread-creation failures are reported to the caller, exceptions cannot escape a detached task and terminate the process, and each telemetry worker owns its URL, root path, and writable store. This removes `_beginthreadex`, the raw thread handle, `CloseHandle`, and the shared fixed-size URL buffer from the startup path.

Native tests cover detached execution, captured-state ownership, empty-task rejection, and exception containment. The Windows x86 game builds with the same implementation. A source audit finds no remaining active Win32 thread creation, event, critical-section, TLS, or interlocked operations in maintained runtime code. `MsgWaitForMultipleObjectsEx` remains only in the Windows application host's message/deadline wait. The native SDL3 host now implements the same one-event-per-iteration contract with `SDL_WaitEventTimeout`; selecting it for the production loop remains future wiring rather than engine thread lifecycle work.

## Native-Build Progress: Shared Core

The root build now selects `JA2_PLATFORM_BACKEND=LINUX` by default on a Linux host and produces `ja2_shared_core` instead of entering the Windows application, renderer, production input event source, production audio, tools, and resource-file source groups. The target contains 37 first-party translation units: the complete file/resource service boundary, POSIX durable operations and paths, the legacy `FileMan` compatibility facade, monotonic clocks, fixed-step scheduling, the portable application-loop and shutdown guard, portable sleep/window policy, detached tasks, process arguments and property parsing, explicit native restart/single-instance/telemetry policy, stderr-backed host dialogs, the portable input state backend, the legacy sound-manager policy with a portable null-audio backend, zlib compression, CPU line rendering, key translation, portable `HIMAGE` layout/palette/copy operations, color quantization, dirty-region tracking, heap-backed indexed/RGB565 pixel surfaces, the legacy clock facade, a platform-neutral string utility, the isolated legacy host-wide conversion implementation, and the UTF-8/UTF-16 conversion service. It also builds the pinned patched bfVFS, LZMA SDK, zlib, and utf8cpp dependencies natively from the same sources used by the Windows baseline.

An integrated `ja2_shared_core_smoke` executable links and exercises the combined target: monotonic time and sleeping, scheduler construction, detached execution, physical writable storage, durable file sync, metadata, and executable-path discovery. The existing timing/thread and file-I/O suites are registered alongside fixed-width type, legacy `FileMan`, compression, CPU rendering/key translation, image-layout/palette/copy, color-quantization, UTF conversion, process-service, audio-backend, platform-neutral-header, input-backend, application-lifecycle, presentation-foundation, SDL3-presenter, SDL3-application-host, SDL3-input-translation, legacy-input-sink, and end-to-end SDL-input-pipeline tests, giving the root Linux build one 29-test CTest gate. GCC 16 and Clang 22 builds pass with strict first-party warnings; a GCC AddressSanitizer build also passes when leak detection is disabled under the ptrace-based test container.

The former fundamental-type and umbrella-header boundary is resolved. Integer and flag aliases use fixed-width standard types; `CHAR16` remains `wchar_t` on Windows for source and ABI compatibility but is an explicit 16-bit `char16_t` on native targets. The two host-wide diagnostic conversion helpers and their bfVFS includes have moved out of `types.h`; their explicitly named compatibility header now contains declarations only, while the implementation is isolated in one translation unit. This also prevents UTF conversion changes from invalidating every source reached through the legacy debug-header fan-out. `sgp.h` no longer includes `local.h`, `video.h`, Windows, DirectDraw, or FMOD. Existing sources that still require the historical fan-out include the explicitly named `LegacySGP.h` compatibility umbrella, making that debt visible and allowing new shared code to use the neutral header.

The first presentation-header boundary is also complete. `video.h` no longer includes Windows or DirectDraw headers, exposes no `HWND`, `HINSTANCE`, or DirectDraw object accessors, and no longer drags in `local.h`. Native host handles and DirectDraw access live in `video_windows.h`; the concrete DirectDraw implementation includes its helper API directly. `WinFont.h` is also neutral now: its private `LOGFONT` construction API is confined to `WinFont.cpp`. A native compile test locks down the engine-facing video and font signatures without compiling a Windows host. Win32 window registration/creation and DirectDraw presenter construction have since moved from the common video manager into `platform/windows/VideoBootstrap.cpp`; `video.cpp` accepts an owned `Presenter`, contains no Win32 types or APIs, and now passes a strict native GCC/Clang object-build gate. `LegacySGP.h` still conditionally supplies the Windows video header to old translation units, so migrating its remaining consumers and implementation-only GDI state remain follow-up work.

A follow-up Win32 scalar cleanup replaces the strategic map's public `POINT` array with the layout-equivalent engine `SGPPoint`, removes `windows.h` and `MAX_PATH` from `INIReader`, removes the `MAX_PATH` dependency from the logical-body XML loader, and makes packed input-coordinate extraction independent of `HIWORD`/`LOWORD`. The median-cut quantizer now uses fixed-width engine scalars, `SGPPaletteEntry`, and standard allocation instead of `BOOL`, `BYTE`, `DWORD`, `RGBQUAD`, and process-heap APIs. It is part of the native shared core and has focused palette and reduction tests.

The `himage.cpp` compiler boundary is resolved without compiling or refactoring the 14,885-line `vobject_blitters.cpp`: anonymous image/STCI records now use standard layouts with explicit ABI checks, MSVC-only helpers are gone, and the one flat-pixel blitter dependency has a narrow header. The file-format loader implementations and the legacy blitter implementation remain outside the native target; the focused image test supplies dispatch stubs so the portable image operations can be linked and exercised independently. The UTF conversion boundary is complete and direct host conversion calls are gone. Broad gameplay compilation still requires migrating internal `wchar_t`, `L"..."`, and `wcs*` manipulation as each legacy module moves to the fixed-width engine text type; that is an internal representation cleanup rather than a host encoding boundary. No native game executable is claimed yet.

The native checkpoint is built with:

```sh
cmake -S . -B build/native -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/native --parallel 16
ctest --test-dir build/native --output-on-failure
```

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

It also provides a useful native-build foothold. File I/O now compiles both in standalone host-native tests and as part of the root `ja2_shared_core` Linux target. This proves that the storage, timing, and basic platform layers have escaped the global Windows build assumptions and establishes a compiler-enforced boundary for extracting the next platform-neutral layers.

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
- Wall-clock services, sleeping, and the remaining host-specific event-loop wait/pump.
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

The audio extraction and first native backend are now implemented. `soundman.h` and the legacy sound-manager policy no longer expose FMOD declarations; device initialization, file callback adaptation, stream creation, channel playback, looping, volume, pan, and shutdown go through `Audio::Backend`. Windows retains the FMOD/DirectSound adapter as the behavior oracle. Linux selects an SDL3_mixer 3.2.4 backend with pinned, self-contained WAV, Ogg Vorbis, and MP3 decoders, while `soundman.cpp` continues to own cache/channel policy, random scheduling, fades, and callbacks on the main thread. Compatibility hardening includes play-time loop configuration, explicit encoded-buffer and VFS-stream ownership, reset of pooled-track gain and pan, repair of undersized legacy RIFF headers, and normalization of decoder seeks around bfVFS's broken `SEEK_END` behavior for uncompressed SLF members. Focused dummy-audio tests cover finite playback, looping, pause/resume, volume, pan, malformed memory and streamed WAVs, and shutdown with an outstanding stream; a native headless save-load also exercises packaged music. Sound diagnostics use the portable writable log store, and FMOD calls and calling conventions remain confined to `sgp/audio/windows/FmodAudioBackend.cpp`.

### Native Build Enablement

CMake now has a bounded `LINUX` backend for the shared core. Expanding it into a native game target includes:

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
| Native core build | Root `ja2_shared_core` builds 36 first-party units with GCC and Clang; all 24 registered tests pass | Storage, resources, legacy file handles, compression, CPU line/image operations, color quantization, UTF conversion, timing, threading, application-loop policy, input state, presentation primitives, bounded process services, and the sound-manager/null-backend boundary form a proven host-native island |
| Linux game executable | Not defined yet | Legacy UTF-16 manipulation, image-format loaders/blitters, and remaining subsystem backends are the next blockers |
| Window/event backend | Win32 only | Hard Linux blocker |
| Rendering/presentation | DirectDraw 2 plus cnc-ddraw | Works as a legacy compatibility path; no native Linux renderer |
| Input backend | Win32 mouse hook/messages and direct cursor polling | Hard Linux blocker, although the engine event queue is reusable |
| Audio backend | Project-owned interface; FMOD 3.75 Windows adapter and Linux null backend | Real native playback remains, but FMOD no longer contaminates shared sound policy |
| Cinematic decoder | Source-built libsmacker | Decoder is portable; final video/audio backends are not |
| Linux durable save backend | Implemented and natively tested | Save publication no longer blocks a future native game |
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
5. The root native target now compiles and links the real `BfVfsResourceStore` and bfVFS archive providers. The integrated smoke test does not yet mount retail SLF/JPC fixtures, so resource precedence and archive behavior still rely on the Windows baseline and future native fixtures.
6. Equipment-template filename conversion now uses the project UTF service rather than `WideCharToMultiByte`; the remaining text-encoding migration is tracked below.

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

The original root-build blockers have been removed for the bounded Linux target:

- `CMakeLists.txt` selects `WINDOWS` or `LINUX` according to the target platform and enters backend-specific source groups.
- The Linux path returns after defining `ja2_shared_core` and its smoke test, so it does not create the Windows GUI executable, compile `Ja2/Res/ja2.rc`, or link WinMM, WinHTTP, DirectDraw, and `fmodvc.lib`.
- Frame-pointer, warning, and AddressSanitizer options distinguish MSVC/clang-cl from native GCC and Clang.
- Pinned LZMA, zlib, utf8cpp, and patched bfVFS dependencies build on both backends.
- The Windows application graph remains intact and continues to provide the behavioral baseline.

The controlled target now includes legacy `FileMan`, compression, key translation, CPU line drawing, and the portable portion of `HIMAGE` while deliberately stopping before the image-format loader and full rendering graph. This exposes the next errors without conflating text, application-host, full rendering, input, and audio work.

### Fundamental Types and Header Fan-Out

The most consequential fundamental type problems in `sgp/types.h` are now resolved:

- `INT8` through `UINT64` and `FLAGS32` use standard fixed-width integer types and have native compile-time size checks.
- `CHAR16` is guaranteed to remain two bytes: Windows retains its 16-bit `wchar_t`, while non-Windows builds use `char16_t` instead of changing the platform wide-character ABI.
- `FileMan` exposes a project calling-convention macro that maps to `__cdecl` only where required, and the complete legacy facade now compiles and runs native tests over the portable file backend.

`-fshort-wchar` would be a poor shortcut because it makes the program disagree with the platform C/C++ wide-character ABI. The durable solution is an explicit 16-bit engine/serialized character type plus conversion at OS, library, and UI boundaries.

Windows headers still spread through subsystem-specific interfaces, but no longer through neutral `sgp.h`, `video.h`, `WinFont.h`, `INIReader.h`, or the strategic map interface. The 161 existing consumers of the historical SGP fan-out use `LegacySGP.h`; on Windows that compatibility umbrella still supplies `video_windows.h`, while native builds see only the neutral interfaces. Public strategic geometry now uses `SGPPoint`, CPU and quantizer interfaces use engine Boolean/fixed-width types, palette output uses `SGPPaletteEntry`, and packed input coordinates no longer depend on Win32 extraction macros.

The remaining obvious Windows types are deliberately confined to backend or implementation headers such as `video_windows.h`, `vsurface_private.h`, the legacy DirectSound helper, crash reporting, and development-only VTune integration. The next header cleanup should migrate individual `LegacySGP.h` consumers to specific subsystem headers, then extract or condition those implementation-only interfaces as their owning subsystem moves native.

### Application Host and Event Loop

`sgp/sgp.cpp` still owns Windows startup and its window procedure, but the main-loop policy is now portable:

- `sgp/sgp.cpp:705` defines `WinMain`.
- `sgp/sgp.cpp:176-369` handles focus, activation, input, rendering restoration, shutdown, and other window messages in one WndProc.
- `application/ApplicationLoop` owns clock-update, active-frame, wait/dispatch, quit, and host-failure ordering behind a platform host-pump contract.
- `platform/windows/ApplicationHost.cpp` retains the current `MsgWaitForMultipleObjectsEx` plus one-message dispatch policy; `platform/sdl/Sdl3ApplicationHost.cpp` implements the same contract with an SDL-owned window and one waited event.
- Game ticks and `SGPGameLoop` now run on the main thread rather than from a timer notification thread.
- `WindowProcedure` still owns Windows focus, activation, input, restoration, and shutdown messages.

Subsystem teardown is now guarded at `ShutdownStandardGamingPlatform` itself. `WM_DESTROY` requests quit without tearing subsystems down, the normal path explicitly calls `SGPExit`, and the registered `atexit` handler remains an idempotent emergency fallback. This removes the former double-shutdown path while retaining the established shutdown order.

This coupling means SDL cannot be introduced cleanly by replacing only DirectDraw. The neutral loop contract and separate Win32/SDL host implementations now exist. The SDL host owns video-subsystem and window lifetime, preserves one-event-per-scheduler-iteration behavior, and maps process quit or its window-close request to the existing application-loop result. Its event translator uses the presenter's logical letterbox transform for pointer coordinates and maps SDL keys into the current Win32-compatible virtual-key/scancode scheme, including navigation and NumLock-sensitive keypad distinctions. A concrete sink now connects translated keyboard and mouse events to the legacy input manager, maintains the portable physical-key snapshot, and synthesizes releases on focus loss so modifiers cannot remain stuck after task switching. Production host/presenter selection and the focus callback into game lifecycle state remain before SDL can replace the Win32 host.

The same extraction should address lifecycle hazards already visible in the reference backend: `atexit(SafeSGPExit)` is registered at `sgp/sgp.cpp:372-377`, while `WM_DESTROY` invokes shutdown at `sgp/sgp.cpp:302-305`; only part of the shutdown chain is explicitly idempotent (`sgp/sgp.cpp:909-923`). Backend work should define one owner for shutdown rather than reproduce this ambiguity.

### Process, Paths, and Operating-System Services

The bounded process-service extraction is now implemented:

- `Platform::GetProcessArguments` returns UTF-8 arguments through Windows and Linux backends; VFS command-line property parsing is portable, allocation-safe, and covered for inline, separated, empty, and Unicode values.
- Executable restart and previous-window activation are behind project-owned result types. Windows retains the startup behavior using wide host APIs; Linux explicitly reports these compatibility policies unavailable until a native host needs them.
- Lifecycle, renderer-failure, fatal-error, and telemetry prompts use a UTF-8 dialog service. Windows retains task-modal message boxes; the pre-window Linux backend writes messages to stderr and answers questions with a privacy-safe `no`.
- Crash telemetry accepts a portable UTF-8 endpoint and advertises transport availability. The existing consent/file-retention/WinHTTP implementation remains the Windows backend; Linux is an explicit no-network implementation.

Remaining process/host assumptions are the second fixed 99-byte legacy command-line representation, Wine registry override setup, the Windows crash reporter/SEH boundary, and ownership of shutdown and the event loop. The obsolete time-limited-build dialog is also deliberately dormant rather than part of the maintained runtime boundary.

### Timing, Threads, and Synchronization

The clock and scheduling layer is now portable:

- `sgp/timer.h:20-24` is already platform-neutral.
- `sgp/platform/Clock.cpp` wraps `std::chrono::steady_clock` and supplies the legacy clock views.
- `sgp/timing/MainLoopScheduler.cpp` owns fixed-tick and notification deadlines without host APIs.
- `Utils/Timer Control.cpp` retains only engine timing policy and legacy counter advancement.
- `sgp/sgp.cpp` is the remaining Windows adapter for waiting on scheduler deadlines and pumping messages.

The game-loop and timer-notification critical sections are gone because all clock and game-loop work now runs on the host thread. The input queue retains synchronization through `std::recursive_mutex`, detached best-effort work runs through `Platform::RunDetached`, and `Platform::Sleep` has both Windows and portable implementations; none expose Windows thread types or lifecycle calls.

Deterministic tests now cover scheduler cadence, delayed catch-up, fast-forward wake behavior, and the legacy countdown edge cases in addition to the clock-source tests. Playtesting remains important because timing affects AI, input repetition, sound callbacks, and animation, but no platform thread abstraction is required for timer delivery.

### Text and Encoding

The project-owned `ja2::text` service now converts between UTF-8 byte strings and the engine's fixed-width `CHAR16` UTF-16 strings. It offers strict conversion with project-owned errors and explicit replacement conversion for compatibility boundaries. Its bounded-buffer adapters work in both directions, record validity and truncation, always terminate a non-empty destination, and never split either a UTF-16 surrogate pair or a UTF-8 sequence. Characterization tests cover multilingual text, supplementary characters, embedded NULs, malformed UTF-8, lone UTF-16 surrogates, fixed-buffer truncation in both directions, and third-party exception containment. utf8cpp was upgraded from 2.3.4 to the pinned 4.1.1 release and remains private to the adapter.

All maintained `MultiByteToWideChar` and `WideCharToMultiByte` consumers have now migrated to that service. This covers every localized XML group, Lua strings with explicit lengths, equipment templates, map and tile names, multiplayer diagnostics, clipboard fallback text, and gameplay UI adapters. The migration also replaces LogicalBodyTypes filter strings with fixed-width engine UTF-16 and removes an unused ANSI font conversion. A configure-time source audit rejects any reintroduction of the two Win32 conversion APIs. Two Laptop loaders deliberately retain their historical explicit capacity limits even though their backing arrays have one additional element.

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
- `sgp/crash_report.h:7-30` exposes `_EXCEPTION_POINTERS`.
- `sgp/crash_report.cpp:119-229` assumes 32-bit registers/addresses and walks Windows process internals.

Linux needs a signal/backtrace backend, but should not imitate the current practice of catching an access violation and retrying execution. A portable contract should capture minimal diagnostics and terminate predictably. Normal diagnostics should route to the logger or stderr; crash-safe output should remain a separate allocation-minimal implementation.

## Rendering and Input Assessment

### What Must Be Replaced

The Windows `DirectDrawPresenter` still creates DirectDraw, requests cooperative/display modes, owns its native primary/back surfaces, uploads completed RGB565 frames, and performs windowed blits or fullscreen flips. Logical primary/back/frame/cursor surfaces are project-owned `PixelSurface` objects. A native SDL3 presenter now implements the same final-frame seam without being wired into the production host yet.

On contemporary Windows, cnc-ddraw supplies much of the behavior users perceive as modern compatibility. Detection in `sgp/sgp.cpp:939-945` and `1221-1226` alters the engine path, while `gamedir/ddraw.ini:8-68` configures scaling, borderless/windowed behavior, presentation, and mouse adjustment. The native engine itself has not yet been modernized beyond this wrapper strategy.

DirectDraw has been removed from the engine-facing video and surface interfaces. `SGPVSurface` stores no DirectDraw pointers or mirror state, the video manager exposes no native DirectDraw object accessors, and the obsolete shared DirectDraw wrapper is no longer compiled. `sgp/video.cpp` owns the neutral `Presenter` interface and obtains its compatibility backend through a Windows factory, so it no longer includes or names `DirectDrawPresenter`. The palette entry ABI is defined in a neutral presentation header. `sgp/WinFont.cpp` renders through a GDI DIB and copies the result into portable storage. Native DirectDraw types and calls are confined to the Windows presenter implementation.

### What Can Be Preserved

The engine's CPU rendering architecture provides a strong seam:

- The game deliberately uses 16-bit rendering (`Ja2/local.h:21-23` and `76`).
- `sgp/vobject.cpp:335-368` locks a destination, runs existing software blitters, and unlocks it.
- Logical surface handles are defined in `sgp/vsurface.h:19-22`.
- Lock/unlock and blit contracts are in `sgp/vsurface.h:145-176`.
- Surface registry and special-surface routing are in `sgp/vsurface.cpp:382-543`.

The first SDL renderer should therefore preserve 16-bit CPU surfaces, existing pitch/color-key behavior, logical handles, dirty regions, and the current software blitters. It should replace surface storage where necessary and convert/upload only at final presentation. Rewriting gameplay drawing or `vobject_blitters.cpp` at the same time would greatly increase regression risk.

That portable foundation now exists. `presentation/DirtyRegionTracker` owns the characterized legacy invalidation, clipping, split, overflow-to-full, and reset rules and is used by the Windows refresh path. `presentation/PixelSurface` provides tested indexed8/RGB565 heap storage, aligned pitch, palettes, color keys, clipped/overlap-safe copying, fill, nearest-neighbor stretch, and fast unkeyed row copies. Its golden framebuffer test covers padded pitch, clipping, both color-key directions, cursor composition, overlap-safe horizontal and vertical movement, and backup/restore direction. `presentation/Presenter` describes final framebuffer upload, suspend, and resume without exposing SDL or Windows types. Every engine-created `SGPVSurface`, including the reserved primary, back, frame, and cursor handles, uses `PixelSurface` as canonical storage regardless of legacy memory-placement flags. Frame refresh, tactical scrolling, overlays, fades, rain, cursor save/restore/composition, screenshot capture, movie-frame capture, and Windows WinFont rendering stay on portable storage; WinFont uses a GDI DIB rather than DirectDraw `GetDC`. The Windows `DirectDrawPresenter` alone owns the DirectDraw device, display mode, native primary/flip surfaces, pixel-format query, palette attachment, completed-frame upload, windowed blit, fullscreen flip, retry, and presentation-surface recovery. The native `Sdl3Presenter` owns an SDL renderer and RGB565 streaming texture attached to an SDL-host-owned window, performs clipped full/dirty uploads, and presents through logical letterboxing with nearest scaling. Headless dummy-video tests validate both the presenter boundary and the combined host/window/presenter lifetime. The video manager, logical surface manager, and WinFont contain no DirectDraw API calls or native object accessors; the complete common video-manager translation unit now compiles natively without pulling the legacy gameplay header umbrella.

`DIRECTDRAW_MIGRATION_NOTES.md` records logical and presenter surface ownership, lost-surface and cursor rules, the completed isolation steps, and the required shutdown order. An earlier reversed backup/restore copy was corrected before the obsolete DirectDraw-backed logical backup path was removed; portable copy direction remains covered by the golden surface test.

WinFont policy is explicit: Windows retains GDI rendering through a memory DIB, while portable builds force bitmap fonts. This keeps the common font interface portable without pretending the Chinese configuration's `USE_WINFONTS=1` has a native implementation yet.

### Input Migration Seam

`InputAtom`, engine event values, repeat/double-click policy, and the queue API in `sgp/input.h` remain the backend-neutral destination for host events. Gameplay cursor polling, clipping, warping, physical-key queries, and keyboard flushing now route through `platform/Input.h`; unused direct polls were deleted. The Windows adapter owns the thread-local mouse hook, Win32 mouse-message translation, async key polling, keyboard-message dispatch, and the exact screen/client conversions intercepted by cnc-ddraw. Common `input.cpp` consumes logical injected mouse events and no longer includes Windows or accesses the native window; Alt-Tab minimization uses a small platform window service. The complete input-manager translation unit now passes strict GCC/Clang native compilation. The SDL translator supplies tested logical mouse/button/wheel events and compatible key records after applying SDL's renderer-coordinate conversion, including explicit Windows Set-1 scancodes rather than incompatible SDL HID scancode numbers. `Sdl3LegacyInputSink` delivers them to common `input.cpp`, mirrors event-driven physical-key state for configurable bindings, and releases held keys on focus loss. An end-to-end native test verifies keyboard character translation, scaled mouse coordinates, queue delivery, and modifier release through the real manager. SDL text-input/IME handling and production host/lifecycle wiring remain.

## Audio and Cinematic Assessment

FMOD coupling is confined to the Windows compatibility backend: the library is linked only into the Windows SGP target, and direct calls live in `sgp/audio/windows/FmodAudioBackend.cpp`. Native Linux uses SDL3_mixer through the same project-owned interface. Gameplay continues to use the existing `soundman.h`/`Sound Control` facade, while `soundman.cpp` retains cache/channel policy and behavior-sensitive timing.

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
- Continue runtime validation of the presenter-owned DirectDraw lifetime and restore semantics.
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
- Completed root integration of the 17 native shared-core, type, compression, CPU-rendering/image, text/process-service, timing/thread, and file-I/O tests; CI wiring remains.

Exit gate: the current backend is measurable enough to identify whether later differences are intentional.

### Phase 2: Make the Shared Core Compilable

Deliverables:

- Completed: backend-conditioned CMake structure and a `LINUX` shared-core target.
- Completed: fixed-width fundamental types, especially `FLAGS32`, plus a deliberate two-byte `CHAR16` design.
- Completed first boundary: neutral `sgp.h` is separated from `windows.h`/DirectDraw declarations; remaining consumers are explicitly marked through `LegacySGP.h` for incremental migration.
- Completed: central UTF-8/UTF-16 conversion service with strict/replacement policies, bounded conversion in both directions, full direct-call migration, and a configure-time regression guard.
- Pointer-safe callback/userdata payload designs for interfaces touched by platform work.

Exit gate: a meaningful shared source subset compiles under a native GCC/Clang toolchain without pretending to be Windows.

### Phase 3: Implement Platform Core Backends

Deliverables:

- Application host and event-loop interface with Win32 and SDL/Linux implementations.
- Host event-loop, sleep, synchronization, and any independently required thread lifecycle backends.
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

1. Completed: conditioned CMake and established the `ja2_shared_core` Linux target with an integrated smoke test.
2. Completed first process-service boundary: portable command-line handling, backend-owned restart/single-instance behavior, dialogs, and explicit native telemetry policy.
3. Completed first audio seam: put `Audio::Backend` in front of FMOD, retained the existing Windows behavior in an adapter, added a native null backend plus interface test, and compiled the complete legacy sound-manager policy in the native shared core.
4. Completed: extracted monotonic time and timer scheduling, removed the timer threads, and migrated the remaining active input synchronization and detached worker lifecycle to portable C++17 primitives.
5. Completed presentation isolation: the application loop, cursor/input backend, exactly-once shutdown, dirty-region tracker, presenter contract, and heap-backed indexed/RGB565 logical surfaces are portable and tested. Every generic and reserved logical surface is PixelSurface-canonical. Tactical scrolling and cursor composition are portable, and DirectDraw receives cadence-limited completed compositions only at the Windows presentation boundary without blocking the game thread.
6. Introduce the SDL window, input translation, and framebuffer presenter after that coherent surface conversion; SDL should not enter engine-facing headers or the existing software blitters.

Deferring SDL preserves a runnable Windows compatibility oracle while the current `WndProc` responsibilities--lifecycle, timers, input, focus, and rendering--are separated and DirectDraw types are removed from shared interfaces.

## Native x64 Save Compatibility Checkpoint (2026-09-18)

The native Linux x64 build now serializes pointer-bearing and ABI-sensitive save records through fixed Win32-compatible disk layouts instead of dumping host-native structures. This includes soldier data and paths, strategic groups and events, schedules, underground sectors, campaign incidents, laptop state, militia paths, the item cursor, and related records. Keyring reconstruction was also corrected to use the saved soldier ID and to allocate or release storage according to the serialized presence flag.

Compatibility was exercised against three controlled Win32 saves (game start, one recruited mercenary, and tactical entry) plus 29 older Wine/Win32 saves. All loaded in native x64. A feature-rich tactical save was then round-tripped native x64 -> current Win32/Wine -> native x64, and the same native save loaded in native i686. The save stream was consumed to its expected final byte in these tests.

Remaining validation is behavioral rather than a known format failure: play for an extended period after loading representative older saves, save again from both platforms, and confirm strategic/tactical transitions and mod-specific state. Keep the legacy Win32 save layout as the single supported on-disk format; host pointer width and compiler ABI must never define new save data.
