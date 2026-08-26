"""Command-line interface for the Neo-Wolf DevKit toolchain."""

import argparse
import sys
import unittest
from pathlib import Path
from typing import List, Optional

from tools.devkit.asset_tools import (
    create_brick_ppm,
    create_checkerboard_ppm,
    create_solid_ppm,
    generate_beep_wav,
    inspect_ppm,
    inspect_wav,
)
from tools.devkit.def_tools import sync_entity_definitions
from tools.devkit.doctor import render_doctor_report, run_doctor
from tools.devkit.map_tools import (
    analyze_reachability,
    create_template_map,
    get_map_stats,
    render_ascii_preview,
)
from tools.devkit.packager import create_release_package
from tools.devkit.validator import (
    validate_map,
    validate_ppm,
    validate_project,
    validate_wav,
)


def find_project_root() -> Path:
    """Locate the root of the Neo-Wolf repository."""
    curr = Path.cwd()
    for parent in [curr] + list(curr.parents):
        if (parent / "CMakeLists.txt").is_file() and (parent / "game").is_dir():
            return parent
    return curr


def cmd_doctor(args: argparse.Namespace) -> int:
    root = Path(args.root) if args.root else find_project_root()
    report = run_doctor(root, color=not args.no_color)
    print(render_doctor_report(report, color=not args.no_color))
    return 1 if report.has_failures else 0


def cmd_validate(args: argparse.Namespace) -> int:
    root = Path(args.root) if args.root else find_project_root()
    color = not args.no_color

    C_RESET = "\033[0m" if color else ""
    C_GREEN = "\033[32;1m" if color else ""
    C_YELLOW = "\033[33;1m" if color else ""
    C_RED = "\033[31;1m" if color else ""
    C_BOLD = "\033[1m" if color else ""

    print(f"{C_BOLD}Running Neo-Wolf Content Validation...{C_RESET}")

    if args.map:
        report, _ = validate_map(Path(args.map))
    elif args.asset:
        target = Path(args.asset)
        if target.suffix.lower() == ".ppm":
            report = validate_ppm(target)
        elif target.suffix.lower() == ".wav":
            report = validate_wav(target)
        else:
            print(f"{C_RED}Unsupported asset format: {target}{C_RESET}")
            return 1
    else:
        report = validate_project(root)

    # Print Report
    for info_msg in report.info:
        print(f"  [INFO] {info_msg}")

    for warn_msg in report.warnings:
        print(f"  {C_YELLOW}[WARN]{C_RESET} {warn_msg}")

    for err_msg in report.errors:
        print(f"  {C_RED}[FAIL]{C_RESET} {err_msg}")

    print("-" * 50)
    if report.is_valid:
        if report.warnings and args.strict:
            print(f"{C_RED}Validation failed due to strict mode ({len(report.warnings)} warning(s)).{C_RESET}")
            return 1
        print(f"{C_GREEN}Validation PASSED: Content is valid and engine-ready.{C_RESET}")
        return 0
    else:
        print(f"{C_RED}Validation FAILED: {len(report.errors)} error(s) detected.{C_RESET}")
        return 1


def cmd_inspect_map(args: argparse.Namespace) -> int:
    map_path = Path(args.map_path)
    if not map_path.exists():
        print(f"Error: Map file '{map_path}' not found.")
        return 1

    color = not args.no_color
    entities = None
    if args.entities:
        ent_path = Path(args.entities)
        if ent_path.suffix.lower() == ".csv":
            from tools.devkit.def_tools import load_entity_csv
            entities = load_entity_csv(ent_path)
        elif ent_path.suffix.lower() == ".json":
            from tools.devkit.def_tools import load_entity_json
            _, entities = load_entity_json(ent_path)

    # ASCII Preview
    preview = render_ascii_preview(map_path, entity_placements=entities, color=color)
    print(preview)

    if args.stats:
        stats = get_map_stats(map_path)
        print("\n--- Map Statistics ---")
        for k, v in stats.items():
            print(f"  {k:20s}: {v}")

    if args.reachability:
        res = analyze_reachability(map_path, spawn_x=args.spawn_x, spawn_y=args.spawn_y, entities=entities)
        print("\n--- Reachability Analysis ---")
        if res["success"]:
            print(f"  Status: ALL {res['reachable_count']} walkable cells and entities are reachable from spawn {res['spawn']}.")
        else:
            print(f"  Status: REACHABILITY ISSUES DETECTED!")
            if "error" in res:
                print(f"  Error: {res['error']}")
            print(f"  Unreachable cells ({res['unreachable_count']}): {res['unreachable_cells'][:10]}")
            if res.get("unreachable_entities"):
                print(f"  Unreachable entities: {res['unreachable_entities']}")

    return 0


