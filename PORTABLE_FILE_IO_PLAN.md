# Portable File I/O Migration Plan

## Status

Implementation checkpoint completed on 2026-09-15:

- Stages 1-2 are implemented for the current compatibility scope: project-owned file/store
  interfaces, generation-checked legacy handles, a bfVFS resource adapter, confined physical
  writable stores, namespace routing, writable-profile refresh, and explicit log/scratch paths.
  The roots still share the active writable profile physically; independently configurable
  per-purpose roots remain a later policy refinement.
- Stage 3 is implemented for save publication: save and optional `.IPQ` publication use a
  durable journaled transaction with recovery. Embedded tactical-file extraction currently
  preserves the legacy delete/create/read/write sequence; transactional scratch extraction
  remains follow-up work after its caller-visible behavior is characterized.
- Stages 4-5 are implemented for active runtime paths. Host file calls and direct bfVFS
  file/profile iteration have moved behind the portable services. The allocation-minimal crash
  writer, process restart code, compile-time-only legacy diagnostics, and standalone tools remain
  deliberately outside the runtime service.
- Stage 6 has been evaluated: bfVFS remains the resource backend. `sgp/PngLoader.cpp` retains its
  specialized bfVFS-backed JPC/7z provider as required by the plan.
- Four native CTest executables cover physical I/O and confinement, routing, save recovery,
  local-time conversion, and logging. The Windows/x86 `JA2` target builds successfully under the
  Wine/MSVC baseline; startup plus strategic and tactical save loading have been playtested.

Stage 0 is not a finished release gate. The full profile/archive fixture matrix, retail-data
playthroughs, save compatibility corpus, native Windows run, and independently configurable
per-purpose roots remain follow-up validation and product work. This checkpoint is therefore a
buildable migration boundary, not a final playtest release.

Post-checkpoint platform work now includes a selected POSIX durability backend with native
`fsync`, no-clobber publication, and directory synchronization, plus Linux modification timestamps
and `/proc/self/exe` discovery. These paths have native integration coverage while the Windows
backend remains the production game target.

## Goals

- Remove Win32 file, directory, path, enumeration, and timestamp types from public engine interfaces.
- Preserve current resource lookup behavior, including bfVFS profiles, overlays, named-profile reads, SLF archives, and 7z/JPC resources.
- Preserve savegame and temporary-file byte formats.
- Keep the current Windows/Wine build working throughout the migration.
- Make physical file access implementable with portable C++17 facilities without requiring SDL.
- Remove the `HWFILE` pointer-to-`UINT32` truncation so the file layer is not inherently limited to 32-bit processes.
- Make writable locations explicit instead of routing resources, saves, settings, logs, and scratch files through one global VFS.

## Non-Goals

- Replacing bfVFS, SLF, or 7z support in the first migration stage.
- Changing save serialization or resource formats.
- Rewriting resource consumers in one pass.
- Introducing SDL or requiring an SDL context.
- Changing the spelling or case behavior of existing logical resource names until compatibility tests define it.
- Making the whole game 64-bit as part of this work.

## Design Rules

1. Logical resource names and physical filesystem paths are different types.
2. Public game headers do not include `windows.h`, expose Win32 handles, or expose bfVFS classes.
3. Resource access is read-only; saves, user files, and scratch data use separate writable stores.
4. File objects either own their stream or hold an explicit lifetime-tracked lease, and close it through RAII.
5. Legacy `FileMan` functions remain adapters until their callers can migrate safely.
6. Backend completion and error details are translated to project-owned error values at the boundary.
7. Every migration stage keeps a working Windows backend and preserves existing on-disk bytes.
8. Native path conversion occurs only in a physical-filesystem backend.

## Original Public Boundary Problems

- `sgp/FileMan.h` includes `windows.h` and exposes `DWORD`, `FILETIME`, and Win32-derived flags.
- `sgp/types.h` defines `HWFILE` as `UINT32`; the current implementation stores a file-object pointer in that value.
- The declared access/create flags imply more modes than the implementation currently honors.
- File enumeration is represented by a fixed-size, Win32-shaped `GETFILESTRUCT`.
- Resource and writable-file operations share one global abstraction despite requiring different behavior.
- Direct bfVFS and operating-system calls let consumers bypass `FileMan` semantics.
- There are no conventional tests describing the behavior of the 213 `FileMan` integration files, 73 direct bfVFS integration files, or 25 direct host-I/O files.

