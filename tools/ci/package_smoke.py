#!/usr/bin/env python3
"""Smoke test of a Neo-Wolf release package (Roadmap Phase 9, docs/RELEASE.md).

For each archive (neo-wolf-<version>-<platform>.zip or .tar.gz):

  1. checks its SHA-256 against SHA256SUMS (when given a folder, or when SHA256SUMS lies next to it);
  2. extracts it into a fresh folder whose path contains spaces (keeping the Unix permissions,
     as Finder, Explorer and tar do);
  3. checks the layout: one top folder named like the archive, the executable, the content,
     README.txt (with the version), LICENSE.md (the project licence), docs/, LICENSES/ (third-party
     notices), and no test or autoplay executable;
  4. per OS: macOS - Info.plist values (and `plutil -lint`), `codesign --verify --deep --strict`,
     `lipo -info` (both architectures for a universal package), `otool -L` (system libraries
     only); Windows - the PE header of neo_wolf.exe: x64, GUI subsystem, an icon and a version
     resource with the package's version, no DLLs beyond Windows' own (static C runtime and SDL).
     The Windows checks read the file, so they also run on another OS (a MinGW cross-build);
  5. on the package's own OS: runs the game headless (SDL dummy drivers) with
     `--level 10 --frames 300 --pref-dir <temporary folder>`, once from the extracted folder and
     once with the working directory set to the file-system root (/ or C:\\), and requires exit
     code 0 and a log whose content root is the bundle's Contents/Resources (macOS) or the
     executable's folder (Windows, Linux); plus `--version`.

Standard library only. Exit codes: 0 pass, 1 a check failed, 2 bad usage.

Usage:
  python3 tools/ci/package_smoke.py release/                    # every archive in SHA256SUMS
  python3 tools/ci/package_smoke.py release/neo-wolf-0.9.0-macos-universal.zip
  python3 tools/ci/package_smoke.py <archive> --no-run          # static checks only
Options: --level N (default 10), --frames N (default 300), --timeout S (per run, default 180),
--keep (keep the extracted folder).
"""
from __future__ import annotations

import argparse
import hashlib
import os
import platform
import plistlib
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path
from typing import Dict, List, Optional, Tuple

ARCHIVE_NAME = re.compile(r"^neo-wolf-(?P<version>\d+\.\d+\.\d+)-(?P<platform>[a-z0-9_-]+)\.(?P<ext>zip|tar\.gz)$")
FORBIDDEN_EXECUTABLES = ("neo_wolf_tests", "neo_wolf_autoplay")
REQUIRED_FILES = ("README.txt", "LICENSE.md", "docs/CONTROLS.md", "docs/TROUBLESHOOTING.md", "LICENSES/SDL2-LICENSE.txt",
                  "LICENSES/nlohmann-json-LICENSE.MIT.txt", "LICENSES/THIRD_PARTY_LICENSES.md")
BUNDLE_ID = "io.github.mizan-muf.neowolf"
COPYRIGHT = "Copyright (c) 2026 Mizan-muf"
# DLLs a static build must not need (SDL, the MSVC and MinGW C/C++ runtimes).
FORBIDDEN_DLLS = re.compile(r"^(sdl2.*|vcruntime.*|msvcp.*|concrt.*|libstdc\+\+.*|libgcc.*|libwinpthread.*)\.dll$",
                            re.IGNORECASE)


class Checker:
    def __init__(self) -> None:
        self.failures: List[str] = []

    def ok(self, message: str) -> None:
        print(f"[smoke]   ok: {message}")

    def check(self, condition: bool, message: str, detail: str = "") -> bool:
        if condition:
            self.ok(message)
        else:
            self.failures.append(message + (f" ({detail})" if detail else ""))
            print(f"[smoke] FAIL: {message}" + (f" ({detail})" if detail else ""))
        return condition


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def read_sums(path: Path) -> Dict[str, str]:
    sums: Dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        parts = line.split(None, 1)
        if len(parts) == 2:
            sums[parts[1].lstrip("*").strip()] = parts[0].lower()
    return sums


def host_os() -> str:
    return {"darwin": "macos", "windows": "windows"}.get(platform.system().lower(), "linux")