def cmd_inspect_asset(args: argparse.Namespace) -> int:
    target = Path(args.asset_path)
    if not target.exists():
        print(f"Error: File '{target}' not found.")
        return 1

    ext = target.suffix.lower()
    if ext == ".ppm":
        meta = inspect_ppm(target)
        print(f"\n--- PPM Texture Inspector: {target.name} ---")
        for k, v in meta.items():
            print(f"  {k:25s}: {v}")
    elif ext == ".wav":
        meta = inspect_wav(target)
        print(f"\n--- WAV Audio Inspector: {target.name} ---")
        for k, v in meta.items():
            print(f"  {k:25s}: {v}")
    else:
        print(f"Unsupported file type '{ext}'. DevKit inspects .ppm and .wav files.")
        return 1
    return 0


def cmd_create_map(args: argparse.Namespace) -> int:
    out_path = Path(args.output)
    create_template_map(out_path, width=args.width, height=args.height, style=args.style)
    print(f"Created template map ({args.width}x{args.height}, style: {args.style}) at: {out_path}")
    return 0


def cmd_create_asset(args: argparse.Namespace) -> int:
    out_path = Path(args.output)
    atype = args.type.lower()
    if atype == "checker":
        create_checkerboard_ppm(out_path, width=args.width, height=args.height)
        print(f"Created checkerboard PPM at {out_path}")
    elif atype == "brick":
        create_brick_ppm(out_path, width=args.width, height=args.height)
        print(f"Created procedural brick PPM at {out_path}")
    elif atype == "solid":
        create_solid_ppm(out_path, width=args.width, height=args.height)
        print(f"Created solid color PPM at {out_path}")
    elif atype == "beep":
        generate_beep_wav(out_path, duration_sec=args.duration)
        print(f"Created procedural beep WAV at {out_path}")
    else:
        print(f"Unknown asset type '{atype}'. Options: checker, brick, solid, beep")
        return 1
    return 0


def cmd_sync_entities(args: argparse.Namespace) -> int:
    root = Path(args.root) if args.root else find_project_root()
    synced = sync_entity_definitions(root, prefer_csv=not args.prefer_json)
    for s in synced:
        print(f"  {s}")
    print(f"Entity definitions synchronization complete ({len(synced)} file pairs).")
    return 0


def cmd_package(args: argparse.Namespace) -> int:
    root = Path(args.root) if args.root else find_project_root()
    out_dir = Path(args.out_dir) if args.out_dir else root / "release"
    print(f"Building Neo-Wolf v{args.version} standalone release package in {out_dir}...")
    res = create_release_package(
        project_root=root,
        output_dir=out_dir,
        version=args.version,
        build_executable=not args.no_build,
        create_zip=not args.no_zip,
        create_tar=not args.no_tar,
    )

    if res.success:
        print(f"\nPackaging SUCCESSFUL:")
        print(f"  Target bundle: {res.package_dir}")
        if res.zip_path:
            print(f"  ZIP Archive:   {res.zip_path} ({res.zip_path.stat().st_size} bytes)")
        if res.tar_path:
            print(f"  Tarball:       {res.tar_path} ({res.tar_path.stat().st_size} bytes)")
        print(f"  Files packed:  {len(res.files_included)}")
        return 0
    else:
        print(f"\nPackaging FAILED:")
        for err in res.errors:
            print(f"  [ERROR] {err}")
        return 1