## Audited Legacy Stack

The configured Wine baseline is a Windows x86 build, so bfVFS selects its Win32 physical-file implementation even though the executable runs under Wine.

```text
game caller
  -> FileOpen / FileRead / FileWrite / FileSeek / FileClose
  -> sgp/FileMan.cpp
  -> cached vfs::IBaseFile selected by CVirtualFileSystem
  -> CDirFile or archive member
  -> bfVFS TFile/CFile
  -> CreateFile / ReadFile / WriteFile / SetFilePointerEx / CloseHandle
```

Key locations:

- `sgp/FileMan.cpp:287-320` selects a bfVFS file and casts its pointer to `HWFILE`.
- `sgp/FileMan.cpp:350-357`, `407-415`, `486-496`, and `636-643` cast it back for close, read, write, and seek.
- Fetched bfVFS `src/Core/File/vfs_file.cpp:115-619` is the physical system-call boundary.
- Fetched bfVFS `src/Core/Location/vfs_directory_tree.cpp:415-471` scans physical directories into the startup catalogue.
- Fetched bfVFS `src/Core/Location/vfs_uncompressed_lib_base.cpp:197-218` seeks and reads shared archive streams.
- `sgp/sgp.cpp:985-1011` selects VFS configuration; `sgp/sgp.cpp:443-457` initializes it and marks writable/exclusive paths.

### Profile and Path Behavior

The default `gamedir/vfs_config.JA2113.ini` profile precedence is:

```text
Player Profile       Profiles/UserProfile_JA2113, writable
v1.13                Data-1.13
Mod Base Files       Base
Vanilla Dirs         Data
SLF Libs             Data/*.slf
```

- New files go to the first writable profile.
- Multiplayer temporarily inserts another writable profile at the front.
- Named profile lookup uses the configured display name and is case-sensitive.
- Normal virtual path lookup is intended to be case-insensitive and accepts both slash styles.
- Relative profile roots are resolved from process CWD, not reliably from the executable directory.
- `FileExists` normally checks the startup-built catalogue and does not see files created externally afterward.
- Deleting an ordinary writable override reveals any lower-profile version because bfVFS has no tombstones.
- `Temp`, `ShadeTables`, localized saves, and multiplayer saves are exclusive namespaces: lookup must stop at the writable profile even when a lower profile contains the same name.

These are compatibility requirements until tests and an explicit product decision say otherwise.

### Original Direct Runtime Bypasses

The audit found substantive host file/path handling in 25 first-party C/C++ files. Most occurrences are diagnostics or dormant tools; the active runtime migration targets are:

| Location | Current bypass | Required replacement |
|---|---|---|
| `Tactical/Handle Items.cpp:8761-8827` | Opens through bfVFS, extracts `_getRealPath`, then reopens fortification plans with `std::ifstream`/`std::fstream` | Writable-store text stream; reads must not create missing files |
| `Tactical/Handle Items.cpp:9613-9771` | Same physical-path escape for equipment templates plus `FindFirstFile` enumeration | Writable-store read/write and owned enumeration |
| `Ja2/SaveLoadScreen.cpp:1348-1362` | `_getRealPath`, `CreateFile`, `GetFileTime`, time-zone conversion | Writable-store metadata with project timestamp formatting |
| `TileEngine/worlddef.cpp:359-447` | Executable-relative `engine.ini` read through `GetPrivateProfileString` | VFS/profile-aware INI reader using a logical resource path |
| `TileEngine/lighting.cpp:182-205` | Dormant `ShadeTables.txt` `fopen` parser | Remove if obsolete or migrate to a resource line reader |
| `TacticalAI/AIMain.cpp:286-396` | Active `fopen`, `fputs`, `remove` diagnostics under `Logs` | Project logger or writable log store |
| `Tactical/LOS.cpp:5624-6830` | Spread diagnostics compiled into release because guards are commented out | Remove unintended release logging or route explicitly to log store |
| `Tactical/Civ Quotes.cpp:2305-2311` | Option-controlled `VoiceTauntLog.txt` CRT output | Project logger or writable log store |
| `Utils/INIReader.cpp:64-67` | `_splitpath`/`_makepath` for `.Override` logical names | Logical resource-path extension replacement |

