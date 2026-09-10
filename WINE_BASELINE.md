# Windows/x86 Baseline Under Wine

This is upstream JA2 1.13, not a replacement engine. The starting commit is
`ddb691318eb3dd0cdc6eab42139739b6d498c645`, on branch `port/wine-baseline`.
The original Windows backend, MSVC inline assembly, and legacy libraries remain.
No SDL implementation or platform-boundary extraction is included yet.

## Validated Toolchain

`tools/wine-baseline.json` pins the wrapper revision, installer manifest URL and
actual downloaded-byte SHA-256, and resolved toolchain versions:

| Component | Version / selection |
| --- | --- |
| msvc-wine | `514f8ea34842cd6d831804d0e9658d3a32870ae1` |
| VS 2022 installer | `17.14.40` |
| MSVC tools directory | `14.44.35207` |
| Actual cl.exe / linker | `19.44.35228` / `14.44.35228` |
| Host / target | x64-hosted tools / x86 output |
| Windows SDK | `10.0.26100.0` |

The manifest URL's embedded hash is not the hash of the bytes served during
validation. The lock deliberately verifies the observed SHA-256
`b60efac8768e31b4b0bb74d312a3fd3145de9bef7f794c050d498d079e540e11`.
Do not replace it automatically if Microsoft changes the response. The saved
manifest and downloader cache should be retained for future reproduction;
continued availability of Microsoft's download URLs is outside this repository.
This pins inputs, not byte-for-byte reproducible PE/PDB output.

Validated on Manjaro on 2026-09-10 with Wine `11.14-2`, CMake `4.4.2-1`, Ninja
`1.13.2-3`, Python `3.14.6-1`, msitools `0.106-3`, libgsf `1.14.58-1`,
cabextract `1.11-3`, and gcab `1.6-2`.

## Setup and Build

Required: Python 3, Git, Wine with both 64-bit tool execution and 32-bit game
support, CMake 3.25+, Ninja, `msiextract` (msitools), `cabextract`, `gcab`,
`unzip`, Perl, and ordinary POSIX shell utilities. The script checks prerequisites;
it never installs system packages or invokes sudo. Runtime also needs a working
desktop and audio. Run from a normal Linux shell, not Steam's bundled Wine
environment. A 64-bit Wine prefix is intentional even though JA2 is 32-bit.

