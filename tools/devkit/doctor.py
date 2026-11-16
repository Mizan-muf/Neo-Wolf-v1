"""`neo_devkit doctor`: checks that this machine can build and run Neo-Wolf, and that the content
is healthy (Roadmap Phase 7, known issue D4).

Checks, with platform-specific hints:

* Python 3.8 or newer (the DevKit);
* CMake 3.25 or newer (CMakeLists.txt, CMakePresets.json);
* Ninja on macOS and Linux (the presets use it; Windows uses the Visual Studio generator);
* a C++ compiler: Apple clang (Xcode Command Line Tools), g++/clang++ on Linux, Visual Studio
  with the C++ workload on Windows (found with vswhere, so no Developer prompt is needed);
* the SDL2 source: the pinned FetchContent tarball already downloaded or extracted under build/,
  or an offline source folder (FETCHCONTENT_SOURCE_DIR_SDL2), or github.com reachable for the
  first configure (a connection test only: nothing is downloaded);
* the project layout and the content (`validate`), and the campaign size.

Statuses: PASS, INFO (optional or informative), WARN (works, but look at it), FAIL (a required
tool or file is missing; exit code 1). Optional things never FAIL.
"""

import hashlib
import os
import platform as platform_module
import re
import shutil
import socket
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, List, Mapping, Optional, Sequence, Tuple

from tools.devkit.term import Palette
from tools.devkit.validator import validate_project

MIN_CMAKE = (3, 25)
MIN_PYTHON = (3, 8)
COMMAND_TIMEOUT_SECONDS = 5  # a `--version` query that takes longer is treated as failing
CONTENT_ERRORS_SHOWN = 5  # validation errors repeated as the hint of a failing content check
CONTENT_WARNINGS_SHOWN = 3

HINTS = {
    "cmake": {
        "Darwin": "brew install cmake (or download it from https://cmake.org/download/)",
        "Linux": "pip install --user 'cmake>=3.25', or Kitware's APT repository (Ubuntu 22.04 ships 3.22, too old)",
        "Windows": "winget install Kitware.CMake (Visual Studio 2022 17.5+ also bundles a recent CMake)",
    },
    "ninja": {
        "Darwin": "brew install ninja",
        "Linux": "sudo apt install ninja-build (or: pip install --user ninja)",
        "Windows": "not needed: the windows-msvc presets use the Visual Studio generator",
    },
    "compiler": {
        "Darwin": "xcode-select --install (Apple clang from the Xcode Command Line Tools)",
        "Linux": "sudo apt install g++ (or clang)",
        "Windows": "install Visual Studio 2022 or its Build Tools with the 'Desktop development with C++' "
        "workload: winget install Microsoft.VisualStudio.2022.BuildTools",
    },
    "python": {
        "Darwin": "brew install python@3.12",
        "Linux": "sudo apt install python3",
        "Windows": "winget install Python.Python.3.12 (tick 'Add python.exe to PATH')",
    },
}


@dataclass
class DiagnosticItem:
    category: str
    name: str
    status: str  # "PASS", "INFO", "WARN", "FAIL"
    message: str
    remediation: Optional[str] = None


class DoctorReport:
    def __init__(self) -> None:
        self.items: List[DiagnosticItem] = []

    def add(self, category: str, name: str, status: str, message: str, remediation: Optional[str] = None) -> None:
        self.items.append(DiagnosticItem(category, name, status, message, remediation))

    def find(self, name: str) -> Optional[DiagnosticItem]:
        return next((item for item in self.items if item.name == name), None)

    @property
    def has_failures(self) -> bool:
        return any(item.status == "FAIL" for item in self.items)

    @property
    def has_warnings(self) -> bool:
        return any(item.status == "WARN" for item in self.items)


Runner = Callable[[Sequence[str]], Tuple[int, str]]


def _run(command: Sequence[str]) -> Tuple[int, str]:
    """Runs a command (with a timeout); returns (exit code, stdout + stderr). -1 if it cannot run."""
    try:
        res = subprocess.run(
            list(command), capture_output=True, text=True, timeout=COMMAND_TIMEOUT_SECONDS, check=False
        )
        return res.returncode, (res.stdout or "") + (res.stderr or "")
    except (OSError, subprocess.SubprocessError):
        return -1, ""


