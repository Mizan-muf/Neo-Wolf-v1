"""Command-line interface for the Neo-Wolf DevKit toolchain."""

import argparse
import json
import sys
import unittest
from pathlib import Path
from typing import List, Optional, Tuple

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
from tools.devkit.engine_formats import check_light_bounds, load_json_strict, parse_entities, parse_map
from tools.devkit.map_tools import (
    analyze_reachability,
    create_level_stub_entities,
    create_template_map,
    get_map_stats,
    render_ascii_preview,
    save_entities_json,
)
from tools.devkit.packager import create_release_package
from tools.devkit.term import Palette, supports_color
from tools.devkit.validator import (
    CampaignLevelInfo,
    ValidationReport,
    _is_content_id,
    read_campaign,
    validate_map,
    validate_ppm,
    validate_project,
    validate_wav,
)


def is_project_root(path: Path) -> bool:
    """A Neo-Wolf content root: it has game/maps and game/defs (what the engine looks for)."""
    return (path / "game" / "maps").is_dir() and (path / "game" / "defs").is_dir()


def find_project_root() -> Path:
    """The repository this DevKit belongs to (resolved from this file, so any working directory
    works); failing that, the working directory or one of its parents that holds a game/."""
    here = Path(__file__).resolve().parents[2]
    if is_project_root(here):
        return here
    cwd = Path.cwd()
    for parent in [cwd] + list(cwd.parents):
        if is_project_root(parent):
            return parent
    return here


def resolve_root(args: argparse.Namespace) -> Path:
    return Path(args.root).resolve() if args.root else find_project_root()


def cmd_doctor(args: argparse.Namespace) -> int:
    root = resolve_root(args)
    report = run_doctor(root, check_network=not args.no_network)
    print(render_doctor_report(report, color=args.color))
    return 1 if report.has_failures else 0


def cmd_validate(args: argparse.Namespace) -> int:
    root = resolve_root(args)
    c = Palette(args.color)
    print(f"{c.bold}Running Neo-Wolf Content Validation...{c.reset}")

    if args.map:
        report, _ = validate_map(Path(args.map))
    elif args.asset:
        target = Path(args.asset)
        if target.suffix.lower() == ".ppm":
            report = validate_ppm(target)
        elif target.suffix.lower() == ".wav":
            report = validate_wav(target)
        else:
            print(f"{c.red}Unsupported asset format: {target}{c.reset}")
            return 1
    else:
        report = validate_project(root, require_levels=args.require_levels)

    for info_msg in report.info:
        print(f"  [INFO] {info_msg}")
    for warn_msg in report.warnings:
        print(f"  {c.yellow}[WARN]{c.reset} {warn_msg}")
    for err_msg in report.errors:
        print(f"  {c.red}[FAIL]{c.reset} {err_msg}")

    print("-" * 50)
    if report.is_valid:
        if report.warnings and args.strict:
            print(f"{c.red}Validation failed due to strict mode ({len(report.warnings)} warning(s)).{c.reset}")
            return 1
        print(f"{c.green}Validation PASSED: Content is valid and engine-ready.{c.reset}")
        return 0
    print(f"{c.red}Validation FAILED: {len(report.errors)} error(s) detected.{c.reset}")
    return 1


def _find_campaign_level(root: Path, wanted: str) -> Tuple[Optional[CampaignLevelInfo], List[str]]:
    """A campaign level by id or by 1-based number."""
    scratch = ValidationReport()
    levels = read_campaign(root, scratch) or []
    for level in levels:
        if level.id == wanted or (wanted.isdigit() and int(wanted) == level.index + 1):
            return level, []
    return None, scratch.errors or [f"campaign.json has no level '{wanted}' (ids: {', '.join(l.id for l in levels)})"]


