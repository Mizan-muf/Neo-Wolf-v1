#!/usr/bin/env python3
"""Run the code-quality checks from docs/CODING_STANDARDS.md.

    python3 tools/ci/lint.py                 # format + python (no build needed)
    python3 tools/ci/lint.py --tidy -p build/tidy
    python3 tools/ci/lint.py --all -p build/tidy

Checks:
  --format   clang-format --dry-run --Werror on every tracked C++ file (third_party excluded)
  --python   ruff check and ruff format --check on the Python tools
  --tidy     clang-tidy (.clang-tidy, tests/.clang-tidy) on every engine, src and test
             translation unit in <build>/compile_commands.json; needs a configured and built
             tree (configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)

Tools are found on PATH, or in --tool-dir. CI pins the versions (see .github/workflows/ci.yml).
Exit codes: 0 all selected checks pass, 1 a check failed, 2 usage error or a tool is missing.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CPP_SUFFIXES = (".cpp", ".h")
EXCLUDED_DIRS = ("third_party/",)
TIDY_SOURCE_DIRS = ("engine/", "src/", "tests/")


class ToolMissingError(Exception):
    pass


def find_tool(name: str, tool_dir: str | None) -> str:
    if tool_dir:
        candidate = Path(tool_dir) / (name + (".exe" if os.name == "nt" else ""))
        if candidate.is_file():
            return str(candidate)
    found = shutil.which(name)
    if not found:
        raise ToolMissingError(f"{name} not found (install it, or pass --tool-dir)")
    return found


def tracked_cpp_files() -> list[str]:
    out = subprocess.run(["git", "ls-files"], cwd=REPO, capture_output=True, text=True, check=True).stdout
    return [f for f in out.splitlines() if f.endswith(CPP_SUFFIXES) and not f.startswith(EXCLUDED_DIRS)]


def check_format(tool_dir: str | None) -> bool:
    clang_format = find_tool("clang-format", tool_dir)
    files = tracked_cpp_files()
    result = subprocess.run([clang_format, "--dry-run", "--Werror", *files], cwd=REPO, check=False)
    print(f"clang-format: {len(files)} files, {'OK' if result.returncode == 0 else 'needs formatting'}")
    return result.returncode == 0


def check_python(tool_dir: str | None) -> bool:
    ruff = find_tool("ruff", tool_dir)
    lint = subprocess.run([ruff, "check", "--no-cache", "."], cwd=REPO, check=False)
    fmt = subprocess.run([ruff, "format", "--no-cache", "--check", "."], cwd=REPO, check=False)
    ok = lint.returncode == 0 and fmt.returncode == 0
    print(f"ruff: {'OK' if ok else 'failed'}")
    return ok


def macos_sysroot_args() -> list[str]:
    # A clang-tidy that is not Apple's needs the SDK path to find the system headers.
    if platform.system() != "Darwin":
        return []
    sdk = subprocess.run(["xcrun", "--show-sdk-path"], capture_output=True, text=True, check=False).stdout.strip()
    return [f"--extra-arg=-isysroot{sdk}"] if sdk else []


def is_tidy_source(path: str) -> bool:
    try:
        relative = Path(path).resolve().relative_to(REPO).as_posix()
    except ValueError:  # outside the repository (e.g. a system header)
        return False
    return relative.startswith(TIDY_SOURCE_DIRS)


def check_tidy(tool_dir: str | None, build_dir: str, jobs: int) -> bool:
    clang_tidy = find_tool("clang-tidy", tool_dir)
    database = Path(build_dir) / "compile_commands.json"
    if not database.is_file():
        raise ToolMissingError(f"{database} not found (configure with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON and build)")
    entries = json.loads(database.read_text(encoding="utf-8"))
    sources = sorted({entry["file"] for entry in entries if is_tidy_source(entry["file"])})
    extra = macos_sysroot_args()

    def run(source: str) -> tuple[int, str]:
        result = subprocess.run(
            [clang_tidy, "-p", build_dir, "--quiet", *extra, source],
            cwd=REPO,
            capture_output=True,
            text=True,
            check=False,
        )
        return result.returncode, result.stdout

    failures = 0
    with ThreadPoolExecutor(max_workers=jobs) as pool:
        for code, output in pool.map(run, sources):
            if code != 0:
                failures += 1
                print(output.rstrip())
    print(f"clang-tidy: {len(sources)} translation units, {failures} with diagnostics")
    return failures == 0


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--format", action="store_true", help="check C++ formatting")
    parser.add_argument("--python", action="store_true", help="ruff lint and format check")
    parser.add_argument("--tidy", action="store_true", help="clang-tidy (needs -p)")
    parser.add_argument("--all", action="store_true", help="all three checks")
    parser.add_argument("-p", "--build-dir", help="build folder with compile_commands.json (for --tidy)")
    parser.add_argument("--tool-dir", help="folder holding clang-format, clang-tidy and ruff")
    parser.add_argument("-j", "--jobs", type=int, default=os.cpu_count() or 4, help="parallel clang-tidy runs")
    args = parser.parse_args(argv)

    run_format = args.format or args.all or not (args.python or args.tidy)
    run_python = args.python or args.all or not (args.format or args.tidy)
    run_tidy = args.tidy or args.all
    if run_tidy and not args.build_dir:
        parser.error("--tidy needs -p <build folder>")

    try:
        results = []
        if run_format:
            results.append(check_format(args.tool_dir))
        if run_python:
            results.append(check_python(args.tool_dir))
        if run_tidy:
            results.append(check_tidy(args.tool_dir, args.build_dir, args.jobs))
    except ToolMissingError as error:
        print(f"lint.py: {error}", file=sys.stderr)
        return 2
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