### Specialized and Out-of-Scope Access

- `sgp/crash_report.cpp:109-242` deliberately uses allocation-free Win32 calls in crash context. Keep this in a small crash-safe platform backend; normal VFS/C++ stream allocation is not an acceptable replacement.
- `sgp/crash_telemetry.cpp:24-156` can use normal writable-store access outside crash context, but its report directory must stay coordinated with the crash-safe writer.
- `wine/wine.cpp:20-25` and `sgp/sgp.cpp:726-728` perform executable discovery and process restart, not resource I/O. Move path discovery behind a platform service, not `ResourceStore`.
- `tools/symbolize_crash.cpp`, `tools/wine-baseline.py`, CMake host-file operations, and Ja2Export's own bfVFS setup are standalone tooling concerns and do not need the game runtime service.
- Dormant CRT debug dumps should normally be deleted rather than migrated unless their controlling feature is restored and tested.

### Original Direct bfVFS Consumers

These bypass `FileMan` without bypassing bfVFS and therefore need the resource-store adapter before bfVFS headers can be removed:

| Operation | Current consumers |
|---|---|
| Direct read lease | `lua/lua_state.cpp:47-49`, `Utils/Cinematics.cpp:208-212` |
| Direct write lease | `Utils/XMLWriter.cpp:54-82`, `sgp/video.cpp:1944-1987` |
| Existence/file selection | `TileEngine/WorldDat.cpp:32-36` |
| VFS iteration | `Tactical/InterfaceItemImages.cpp:60`, `i18n/ExportStrings.cpp:600-643` |
| Per-profile reads | `Utils/INIReader.cpp:47-56`, `84-93` |
| Raw physical `vfs::CFile` fallback | `Multiplayer/transfer_rules.cpp:14-40`, `Utils/XMLProperties.cpp:173-207` |
| Physical directory/path escape | `Ja2/MPJoinScreen.cpp:323-325`, `Ja2/SaveLoadScreen.cpp:1347-1351` |

### Dormant and Development-Only CRT Access

The following should be evaluated for deletion before any portability work is spent on them:

| Location | Status/purpose |
|---|---|
| `Tactical/Campaign.cpp:1747-1820` | `JA2TESTVERSION` hard-coded `C:\Temp\StatChanges.TXT` dump |
| `Tactical/Items.cpp:10352-10371` | Test-build item dump |
| `Strategic/Strategic Town Loyalty.cpp:1375-1412` | No active caller found |
| `Strategic/Strategic Movement Costs.cpp:440-592` | XML export helper with only a commented call |
| `Ja2/TimeLogging.cpp:15-69` | Disabled load/save timing feature |
| `Utils/Multilingual Text Code Generator.cpp:148-154` | Debug-only language utility |
| `Utils/Debug Control.cpp:13-49` | Controlling macros are disabled |
| `sgp/vobject.cpp:1711-1769`, `sgp/vsurface.cpp:2715-2773` | Disabled video debugging dumps |
| `sgp/MemMan.cpp:661-729` | Disabled extreme-memory dump |
| `sgp/DEBUG.cpp:238-369` | Disabled legacy debug-file implementation |
| `Ja2/aniviewscreen.cpp:340-383` | In-game animation utility; leaks its successful CRT open |

CMake filesystem operations, `tools/wine-baseline.py`, and the standalone crash symbolizer are appropriate host-tool operations and are excluded from runtime migration.

### Known Defects Requiring Explicit Decisions

- The documented `FileOpen` create/disposition flags are mostly ignored. Any write option opens or creates without truncating; read/write becomes write-only; delete-on-close is unused.
- Multiple opens of one virtual path commonly share one bfVFS object, cursor, and open state. Closing one can invalidate another.
- `FileRead` treats an ordinary short EOF read as failure, while deeper exceptions are rethrown.
- Zero-byte `FileWrite` can dereference a null byte-count pointer.
- `FileLoad` can throw for a missing file before reaching its null check.
- `GetFileFirst/GetFileNext` use one process-global iterator even though bfVFS supports independent iterators.
- Enumeration copies names with `sprintf(name, filename)`, creating format-string and overflow defects.
- Archive members have per-member cursors but share an unsynchronized seek/read stream.
- End-relative archive seek is implemented like current-relative seek.
- Sizes, offsets, and handles are truncated to 32 bits in several layers.
- bfVFS's Win32 open path checks stale `GetLastError()` instead of only validating the returned handle.

