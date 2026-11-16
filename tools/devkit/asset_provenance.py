#!/usr/bin/env python3
"""Checks that every file under game/assets is made by one of the repository's generators
(Roadmap Phase 10, known issue X5).

MANIFEST maps each generator to the folder under game/assets it owns. The check runs every
generator in memory and compares its output with the files on disk, byte for byte:

- a file on disk that no generator produces is an exception (an asset of unknown origin);
- a generated file that is missing or differs on disk is out of date (run the generator).

The application icon is not listed: tools/devkit/gen_icon.py draws it at build time and
nothing it makes is committed (docs/ASSET_CREDITS.md).

DevKit test test_asset_provenance runs the check on the repository, so an asset of unknown
origin cannot be added unnoticed.

Usage:
    python3 tools/devkit/asset_provenance.py            # exit 1 on any exception
    python3 tools/devkit/asset_provenance.py --list     # also list every file and its generator

Standard library only.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Callable, NamedTuple

try:  # run as a module (tests) or as a script
    from tools.devkit import gen_sounds, gen_sprites, gen_textures
except ImportError:  # pragma: no cover
    sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
    from tools.devkit import gen_sounds, gen_sprites, gen_textures

REPO_ROOT = Path(__file__).resolve().parents[2]
ASSETS_DIR = REPO_ROOT / "game" / "assets"

# Files the operating system may drop into any folder; they are not assets and never shipped
# from a clean checkout.
IGNORED_NAMES = {".DS_Store", "Thumbs.db", "desktop.ini"}


class Generator(NamedTuple):
    script: str  # path from the repository root
    folder: str  # folder under game/assets that its output goes to ("" = game/assets itself)
    generate: Callable[[], dict[str, str | bytes]]


MANIFEST: tuple[Generator, ...] = (
    Generator("tools/devkit/gen_textures.py", "themes", gen_textures.generate),
    Generator("tools/devkit/gen_sprites.py", "sprites", gen_sprites.generate),
    Generator("tools/devkit/gen_sounds.py", "", gen_sounds.generate),
)


def expected_files() -> dict[str, tuple[bytes, str]]:
    """{"<path under game/assets, with />": (bytes, generator script)} for every generated file.
    Text output is UTF-8 with \\n line endings, exactly as the generators write it."""
    files: dict[str, tuple[bytes, str]] = {}
    for generator in MANIFEST:
        for rel, content in generator.generate().items():
            path = f"{generator.folder}/{rel}" if generator.folder else rel
            data = content.encode("utf-8") if isinstance(content, str) else content
            if path in files:
                raise ValueError(f"{path} is made by both {files[path][1]} and {generator.script}")
            files[path] = (data, generator.script)
    return files


class Report(NamedTuple):
    matched: dict[str, str]  # path -> generator script
    unknown: list[str]  # on disk, no generator makes it
    stale: list[str]  # generated, but missing on disk or different


def check(assets_dir: Path = ASSETS_DIR) -> Report:
    expected = expected_files()
    on_disk = sorted(
        p.relative_to(assets_dir).as_posix()
        for p in assets_dir.rglob("*")
        if p.is_file() and p.name not in IGNORED_NAMES
    )
    matched: dict[str, str] = {}
    unknown: list[str] = []
    stale: list[str] = []
    for rel in on_disk:
        if rel not in expected:
            unknown.append(rel)
        elif (assets_dir / rel).read_bytes() != expected[rel][0]:
            stale.append(rel)
        else:
            matched[rel] = expected[rel][1]
    stale.extend(sorted(rel for rel in expected if rel not in on_disk))
    return Report(matched, unknown, stale)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--assets", type=Path, default=ASSETS_DIR, help="folder to check (default: game/assets)")
    parser.add_argument("--list", action="store_true", help="list every file and the generator that makes it")
    args = parser.parse_args(argv if argv is not None else sys.argv[1:])

    report = check(args.assets)
    if args.list:
        for rel, script in sorted(report.matched.items()):
            print(f"{rel}  <-  {script}")
    for rel in report.unknown:
        print(f"Not made by any generator (unknown origin): game/assets/{rel}")
    for rel in report.stale:
        print(f"Missing or out of date (run its generator): game/assets/{rel}")
    if report.unknown or report.stale:
        return 1
    counts: dict[str, int] = {}
    for script in report.matched.values():
        counts[script] = counts.get(script, 0) + 1
    summary = ", ".join(f"{Path(script).name} {count}" for script, count in counts.items())
    print(
        f"All {len(report.matched)} files under game/assets are reproduced byte for byte by the generators ({summary})."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