def _network_reachable(host: str = "github.com", port: int = 443, timeout: float = 3.0) -> bool:
    """A TCP connection test only (nothing is downloaded)."""
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def parse_version(text: str) -> Optional[Tuple[int, ...]]:
    match = re.search(r"(\d+)\.(\d+)(?:\.(\d+))?", text)
    if not match:
        return None
    return tuple(int(part) for part in match.groups() if part is not None)


def read_sdl_pin(project_root: Path) -> Tuple[str, str, str]:
    """(version, URL, SHA-256) of the pinned SDL2 release in CMakeLists.txt."""
    try:
        text = (project_root / "CMakeLists.txt").read_text(encoding="utf-8")
    except OSError:
        return "", "", ""
    version = re.search(r'set\(NEOWOLF_SDL2_VERSION\s+"([^"]+)"\)', text)
    sha = re.search(r'set\(NEOWOLF_SDL2_SHA256\s+"([0-9a-fA-F]{64})"\)', text)
    url = re.search(r'"(https://[^"]+SDL2-\$\{NEOWOLF_SDL2_VERSION\}\.tar\.gz)"', text)
    v = version.group(1) if version else ""
    return v, (url.group(1).replace("${NEOWOLF_SDL2_VERSION}", v) if url else ""), sha.group(1).lower() if sha else ""


def _sdl_header_version(header: Path) -> str:
    try:
        text = header.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""
    parts = [
        re.search(rf"#define\s+SDL_{name}\s+(\d+)", text) for name in ("MAJOR_VERSION", "MINOR_VERSION", "PATCHLEVEL")
    ]
    return ".".join(p.group(1) for p in parts if p) if all(parts) else ""


def check_sdl_source(
    project_root: Path, report: DoctorReport, check_network: bool, network: Callable[[], bool], environ: dict
) -> None:
    version, url, sha = read_sdl_pin(project_root)
    if not version:
        report.add("SDL2", "SDL2 source", "WARN", "Cannot read the pinned SDL2 version from CMakeLists.txt.")
        return
    build = project_root / "build"
    # 1. Extracted sources from an earlier configure (FetchContent).
    for header in sorted(build.glob("*/_deps/sdl2-src/include/SDL_version.h")):
        found = _sdl_header_version(header)
        rel = header.parents[1].relative_to(project_root).as_posix()
        if found == version:
            report.add(
                "SDL2", "SDL2 source", "PASS", f"SDL2 {version} is already extracted in {rel} (no download needed)"
            )
            return
        report.add("SDL2", "SDL2 source (stale)", "INFO", f"{rel} holds SDL2 {found or '?'}; the pin is {version}")
    # 2. The downloaded tarball, checked against the pinned SHA-256.
    for tarball in sorted(build.glob(f"*/_deps/sdl2-subbuild/sdl2-populate-prefix/src/SDL2-{version}.tar.gz")):
        digest = hashlib.sha256(tarball.read_bytes()).hexdigest()
        rel = tarball.relative_to(project_root).as_posix()
        if digest == sha:
            report.add(
                "SDL2", "SDL2 source", "PASS", f"The pinned tarball SDL2-{version}.tar.gz is cached ({rel}, SHA-256 OK)"
            )
            return
        report.add("SDL2", "SDL2 tarball", "WARN", f"{rel} does not match the pinned SHA-256; CMake downloads it again")
    # 3. An offline source folder, or an installed SDL2 chosen explicitly.
    offline = environ.get("FETCHCONTENT_SOURCE_DIR_SDL2", "")
    for cache in sorted(build.glob("*/CMakeCache.txt")):
        text = cache.read_text(encoding="utf-8", errors="replace")
        match = re.search(r"^FETCHCONTENT_SOURCE_DIR_SDL2:\w+=(.+)$", text, re.MULTILINE)
        if match and match.group(1).strip():
            offline = offline or match.group(1).strip()
        if re.search(r"^NEOWOLF_SYSTEM_SDL2:BOOL=ON$", text, re.MULTILINE):
            report.add(
                "SDL2",
                "SDL2 source",
                "PASS",
                f"{cache.parent.relative_to(project_root).as_posix()} uses an installed SDL2 (NEOWOLF_SYSTEM_SDL2=ON)",
            )
            return
    if offline:
        if (Path(offline) / "include" / "SDL_version.h").is_file():
            report.add(
                "SDL2", "SDL2 source", "PASS", f"Offline SDL2 source folder: {offline} (FETCHCONTENT_SOURCE_DIR_SDL2)"
            )
            return
        report.add(
            "SDL2", "SDL2 source", "WARN", f"FETCHCONTENT_SOURCE_DIR_SDL2={offline} has no include/SDL_version.h"
        )
        return
    # 4. Not cached: the first configure downloads it, so the network must be reachable.
    hint = (
        f"Configure once with network access, or download {url} (SHA-256 {sha}), extract it and pass "
        f"-DFETCHCONTENT_SOURCE_DIR_SDL2=<folder>, or use an installed SDL2 with -DNEOWOLF_SYSTEM_SDL2=ON "
        "(docs/BUILD_MACOS.md, docs/BUILD_WINDOWS.md)"
    )
    if not check_network:
        report.add(
            "SDL2",
            "SDL2 source",
            "INFO",
            f"SDL2 {version} is not cached yet; the first configure downloads it (network test skipped)",
            hint,
        )
    elif network():
        report.add(
            "SDL2",
            "SDL2 source",
            "PASS",
            f"SDL2 {version} is not cached yet; github.com is reachable, so the first configure can download it",
        )
    else:
        report.add(
            "SDL2",
            "SDL2 source",
            "WARN",
            f"SDL2 {version} is not cached and github.com is not reachable: the first configure would fail",
            hint,
        )