Security, data-loss, and undefined-behavior defects should be fixed behind characterization tests; incidental quirks must not silently become a new compatibility contract.

## Compatibility-Sensitive Workflows

| Workflow | Contract to preserve |
|---|---|
| Bootstrap | Process-relative `Ja2.ini`, `Language.ini`, selected `vfs_config*.ini`, command-line property precedence, and pre-VFS physical reads |
| Profiles | JA2113, Vanilla, and UB profile order; named-profile reads; writable top layer; dynamic multiplayer profile; mount points and optional locations |
| INI/XML/Lua | Bottom-to-top merged INIs plus `.Override`, localization fallback/patch order, optional versus mandatory resources, BOM behavior, and backslash logical names |
| SLF | Fixed header/directory layout, little-endian fields, deleted-entry filtering, mount paths, member bytes, sizes, and seek behavior |
| JPC/JDC/7z | Existing Copy-method member naming/order, PNG and `appdata.xml` bytes, and memory-backed resource behavior |
| Saves | Localized names/extensions, raw ABI-sensitive structures, quicksave/autosave/assertion slots, and load compatibility |
| Save sidecars | A `.sav` and its `.IPQ` inventory sidecar form one recovery unit even though they are currently rewritten separately |
| Embedded tactical files | Exact `UINT32 size` plus payload representation and existing `Temp` sector-file names |
| Laptop scratch | Exact fixed records and append/rewrite behavior of `TEMP/files.dat`, `finances.dat`, and `History.dat` |
| Settings | Writable-profile location, key spelling, ordering expectations, truncation, and CRLF output |
| Images | Screenshot/capture naming and collision behavior plus byte-identical 16-bit TGA headers, rows, and pixels |
| Logs | Explicit destination and append/truncate policy for each maintained log; unintended release diagnostics should be removed |
| Crash telemetry | Collision-safe names, CRLF grammar, durable flush, consent byte, age/size limits, and delete-only-after-settlement behavior |
| Exporter | Independent source/destination profiles and golden PNG/JPC/JDC/SLF conversion fixtures |

Byte-identity gates apply to saves, `.IPQ`, tactical/laptop temporary blobs, EDT fixed records, SLF member reads, JPC/JDC output, screenshots, and generated settings. Logs and diagnostics generally require semantic rather than byte-for-byte equivalence.

## Target Model

Logical resource paths remain narrow project strings during the compatibility phase. Physical paths are private `std::filesystem::path` values constructed at the writable-store boundary.

- Treat logical paths as UTF-8 byte strings because Unicode-enabled bfVFS currently interprets `char*` paths as UTF-8.
- Normalize logical separators and case only inside `ResourceStore`; do not apply host filesystem rules to archive/profile names.
- Construct Windows physical paths through the native wide representation rather than the active ANSI code page.
- Confine every writable relative name beneath its configured root. Reject rooted, drive-relative, parent-traversing, device, alternate-data-stream, and UNC names; prevent symlink/junction escapes with canonical or no-follow containment checks.
- Do not depend on mutable process CWD after bootstrap configuration has resolved its base directory.