def cmd_test(args: argparse.Namespace) -> int:
    """Run DevKit Python test suite."""
    print("Running DevKit automated test suite...")
    loader = unittest.TestLoader()
    start_dir = Path(__file__).parent / "tests"
    if not start_dir.exists():
        print(f"Test directory not found at {start_dir}")
        return 1
    suite = loader.discover(str(start_dir), pattern="test_*.py")
    runner = unittest.TextTestRunner(verbosity=2)
    res = runner.run(suite)
    return 0 if res.wasSuccessful() else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="neo-devkit",
        description="Neo-Wolf DevKit: Developer toolchain, validator, map/asset inspector, and packager.",
    )
    parser.add_argument("--no-color", action="store_true", help="Disable ANSI terminal colors.")
    parser.add_argument("--root", type=str, default=None, help="Explicit repository root directory.")

    subparsers = parser.add_subparsers(dest="command", required=True)

    # doctor
    subparsers.add_parser("doctor", help="Run comprehensive environment and project health checks.")

    # validate
    val_p = subparsers.add_parser("validate", help="Validate maps, assets, and game definitions.")
    val_p.add_argument("--map", type=str, help="Validate a specific .map file.")
    val_p.add_argument("--asset", type=str, help="Validate a specific .ppm or .wav file.")
    val_p.add_argument("--strict", action="store_true", help="Treat warnings as errors.")

    # inspect-map
    map_p = subparsers.add_parser("inspect-map", help="Inspect and preview a .map file in terminal.")
    map_p.add_argument("map_path", type=str, help="Path to .map file.")
    map_p.add_argument("--entities", type=str, help="Overlay entities from .csv or .json.")
    map_p.add_argument("--stats", action="store_true", default=True, help="Display map statistics.")
    map_p.add_argument("--reachability", action="store_true", default=True, help="Run reachability BFS from spawn.")
    map_p.add_argument("--spawn-x", type=float, default=2.5, help="Spawn X coordinate for reachability.")
    map_p.add_argument("--spawn-y", type=float, default=2.5, help="Spawn Y coordinate for reachability.")

    # inspect-asset
    ast_p = subparsers.add_parser("inspect-asset", help="Inspect metadata of PPM texture or WAV audio.")
    ast_p.add_argument("asset_path", type=str, help="Path to .ppm or .wav asset.")

    # create-map
    cmap_p = subparsers.add_parser("create-map", help="Create a new template map.")
    cmap_p.add_argument("output", type=str, help="Output .map file path.")
    cmap_p.add_argument("--width", type=int, default=16, help="Map width in cells.")
    cmap_p.add_argument("--height", type=int, default=12, help="Map height in cells.")
    cmap_p.add_argument("--style", type=str, choices=["box", "arena", "dungeon", "crossroads"], default="arena")

    # create-asset
    cast_p = subparsers.add_parser("create-asset", help="Generate procedural PPM textures or WAV audio.")
    cast_p.add_argument("output", type=str, help="Output asset file path.")
    cast_p.add_argument("--type", type=str, choices=["checker", "brick", "solid", "beep"], default="checker")
    cast_p.add_argument("--width", type=int, default=64)
    cast_p.add_argument("--height", type=int, default=64)
    cast_p.add_argument("--duration", type=float, default=0.25)

    # sync-entities
    sync_p = subparsers.add_parser("sync-entities", help="Synchronize CSV and JSON entity definition files.")
    sync_p.add_argument("--prefer-json", action="store_true", help="Use JSON as source of truth instead of CSV.")

    # package
    pkg_p = subparsers.add_parser("package", help="Assemble standalone release bundle.")
    pkg_p.add_argument("--out-dir", type=str, default=None, help="Output directory (defaults to release/).")
    pkg_p.add_argument("--version", type=str, default="1.0.0", help="Release version string.")
    pkg_p.add_argument("--no-build", action="store_true", help="Do not attempt to compile binary if missing.")
    pkg_p.add_argument("--no-zip", action="store_true", help="Skip creating .zip archive.")
    pkg_p.add_argument("--no-tar", action="store_true", help="Skip creating .tar.gz archive.")

    # test
    subparsers.add_parser("test", help="Run DevKit unit test suite.")

    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)

    handlers = {
        "doctor": cmd_doctor,
        "validate": cmd_validate,
        "inspect-map": cmd_inspect_map,
        "inspect-asset": cmd_inspect_asset,
        "create-map": cmd_create_map,
        "create-asset": cmd_create_asset,
        "sync-entities": cmd_sync_entities,
        "package": cmd_package,
        "test": cmd_test,
    }

    handler = handlers.get(args.command)
    if handler:
        return handler(args)
    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