def package_os(platform_label: str) -> str:
    return platform_label.split("-", 1)[0]


# --- Extraction ------------------------------------------------------------------------------
def _safe_target(root: Path, name: str) -> Path:
    target = (root / name).resolve()
    if os.path.commonpath([str(root.resolve()), str(target)]) != str(root.resolve()):
        raise ValueError(f"unsafe path in archive: {name}")
    return target


def extract(archive: Path, dest: Path) -> None:
    dest.mkdir(parents=True)
    if archive.name.endswith(".zip"):
        with zipfile.ZipFile(archive) as zf:
            for member in zf.infolist():
                target = _safe_target(dest, member.filename)
                if member.is_dir():
                    target.mkdir(parents=True, exist_ok=True)
                    continue
                target.parent.mkdir(parents=True, exist_ok=True)
                with zf.open(member) as src, open(target, "wb") as out:
                    shutil.copyfileobj(src, out)
                mode = (member.external_attr >> 16) & 0o777
                if member.create_system == 3 and mode:
                    os.chmod(target, mode)
    else:
        with tarfile.open(archive, "r:gz") as tf:
            for member in tf.getmembers():
                _safe_target(dest, member.name)
                if not (member.isfile() or member.isdir()):
                    raise ValueError(f"unexpected entry type in archive: {member.name}")
            tf.extractall(dest)  # members checked above (regular files and folders inside dest)


# --- Windows PE checks (pure Python, so they also run on macOS/Linux) --------------------------
def pe_summary(data: bytes) -> Dict[str, object]:
    """Machine, subsystem, imported DLLs and top-level resource types of a PE32+ file."""
    if data[:2] != b"MZ":
        raise ValueError("not an MZ executable")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("no PE signature")
    machine, sections, _, _, _, opt_size, _ = struct.unpack_from("<HHIIIHH", data, pe + 4)
    opt = pe + 24
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic != 0x20B:
        raise ValueError(f"not a PE32+ (64-bit) image (magic {magic:#x})")
    subsystem = struct.unpack_from("<H", data, opt + 68)[0]
    dirs = opt + 112
    import_rva = struct.unpack_from("<I", data, dirs + 8)[0]
    resource_rva = struct.unpack_from("<I", data, dirs + 16)[0]
    table = opt + opt_size
    section_list = []
    for i in range(sections):
        base = table + 40 * i
        _, vsize, vaddr, rawsize, rawptr = struct.unpack_from("<8sIIII", data, base)
        section_list.append((vaddr, max(vsize, rawsize), rawptr))

    def offset(rva: int) -> int:
        for vaddr, size, rawptr in section_list:
            if vaddr <= rva < vaddr + size:
                return rawptr + rva - vaddr
        raise ValueError(f"RVA {rva:#x} is in no section")

    def c_string(pos: int) -> str:
        return data[pos:data.index(b"\0", pos)].decode("ascii", "replace")

    dlls: List[str] = []
    if import_rva:
        pos = offset(import_rva)
        while True:
            name_rva = struct.unpack_from("<I", data, pos + 12)[0]
            if name_rva == 0:
                break
            dlls.append(c_string(offset(name_rva)))
            pos += 20
    resource_types: List[int] = []
    if resource_rva:
        root = offset(resource_rva)
        named, ids = struct.unpack_from("<HH", data, root + 12)
        for i in range(named + ids):
            name = struct.unpack_from("<I", data, root + 16 + 8 * i)[0]
            if not name & 0x80000000:
                resource_types.append(name)
    return {"machine": machine, "subsystem": subsystem, "dlls": dlls, "resource_types": resource_types}