```cpp
namespace io
{
enum class SeekOrigin
{
	begin,
	current,
	end
};

struct Metadata
{
	std::uint64_t size;
	std::optional<std::int64_t> modifiedUnixNanoseconds;
	bool directory;
	bool readOnly;
};

struct DirectoryEntry
{
	std::string name;
	Metadata metadata;
};

class File
{
public:
	virtual ~File() = default;
	virtual std::size_t read(void* destination, std::size_t size) = 0;
	virtual void readExact(void* destination, std::size_t size) = 0;
	virtual void writeExact(const void* source, std::size_t size) = 0;
	virtual std::uint64_t seek(std::int64_t offset, SeekOrigin origin) = 0;
	virtual std::uint64_t position() const = 0;
	virtual std::uint64_t size() const = 0;
	virtual void flush() = 0;
	virtual void sync() = 0;
};

struct ResourceVersion
{
	std::string profile;
	std::unique_ptr<File> file;
};

class ResourceStore
{
public:
	virtual ~ResourceStore() = default;
	virtual std::unique_ptr<File> open(std::string_view resource) = 0;
	virtual std::unique_ptr<File> openFromProfile(
		std::string_view resource,
		std::string_view profile) = 0;
	virtual std::vector<ResourceVersion> openAll(std::string_view resource) = 0;
	virtual bool exists(std::string_view resource) const = 0;
	virtual std::vector<DirectoryEntry> list(std::string_view pattern) const = 0;
};

class WritableStore
{
public:
	virtual ~WritableStore() = default;
	virtual std::unique_ptr<File> openRead(std::string_view name) = 0;
	virtual std::unique_ptr<File> create(std::string_view name) = 0;
	virtual std::unique_ptr<File> createExclusive(std::string_view name) = 0;
	virtual std::unique_ptr<File> openReadWrite(std::string_view name) = 0;
	virtual bool exists(std::string_view name) const = 0;
	virtual Metadata metadata(std::string_view name) const = 0;
	virtual std::vector<DirectoryEntry> list(std::string_view pattern) const = 0;
	virtual void remove(std::string_view name) = 0;
	virtual void replace(std::string_view source, std::string_view destination) = 0;
};
}
```

The exact error API should be selected during implementation. It must distinguish not-found, already-exists, permission, invalid path, short read/write, unsupported operation, and generic I/O failure without exposing `GetLastError()` or `errno` to callers.

`modifiedUnixNanoseconds` is a project timestamp, not `std::filesystem::file_time_type`. C++17 provides no standard conversion from the filesystem clock to `system_clock`, so conversion belongs in a small backend/time adapter. Save-slot local-time formatting and DST behavior need a compatibility test.

### Initial Implementations

- `BfVfsResourceStore`: wraps the configured bfVFS profiles and retains all current overlay/archive behavior.
- `BfVfsFileLease`: opens and closes a non-owning cached bfVFS file object behind the project `File` interface.
- `PhysicalWritableStore`: confines names beneath a configured root and uses `std::filesystem` plus RAII standard streams.
- `LegacyFileRegistry`: maps generation-checked 32-bit `HWFILE` tokens to owned `File` leases so existing callers no longer carry pointers in integers.
- `LegacyFileMan`: implements the existing `FileOpen`, `FileRead`, `FileWrite`, and related functions over those objects.
- `StoreRouter`: routes read, write, existence, metadata, deletion, and enumeration by namespace while maintaining the active writable-profile stack and exclusive-path rules.
- `SaveTransaction`: stages `.sav`, optional `.IPQ`, and a recovery journal, then publishes or rolls back one recoverable generation.

The first adapter deliberately preserves bfVFS's shared-object behavior. Independent repeated opens require either a per-open bfVFS stream abstraction or a replacement resource provider and must be introduced as a separately characterized change.

`BfVfsFileLease` owns only the open/close lease, not the cached bfVFS object. All profile insertion/removal must be coordinated by `BfVfsResourceStore`: stop new leases, close or drain live leases belonging to the affected profile, mutate the profile stack, then resume access. Existing multiplayer `pushProfile`/`popProfile` callers must move through this gate during Stage 1.

There is no fully portable C++17 primitive for durable sync or atomic replacement. Save staging should use project-owned operations with small backend implementations: file/directory sync plus `ReplaceFileW` or `MoveFileExW` on Windows, and `fsync` plus same-filesystem `rename` on POSIX. No platform identifier should escape those implementation files.

## Migration Stages

### Stage 0: Characterize Current Behavior

