"""System, environment, and repository health diagnostics for Neo-Wolf developers."""

import os
import platform
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from tools.devkit.validator import validate_project


@dataclass
class DiagnosticItem:
    category: str
    name: str
    status: str  # "PASS", "WARN", "FAIL"
    message: str
    remediation: Optional[str] = None


class DoctorReport:
    def __init__(self) -> None:
        self.items: List[DiagnosticItem] = []

    def add(self, category: str, name: str, status: str, message: str, remediation: Optional[str] = None) -> None:
        self.items.append(DiagnosticItem(category, name, status, message, remediation))

    @property
    def has_failures(self) -> bool:
        return any(item.status == "FAIL" for item in self.items)

    @property
    def has_warnings(self) -> bool:
        return any(item.status == "WARN" for item in self.items)


def check_command(cmd: str, version_flag: str = "--version") -> Tuple[bool, str]:
    """Check if command exists and get output."""
    path = shutil.which(cmd)
    if not path:
        return False, ""
    try:
        res = subprocess.run([cmd, version_flag], capture_output=True, text=True, timeout=5)
        out = (res.stdout or res.stderr).splitlines()
        first_line = out[0].strip() if out else "Available"
        return True, first_line
    except Exception:
        return True, "Available"


def run_doctor(project_root: Path, color: bool = True) -> DoctorReport:
    """Execute all system and project diagnostic checks."""
    project_root = Path(project_root).resolve()
    report = DoctorReport()

    # 1. Host & OS Environment
    report.add("Host", "OS Platform", "PASS", f"{platform.system()} {platform.release()} ({platform.machine()})")
    py_ver = f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
    if sys.version_info >= (3, 8):
        report.add("Host", "Python Runtime", "PASS", f"Python {py_ver} ({sys.executable})")
    else:
        report.add(
            "Host",
            "Python Runtime",
            "WARN",
            f"Python {py_ver} is older than recommended 3.8+",
            "Upgrade to Python 3.8 or newer for complete DevKit features.",
        )

    # 2. Toolchain / Compilers
    compilers_found = []
    for comp in ["g++", "clang++", "cl"]:
        found, ver = check_command(comp, version_flag="--version" if comp != "cl" else "/?")
        if found:
            compilers_found.append((comp, ver))

    if compilers_found:
        primary_comp, comp_ver = compilers_found[0]
        report.add("Toolchain", "C++ Compiler", "PASS", f"Found {primary_comp} ({comp_ver})")
    else:
        report.add(
            "Toolchain",
            "C++ Compiler",
            "FAIL",
            "No C++ compiler found on PATH (g++, clang++, or MSVC cl.exe)",
            "Install GCC (g++), Clang (clang++), or Visual Studio Build Tools.",
        )

    # Build utilities
    make_found, make_ver = check_command("make")
    cmake_found, cmake_ver = check_command("cmake")
    ninja_found, ninja_ver = check_command("ninja")

    if cmake_found:
        report.add("Toolchain", "CMake", "PASS", f"Found {cmake_ver}")
    else:
        report.add(
            "Toolchain",
            "CMake",
            "WARN",
            "CMake not found on PATH.",
            "Install CMake 3.25+ (required by CMakeLists.txt and CMakePresets.json).",
        )

    if make_found:
        report.add("Toolchain", "GNU Make", "PASS", f"Found {make_ver}")
    else:
        report.add("Toolchain", "GNU Make", "WARN", "Make not found on PATH.")

    # 3. Project Directory Structure
    required_dirs = [
        "engine/core",
        "engine/platform",
        "engine/render",
        "engine/world",
        "engine/assets",
        "engine/audio",
        "engine/particles",
        "engine/ui",
        "game/assets",
        "game/maps",
        "game/defs",
        "docs",
        "tests",
        "tools/devkit",
    ]
    missing_dirs = [d for d in required_dirs if not (project_root / d).is_dir()]
    if not missing_dirs:
        report.add("Project", "Directory Structure", "PASS", f"All {len(required_dirs)} modules verified.")
    else:
        report.add(
            "Project",
            "Directory Structure",
            "FAIL",
            f"Missing required directories: {', '.join(missing_dirs)}",
            "Recreate missing folders or verify repository integrity.",
        )

    # 4. Critical Project Files
    required_files = [
        "CMakeLists.txt",
        "README.md",
        "src/main.cpp",
        "tests/LogicTests.cpp",
        "game/maps/level01.map",
        "game/defs/wall_textures.cfg",
        "game/defs/sprites.cfg",
        "game/defs/audio.cfg",
        "game/defs/entities.json",
    ]
    missing_files = [f for f in required_files if not (project_root / f).is_file()]
    if not missing_files:
        report.add("Project", "Core Files", "PASS", f"All {len(required_files)} baseline files present.")
    else:
        report.add(
            "Project",
            "Core Files",
            "FAIL",
            f"Missing core files: {', '.join(missing_files)}",
            "Restore deleted files from version control.",
        )

    # 5. Content & Asset Validation Pass
    val_report = validate_project(project_root)
    if val_report.is_valid:
        num_warnings = len(val_report.warnings)
        if num_warnings == 0:
            report.add("Content", "Asset/Map Validation", "PASS", "All maps, textures, audio, and defs are 100% valid.")
        else:
            report.add(
                "Content",
                "Asset/Map Validation",
                "WARN",
                f"Validation passed with {num_warnings} warnings.",
                "\n".join(val_report.warnings[:3]),
            )
    else:
        report.add(
            "Content",
            "Asset/Map Validation",
            "FAIL",
            f"Encountered {len(val_report.errors)} content validation error(s).",
            "\n".join(val_report.errors[:5]),
        )

    return report


def render_doctor_report(report: DoctorReport, color: bool = True) -> str:
    """Format doctor report for terminal display."""
    C_RESET = "\033[0m" if color else ""
    C_GREEN = "\033[32;1m" if color else ""
    C_YELLOW = "\033[33;1m" if color else ""
    C_RED = "\033[31;1m" if color else ""
    C_BOLD = "\033[1m" if color else ""

    lines: List[str] = [
        f"{C_BOLD}======================================================{C_RESET}",
        f"{C_BOLD}           Neo-Wolf DevKit System Doctor              {C_RESET}",
        f"{C_BOLD}======================================================{C_RESET}",
    ]

    current_cat = None
    for item in report.items:
        if item.category != current_cat:
            current_cat = item.category
            lines.append(f"\n{C_BOLD}[{current_cat}]{C_RESET}")

        if item.status == "PASS":
            badge = f"{C_GREEN}[PASS]{C_RESET}"
        elif item.status == "WARN":
            badge = f"{C_YELLOW}[WARN]{C_RESET}"
        else:
            badge = f"{C_RED}[FAIL]{C_RESET}"

        lines.append(f"  {badge} {item.name}: {item.message}")
        if item.remediation:
            lines.append(f"         Remedy: {item.remediation}")

    lines.append(f"\n{C_BOLD}------------------------------------------------------{C_RESET}")
    if report.has_failures:
        lines.append(f"{C_RED}{C_BOLD}Result: Doctor found FAILURES. Address remedy steps above.{C_RESET}")
    elif report.has_warnings:
        lines.append(f"{C_YELLOW}{C_BOLD}Result: Doctor passed with WARNINGS. System is operational.{C_RESET}")
    else:
        lines.append(f"{C_GREEN}{C_BOLD}Result: All checks PASSED! System is in excellent health.{C_RESET}")
    lines.append(f"{C_BOLD}======================================================{C_RESET}")

    return "\n".join(lines)