def cmd_inspect_map(args: argparse.Namespace) -> int:
    root = resolve_root(args)
    entities_path: Optional[Path] = None
    if args.level:
        level, problems = _find_campaign_level(root, args.level)
        if level is None:
            for problem in problems:
                print(f"Error: {problem}")
            return 1
        map_path = root / level.map
        entities_path = root / level.entities
    elif args.map_path:
        map_path = Path(args.map_path)
        if args.entities:
            entities_path = Path(args.entities)
        elif not args.no_entities:
            # The campaign level that uses this map, else game/defs/<map name>.entities.json.
            scratch = ValidationReport()
            for level in read_campaign(root, scratch) or []:
                if map_path.exists() and (root / level.map).resolve() == map_path.resolve():
                    entities_path = root / level.entities
            sibling = root / "game" / "defs" / f"{map_path.stem}.entities.json"
            if entities_path is None and sibling.is_file():
                entities_path = sibling
    else:
        print("Error: give a map file, or --level <id or number>.")
        return 2
    if args.entities and args.level:
        entities_path = Path(args.entities)

    if not map_path.exists():
        print(f"Error: Map file '{map_path}' not found.")
        return 1
    parsed_map = parse_map(map_path)
    if not parsed_map.ok:
        print(f"Error: the engine cannot load {map_path}:")
        for error in parsed_map.errors:
            print(f"  {error}")
        return 1

    entities: Optional[List[dict]] = None
    lights: List[dict] = []
    if entities_path is not None:
        if not entities_path.exists():
            print(f"Error: Entity file '{entities_path}' not found.")
            return 1
        parsed = parse_entities(entities_path)
        for warning in parsed.warnings:
            print(f"  [WARN] {warning}")
        if not parsed.ok:
            print(f"Error: the engine cannot load {entities_path}:")
            for error in parsed.errors:
                print(f"  {error}")
            return 1
        light_errors = check_light_bounds(parsed.lights, parsed_map.width, parsed_map.height, str(entities_path) + ":")
        if light_errors:
            print(f"Error: the engine cannot build the level from {entities_path}:")
            for error in light_errors:
                print(f"  {error}")
            return 1
        entities = parsed.entities
        lights = parsed.lights
        print(f"Entities: {entities_path} ({len(entities)} placements"
              + (f", {len(lights)} light zone(s))" if lights else ")"))

    print(render_ascii_preview(map_path, entity_placements=entities, color=args.color))

    if not args.no_stats:
        stats = get_map_stats(map_path)
        print("\n--- Map Statistics ---")
        for k, v in stats.items():
            print(f"  {k:20s}: {v}")

    if lights:
        print("\n--- Light Zones (applied in order; every other cell is 1.0) ---")
        for index, zone in enumerate(lights):
            print(f"  lights[{index}]: cells ({zone['x']}, {zone['y']}) to ({zone['x'] + zone['w'] - 1}, "
                  f"{zone['y'] + zone['h'] - 1}), {zone['w']}x{zone['h']}, level {zone['level']}")

    if args.no_reachability:
        return 0
    print("\n--- Reachability Analysis ---")
    if args.spawn_x is not None or args.spawn_y is not None:
        if args.spawn_x is None or args.spawn_y is None:
            print("  Error: give both --spawn-x and --spawn-y.")
            return 2
        spawn = (args.spawn_x, args.spawn_y)
        source = "--spawn-x/--spawn-y"
    else:
        players = [e for e in (entities or []) if e.get("type") == "player"]
        if len(players) != 1:
            print("  Error: no spawn: " + ("the entity file has %d player entities" % len(players) if entities is not None
                                           else "no entity file found") +
                  "; pass --entities FILE or --spawn-x X --spawn-y Y.")
            return 1
        spawn = (players[0]["x"], players[0]["y"])
        source = "the player entity"
    res = analyze_reachability(map_path, spawn_x=spawn[0], spawn_y=spawn[1], entities=entities)
    print(f"  Spawn: ({spawn[0]}, {spawn[1]}) from {source}, cell {res['spawn_cell']}"
          + ("" if entities is not None else "; no entity file: every 'D' counts as an openable door"))
    if res["error"]:
        print(f"  Status: FAILED: {res['error']}")
        return 1
    print(f"  Reachable cells: {res['reachable_count']} of {res['total_walkable']} walkable")
    if res["unreachable_cells"]:
        print(f"  Not reachable ({res['unreachable_count']}): {res['unreachable_cells'][:10]}"
              + (" ..." if res["unreachable_count"] > 10 else ""))
    print(f"  Exit switches: {res['usable_exits']} of {res['exits']} usable")
    if res["key_order"]:
        print(f"  Keys, in the order they become reachable: {', '.join(res['key_order'])}")
    for problem in res["problems"]:
        print(f"  Problem: {problem}")
    for ent in res["unreachable_entities"]:
        print(f"  Unreachable {ent.get('type')}: ({ent.get('x')}, {ent.get('y')}) {ent.get('kind') or ent.get('variant') or ''}")
    if res["success"]:
        print("  Status: OK (an exit switch is reachable; every pickup and enemy can be reached)")
        return 0
    print("  Status: FAILED" + ("" if res["usable_exits"] else " (no exit switch can be reached)"))
    return 1


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
    root = resolve_root(args)
    level_id = args.level_id
    if level_id is not None and not _is_content_id(level_id):
        print(f"Error: --level-id must use only a-z, 0-9, '_' and '-', got '{level_id}'.")
        return 2
    if args.register and not level_id:
        print("Error: --register needs --level-id.")
        return 2
    if args.output:
        map_path = Path(args.output)
    elif level_id:
        map_path = root / "game" / "maps" / f"{level_id}.map"
    else:
        print("Error: give an output .map path, or --level-id <id>.")
        return 2
    entities_path = root / "game" / "defs" / f"{level_id}.entities.json" if level_id else None

    existing = [path for path in (map_path, entities_path) if path is not None and path.exists()]
    if existing and not args.force:
        for path in existing:
            print(f"Error: {path} already exists; not overwritten (use --force to replace it).")
        return 1

    campaign_path = root / "game" / "defs" / "campaign.json"
    campaign: Optional[dict] = None
    relative_map = ""
    if args.register:
        try:
            relative_map = map_path.resolve().relative_to(root.resolve()).as_posix()
        except ValueError:
            print(f"Error: --register needs the map inside the content root {root} (campaign paths are relative to it).")
            return 1
        document, errors = load_json_strict(campaign_path)
        if document is None or errors or not isinstance(document, dict) or not isinstance(document.get("levels"), list):
            print(f"Error: cannot register the level: {campaign_path} is missing or invalid {errors[:1]}")
            return 1
        campaign = document
        theme = args.theme or next((lvl.get("theme") for lvl in reversed(campaign["levels"])
                                    if isinstance(lvl, dict) and lvl.get("theme")), "bunker")
        if not (root / "game" / "defs" / "themes" / f"{theme}.cfg").is_file():
            print(f"Error: theme '{theme}' not found (game/defs/themes/{theme}.cfg).")
            return 1
        args.theme = theme
        if any(isinstance(lvl, dict) and lvl.get("id") == level_id for lvl in campaign["levels"]) and not args.force:
            print(f"Error: campaign.json already has a level '{level_id}' (use --force to replace its entry).")
            return 1

    try:
        map_path.parent.mkdir(parents=True, exist_ok=True)
        map_data = create_template_map(map_path, width=args.width, height=args.height, style=args.style)
    except ValueError as exc:
        print(f"Error: {exc}")
        return 2
    print(f"Created template map ({args.width}x{args.height}, style: {args.style}) at: {map_path}")

    if level_id and entities_path is not None:
        stub = create_level_stub_entities(map_data, level_id)
        entities_path.parent.mkdir(parents=True, exist_ok=True)
        save_entities_json(stub, entities_path)
        print(f"Created entity stub ({len(stub['entities'])} placements: the player, and a door/trigger for every "
              f"'D'/'T') at: {entities_path}")

    if campaign is not None and entities_path is not None:
        entry = {
            "id": level_id,
            "name": args.name or level_id,
            "map": relative_map,
            "entities": entities_path.resolve().relative_to(root.resolve()).as_posix(),
            "theme": args.theme,
        }
        replaced = False
        for index, lvl in enumerate(campaign["levels"]):
            if isinstance(lvl, dict) and lvl.get("id") == level_id:
                campaign["levels"][index] = entry
                replaced = True
        if not replaced:
            campaign["levels"].append(entry)
        campaign_path.write_text(json.dumps(campaign, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"{'Updated' if replaced else 'Registered'} level {level_id} as level {len(campaign['levels']) if not replaced else index + 1} "
              f"in {campaign_path} (theme {args.theme}).")
    if level_id:
        print("Next: place enemies, pickups and keys (docs/LEVEL_AUTHORING.md), then run "
              "`neo_devkit validate --strict` and `neo_wolf_autoplay --level N`.")
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
    root = resolve_root(args)
    try:
        synced = sync_entity_definitions(root, from_csv=args.from_csv)
    except ValueError as exc:
        print(f"Error: {exc}")
        return 1
    pairs = [s for s in synced if not s.startswith("Warning:")]
    for s in synced:
        print(f"  {s}")
    print(f"Entity definitions synchronization complete ({len(pairs)} file pairs).")
    return 0


def cmd_package(args: argparse.Namespace) -> int:
    root = resolve_root(args)
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
        prog="neo_devkit",
        description="Neo-Wolf DevKit: content validation, map and asset tools, level stubs, environment "
                    "checks and packaging (docs/DEVKIT_GUIDE.md).",
    )
    parser.add_argument("--no-color", action="store_true",
                        help="Disable ANSI colours (also off with NO_COLOR, when not a terminal, and on "
                             "Windows consoles without ANSI support).")
    parser.add_argument("--root", type=str, default=None,
                        help="Content/repository root (default: the repository this DevKit is in).")

    subparsers = parser.add_subparsers(dest="command", required=True)

    doc_p = subparsers.add_parser("doctor", help="Check the build tools, the SDL source, Python and the content.")
    doc_p.add_argument("--no-network", action="store_true",
                       help="Do not test whether github.com is reachable (for the SDL download).")

    val_p = subparsers.add_parser("validate", help="Validate maps, assets, definitions and the campaign.")
    val_p.add_argument("--map", type=str, help="Validate a single .map file only.")
    val_p.add_argument("--asset", type=str, help="Validate a single .ppm or .wav file only.")
    val_p.add_argument("--strict", action="store_true", help="Warnings fail the validation too.")
    val_p.add_argument("--require-levels", type=int, default=None, metavar="N",
                       help="The campaign must have exactly N levels (default: any number).")

    map_p = subparsers.add_parser("inspect-map", help="Show a map with its entities, statistics and reachability.")
    map_p.add_argument("map_path", type=str, nargs="?", help="Path to a .map file (or use --level).")
    map_p.add_argument("--level", type=str, help="A campaign level by id or 1-based number (map and entities).")
    map_p.add_argument("--entities", type=str,
                       help="Entity file (.json or .csv); default: the campaign level's, or "
                            "game/defs/<map name>.entities.json.")
    map_p.add_argument("--no-entities", action="store_true", help="Do not look for an entity file.")
    map_p.add_argument("--no-stats", action="store_true", help="Skip the tile statistics.")
    map_p.add_argument("--no-reachability", action="store_true", help="Skip the reachability analysis.")
    map_p.add_argument("--stats", action="store_true", help=argparse.SUPPRESS)          # on by default
    map_p.add_argument("--reachability", action="store_true", help=argparse.SUPPRESS)   # on by default
    map_p.add_argument("--spawn-x", type=float, default=None, help="Spawn X (default: the player entity).")
    map_p.add_argument("--spawn-y", type=float, default=None, help="Spawn Y (default: the player entity).")

    ast_p = subparsers.add_parser("inspect-asset", help="Show the metadata of a .ppm texture or .wav sound.")
    ast_p.add_argument("asset_path", type=str, help="Path to a .ppm or .wav file.")

    cmap_p = subparsers.add_parser("create-map", help="Create a template map (and, with --level-id, a level stub).")
    cmap_p.add_argument("output", type=str, nargs="?",
                        help="Output .map path (default with --level-id: game/maps/<id>.map).")
    cmap_p.add_argument("--level-id", type=str, default=None,
                        help="Also write game/defs/<id>.entities.json with the player and a door/trigger "
                             "entity for every 'D'/'T' of the template.")
    cmap_p.add_argument("--register", action="store_true", help="Append the level to campaign.json (needs --level-id).")
    cmap_p.add_argument("--name", type=str, default=None, help="Level name for --register (default: the id).")
    cmap_p.add_argument("--theme", type=str, default=None,
                        help="Theme for --register (default: the last campaign level's theme).")
    cmap_p.add_argument("--force", action="store_true", help="Overwrite existing files / replace the campaign entry.")
    cmap_p.add_argument("--width", type=int, default=16, help="Map width in cells (at least 4).")
    cmap_p.add_argument("--height", type=int, default=12, help="Map height in cells (at least 4).")
    cmap_p.add_argument("--style", type=str, choices=["box", "arena", "dungeon", "crossroads"], default="arena")

    cast_p = subparsers.add_parser("create-asset", help="Generate a procedural PPM texture or WAV tone.")
    cast_p.add_argument("output", type=str, help="Output asset file path.")
    cast_p.add_argument("--type", type=str, choices=["checker", "brick", "solid", "beep"], default="checker")
    cast_p.add_argument("--width", type=int, default=64)
    cast_p.add_argument("--height", type=int, default=64)
    cast_p.add_argument("--duration", type=float, default=0.25, help="Seconds (beep).")

    sync_p = subparsers.add_parser(
        "sync-entities",
        help="Export the canonical <level>.entities.json files to <level>.entities.csv (overwrites the CSVs).",
    )
    sync_p.add_argument("--from-csv", action="store_true",
                        help="Reverse direction: overwrite each <level>.entities.json from its CSV.")
    # JSON -> CSV is the default since Phase 4; the old flag is still accepted and does nothing.
    sync_p.add_argument("--prefer-json", action="store_true", help=argparse.SUPPRESS)

    pkg_p = subparsers.add_parser("package", help="Assemble a release bundle (Roadmap Phase 9 reworks it).")
    pkg_p.add_argument("--out-dir", type=str, default=None, help="Output directory (default: release/).")
    pkg_p.add_argument("--version", type=str, default="1.0.0", help="Release version string.")
    pkg_p.add_argument("--no-build", action="store_true", help="Do not try to compile a missing executable.")
    pkg_p.add_argument("--no-zip", action="store_true", help="Skip the .zip archive.")
    pkg_p.add_argument("--no-tar", action="store_true", help="Skip the .tar.gz archive.")

    subparsers.add_parser("test", help="Run the DevKit's unit tests.")
    return parser


def main(argv: Optional[List[str]] = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    args.color = supports_color(not args.no_color)

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