- Add a small CTest executable without introducing a test framework dependency.
- Split the portable file-layer test target from the game-only `WIN32` configuration gate and global MSVC flags so it can configure on a native host before claiming portability.
- Record actual `FileOpen` behavior for every access/create flag combination instead of assuming the header comments are correct.
- Record that no current caller uses `FILE_CREATE_NEW`, `FILE_TRUNCATE_EXISTING`, or `FILE_ACCESS_READWRITE`; this allows corrected semantics in the new API while the legacy adapter preserves used behavior.
- Test short reads, zero-byte files, seek origins, write failures, and close behavior.
- Test repeated opens of one physical resource and one archive member, including interleaved reads and close order.
- Test resource overlay precedence, named profiles, path separator handling, case behavior, SLF, and 7z/JPC reads.
- Test dynamic multiplayer profiles, exclusive namespaces, writable deletion, nested enumeration, simultaneous enumeration, and the first-result behavior used by `Tile Cache.cpp`.
- Test removal of a multiplayer profile while files from that profile are open; Stage 1 must reject, drain, or safely invalidate those leases.
- Add archive-negative fixtures for truncated/out-of-bounds SLF and compressed, solid, multi-coder, duplicate, traversal, bad-CRC, and oversized 7z entries.
- Test merged INIs, `.Override`, localized XML fallback, EDT record sizes, Lua BOM handling, and missing optional/mandatory resources.
- Save and reload representative games, then compare produced files where deterministic bytes are expected.

Exit criterion: tests describe compatibility that later stages must preserve or intentionally change.

### Stage 1: Introduce Portable Types and Ownership

- Add the project-owned `File`, metadata, directory-entry, resource-store, and writable-store interfaces.
- Implement `BfVfsFileLease` and `BfVfsResourceStore` without changing gameplay call sites.
- Replace pointer-cast `HWFILE` values with generation-checked registry tokens.
- Keep `HWFILE` as a compatibility typedef temporarily, but move it out of the global platform type header.
- Make file objects non-copyable and automatically closed.
- Route dynamic multiplayer profile insertion/removal through the resource store and drain affected leases before bfVFS deletes profile-owned locations.

Exit criterion: all existing `FileMan` callers run through registry-owned leases, no pointer is stored in `UINT32`, and existing shared-open behavior has not changed accidentally.

### Stage 2: Separate Read-Only Resources from Writable Data

- Define explicit roots for resources, saves, user settings/data, logs, and scratch files.
- Continue using bfVFS only for resource/profile/archive reads.
- Route every legacy operation through `StoreRouter`, including read, write, exists, metadata, delete, and enumerate; routing only writes would leave newly created files invisible to bfVFS's startup catalogue.
- Model the active writable-root stack, dynamic multiplayer root, and exclusive namespaces explicitly rather than caching one startup write directory.
- Route writable operations through `PhysicalWritableStore` and immediately make their matching reads and enumeration visible through the same store.
- Reject path escape through textual traversal, symlinks, junctions, drive-relative paths, UNC paths, device names, and alternate data streams.
- Preserve current logical resource-name normalization separately from native path handling.

Exit criterion: save/user/temp writes do not depend on VFS overlay selection.

### Stage 3: Make Save Writes Recoverable

- Write each save to a uniquely named sibling temporary file.
- Flush, durably sync, and close every completed temporary file before publication.
- Write and durably sync a versioned transaction journal describing the transaction ID and unique old, staged, backup, and final names before modifying either final file.
- Move existing `.sav` and `.IPQ` files to recoverable backup names, publishing the new `.IPQ` first and the new `.sav` last.
- Before each backup or publication operation, append and durably sync a write-ahead intent record. After the operation and directory sync, append and durably sync its completion record.
- Make journal records length-delimited and checksummed so recovery can reject a torn final record and retain the last valid prefix.
- On startup and before save discovery, reconcile every intent idempotently against the uniquely named staged, backup, and final files, then deterministically complete publication or restore both backups; never expose a mixed pair to the loader.
- Durably sync the containing directory where supported, then remove backups and the journal only after both final files are committed.
- Retain or clean abandoned pre-journal temporary files according to an explicit recovery policy.
- Do not change save serialization or embedded temporary-file formats.
- Treat a missing optional `.IPQ` as explicit transaction state, not as an implicit leftover from another generation.
- Stage extraction of embedded tactical files so invalid lengths or failed writes do not destroy the previous scratch state.

Exit criterion: failures before open, during serialization, during sync, during sidecar write, and before each publication leave a journal that startup recovery resolves to one internally consistent, readable save generation.

### Stage 4: Remove Direct Operating-System Bypasses