def vswhere_path(environ: Mapping[str, str]) -> Path:
    """Where the Visual Studio Installer puts vswhere.exe (the same path on every Windows)."""
    return (
        Path(environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"))
        / "Microsoft Visual Studio"
        / "Installer"
        / "vswhere.exe"
    )


def _first_line(code: int, out: str, fallback: str = "") -> str:
    """The first line of a command's output when it succeeded and printed something."""
    return out.strip().splitlines()[0] if code == 0 and out.strip() else fallback


def _check_cmake(report: DoctorReport, which: Callable[[str], Optional[str]], run: Runner, hint: str) -> None:
    if not which("cmake"):
        report.add("Toolchain", "CMake", "FAIL", "CMake not found on PATH (3.25 or newer is required)", hint)
        return
    code, out = run(["cmake", "--version"])
    version = parse_version(out) if code == 0 else None
    if version is None:
        report.add("Toolchain", "CMake", "WARN", "cmake is on PATH but `cmake --version` failed", hint)
    elif version[:2] >= MIN_CMAKE:
        report.add("Toolchain", "CMake", "PASS", f"CMake {'.'.join(map(str, version))}")
    else:
        report.add(
            "Toolchain",
            "CMake",
            "FAIL",
            f"CMake {'.'.join(map(str, version))} is older than the required {MIN_CMAKE[0]}.{MIN_CMAKE[1]}",
            hint,
        )


def _check_ninja(
    report: DoctorReport, system: str, which: Callable[[str], Optional[str]], run: Runner, hint: str
) -> None:
    if which("ninja"):
        code, out = run(["ninja", "--version"])
        report.add(
            "Toolchain", "Ninja", "PASS", f"Ninja {out.strip() if code == 0 and out.strip() else '(version unknown)'}"
        )
    elif system == "Windows":
        report.add("Toolchain", "Ninja", "INFO", "Ninja not found (not needed for the windows-msvc presets)")
    else:
        report.add("Toolchain", "Ninja", "FAIL", "Ninja not found on PATH (the macos/linux presets use it)", hint)


def _check_windows_compiler(
    report: DoctorReport, which: Callable[[str], Optional[str]], run: Runner, environ: dict, hint: str
) -> None:
    """Visual Studio with the C++ workload (found with vswhere), else cl.exe or MinGW/clang on PATH."""
    vswhere = vswhere_path(environ)
    exe = str(vswhere) if vswhere.is_file() else ("vswhere" if which("vswhere") else "")
    if exe:
        query = [
            exe,
            "-latest",
            "-products",
            "*",
            "-requires",
            "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
            "-property",
        ]
        found_vs = _first_line(*run([*query, "displayName"]))
        if found_vs:
            version = _first_line(*run([*query, "catalog_productDisplayVersion"]))
            report.add(
                "Toolchain",
                "C++ compiler",
                "PASS",
                f"{found_vs}{' ' + version if version else ''} with the C++ tools (found by vswhere)",
            )
            return
    if which("cl"):
        report.add("Toolchain", "C++ compiler", "PASS", "MSVC cl.exe on PATH (Developer prompt)")
    elif which("g++") or which("clang++"):
        report.add(
            "Toolchain",
            "C++ compiler",
            "WARN",
            "Only MinGW/clang found; the supported Windows compiler is MSVC (windows-msvc presets)",
            hint,
        )
    else:
        report.add(
            "Toolchain",
            "C++ compiler",
            "FAIL",
            "No Visual Studio with the C++ workload found (vswhere) and no compiler on PATH",
            hint,
        )


def _check_unix_compiler(
    report: DoctorReport, system: str, which: Callable[[str], Optional[str]], run: Runner, hint: str
) -> None:
    candidates = ["clang++", "g++", "c++"] if system == "Darwin" else ["g++", "clang++", "c++"]
    for compiler in candidates:
        if which(compiler):
            first = _first_line(*run([compiler, "--version"]), fallback="version unknown")
            report.add("Toolchain", "C++ compiler", "PASS", f"{compiler}: {first}")
            return
    report.add("Toolchain", "C++ compiler", "FAIL", "No C++ compiler (clang++, g++) on PATH", hint)


def check_toolchain(
    report: DoctorReport, system: str, which: Callable[[str], Optional[str]], run: Runner, environ: dict
) -> None:
    def hint(tool: str) -> str:
        return HINTS[tool].get(system, HINTS[tool]["Linux"])

    _check_cmake(report, which, run, hint("cmake"))
    _check_ninja(report, system, which, run, hint("ninja"))
    if system == "Windows":
        _check_windows_compiler(report, which, run, environ, hint("compiler"))
    else:
        _check_unix_compiler(report, system, which, run, hint("compiler"))
    if which("make"):
        report.add("Toolchain", "make", "INFO", "Found (optional: the Makefile wraps the CMake presets)")
    else:
        report.add("Toolchain", "make", "INFO", "Not found (optional: only the Makefile wrapper needs it)")


def run_doctor(
    project_root: Path,
    *,
    check_network: bool = True,
    which: Callable[[str], Optional[str]] = shutil.which,
    run: Runner = _run,
    system: Optional[str] = None,
    network: Callable[[], bool] = _network_reachable,
    environ: Optional[dict] = None,
    python_version: Optional[Tuple[int, ...]] = None,
) -> DoctorReport:
    """Every check. The keyword arguments stand in for the real environment in tests (colours are
    render_doctor_report's job)."""
    project_root = Path(project_root).resolve()
    system = system or platform_module.system()
    env = dict(os.environ) if environ is None else environ
    report = DoctorReport()

    report.add("Host", "OS Platform", "PASS", f"{system} {platform_module.release()} ({platform_module.machine()})")
    py = python_version or tuple(sys.version_info[:3])
    py_text = ".".join(str(p) for p in py)
    if py[:2] >= MIN_PYTHON:
        report.add("Host", "Python", "PASS", f"Python {py_text} ({sys.executable})")
    else:
        report.add(
            "Host",
            "Python",
            "FAIL",
            f"Python {py_text} is older than {MIN_PYTHON[0]}.{MIN_PYTHON[1]}",
            HINTS["python"].get(system, HINTS["python"]["Linux"]),
        )

    check_toolchain(report, system, which, run, env)
    check_sdl_source(project_root, report, check_network, network, env)
    _check_project_layout(project_root, report)
    _check_content(project_root, report)
    return report


def _check_project_layout(project_root: Path, report: DoctorReport) -> None:
    """The source folders and core files, and which builds exist."""
    required_dirs = [
        "engine/core",
        "engine/world",
        "engine/autoplay",
        "game/assets",
        "game/maps",
        "game/defs",
        "docs",
        "tests",
        "tools/devkit",
    ]
    missing_dirs = [d for d in required_dirs if not (project_root / d).is_dir()]
    if missing_dirs:
        report.add(
            "Project",
            "Directory Structure",
            "FAIL",
            f"Missing directories: {', '.join(missing_dirs)}",
            "Restore them from version control.",
        )
    else:
        report.add("Project", "Directory Structure", "PASS", f"All {len(required_dirs)} directories present.")
    required_files = [
        "CMakeLists.txt",
        "CMakePresets.json",
        "src/main.cpp",
        "src/autoplay_main.cpp",
        "game/defs/campaign.json",
        "game/defs/gameplay.json",
        "game/defs/sprites.cfg",
        "game/defs/audio.cfg",
    ]
    missing_files = [f for f in required_files if not (project_root / f).is_file()]
    if missing_files:
        report.add(
            "Project",
            "Core Files",
            "FAIL",
            f"Missing files: {', '.join(missing_files)}",
            "Restore them from version control.",
        )
    else:
        report.add("Project", "Core Files", "PASS", f"All {len(required_files)} core files present.")
    build = project_root / "build"
    built = sorted(
        p.relative_to(project_root).as_posix()
        for pattern in ("*/neo_wolf_autoplay", "*/neo_wolf_autoplay.exe", "*/*/neo_wolf_autoplay.exe")
        for p in build.glob(pattern)
        if p.is_file()
    )
    report.add(
        "Project",
        "Builds",
        "INFO",
        ("autoplay bot built: " + ", ".join(built))
        if built
        else "No build yet (cmake --preset <preset> && cmake --build --preset <preset>)",
    )


def _check_content(project_root: Path, report: DoctorReport) -> None:
    """`validate` on the content: errors fail, warnings warn (`validate --strict` would fail)."""
    val_report = validate_project(project_root)
    levels = next((line for line in val_report.info if line.startswith("Campaign: ")), "Campaign: ?")
    if not val_report.is_valid:
        report.add(
            "Content",
            "Content validation",
            "FAIL",
            f"{len(val_report.errors)} error(s) ({levels}); run `neo_devkit validate`",
            "\n".join(val_report.errors[:CONTENT_ERRORS_SHOWN]),
        )
    elif val_report.warnings:
        report.add(
            "Content",
            "Content validation",
            "WARN",
            f"Valid with {len(val_report.warnings)} warning(s) ({levels}); `validate --strict` fails",
            "\n".join(val_report.warnings[:CONTENT_WARNINGS_SHOWN]),
        )
    else:
        report.add(
            "Content", "Content validation", "PASS", f"Maps, assets, definitions and campaign are valid ({levels})."
        )


def render_doctor_report(report: DoctorReport, color: bool = True) -> str:
    """Format the report for the terminal."""
    c = Palette(color)
    lines: List[str] = [
        f"{c.bold}======================================================{c.reset}",
        f"{c.bold}           Neo-Wolf DevKit System Doctor              {c.reset}",
        f"{c.bold}======================================================{c.reset}",
    ]
    badges = {"PASS": c.green, "INFO": c.cyan, "WARN": c.yellow, "FAIL": c.red}
    current_cat = None
    for item in report.items:
        if item.category != current_cat:
            current_cat = item.category
            lines.append(f"\n{c.bold}[{current_cat}]{c.reset}")
        lines.append(f"  {badges.get(item.status, '')}[{item.status}]{c.reset} {item.name}: {item.message}")
        if item.remediation:
            for index, line in enumerate(item.remediation.splitlines()):
                lines.append(f"         {'Hint: ' if index == 0 else '      '}{line}")
    lines.append(f"\n{c.bold}------------------------------------------------------{c.reset}")
    if report.has_failures:
        lines.append(f"{c.red}{c.bold}Result: FAIL - a required tool or file is missing (see the hints).{c.reset}")
    elif report.has_warnings:
        lines.append(f"{c.yellow}{c.bold}Result: OK with warnings.{c.reset}")
    else:
        lines.append(f"{c.green}{c.bold}Result: all checks passed.{c.reset}")
    lines.append(f"{c.bold}======================================================{c.reset}")
    return "\n".join(lines)
