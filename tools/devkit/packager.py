"""Release packaging, distribution builder, and standalone bundle verification for Neo-Wolf."""

import hashlib
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from tools.devkit.validator import validate_project


@dataclass
class PackageResult:
    success: bool
    version: str
    target_platform: str
    package_dir: Path
    zip_path: Optional[Path] = None
    tar_path: Optional[Path] = None
    files_included: List[str] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


def calculate_sha256(file_path: Path) -> str:
    """Calculate SHA256 hex digest for a file."""
    h = hashlib.sha256()
    with open(file_path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def find_or_build_executable(project_root: Path, build_if_missing: bool = True) -> Optional[Path]:
    """Locate existing neo_wolf binary or build it using available compiler/make."""
    project_root = Path(project_root).resolve()
    system = platform.system().lower()

    possible_binaries = [
        # CMake preset build directories (CMakePresets.json: build/<preset>/).
        project_root / "build" / "macos" / "neo_wolf",
        project_root / "build" / "linux" / "neo_wolf",
        project_root / "build" / "windows-msvc" / "Release" / "neo_wolf.exe",
        project_root / "build" / "ci-macos" / "neo_wolf",
        project_root / "build" / "ci-linux" / "neo_wolf",
        project_root / "build" / "ci-windows-msvc" / "Release" / "neo_wolf.exe",
        # Legacy locations.
        project_root / "build" / "neo_wolf",
        project_root / "build" / "Release" / "neo_wolf.exe",
        project_root / "build" / "neo_wolf.exe",
        project_root / "neo_wolf",
        project_root / "neo_wolf.exe",
    ]

    for p in possible_binaries:
        if p.is_file() and os.access(p, os.X_OK if system != "windows" else os.R_OK):
            return p

    if not build_if_missing:
        return None

    # Try building with make or g++
    if system != "windows":
        # Check if Makefile exists or build via g++
        try:
            cmd = [
                "g++",
                "-std=c++17",
                "-O2",
                "-I.",
                "-o",
                str(project_root / "build" / "neo_wolf"),
                "src/main.cpp",
                "engine/core/App.cpp",
                "engine/core/Log.cpp",
                "engine/core/Engine.Core.cpp",
                "engine/core/Engine.Init.cpp",
                "engine/core/Engine.Update.cpp",
                "engine/core/Engine.Render.cpp",
                "engine/core/Engine.Shutdown.cpp",
                "engine/assets/AssetManager.cpp",
                "engine/assets/AudioConfig.cpp",
                "engine/assets/ContentConfig.cpp",
                "engine/assets/Texture.cpp",
                "engine/assets/TextureLoader.cpp",
                "engine/audio/AudioSystem.cpp",
                "engine/platform/PlatformSDL.cpp",
                "engine/particles/Particle.cpp",
                "engine/render/Framebuffer.cpp",
                "engine/render/ParticleRenderer.cpp",
                "engine/render/Raycaster.cpp",
                "engine/render/SpriteRenderer.cpp",
                "engine/ui/UiRenderer.cpp",
                "engine/world/Entity.cpp",
                "engine/world/EntityPlacementLoader.cpp",
                "engine/world/InteractionRules.cpp",
                "engine/world/MapLoader.cpp",
                "engine/world/Map.cpp",
                "engine/world/MovementRules.cpp",
                "-lSDL2",
            ]
            (project_root / "build").mkdir(exist_ok=True)
            res = subprocess.run(cmd, cwd=project_root, capture_output=True, text=True, timeout=60)
            if res.returncode == 0 and (project_root / "build" / "neo_wolf").is_file():
                return project_root / "build" / "neo_wolf"
        except Exception:
            pass

    return None


def create_release_package(
    project_root: Path,
    output_dir: Path,
    version: str = "1.0.0",
    build_executable: bool = True,
    create_zip: bool = True,
    create_tar: bool = True,
) -> PackageResult:
    """Assemble a standalone release package with all assets, configs, maps, and launcher."""
    project_root = Path(project_root).resolve()
    output_dir = Path(output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    sys_name = platform.system().lower()
    pkg_name = f"neo-wolf-v{version}-{sys_name}"
    bundle_dir = output_dir / pkg_name

    # Reset clean bundle directory
    if bundle_dir.exists():
        shutil.rmtree(bundle_dir)
    bundle_dir.mkdir(parents=True, exist_ok=True)

    result = PackageResult(
        success=False,
        version=version,
        target_platform=sys_name,
        package_dir=bundle_dir,
    )

    # 1. Content validation pass
    val_rep = validate_project(project_root)
    if not val_rep.is_valid:
        result.errors.extend(val_rep.errors)
        result.errors.append("Packaging aborted due to content validation errors.")
        return result
    result.warnings.extend(val_rep.warnings)

    # 2. Copy Game Assets, Maps, and Defs
    for sub in ["assets", "maps", "defs"]:
        src_sub = project_root / "game" / sub
        dest_sub = bundle_dir / "game" / sub
        if src_sub.exists():
            shutil.copytree(src_sub, dest_sub)
            for f in dest_sub.rglob("*"):
                if f.is_file():
                    result.files_included.append(str(f.relative_to(bundle_dir)))

    # 3. Locate / Copy Executable
    exe_path = find_or_build_executable(project_root, build_if_missing=build_executable)
    if exe_path and exe_path.is_file():
        dest_exe = bundle_dir / exe_path.name
        shutil.copy2(exe_path, dest_exe)
        dest_exe.chmod(0o755)
        result.files_included.append(exe_path.name)
    else:
        result.warnings.append(
            "Executable binary could not be found or built automatically. Standalone bundle will contain content assets and launchers."
        )

    # 4. Generate Launchers
    # Linux launch.sh
    launch_sh = bundle_dir / "launch.sh"
    launch_sh.write_text(
        "#!/usr/bin/env bash\n"
        "# Neo-Wolf standalone game launcher\n"
        "set -e\n"
        "SCRIPT_DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n"
        "cd \"$SCRIPT_DIR\"\n"
        "if [ -f \"./neo_wolf\" ]; then\n"
        "    chmod +x ./neo_wolf\n"
        "    exec ./neo_wolf \"$@\"\n"
        "else\n"
        "    echo \"Error: neo_wolf binary not found in $SCRIPT_DIR\"\n"
        "    exit 1\n"
        "fi\n",
        encoding="utf-8",
    )
    launch_sh.chmod(0o755)
    result.files_included.append("launch.sh")

    # Windows launch.cmd
    launch_cmd = bundle_dir / "launch.cmd"
    launch_cmd.write_text(
        "@echo off\n"
        "cd /d \"%~dp0\"\n"
        "if exist neo_wolf.exe (\n"
        "    start \"\" neo_wolf.exe %*\n"
        ") else (\n"
        "    echo Error: neo_wolf.exe binary not found.\n"
        "    pause\n"
        ")\n",
        encoding="utf-8",
    )
    result.files_included.append("launch.cmd")

    # 5. Generate README.txt & RELEASE_NOTES.md
    readme_txt = bundle_dir / "README.txt"
    readme_txt.write_text(
        f"===============================================================\n"
        f"               Neo-Wolf v{version} (Classic 2.5D Raycaster)     \n"
        f"===============================================================\n\n"
        f"CONTROLS:\n"
        f"  W / Up Arrow          : Move Forward\n"
        f"  S / Down Arrow        : Move Backward\n"
        f"  A / D                 : Strafe Left / Right\n"
        f"  Left Arrow / Q        : Turn Left\n"
        f"  Right Arrow           : Turn Right\n"
        f"  Space / F / Ctrl      : Fire\n"
        f"  E                     : Open / Close Doors\n"
        f"  Esc / P               : Pause / Resume\n"
        f"  R                     : Restart Level\n"
        f"  Enter                 : Start / Confirm\n"
        f"  Tab                   : Toggle Minimap / Debug Overlay\n"
        f"  F11 / Alt+Enter       : Toggle Fullscreen (macOS: also Cmd+Ctrl+F)\n"
        f"  Esc (main menu)       : Quit\n\n"
        f"LAUNCHING:\n"
        f"  Linux:   Run ./launch.sh or ./neo_wolf\n"
        f"  Windows: Run launch.cmd or double-click neo_wolf.exe\n\n"
        f"SYSTEM REQUIREMENTS:\n"
        f"  - Linux (x86_64) with SDL2 or Windows 10/11 (x64)\n"
        f"  - Minimum 640x480 resolution display\n"
        f"  - Software CPU rendering (no dedicated GPU required)\n\n"
        f"Built with Neo-Wolf Engine & DevKit.\n",
        encoding="utf-8",
    )
    result.files_included.append("README.txt")

    release_notes = bundle_dir / "RELEASE_NOTES.md"
    release_notes.write_text(
        f"# Neo-Wolf v{version} Release Notes\n\n"
        f"### Highlights\n"
        f"- Software-rendered 2.5D classic raycasting engine with custom software framebuffer.\n"
        f"- Data-driven content pipeline (Netpbm P3 PPM textures, PCM WAV audio, JSON/CSV entity placements).\n"
        f"- DevKit toolchain with comprehensive asset/map validation, reachability analysis, and packaging.\n"
        f"- First playable level with animated doors, collectible pickups, enemy alert audio, and particle effects.\n\n"
        f"### Package Contents\n"
        f"- Native game binary & launcher scripts\n"
        f"- `game/assets/`: 64x64 wall/sprite PPM textures and WAV sound effects\n"
        f"- `game/maps/`: Grid maps with multi-tile wall definitions and triggers\n"
        f"- `game/defs/`: Entity, texture, sprite, and audio configurations\n",
        encoding="utf-8",
    )
    result.files_included.append("RELEASE_NOTES.md")

    # 6. Generate Checksums
    checksum_lines: List[str] = []
    for rel_file in sorted(result.files_included):
        fpath = bundle_dir / rel_file
        if fpath.is_file():
            csum = calculate_sha256(fpath)
            checksum_lines.append(f"{csum}  {rel_file}")
    sha_file = bundle_dir / "SHA256SUMS"
    sha_file.write_text("\n".join(checksum_lines) + "\n", encoding="utf-8")
    result.files_included.append("SHA256SUMS")

    # 7. Create Archives
    if create_zip:
        zip_path = output_dir / f"{pkg_name}.zip"
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
            for root, _, files in os.walk(bundle_dir):
                for file in files:
                    full_p = Path(root) / file
                    arc_name = Path(pkg_name) / full_p.relative_to(bundle_dir)
                    zf.write(full_p, arc_name)
        result.zip_path = zip_path

    if create_tar:
        tar_path = output_dir / f"{pkg_name}.tar.gz"
        with tarfile.open(tar_path, "w:gz") as tf:
            tf.add(bundle_dir, arcname=pkg_name)
        result.tar_path = tar_path

    result.success = True
    return result
