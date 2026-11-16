"""Release packaging for Neo-Wolf (Roadmap Phase 9, docs/RELEASE.md).

The package is defined by CMake (cmake/Packaging.cmake: install rules, the app bundle, icons, the
README.txt); this module only drives it and checks the result:

  1. validate the content (`validate --strict` rules: errors and warnings fail);
  2. configure and build the package preset for this OS (package-macos, package-windows-msvc,
     package-linux), or build an existing build tree given with build_dir (e.g. a cross-compile);
  3. `cmake --install` into a staging folder named after the package;
  4. check the staged tree: the executable must be there (known issue D1: the old packager
     reported success without one), plus the content, README.txt, LICENSE.md, docs/ and LICENSES/, and no
     test or autoplay executable; on macOS also the app's signature;
  5. write <package>.zip (macOS, Windows) or <package>.tar.gz (Linux) with one top folder, and
     add its SHA-256 to SHA256SUMS in the output folder (other entries are kept, so one folder
     can collect the packages of every OS).

Any failure returns a result with errors and no archive; the CLI then exits with code 1.
The command runner is injectable so the tests can simulate CMake (tools/devkit/tests/test_packager.py).
"""

import hashlib
import json
import os
import platform
import plistlib
import shutil
import stat
import subprocess
import tarfile
import time
import zipfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, List, Mapping, Optional, Sequence

from tools.devkit.validator import validate_project

PACKAGE_PRESETS: Dict[str, str] = {
    "darwin": "package-macos",
    "windows": "package-windows-msvc",
    "linux": "package-linux",
}
INFO_FILE = "neowolf-package.json"
CHECKSUM_FILE = "SHA256SUMS"
REQUIRED_INFO_KEYS = ("version", "os", "platform", "package", "format", "executable", "content_root")
# Executables that must never ship (the native tests and the autoplay bot).
FORBIDDEN_EXECUTABLES = ("neo_wolf_tests", "neo_wolf_autoplay")
REQUIRED_FILES = ("README.txt", "LICENSE.md", "docs/CONTROLS.md", "docs/TROUBLESHOOTING.md",
                  "LICENSES/SDL2-LICENSE.txt", "LICENSES/nlohmann-json-LICENSE.MIT.txt",
                  "LICENSES/THIRD_PARTY_LICENSES.md")

# runner(command, working directory) -> exit code. The default streams the output.
Runner = Callable[[Sequence[str], Path], int]


@dataclass
class PackageResult:
    success: bool = False
    archive: Optional[Path] = None
    checksum_file: Optional[Path] = None
    info: Dict[str, object] = field(default_factory=dict)
    build_dir: Optional[Path] = None
    staging_dir: Optional[Path] = None
    files: List[str] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