def check_windows_exe(exe: Path, version: str, c: Checker) -> None:
    data = exe.read_bytes()
    try:
        pe = pe_summary(data)
    except (ValueError, struct.error) as exc:
        c.check(False, f"{exe.name} is a valid PE32+ executable", str(exc))
        return
    c.check(pe["machine"] == 0x8664, f"{exe.name} is x64", f"machine {pe['machine']:#x}")
    c.check(pe["subsystem"] == 2, f"{exe.name} uses the GUI subsystem (no console window)",
            f"subsystem {pe['subsystem']}")
    types = pe["resource_types"]
    c.check(3 in types and 14 in types, f"{exe.name} has an icon resource", f"resource types {types}")
    c.check(16 in types, f"{exe.name} has a version resource", f"resource types {types}")
    c.check(version.encode("utf-16-le") in data and "ProductVersion".encode("utf-16-le") in data,
            f"the version resource says {version}")
    c.check(COPYRIGHT.encode("utf-16-le") in data, f"the version resource has LegalCopyright '{COPYRIGHT}'")
    dlls = sorted(pe["dlls"], key=str.lower)
    print(f"[smoke]   imports: {', '.join(dlls)}")
    bad = [d for d in dlls if FORBIDDEN_DLLS.match(d)]
    c.check(not bad, "no SDL or C/C++ runtime DLLs needed (static build)", ", ".join(bad))


# --- macOS checks ----------------------------------------------------------------------------
def run_tool(cmd: List[str]) -> Tuple[int, str]:
    try:
        done = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError) as exc:
        return 127, str(exc)
    return done.returncode, (done.stdout + done.stderr).strip()


