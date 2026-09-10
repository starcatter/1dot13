#!/usr/bin/env python3
"""Reproduce the Windows/x86 baseline without modifying a retail installation."""

import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import tempfile
import urllib.request


ROOT = Path(__file__).resolve().parents[1]
STATE = ROOT / ".local/wine-baseline"
BUILD = ROOT / "build/wine-baseline"
BUILD_RECORD = BUILD / "baseline-build.json"
MSVC = STATE / "msvc"
BIN = MSVC / "bin/x86"
GAME = STATE / "game"
LOCK = json.loads((ROOT / "tools/wine-baseline.json").read_text())


def run(command, *, cwd=ROOT, env=None):
    command = [str(arg) for arg in command]
    print("+ " + shlex.join(command), flush=True)
    subprocess.run(command, cwd=cwd, env=env, check=True)


def wine_env(kind):
    env = os.environ.copy()
    # Never inherit another prefix, Wine distribution, or compiler DLL overrides.
    for key in ("WINEPATH", "WINEDLLOVERRIDES", "WINELOADER", "WINESERVER"):
        env.pop(key, None)
    env.update(WINEPREFIX=str(STATE / f"prefix-{kind}"), WINEARCH="win64")
    env.setdefault("WINEDEBUG", "-all")
    if kind == "build":
        for key in list(env):
            if key.startswith("WINE_MSVC_"):
                env.pop(key)
        env["PATH"] = str(BIN) + os.pathsep + env["PATH"]
        for key in ("CC", "CXX", "CL", "_CL_", "LINK", "_LINK_",
                    "INCLUDE", "LIB", "LIBPATH", "CFLAGS", "CXXFLAGS", "LDFLAGS"):
            env.pop(key, None)
    return env


def setup(accept_license):
    if not accept_license:
        raise RuntimeError("Read the Microsoft license linked in WINE_BASELINE.md; "
                           "pass --accept-license only if you accept it.")
    for command in ("git", "wine", "wineserver", "cmake", "ninja", "msiextract",
                    "cabextract", "gcab", "unzip", "sed", "awk", "find", "perl"):
        if not shutil.which(command):
            raise RuntimeError(f"Missing prerequisite: {command}; no packages installed automatically")
    STATE.mkdir(parents=True, exist_ok=True)
    wrapper = STATE / "msvc-wine"
    if not wrapper.exists():
        run(["git", "clone", LOCK["wrapper_url"], wrapper])
        run(["git", "checkout", "--detach", LOCK["wrapper_revision"]], cwd=wrapper)
    revision = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=wrapper, text=True).strip()
    dirty = subprocess.check_output(["git", "status", "--porcelain"], cwd=wrapper, text=True)
    if revision != LOCK["wrapper_revision"] or dirty:
        raise RuntimeError("Wrapper checkout differs from the lock; preserve it and inspect manually")

    manifest = STATE / f'{LOCK["visual_studio"]}.manifest'
    data = manifest.read_bytes() if manifest.exists() else urllib.request.urlopen(LOCK["manifest_url"], timeout=120).read()
    if hashlib.sha256(data).hexdigest() != LOCK["manifest_sha256"]:
        raise RuntimeError("Installer manifest hash mismatch; refusing a drifting toolchain")
    if not manifest.exists():
        manifest.write_bytes(data)
    selection = [sys.executable, wrapper / "vsdownload.py", "--manifest", manifest,
                 "--major", "17", "--msvc-version", LOCK["msvc_selection"],
                 "--sdk-version", LOCK["sdk_selection"], "--architecture", "x86",
                 "--host-arch", "x64", "--with-default", "no", "--with-msvc", "yes",
                 "--with-sdk", "yes", "--with-atl", "no", "--with-asan", "no",
                 "--accept-license"]
    with (STATE / "package-selection.txt").open("w") as output:
        subprocess.run([str(arg) for arg in selection + ["--print-selection"]], check=True, stdout=output)
    if not (BIN / "cl").exists():
        run(selection + ["--dest", MSVC, "--cache", STATE / "downloads"])
    # A wrapper filename is not an installation-complete marker. Reapplying
    # the idempotent installer also finishes an interrupted wrapper install.
    run([wrapper / "install.sh", MSVC], env=wine_env("build"))

    settings = (BIN / "msvcenv.sh").read_text()
    for expected in (f'MSVCVER={LOCK["msvc_directory_version"]}',
                     f'SDKVER={LOCK["sdk_directory_version"]}', "ARCH=x86"):
        if expected not in settings.splitlines():
            raise RuntimeError(f"Unexpected installed toolchain: missing {expected}")
    # install.sh skips this helper when the native x64 target was not installed.
    # It maps mt.exe's 0x41020001 result to CMake's Unix-side 0xbb sentinel.
    helper = STATE / "msvctricks-new.exe"
    bootstrap_env = wine_env("build")
    # Bootstrap without executing an old or interrupted helper, and never link
    # over the helper currently executing the compiler/linker wrappers.
    bootstrap_env["WINE_MSVC_RAW_STDOUT"] = "1"
    run([BIN / "cl", "/nologo", "/EHsc", "/O2", "/MT",
         f"/Fe{helper}", f"/Fo{STATE / 'msvctricks.obj'}",
         wrapper / "msvctricks.cpp"], env=bootstrap_env)
    helper.replace(MSVC / "bin/msvctricks.exe")
    run([BIN / "cl"], env=wine_env("build"))
    print(f"Toolchain ready; manifest and package selection retained in {STATE}")