Microsoft tools are proprietary and are not committed or redistributed here.
Read and accept the [Visual Studio license](https://go.microsoft.com/fwlink/?LinkId=2183758)
before using the explicit acceptance switch. The original provisioning was
performed with the user's license acceptance and download/install authorization.

From the repository root:

```sh
python3 tools/wine-baseline.py setup --accept-license
python3 tools/wine-baseline.py build --jobs 12
```

Setup clones the pinned wrapper, verifies and retains the installer manifest,
records `package-selection.txt`, downloads the x64-hosted x86 MSVC tools and SDK,
and installs their wrappers. ATL and ASan packages are not requested. It also
builds msvc-wine's own `msvctricks.exe`, which its installer skips for an x86-only
target installation. That helper translates `mt.exe`'s special manifest-update
exit code (`0x41020001`) into the Unix-side CMake sentinel (`0xbb`). Without it,
the first CMake compiler probe fails. No custom replacement helper is used.
Setup reapplies the wrapper installer and builds the helper under a temporary
name before publishing it, so an interrupted helper build can be retried.
Rerunning setup may touch SDK headers and trigger recompilation; normally only
the build command is needed after initial setup.

Build uses native Linux CMake/Ninja with the real Microsoft `cl`, `link`, `lib`,
`rc`, and `mt` wrappers. It configures upstream CMake directly, not an old Visual
Studio project. The selected settings are `Applications=JA2`, target `JA2`,
`RelWithDebInfo`, static CRT (`/MT`), embedded compiler debug info (`/Z7`),
ASan off, LTO off, and compiler caching off. `/MT` and embedded debug information
are set before compiler probes. A linker PDB is still produced; `/Z7` does not
mean that final link debugging information is disabled. Unrelated developer
tools and other applications are not built as top-level targets.

Output: `build/wine-baseline/bin/JA2.exe` and `JA2.pdb`.
Only a successful scripted build writes `build/wine-baseline/baseline-build.json`,
recording the source commit/status at build start and the resulting executable
hash. A failed configure/build invalidates that record. Preparation and launch
require a valid record rather than associating a stale binary with newer sources.
Do not edit sources during a build if you need an unambiguous provenance record.

Open-source dependencies managed through CMake `FetchContent` are pinned to
immutable Git commits or official release archives with cryptographic URL hashes
in `cmake/dependencies/`. The default configure populates a missing dependency
in `build/wine-baseline/_deps`; later builds reuse it.
For an existing checkout or offline build, set CMake's standard
`FETCHCONTENT_SOURCE_DIR_<NAME>` cache variable to the dependency root (for
example, `FETCHCONTENT_SOURCE_DIR_EXPAT`) and use the standard FetchContent
disconnected options as appropriate. Each adapter owns source selection,
feature definitions, warning policy, includes, and a namespaced target. Version
upgrades and changes to this packaging mechanism remain separate commits. Local
Git-backed overrides must be checkouts at the pinned commit; any tracked
modifications are explicit developer input. Archive-backed dependencies,
including Lua 5.1.5 and the LZMA SDK, verify all production source inputs by
SHA-256, including local overrides. Unpatched, automatically fetched Git
dependencies must remain clean. Patched dependencies keep their patches in-tree
and verify the resulting production inputs by SHA-256.
Libsmacker is pinned to upstream commit `76094fb9c8e98bd5fac982c504e8d9aeff3ece01`
and carries a downstream decoder patch for localized intro videos whose headers
understate per-frame audio sizes. The patch permits bounded buffer growth and
rejects malformed block and decoded-sample sizes instead of exposing an invalid
audio range to the game.
The bfVFS 7-Zip backend uses the upstream LZMA SDK 9.22 archive preserved by a
timestamped Debian snapshot. Only its ANSI-C decoder closure is built; bfVFS
still owns the writer and still supports its existing non-solid, Copy-method
archive layout rather than arbitrary compressed 7-Zip archives.

For an incremental check without reconfiguring:

```sh
ninja -C build/wine-baseline -n JA2
```

CMake regenerates the Bink import library during configuration, so invoking the
build script again may relink JA2 even if no source changed. Do not use this
build directory for a different architecture, compiler, or configuration.

## Runtime

Supply a legitimate original JA2 installation, not the Wildfire game assets or
an unrelated 1.13 mod overlay. The tested source was Steam's JA2 **Classic**
installation, copied by the user into `JA2Classic/Game/`. That whole supplied
directory is ignored. The Steam launcher was inspected but not executed: it
uses an obsolete bundled Wine and forces every monitor to 640x480. Those settings
are not needed by this baseline and are not inherited.

```sh
python3 tools/wine-baseline.py prepare "$PWD/JA2Classic/Game"
python3 tools/wine-baseline.py run
```

`prepare` also accepts the retail `Data` directory directly. It checks required
archives against `gamedir/vfs_config.JA2113.ini`, rejects symlinks and
case-colliding inputs, then copies the retail data and only Git-tracked files
from this checkout's matching `gamedir` into `.local/wine-baseline/game/`.
Untracked retail data, saves, and generated files already in `gamedir` are not
imported; shipped profile defaults and DLLs are retained. Copying uses temporary
staging so a failed copy does not leave a partial final game directory.
It copies the newly built executable,
not Steam's retail executable, and records source status, executable hash, and
retail archive hashes in `runtime-provenance.json`. The source installation,
its saves, and tracked `gamedir` are not modified. Only the staged `Ja2.ini` has
crash telemetry disabled to avoid external uploads during development.

Preparation refuses to overwrite an existing staged game, preserving its saves
and configuration. For normal iteration, close the game, run `build`, then `run`:
launch refreshes only `JA2.exe` from the last successful build and updates the
build provenance, leaving assets, saves, and settings intact. It prints the
build's source commit and executable hash. If matching 1.13 data has changed,
move the old staged `game` directory aside and prepare a new data snapshot.
Keep the old `Profiles/` and provenance record with that snapshot if testing
save compatibility. Do not run setup/build/prepare/run concurrently.

`run` uses the staged directory as the working directory and a separate
`prefix-game`, not the compiler prefix or `~/.wine`. It preinstalls upstream's
per-executable `ddraw=native,builtin` registry override to avoid upstream's
first-launch restart. It keeps the bundled `ddraw.dll`, `ddraw.ini`, shaders,
`binkw32.dll`, and `fmod.dll`. Do not substitute compiler DLL overrides or copy
SDK DLLs beside the game. The game directly imports no dynamic MSVC runtime;
FMOD imports Wine's normal `MSVCRT.dll`.

## Local Files

All of the following are ignored and must stay out of commits:

- `.local/wine-baseline/msvc/`: proprietary compiler and SDK plus wrappers.
- `.local/wine-baseline/msvc-wine/`: pinned external wrapper checkout.
- `.local/wine-baseline/*.manifest`, `downloads/`, and `package-selection.txt`:
  installer inputs, cache, and resolved packages.
- `.local/wine-baseline/prefix-build/` and `prefix-game/`: separate Wine state.
- `.local/wine-baseline/game/`: staged assets, runtime configuration, and saves.
- `.local/wine-baseline/runtime-provenance.json` and `runtime-window.png`:
  local validation evidence.
- `JA2Classic/`: user-supplied Steam installation.
- `build/`: executable, PDB, object files, and CMake/Ninja state.

The Wine prefixes are isolation for configuration and dependency management,
not security sandboxes. Wine's default Z: mapping exposes accessible host paths.
Respect workspace permissions; do not use Wine or scripts to bypass blocked
directory access. The earlier `JA2-cmake` checkout is not an input to this work.

## Validation Status

As of 2026-09-10:

- **Compiled:** actual Microsoft x86 JA2 completed compilation and linking.
  `file` identified an Intel i386 PE32 Windows GUI executable. Microsoft PE
  dependency inspection confirmed Bink/FMOD/DirectDraw and normal Windows imports.
- **Build fixes:** three constant guards became `if constexpr`; C4127 is
  suppressed only around one existing VFS macro invocation. `/W4 /WX` remains
  enabled. No gameplay formulas, feature stubs, or vendor code changed.
- **Setup exercised:** full download/install commands, helper compilation,
  scripted existing-toolchain setup, and scripted configure/build all succeeded.
  The combined script's cold-install path has not separately been rerun from an
  empty toolchain directory.
- **Workflow guards:** tests exercised real-data staging, temporary-copy cleanup
  after an injected failure, executable refresh without changing saves/settings,
  required build provenance, isolated wrapper environment, explicit license
  acceptance, positive job counts, and refusal to overwrite an existing runtime.
- **Ran:** the compiled executable launched with the supplied Classic archives
  and matching 1.13 data. A window capture showed the in-game A.I.M. laptop and
  hiring dialog. The user confirmed menus/input, working audio, and tactical play.
- **Save/reload:** the user subsequently confirmed saving and loading works.
- **Not verified:** long sessions, full rendering fidelity, all
  resolutions, multiplayer, editors/UB, clean native modern-Windows execution,
  and behavioral equivalence beyond the smoke test. Wine emitted EGL/driver
  warnings during initialization, but the game reached interactive gameplay.

Next, validate native Windows before treating this as broad
regression coverage. Future platform work should extract narrow, behavior-neutral
boundaries, retain this backend, and add selectable SDL implementations one at a
time. This successful Wine smoke test does not establish an SDL or Linux-native
port, nor prove the absence of gameplay or rendering bugs.