def check_macos_app(app: Path, exe: Path, version: str, universal: bool, c: Checker) -> None:
    plist_path = app / "Contents" / "Info.plist"
    with open(plist_path, "rb") as handle:
        plist = plistlib.load(handle)
    c.check(plist.get("CFBundleShortVersionString") == version and plist.get("CFBundleVersion") == version,
            f"Info.plist version {version}", str(plist.get("CFBundleShortVersionString")))
    c.check(plist.get("CFBundleIdentifier") == BUNDLE_ID, f"Info.plist bundle id {BUNDLE_ID}")
    c.check(plist.get("NSHighResolutionCapable") is True, "Info.plist NSHighResolutionCapable")
    c.check(plist.get("LSMinimumSystemVersion") == "10.15", "Info.plist LSMinimumSystemVersion 10.15")
    c.check(plist.get("LSApplicationCategoryType") == "public.app-category.games", "Info.plist category games")
    c.check(plist.get("NSHumanReadableCopyright") == COPYRIGHT, f"Info.plist copyright '{COPYRIGHT}'")
    icon = plist.get("CFBundleIconFile", "")
    c.check(bool(icon) and (app / "Contents" / "Resources" / (icon if icon.endswith(".icns") else icon + ".icns"))
            .is_file(), "the app icon (.icns) is in Contents/Resources")
    if host_os() != "macos":
        print("[smoke]   (codesign, lipo, otool and plutil need macOS: skipped)")
        return
    code, out = run_tool(["plutil", "-lint", str(plist_path)])
    print(f"[smoke]   plutil -lint: {out}")
    c.check(code == 0, "plutil -lint Info.plist")
    code, out = run_tool(["codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app)])
    print("[smoke]   codesign --verify --deep --strict:\n" + "\n".join("             " + l for l in out.splitlines()))
    c.check(code == 0, "codesign --verify --deep --strict")
    code, out = run_tool(["codesign", "-dv", str(app)])
    signature = [l for l in out.splitlines() if l.startswith(("Signature=", "Identifier=", "Format="))]
    print("[smoke]   codesign -dv: " + "; ".join(signature))
    code, out = run_tool(["lipo", "-info", str(exe)])
    print(f"[smoke]   lipo -info: {out}")
    if universal:
        c.check(code == 0 and "arm64" in out and "x86_64" in out, "universal binary (arm64 and x86_64)")
    else:
        c.check(code == 0, "lipo -info")
    code, out = run_tool(["otool", "-L", str(exe)])
    print("[smoke]   otool -L:\n" + "\n".join("             " + l for l in out.splitlines()))
    libs = [l.strip().split(" (")[0] for l in out.splitlines() if l.startswith("\t")]
    foreign = [l for l in libs if not l.startswith(("/usr/lib/", "/System/Library/"))]
    c.check(code == 0 and bool(libs) and not foreign, "only system libraries are linked", ", ".join(foreign))


# --- Running the game ------------------------------------------------------------------------
def run_game(exe: Path, cwd: Path, version: str, expected_root: Path, level: int, frames: int, timeout: float,
             label: str, c: Checker) -> None:
    env = dict(os.environ)
    env["SDL_VIDEODRIVER"] = "dummy"
    env["SDL_AUDIODRIVER"] = "dummy"
    for name in ("SDL_RENDER_DRIVER", "NEOWOLF_DATA_DIR", "NEOWOLF_PREF_DIR"):
        env.pop(name, None)
    with tempfile.TemporaryDirectory(prefix="nw smoke pref ") as pref:
        cmd = [str(exe), "--level", str(level), "--frames", str(frames), "--pref-dir", pref]
        print(f"[smoke] run ({label}): {cmd} in {cwd}")
        with tempfile.TemporaryFile(mode="w+", encoding="utf-8", errors="replace") as out:
            try:
                proc = subprocess.run(cmd, cwd=str(cwd), env=env, stdout=out, stderr=subprocess.STDOUT,
                                      timeout=timeout)
                code: Optional[int] = proc.returncode
            except subprocess.TimeoutExpired:
                code = None
            out.seek(0)
            console = out.read()
        log_path = Path(pref) / "neowolf.log"
        log = log_path.read_text(encoding="utf-8", errors="replace") if log_path.is_file() else console
    c.check(code == 0, f"[{label}] exit code 0", "timed out" if code is None else f"exit code {code}")
    for line in log.splitlines():
        if any(key in line for key in ("starting on", "Content root:", "Starting level", "Ran ")):
            print(f"[smoke]   log: {line}")
    match = re.search(r"Content root: (.*) \(([^()]*)\)\s*$", log, re.MULTILINE)
    if c.check(match is not None, f"[{label}] the log names the content root"):
        assert match is not None
        root = os.path.normcase(os.path.realpath(match.group(1)))
        want = os.path.normcase(os.path.realpath(str(expected_root)))
        c.check(root == want, f"[{label}] content root is inside the package: {expected_root.name}",
                f"got {match.group(1)}")
    c.check(f"Neo-Wolf {version} starting" in log, f"[{label}] the log reports version {version}")
    c.check(f"Starting level {level} " in log, f"[{label}] level {level} started")
    c.check(f"Ran {frames} frame" in log, f"[{label}] {frames} frames rendered")


def check_version_option(exe: Path, version: str, c: Checker) -> None:
    code, out = run_tool([str(exe), "--version"])
    c.check(code == 0 and out.strip() == f"Neo-Wolf {version}", f"--version prints 'Neo-Wolf {version}'", out)


# --- One archive -----------------------------------------------------------------------------
def smoke_archive(archive: Path, run: bool, level: int, frames: int, timeout: float, keep: bool) -> List[str]:
    c = Checker()
    match = ARCHIVE_NAME.match(archive.name)
    print(f"[smoke] === {archive}")
    if not c.check(match is not None, "archive name is neo-wolf-<version>-<platform>.<zip|tar.gz>"):
        return c.failures
    assert match is not None
    version, platform_label = match.group("version"), match.group("platform")
    package = archive.name[: -len("." + match.group("ext"))]
    pkg_os = package_os(platform_label)
    sums = archive.parent / "SHA256SUMS"
    if sums.is_file():
        expected = read_sums(sums).get(archive.name)
        c.check(expected == sha256(archive), "SHA256SUMS matches the archive",
                "no entry" if expected is None else "checksum differs")
    else:
        print("[smoke]   (no SHA256SUMS next to the archive: checksum not checked)")

    work = Path(tempfile.mkdtemp(prefix="nw package smoke "))
    try:
        dest = work / "extracted here"
        extract(archive, dest)
        tops = sorted(p.name for p in dest.iterdir())
        if not c.check(tops == [package], f"one top folder named {package}", f"found {tops}"):
            return c.failures
        top = dest / package
        print(f"[smoke]   extracted to: {top}")

        if pkg_os == "macos":
            apps = sorted(top.glob("*.app"))
            if not c.check(len(apps) == 1, "one .app bundle", f"found {[a.name for a in apps]}"):
                return c.failures
            app = apps[0]
            with open(app / "Contents" / "Info.plist", "rb") as handle:
                exe = app / "Contents" / "MacOS" / plistlib.load(handle).get("CFBundleExecutable", "")
            expected_root = app / "Contents" / "Resources"
        elif pkg_os == "windows":
            exe = top / "neo_wolf.exe"
            expected_root = top
        else:
            exe = top / "neo_wolf"
            expected_root = top

        c.check(exe.is_file(), f"the executable is there: {exe.relative_to(top).as_posix()}")
        if pkg_os != "windows" and exe.is_file():
            c.check(os.access(exe, os.X_OK), "the executable kept its execute permission")
        c.check((expected_root / "game" / "maps").is_dir() and (expected_root / "game" / "defs").is_dir(),
                f"game/maps and game/defs in {expected_root.relative_to(top).as_posix() or '.'}")
        for rel in REQUIRED_FILES:
            c.check((top / rel).is_file(), rel)
        readme = top / "README.txt"
        if readme.is_file():
            text = readme.read_text(encoding="utf-8", errors="replace")
            c.check(version in text, f"README.txt mentions version {version}")
            c.check("LICENSE.md" in text and "noncommercial" in text.lower(), "README.txt summarises the licence")
        licence = top / "LICENSE.md"
        if licence.is_file():
            text = licence.read_text(encoding="utf-8", errors="replace")
            c.check(text.startswith("Required Notice: Copyright") and "PolyForm Noncommercial License 1.0.0" in text,
                    "LICENSE.md has the Required Notice and the PolyForm Noncommercial 1.0.0 terms")
        shipped = [p.relative_to(top).as_posix() for p in top.rglob("*") if p.is_file()]
        bad = [p for p in shipped if p.rsplit("/", 1)[-1].split(".")[0] in FORBIDDEN_EXECUTABLES]
        c.check(not bad, "no test or autoplay executable shipped", ", ".join(bad))
        print(f"[smoke]   {len(shipped)} files")

        if exe.is_file():
            if pkg_os == "macos":
                check_macos_app(app, exe, version, platform_label == "macos-universal", c)
            elif pkg_os == "windows":
                check_windows_exe(exe, version, c)

        if run and exe.is_file():
            if pkg_os != host_os():
                print(f"[smoke]   (a {pkg_os} package cannot run on {host_os()}: runs skipped)")
            else:
                check_version_option(exe, version, c)
                run_game(exe, top, version, expected_root, level, frames, timeout, "cwd = package folder", c)
                fs_root = Path(os.path.abspath(os.sep))
                run_game(exe, fs_root, version, expected_root, level, frames, timeout, f"cwd = {fs_root}", c)
    except (OSError, ValueError, zipfile.BadZipFile, tarfile.TarError) as exc:
        c.check(False, "archive can be extracted and read", str(exc))
    finally:
        if keep:
            print(f"[smoke]   kept: {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)
    return c.failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("target", help="an archive, or a folder with SHA256SUMS and the archives it lists")
    parser.add_argument("--level", type=int, default=10)
    parser.add_argument("--frames", type=int, default=300)
    parser.add_argument("--timeout", type=float, default=180.0)
    parser.add_argument("--no-run", action="store_true", help="static checks only; do not start the game")
    parser.add_argument("--keep", action="store_true", help="keep the extracted files")
    args = parser.parse_args()

    target = Path(args.target).resolve()
    archives: List[Path] = []
    failures: List[str] = []
    if target.is_dir():
        sums = target / "SHA256SUMS"
        if not sums.is_file():
            print(f"[smoke] {sums} not found", file=sys.stderr)
            return 2
        for name in sorted(read_sums(sums)):
            path = target / name
            if path.is_file():
                archives.append(path)
            else:
                print(f"[smoke] (listed in SHA256SUMS but not here: {name})")
        if not archives:
            print(f"[smoke] FAIL: no archive from SHA256SUMS found in {target}", file=sys.stderr)
            return 1
    elif target.is_file():
        archives = [target]
    else:
        print(f"[smoke] not found: {target}", file=sys.stderr)
        return 2

    for archive in archives:
        failures += [f"{archive.name}: {f}" for f in
                     smoke_archive(archive, not args.no_run, args.level, args.frames, args.timeout, args.keep)]
    if failures:
        print(f"\n[smoke] FAILED ({len(failures)}):")
        for failure in failures:
            print(f"  - {failure}")
        return 1
    print(f"\n[smoke] PASS: {len(archives)} package(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