def build(jobs):
    if not (MSVC / "bin/msvctricks.exe").is_file():
        raise RuntimeError("Run setup first; the manifest exit-code helper is required")
    env = wine_env("build")
    BUILD_RECORD.unlink(missing_ok=True)
    source = {"source_commit": subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "source_status": subprocess.check_output(["git", "status", "--short"], cwd=ROOT, text=True)}
    command = ["cmake", "-S", ROOT, "-B", BUILD, "-G", "Ninja",
               "-DCMAKE_SYSTEM_NAME=Windows", "-DCMAKE_SYSTEM_PROCESSOR=x86",
               f"-DCMAKE_C_COMPILER={BIN / 'cl'}", f"-DCMAKE_CXX_COMPILER={BIN / 'cl'}",
               "-DApplications=JA2", "-DCMAKE_BUILD_TYPE=RelWithDebInfo",
               "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
               "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded",
               "-DUSE_SCCACHE=OFF", "-DADDRESS_SANITIZER=OFF", "-DLTO_OPTION=OFF",
               "-DCMAKE_C_COMPILER_LAUNCHER=", "-DCMAKE_CXX_COMPILER_LAUNCHER=",
               f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY={BUILD / 'bin'}"]
    for variable, tool in (("AR", "lib"), ("LINKER", "link"), ("RC_COMPILER", "rc"), ("MT", "mt")):
        command.append(f"-DCMAKE_{variable}={BIN / tool}")
    run(command, env=env)
    run(["cmake", "--build", BUILD, "--target", "JA2", "--parallel", str(jobs)], env=env)
    source["executable_sha256"] = hashlib.sha256((BUILD / "bin/JA2.exe").read_bytes()).hexdigest()
    BUILD_RECORD.write_text(json.dumps(source, indent=2) + "\n")


def successful_build():
    if not BUILD_RECORD.is_file():
        raise RuntimeError("Run build successfully before preparing or launching a runtime")
    record = json.loads(BUILD_RECORD.read_text())
    if hashlib.sha256((BUILD / "bin/JA2.exe").read_bytes()).hexdigest() != record["executable_sha256"]:
        raise RuntimeError("Executable differs from the last successful build; rebuild before running")
    return record


def prepare(retail):
    retail = retail.resolve(strict=True)
    data = retail if retail.name.casefold() == "data" else retail / "Data"
    if not data.is_dir():
        raise RuntimeError("Supply the retail Game directory or its Data directory")
    if GAME.exists():
        raise RuntimeError(f"Refusing to overwrite {GAME}; move it aside to preserve saves/configuration")
    build_record = successful_build()
    tracked = subprocess.check_output(["git", "ls-files", "-z", "--", "gamedir"], cwd=ROOT)
    game_files = [ROOT / os.fsdecode(path) for path in tracked.split(b"\0") if path]
    game_paths = set(game_files)
    for path in game_files:
        game_paths.update(parent for parent in path.parents if ROOT / "gamedir" in parent.parents)
    # Wine is case-insensitive; the Linux staging filesystem is not.
    for base, paths in ((data, data.rglob("*")), (ROOT / "gamedir", game_paths)):
        names = set()
        for path in paths:
            if path.is_symlink():
                raise RuntimeError(f"Refusing symlink in runtime inputs: {path}")
            key = str(path.relative_to(base)).casefold()
            if key in names:
                raise RuntimeError(f"Case-colliding runtime path: {path}")
            names.add(key)
    archives = {p.name.casefold(): p for p in data.iterdir() if p.is_file()}
    config = configparser.ConfigParser(interpolation=None)
    config.read(ROOT / "gamedir/vfs_config.JA2113.ini")
    for section in config.values():
        if section.get("TYPE") == "LIBRARY" and not section.getboolean("OPTIONAL", fallback=False):
            archive = Path(section["PATH"]).name.casefold()
            if archive not in archives:
                raise RuntimeError(f"Missing required retail archive: {archive}")
    provenance = {"retail_data": str(data), "build": build_record,
        "retail_archives_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                   for p in data.glob("*.[sS][lL][fF]")}}
    STATE.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="game-stage-", dir=STATE) as temp:
        staging = Path(temp) / "game"
        for path in game_files:
            target = staging / path.relative_to(ROOT / "gamedir")
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
        shutil.copytree(data, staging / "Data")
        shutil.copy2(BUILD / "bin/JA2.exe", staging / "JA2.exe")
        # Do not send crash reports from a development baseline to an external service.
        ini = staging / "Ja2.ini"
        content = b"\r\n".join(b"CRASH_TELEMETRY_URL =" if line.startswith(b"CRASH_TELEMETRY_URL =") else line
                                for line in ini.read_bytes().split(b"\r\n"))
        ini.write_bytes(content)
        staging.rename(GAME)
    (STATE / "runtime-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n")
    print(f"Prepared {GAME}; retail files are unchanged; crash telemetry disabled locally")


def launch():
    if not (GAME / "JA2.exe").is_file():
        raise RuntimeError("Run prepare with legitimate retail data first")
    record = successful_build()
    if hashlib.sha256((GAME / "JA2.exe").read_bytes()).hexdigest() != record["executable_sha256"]:
        shutil.copy2(BUILD / "bin/JA2.exe", GAME / "JA2.exe")
    provenance_path = STATE / "runtime-provenance.json"
    provenance = json.loads(provenance_path.read_text())
    provenance["build"] = record
    provenance_path.write_text(json.dumps(provenance, indent=2) + "\n")
    print(f'Launching build from {record["source_commit"]}: {record["executable_sha256"]}', flush=True)
    env = wine_env("game")
    # Preseed the override upstream itself installs, avoiding its first-run restart.
    run(["wine", "reg", "add", r"HKCU\Software\Wine\AppDefaults\JA2.exe\DllOverrides",
         "/v", "ddraw", "/t", "REG_SZ", "/d", "native,builtin", "/f"], env=env)
    run(["wine", GAME / "JA2.exe"], cwd=GAME, env=env)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("setup").add_argument("--accept-license", action="store_true")
    commands.add_parser("build").add_argument("--jobs", type=int, default=8)
    commands.add_parser("prepare").add_argument("retail", type=Path)
    commands.add_parser("run")
    args = parser.parse_args()
    if args.command == "setup":
        setup(args.accept_license)
    elif args.command == "build":
        if args.jobs < 1:
            parser.error("--jobs must be positive")
        build(args.jobs)
    elif args.command == "prepare":
        prepare(args.retail)
    else:
        launch()


if __name__ == "__main__":
    try:
        main()
    except (RuntimeError, OSError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