def calculate_sha256(file_path: Path) -> str:
    """SHA-256 hex digest of a file."""
    h = hashlib.sha256()
    with open(file_path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def default_runner(command: Sequence[str], cwd: Path) -> int:
    print("$ " + " ".join(f'"{c}"' if " " in c else c for c in command), flush=True)
    try:
        return subprocess.run(list(command), cwd=str(cwd)).returncode
    except OSError as exc:
        print(f"cannot run {command[0]}: {exc}", flush=True)
        return 127


def host_os() -> str:
    """darwin, windows or linux."""
    return platform.system().lower()


def find_cmake(system: Optional[str] = None, which: Callable[[str], Optional[str]] = shutil.which,
               environ: Optional[Mapping[str, str]] = None) -> Optional[str]:
    """cmake on PATH; on Windows otherwise the CMake bundled with the newest Visual Studio."""
    found = which("cmake")
    if found:
        return found
    if (system or host_os()) != "windows":
        return None
    env = os.environ if environ is None else environ
    vswhere = Path(env.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / \
        "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None
    try:
        out = subprocess.run([str(vswhere), "-latest", "-products", "*", "-property", "installationPath"],
                             capture_output=True, text=True, timeout=30).stdout.strip().splitlines()
    except (OSError, subprocess.SubprocessError):
        return None
    for vs in out:
        candidate = Path(vs.strip()) / "Common7" / "IDE" / "CommonExtensions" / "Microsoft" / "CMake" / \
            "CMake" / "bin" / "cmake.exe"
        if candidate.is_file():
            return str(candidate)
    return None


def read_package_info(build_dir: Path) -> Dict[str, object]:
    """The build's neowolf-package.json (written by cmake/Packaging.cmake at configure time)."""
    path = build_dir / INFO_FILE
    if not path.is_file():
        raise ValueError(f"{path} not found: {build_dir} is not a configured Neo-Wolf build "
                         "(configure it with CMake first, or use a package preset)")
    try:
        info = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise ValueError(f"{path}: {exc}") from exc
    missing = [key for key in REQUIRED_INFO_KEYS if not info.get(key)]
    if missing:
        raise ValueError(f"{path}: missing {', '.join(missing)}")
    if info["format"] not in ("zip", "tar.gz"):
        raise ValueError(f"{path}: unknown archive format {info['format']!r}")
    return info


def _relative_files(tree: Path) -> List[str]:
    return sorted(p.relative_to(tree).as_posix() for p in tree.rglob("*") if p.is_file())


def check_package_tree(tree: Path, info: Mapping[str, object]) -> List[str]:
    """Problems in an installed package tree; an empty list means it may be archived."""
    errors: List[str] = []
    if not tree.is_dir():
        return [f"the install produced no package folder ({tree})"]

    exe = tree / str(info["executable"])
    if not exe.is_file():
        errors.append(f"no executable in the package: {info['executable']} is missing "
                      "(the build or the install rules did not produce the game)")
    elif info["os"] != "windows" and not os.access(exe, os.X_OK):
        errors.append(f"{info['executable']} is not executable")

    content = tree / str(info["content_root"])
    for sentinel in ("game/maps", "game/defs"):
        if not (content / sentinel).is_dir():
            errors.append(f"no {sentinel}/ in {info['content_root']}: the game would not find its content")
    if not (content / "game" / "defs" / "campaign.json").is_file():
        errors.append("game/defs/campaign.json is missing")

    for rel in REQUIRED_FILES:
        if not (tree / rel).is_file():
            errors.append(f"{rel} is missing")
    readme = tree / "README.txt"
    if readme.is_file() and str(info["version"]) not in readme.read_text(encoding="utf-8", errors="replace"):
        errors.append(f"README.txt does not mention version {info['version']}")

    for rel in _relative_files(tree):
        name = rel.rsplit("/", 1)[-1]
        if any(name == bad or name.startswith(bad + ".") for bad in FORBIDDEN_EXECUTABLES):
            errors.append(f"{rel} must not be shipped (test/autoplay executable)")

    if info["os"] == "macos":
        app = str(info["executable"]).split("/Contents/")[0]
        plist_path = tree / app / "Contents" / "Info.plist"
        try:
            with open(plist_path, "rb") as handle:
                plist = plistlib.load(handle)
        except (OSError, plistlib.InvalidFileException, ValueError) as exc:
            errors.append(f"{app}/Contents/Info.plist cannot be read: {exc}")
        else:
            if plist.get("CFBundleShortVersionString") != info["version"]:
                errors.append(f"Info.plist CFBundleShortVersionString is "
                              f"{plist.get('CFBundleShortVersionString')!r}, expected {info['version']!r}")
            if plist.get("CFBundleExecutable") != Path(str(info["executable"])).name:
                errors.append(f"Info.plist CFBundleExecutable is {plist.get('CFBundleExecutable')!r}")
    return errors


def _timestamp(path: Path) -> float:
    epoch = os.environ.get("SOURCE_DATE_EPOCH")
    if epoch and epoch.isdigit():
        return float(epoch)
    return path.stat().st_mtime


def _zip_date(seconds: float):
    return time.localtime(max(seconds, 315532800 + 86400))[:6]  # zip dates start in 1980


def write_archive(tree: Path, out_dir: Path, package: str, fmt: str,
                  executables: Sequence[str] = ()) -> Path:
    """<out_dir>/<package>.<fmt> containing tree as the folder <package>/ (sorted, with Unix
    permissions, so the executable stays executable after extraction).

    `executables` lists paths relative to `tree` that are always stored as 0755. The host
    filesystem cannot be trusted for this: Windows has no exec bit, so a macOS or Linux package
    assembled there would otherwise lose it."""
    forced_exec = {Path(e).as_posix() for e in executables}
    entries = sorted(tree.rglob("*"), key=lambda p: p.relative_to(tree).as_posix())
    archive = out_dir / f"{package}.{fmt}"
    if archive.exists():
        archive.unlink()
    if fmt == "zip":
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
            for path in [tree] + entries:
                rel = path.relative_to(tree).as_posix()
                arcname = package if rel == "." else f"{package}/{rel}"
                if path.is_dir():
                    arcname += "/"
                zi = zipfile.ZipInfo(arcname, _zip_date(_timestamp(path)))
                zi.create_system = 3  # Unix: the permission bits below are honoured on extraction
                if path.is_dir():
                    zi.external_attr = ((stat.S_IFDIR | 0o755) << 16) | 0x10
                    zf.writestr(zi, b"")
                else:
                    is_exec = rel in forced_exec or path.stat().st_mode & stat.S_IXUSR
                    perm = 0o755 if is_exec else 0o644
                    zi.external_attr = (stat.S_IFREG | perm) << 16
                    zi.compress_type = zipfile.ZIP_DEFLATED
                    zf.writestr(zi, path.read_bytes())
    else:
        def normalise(ti: tarfile.TarInfo) -> tarfile.TarInfo:
            ti.uid = ti.gid = 0
            ti.uname = ti.gname = ""
            rel = ti.name.split("/", 1)[1] if "/" in ti.name else ""
            is_exec = ti.isdir() or rel in forced_exec or ti.mode & stat.S_IXUSR
            ti.mode = 0o755 if is_exec else 0o644
            return ti

        with tarfile.open(archive, "w:gz") as tf:
            tf.add(str(tree), arcname=package, recursive=False, filter=normalise)
            for path in entries:
                tf.add(str(path), arcname=f"{package}/{path.relative_to(tree).as_posix()}", recursive=False,
                       filter=normalise)
    return archive


def update_checksums(out_dir: Path, archives: Sequence[Path]) -> Path:
    """Adds or replaces the archives' lines in <out_dir>/SHA256SUMS (`sha256sum -c` format)."""
    sums_path = out_dir / CHECKSUM_FILE
    lines: Dict[str, str] = {}
    if sums_path.is_file():
        for line in sums_path.read_text(encoding="utf-8").splitlines():
            parts = line.split(None, 1)
            if len(parts) == 2:
                lines[parts[1].lstrip("*")] = parts[0]
    for archive in archives:
        lines[archive.name] = calculate_sha256(archive)
    with open(sums_path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write("".join(f"{digest}  {name}\n" for name, digest in sorted(lines.items())))
    return sums_path


def clean_output(out_dir: Path) -> None:
    """Removes earlier archives and SHA256SUMS from the output folder."""
    if not out_dir.is_dir():
        return
    for path in list(out_dir.glob("neo-wolf-*.zip")) + list(out_dir.glob("neo-wolf-*.tar.gz")) + \
            [out_dir / CHECKSUM_FILE]:
        if path.is_file():
            path.unlink()


def create_release_package(
    project_root: Path,
    output_dir: Path,
    preset: Optional[str] = None,
    build_dir: Optional[Path] = None,
    build: bool = True,
    expect_version: Optional[str] = None,
    clean: bool = False,
    keep_staging: bool = False,
    validate: bool = True,
    cmake: Optional[str] = None,
    runner: Runner = default_runner,
    system: Optional[str] = None,
) -> PackageResult:
    """Builds the package for this OS (see the module docstring). Never raises for expected
    failures: they are in result.errors and result.success is False."""
    project_root = Path(project_root).resolve()
    output_dir = Path(output_dir).resolve()
    system = system or host_os()
    result = PackageResult()

    def fail(message: str) -> PackageResult:
        result.errors.append(message)
        return result

    # 1. Content.
    if validate:
        report = validate_project(project_root)
        result.warnings.extend(report.warnings)
        if report.errors or report.warnings:
            result.errors.extend(report.errors + [f"(strict) {w}" for w in report.warnings])
            return fail("content validation failed (as `validate --strict`): nothing was packaged")

    # 2. Configure and build.
    cmake = cmake or find_cmake(system)
    if not cmake:
        return fail("cmake not found: install CMake 3.25+ (docs/BUILD_MACOS.md, docs/BUILD_WINDOWS.md)")
    if build_dir is None:
        preset = preset or PACKAGE_PRESETS.get(system)
        if not preset:
            return fail(f"no package preset for this OS ({system}); give --preset or --build-dir")
        build_dir = project_root / "build" / preset
        if build:
            if runner([cmake, "--preset", preset], project_root) != 0:
                return fail(f"CMake configure failed (preset {preset})")
            if runner([cmake, "--build", "--preset", preset], project_root) != 0:
                return fail(f"build failed (preset {preset})")
    else:
        build_dir = Path(build_dir).resolve()
        if not (build_dir / "CMakeCache.txt").is_file():
            return fail(f"{build_dir} is not a configured CMake build tree (no CMakeCache.txt)")
        if build and runner([cmake, "--build", str(build_dir), "--config", "Release"], project_root) != 0:
            return fail(f"build failed ({build_dir})")
    result.build_dir = build_dir

    try:
        info = read_package_info(build_dir)
    except ValueError as exc:
        return fail(str(exc))
    result.info = info
    if expect_version and info["version"] != expect_version:
        return fail(f"version mismatch: CMake's project(VERSION) is {info['version']}, expected "
                    f"{expect_version} (docs/RELEASE.md, Versioning)")

    # 3. Install into staging.
    output_dir.mkdir(parents=True, exist_ok=True)
    if clean:
        clean_output(output_dir)
    staging_root = output_dir / ".staging"
    staging = staging_root / str(info["package"])
    if staging.exists():
        shutil.rmtree(staging)
    result.staging_dir = staging
    if runner([cmake, "--install", str(build_dir), "--config", "Release", "--prefix", str(staging)],
              project_root) != 0:
        return fail("cmake --install failed")

    # 4. Check the tree.
    problems = check_package_tree(staging, info)
    if problems:
        result.errors.extend(problems)
        return fail("the package is incomplete: no archive was written")
    if info["os"] == "macos" and system == "darwin":
        app = staging / str(info["executable"]).split("/Contents/")[0]
        if runner(["codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app)], project_root) != 0:
            return fail(f"codesign --verify failed for {app.name}")
    result.files = _relative_files(staging)

    # 5. Archive and checksum.
    archive = write_archive(staging, output_dir, str(info["package"]), str(info["format"]),
                            executables=[str(info["executable"])])
    result.archive = archive
    result.checksum_file = update_checksums(output_dir, [archive])
    if not keep_staging:
        shutil.rmtree(staging_root, ignore_errors=True)
        result.staging_dir = None
    result.success = True
    return result