- Replace direct metadata calls with `WritableStore::metadata`.
- Replace Win32 enumeration with owned directory result objects.
- Replace direct `std::fstream` opens of paths obtained from bfVFS with store/file operations.
- Route current-directory, executable-directory, deletion, rename, and temporary-path operations through narrow services.
- Keep crash handling in a minimal platform module where using low-level OS facilities is required for reliability.
- Keep build utilities independent from the runtime service when they do not link game code.

Exit criterion: gameplay and engine consumers contain no direct Win32 file/path APIs.

### Stage 5: Migrate Direct bfVFS Consumers

- Convert direct bfVFS users in small subsystem batches.
- Preserve named-profile and all-layer reads through explicit resource-store operations.
- Keep archive-aware resource loading distinct from writable physical files.
- Remove bfVFS file/path/profile/iterator types from file-I/O public headers. Broader dependencies such as `vfs::Log`, `vfs::HPTimer`, `vfs::String`, and `vfs::PropertyContainer` require separate logging/configuration cleanup and are not hidden by this plan.

Exit criterion: only `BfVfsResourceStore`, VFS initialization, and separately tracked non-file bfVFS services include bfVFS file/profile headers.

### Stage 6: Evaluate bfVFS Replacement

- Retain the existing adapter if it remains reliable and its LGPL obligations are acceptable.
- If replacement is still desired, implement directory and SLF providers behind `ResourceStore` first.
- Retain a dedicated 7z/JPC provider; Stracciatella's resource stack does not provide equivalent behavior.
- Switch providers only after running the complete resource compatibility matrix.

Exit criterion: provider replacement, if any, does not affect callers or resource precedence.

## Validation Matrix

| Area | Required validation |
|---|---|
| Open modes | Read, write, read/write, create-new, create-always, open-existing, open-always, truncate, delete-on-close |
| Binary I/O | Exact and partial reads/writes, zero-byte files, seek/tell, EOF, files larger than 4 GiB in isolated tests |
| Resource lookup | Directory resources, profile precedence, named profile, case variants, slash variants, missing resource |
| Path confinement | Dot segments, non-ASCII names, case collisions, symlink/junction escape, drive-relative/UNC/device/ADS names |
| Archives | SLF listing/read/seek; bounds/deletion failures; valid JPC/JDC; reject compressed/solid/multi-coder, duplicate, traversal, bad-CRC, and oversized 7z inputs |
| Enumeration | Wildcards, attributes, empty directories, nested enumerations, independent simultaneous enumerations |
| Writable roots | Saves, settings, logs, screenshots, exports, scratch files, namespace routing, dynamic writable-profile changes |
| Save safety | Failure before write, during write/sync, `.IPQ`, journal phases, backup moves, and each publication; startup recovery yields one consistent generation |
| Save compatibility | Existing saves load; newly written saves reload; no unintended serialization changes |
| Runtime | JA2113, Vanilla, and UB profiles: Wine launch, new game, load, save, quicksave, quickload, tactical transition, clean shutdown |
| Tooling | JA2 exporter and crash symbolizer continue to process their expected files |
| Compilers | x86 MSVC baseline, clang-cl where available, and an independently configurable host-native portable file-layer test target |

## Commit Sequence

1. Add characterization tests for current `FileMan` and resource behavior.
2. Add portable file/store interfaces and the bfVFS adapter.
3. Replace pointer-valued `HWFILE` with registry tokens.
4. Add physical writable stores and explicit roots.
5. Make save replacement staged and recoverable.
6. Migrate Win32 metadata and enumeration bypasses.
7. Migrate direct standard-stream and CRT bypasses.
8. Migrate direct bfVFS consumers by subsystem.
9. Remove Win32 and bfVFS file-I/O types from public headers.
10. Decide separately whether to replace the bfVFS provider.

Each commit must build and run through the Wine baseline before the next stage starts.

## Licensing

- bfVFS is LGPL-2.1-or-later; retaining or statically distributing it requires continued license-compliance review.
- Stracciatella is useful as a behavioral reference, but the shallow reference checkout cannot establish line-level provenance for inherited code.
- Reimplement interfaces and algorithms from documented behavior and tests rather than copying Stracciatella source verbatim.
