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
from tools.devkit.engine_formats import (
    EntityParse,
    MapParse,
    check_light_bounds,
    load_json_strict,
    parse_entities,
    parse_map,
)
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
    is_content_id,
    read_campaign,
    validate_map,
    validate_ppm,
    validate_project,
    validate_wav,
)

# Exit codes (docs/DEVKIT_GUIDE.md): success, a failed check or command, a usage error.
EXIT_OK = 0
EXIT_FAILURE = 1
EXIT_USAGE = 2
UNREACHABLE_CELLS_SHOWN = 10  # inspect-map lists at most this many unreachable cells
DEFAULT_THEME = "bunker"  # create-map --register on a campaign whose levels name no theme


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
    for parent in [cwd, *cwd.parents]:
        if is_project_root(parent):
            return parent
    return here


def resolve_root(args: argparse.Namespace) -> Path:
    return Path(args.root).resolve() if args.root else find_project_root()


def cmd_doctor(args: argparse.Namespace) -> int:
    root = resolve_root(args)
    report = run_doctor(root, check_network=not args.no_network)
    print(render_doctor_report(report, color=args.color))
    return EXIT_FAILURE if report.has_failures else EXIT_OK


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
            return EXIT_FAILURE
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
            return EXIT_FAILURE
        print(f"{c.green}Validation PASSED: Content is valid and engine-ready.{c.reset}")
        return EXIT_OK
    print(f"{c.red}Validation FAILED: {len(report.errors)} error(s) detected.{c.reset}")
    return EXIT_FAILURE


def _find_campaign_level(root: Path, wanted: str) -> Tuple[Optional[CampaignLevelInfo], List[str]]:
    """A campaign level by id or by 1-based number."""
    scratch = ValidationReport()
    levels = read_campaign(root, scratch) or []
    for level in levels:
        if level.id == wanted or (wanted.isdigit() and int(wanted) == level.index + 1):
            return level, []
    return None, scratch.errors or [
        f"campaign.json has no level '{wanted}' (ids: {', '.join(level.id for level in levels)}; "
        f"or a number from 1 to {len(levels)})"
    ]


def _default_entity_file(root: Path, map_path: Path) -> Optional[Path]:
    """The entity file of the campaign level that uses this map, else game/defs/<map name>.entities.json."""
    entities_path: Optional[Path] = None
    scratch = ValidationReport()
    for level in read_campaign(root, scratch) or []:
        if map_path.exists() and (root / level.map).resolve() == map_path.resolve():
            entities_path = root / level.entities
    sibling = root / "game" / "defs" / f"{map_path.stem}.entities.json"
    if entities_path is None and sibling.is_file():
        entities_path = sibling
    return entities_path


def _inspected_files(args: argparse.Namespace, root: Path) -> Tuple[Optional[Path], Optional[Path], int]:
    """(map, entity file or None, 0) for inspect-map, or (None, None, exit code) after printing why."""
    entities_path: Optional[Path] = None
    if args.level:
        level, problems = _find_campaign_level(root, args.level)
        if level is None:
            for problem in problems:
                print(f"Error: {problem}")
            return None, None, EXIT_FAILURE
        map_path = root / level.map
        entities_path = Path(args.entities) if args.entities else root / level.entities
    elif args.map_path:
        map_path = Path(args.map_path)
        if args.entities:
            entities_path = Path(args.entities)
        elif not args.no_entities:
            entities_path = _default_entity_file(root, map_path)
    else:
        print("Error: give a map file, or --level <id or number>.")
        return None, None, EXIT_USAGE
    return map_path, entities_path, EXIT_OK


def _print_engine_errors(what: str, path: Path, errors: List[str]) -> None:
    print(f"Error: the engine cannot {what} {path}:")
    for error in errors:
        print(f"  {error}")


def _load_inspected_entities(entities_path: Path, parsed_map: MapParse) -> Tuple[Optional[EntityParse], int]:
    """The parsed entity file, or (None, exit code) after printing why the engine rejects it."""
    if not entities_path.exists():
        print(f"Error: Entity file '{entities_path}' not found.")
        return None, EXIT_FAILURE
    parsed = parse_entities(entities_path)
    for warning in parsed.warnings:
        print(f"  [WARN] {warning}")
    if not parsed.ok:
        _print_engine_errors("load", entities_path, parsed.errors)
        return None, EXIT_FAILURE
    light_errors = check_light_bounds(parsed.lights, parsed_map.width, parsed_map.height, str(entities_path) + ":")
    if light_errors:
        _print_engine_errors("build the level from", entities_path, light_errors)
        return None, EXIT_FAILURE
    return parsed, EXIT_OK


def _print_light_zones(lights: List[dict]) -> None:
    print("\n--- Light Zones (applied in order; every other cell is 1.0) ---")
    for index, zone in enumerate(lights):
        print(
            f"  lights[{index}]: cells ({zone['x']}, {zone['y']}) to ({zone['x'] + zone['w'] - 1}, "
            f"{zone['y'] + zone['h'] - 1}), {zone['w']}x{zone['h']}, level {zone['level']}"
        )


def _reachability_spawn(
    args: argparse.Namespace, entities: Optional[List[dict]]
) -> Tuple[Optional[Tuple[float, float]], str, int]:
    """(spawn, where it came from, 0), or (None, "", exit code) after printing why there is none."""
    if args.spawn_x is not None or args.spawn_y is not None:
        if args.spawn_x is None or args.spawn_y is None:
            print("  Error: give both --spawn-x and --spawn-y.")
            return None, "", EXIT_USAGE
        return (args.spawn_x, args.spawn_y), "--spawn-x/--spawn-y", EXIT_OK
    players = [e for e in (entities or []) if e.get("type") == "player"]
    if len(players) != 1:
        found = (
            f"the entity file has {len(players)} player entities" if entities is not None else "no entity file found"
        )
        print(f"  Error: no spawn: {found}; pass --entities FILE or --spawn-x X --spawn-y Y.")
        return None, "", EXIT_FAILURE
    return (players[0]["x"], players[0]["y"]), "the player entity", EXIT_OK


def _print_reachability(args: argparse.Namespace, map_path: Path, entities: Optional[List[dict]]) -> int:
    print("\n--- Reachability Analysis ---")
    spawn, source, code = _reachability_spawn(args, entities)
    if spawn is None:
        return code
    res = analyze_reachability(map_path, spawn_x=spawn[0], spawn_y=spawn[1], entities=entities)
    print(
        f"  Spawn: ({spawn[0]}, {spawn[1]}) from {source}, cell {res['spawn_cell']}"
        + ("" if entities is not None else "; no entity file: every 'D' counts as an openable door")
    )
    if res["error"]:
        print(f"  Status: FAILED: {res['error']}")
        return EXIT_FAILURE
    print(f"  Reachable cells: {res['reachable_count']} of {res['total_walkable']} walkable")
    if res["unreachable_cells"]:
        print(
            f"  Not reachable ({res['unreachable_count']}): {res['unreachable_cells'][:UNREACHABLE_CELLS_SHOWN]}"
            + (" ..." if res["unreachable_count"] > UNREACHABLE_CELLS_SHOWN else "")
        )
    print(f"  Exit switches: {res['usable_exits']} of {res['exits']} usable")
    if res["key_order"]:
        print(f"  Keys, in the order they become reachable: {', '.join(res['key_order'])}")
    for problem in res["problems"]:
        print(f"  Problem: {problem}")
    for ent in res["unreachable_entities"]:
        print(
            f"  Unreachable {ent.get('type')}: ({ent.get('x')}, {ent.get('y')}) {ent.get('kind') or ent.get('variant') or ''}"
        )
    if res["success"]:
        print("  Status: OK (an exit switch is reachable; every pickup and enemy can be reached)")
        return EXIT_OK
    print("  Status: FAILED" + ("" if res["usable_exits"] else " (no exit switch can be reached)"))
    return EXIT_FAILURE


def cmd_inspect_map(args: argparse.Namespace) -> int:
    root = resolve_root(args)
    map_path, entities_path, code = _inspected_files(args, root)
    if map_path is None:
        return code
    if not map_path.exists():
        print(f"Error: Map file '{map_path}' not found.")
        return EXIT_FAILURE
    parsed_map = parse_map(map_path)
    if not parsed_map.ok:
        _print_engine_errors("load", map_path, parsed_map.errors)
        return EXIT_FAILURE

    entities: Optional[List[dict]] = None
    lights: List[dict] = []
    if entities_path is not None:
        parsed, code = _load_inspected_entities(entities_path, parsed_map)
        if parsed is None:
            return code
        entities = parsed.entities
        lights = parsed.lights
        print(
            f"Entities: {entities_path} ({len(entities)} placements"
            + (f", {len(lights)} light zone(s))" if lights else ")")
        )

    print(render_ascii_preview(map_path, entity_placements=entities, color=args.color))

    if not args.no_stats:
        stats = get_map_stats(map_path)
        print("\n--- Map Statistics ---")
        for k, v in stats.items():
            print(f"  {k:20s}: {v}")

    if lights:
        _print_light_zones(lights)

    if args.no_reachability:
        return EXIT_OK
    return _print_reachability(args, map_path, entities)


def cmd_inspect_asset(args: argparse.Namespace) -> int:
    target = Path(args.asset_path)
    if not target.exists():
        print(f"Error: File '{target}' not found.")
        return EXIT_FAILURE

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
        return EXIT_FAILURE
    return EXIT_OK


def _created_map_path(args: argparse.Namespace, root: Path) -> Tuple[Optional[Path], int]:
    """The .map that create-map writes, or (None, EXIT_USAGE) after printing what is missing."""
    level_id = args.level_id
    if level_id is not None and not is_content_id(level_id):
        print(f"Error: --level-id must use only a-z, 0-9, '_' and '-', got '{level_id}'.")
        return None, EXIT_USAGE
    if args.register and not level_id:
        print("Error: --register needs --level-id.")
        return None, EXIT_USAGE
    if args.output:
        return Path(args.output), EXIT_OK
    if level_id:
        return root / "game" / "maps" / f"{level_id}.map", EXIT_OK
    print("Error: give an output .map path, or --level-id <id>.")
    return None, EXIT_USAGE


def _campaign_to_register(args: argparse.Namespace, root: Path, map_path: Path) -> Tuple[Optional[dict], str, int]:
    """(campaign.json document, the map's path relative to root, 0) for --register, or
    (None, "", exit code) after printing why the level cannot be added. Sets args.theme."""
    campaign_path = root / "game" / "defs" / "campaign.json"
    try:
        relative_map = map_path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        print(f"Error: --register needs the map inside the content root {root} (campaign paths are relative to it).")
        return None, "", EXIT_FAILURE
    document, errors = load_json_strict(campaign_path)
    if document is None or errors or not isinstance(document, dict) or not isinstance(document.get("levels"), list):
        print(f"Error: cannot register the level: {campaign_path} is missing or invalid {errors[:1]}")
        return None, "", EXIT_FAILURE
    levels = document["levels"]
    theme = args.theme or next(
        (lvl.get("theme") for lvl in reversed(levels) if isinstance(lvl, dict) and lvl.get("theme")),
        DEFAULT_THEME,
    )
    if not (root / "game" / "defs" / "themes" / f"{theme}.cfg").is_file():
        print(f"Error: theme '{theme}' not found (game/defs/themes/{theme}.cfg).")
        return None, "", EXIT_FAILURE
    args.theme = theme
    if any(isinstance(lvl, dict) and lvl.get("id") == args.level_id for lvl in levels) and not args.force:
        print(f"Error: campaign.json already has a level '{args.level_id}' (use --force to replace its entry).")
        return None, "", EXIT_FAILURE
    return document, relative_map, EXIT_OK


def _register_level(
    args: argparse.Namespace, root: Path, campaign: dict, relative_map: str, entities_path: Path
) -> None:
    """Adds the level to campaign.json, or replaces the entry with its id (--force)."""
    level_id = args.level_id
    campaign_path = root / "game" / "defs" / "campaign.json"
    entry = {
        "id": level_id,
        "name": args.name or level_id,
        "map": relative_map,
        "entities": entities_path.resolve().relative_to(root.resolve()).as_posix(),
        "theme": args.theme,
    }
    position = None  # 1-based campaign position of the entry
    for index, lvl in enumerate(campaign["levels"]):
        if isinstance(lvl, dict) and lvl.get("id") == level_id:
            campaign["levels"][index] = entry
            position = index + 1
            break
    replaced = position is not None
    if not replaced:
        campaign["levels"].append(entry)
        position = len(campaign["levels"])
    campaign_path.write_text(json.dumps(campaign, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(
        f"{'Updated' if replaced else 'Registered'} level {level_id} as level {position} "
        f"in {campaign_path} (theme {args.theme})."
    )


def cmd_create_map(args: argparse.Namespace) -> int:
    root = resolve_root(args)
    level_id = args.level_id
    map_path, code = _created_map_path(args, root)
    if map_path is None:
        return code
    entities_path = root / "game" / "defs" / f"{level_id}.entities.json" if level_id else None

    existing = [path for path in (map_path, entities_path) if path is not None and path.exists()]
    if existing and not args.force:
        for path in existing:
            print(f"Error: {path} already exists; not overwritten (use --force to replace it).")
        return EXIT_FAILURE

    campaign: Optional[dict] = None
    relative_map = ""
    if args.register:
        campaign, relative_map, code = _campaign_to_register(args, root, map_path)
        if campaign is None:
            return code

    try:
        map_path.parent.mkdir(parents=True, exist_ok=True)
        map_data = create_template_map(map_path, width=args.width, height=args.height, style=args.style)
    except ValueError as exc:
        print(f"Error: {exc}")
        return EXIT_USAGE
    print(f"Created template map ({args.width}x{args.height}, style: {args.style}) at: {map_path}")

    if level_id and entities_path is not None:
        stub = create_level_stub_entities(map_data, level_id)
        entities_path.parent.mkdir(parents=True, exist_ok=True)
        save_entities_json(stub, entities_path)
        print(
            f"Created entity stub ({len(stub['entities'])} placements: the player, and a door/trigger for every "
            f"'D'/'T') at: {entities_path}"
        )

    if campaign is not None and entities_path is not None:
        _register_level(args, root, campaign, relative_map, entities_path)
    if level_id:
        print(
            "Next: place enemies, pickups and keys (docs/LEVEL_AUTHORING.md), then run "
            "`neo_devkit validate --strict` and `neo_wolf_autoplay --level N`."
        )
    return EXIT_OK


# create-asset --type: the generator and what the message calls its result.
ASSET_TYPES = {
    "checker": (
        lambda path, args: create_checkerboard_ppm(path, width=args.width, height=args.height),
        "checkerboard PPM",
    ),
    "brick": (lambda path, args: create_brick_ppm(path, width=args.width, height=args.height), "procedural brick PPM"),
    "solid": (lambda path, args: create_solid_ppm(path, width=args.width, height=args.height), "solid color PPM"),
    "beep": (lambda path, args: generate_beep_wav(path, duration_sec=args.duration), "procedural beep WAV"),
}


def cmd_create_asset(args: argparse.Namespace) -> int:
    out_path = Path(args.output)
    create, what = ASSET_TYPES[args.type]  # argparse accepts only these types
    create(out_path, args)
    print(f"Created {what} at {out_path}")
    return EXIT_OK


def cmd_sync_entities(args: argparse.Namespace) -> int:
    root = resolve_root(args)
    entities_path: Optional[Path] = None
    if args.level is not None:
        # One campaign level (by id or 1-based number, like inspect-map --level): its entity
        # file pair, wherever campaign.json puts it.
        level, problems = _find_campaign_level(root, args.level)
        if level is None:
            for problem in problems:
                print(f"Error: {problem}")
            return EXIT_FAILURE
        entities_path = root / level.entities
    try:
        synced = sync_entity_definitions(root, from_csv=args.from_csv, entities_path=entities_path)
    except ValueError as exc:
        print(f"Error: {exc}")
        return EXIT_FAILURE
    pairs = [s for s in synced if not s.startswith("Warning:")]
    for s in synced:
        print(f"  {s}")
    print(f"Entity definitions synchronization complete ({len(pairs)} file pairs).")
    return EXIT_OK


def cmd_package(args: argparse.Namespace) -> int:
    """Builds the release package for this OS through CMake (docs/RELEASE.md)."""
    root = resolve_root(args)
    out_dir = Path(args.out_dir) if args.out_dir else root / "release"
    print(f"Packaging Neo-Wolf into {out_dir} ...", flush=True)
    res = create_release_package(
        project_root=root,
        output_dir=out_dir,
        preset=args.preset,
        build_dir=Path(args.build_dir) if args.build_dir else None,
        build=not args.no_build,
        expect_version=args.expect_version,
        clean=args.clean,
        keep_staging=args.keep_staging,
        cmake=args.cmake,
    )
    for warning in res.warnings:
        print(f"  [WARN] {warning}")
    if not res.success:
        print("\nPackaging FAILED:")
        for err in res.errors:
            print(f"  [ERROR] {err}")
        if res.staging_dir and res.staging_dir.exists():
            print(f"  (the staged files are kept for inspection in {res.staging_dir})")
        return EXIT_FAILURE
    assert res.archive is not None and res.checksum_file is not None
    print("\nPackaging SUCCESSFUL:")
    print(f"  Version:   {res.info.get('version')} ({res.info.get('platform')})")
    print(f"  Archive:   {res.archive} ({res.archive.stat().st_size} bytes, {len(res.files)} files)")
    print(f"  Checksums: {res.checksum_file}")
    print(f'  Check it:  python3 tools/ci/package_smoke.py "{res.archive}"')
    return EXIT_OK


def cmd_test(args: argparse.Namespace) -> int:
    """Run DevKit Python test suite."""
    print("Running DevKit automated test suite...")
    loader = unittest.TestLoader()
    start_dir = Path(__file__).parent / "tests"
    if not start_dir.exists():
        print(f"Test directory not found at {start_dir}")
        return EXIT_FAILURE
    suite = loader.discover(str(start_dir), pattern="test_*.py")
    runner = unittest.TextTestRunner(verbosity=2)
    res = runner.run(suite)
    return EXIT_OK if res.wasSuccessful() else EXIT_FAILURE


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="neo_devkit",
        description="Neo-Wolf DevKit: content validation, map and asset tools, level stubs, environment "
        "checks and packaging (docs/DEVKIT_GUIDE.md).",
    )
    parser.add_argument(
        "--no-color",
        action="store_true",
        help="Disable ANSI colours (also off with NO_COLOR, when not a terminal, and on "
        "Windows consoles without ANSI support).",
    )
    parser.add_argument(
        "--root", type=str, default=None, help="Content/repository root (default: the repository this DevKit is in)."
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    doc_p = subparsers.add_parser("doctor", help="Check the build tools, the SDL source, Python and the content.")
    doc_p.add_argument(
        "--no-network", action="store_true", help="Do not test whether github.com is reachable (for the SDL download)."
    )

    val_p = subparsers.add_parser("validate", help="Validate maps, assets, definitions and the campaign.")
    val_p.add_argument("--map", type=str, help="Validate a single .map file only.")
    val_p.add_argument("--asset", type=str, help="Validate a single .ppm or .wav file only.")
    val_p.add_argument("--strict", action="store_true", help="Warnings fail the validation too.")
    val_p.add_argument(
        "--require-levels",
        type=int,
        default=None,
        metavar="N",
        help="The campaign must have exactly N levels (default: any number).",
    )

    map_p = subparsers.add_parser("inspect-map", help="Show a map with its entities, statistics and reachability.")
    map_p.add_argument("map_path", type=str, nargs="?", help="Path to a .map file (or use --level).")
    map_p.add_argument("--level", type=str, help="A campaign level by id or 1-based number (map and entities).")
    map_p.add_argument(
        "--entities",
        type=str,
        help="Entity file (.json or .csv); default: the campaign level's, or game/defs/<map name>.entities.json.",
    )
    map_p.add_argument("--no-entities", action="store_true", help="Do not look for an entity file.")
    map_p.add_argument("--no-stats", action="store_true", help="Skip the tile statistics.")
    map_p.add_argument("--no-reachability", action="store_true", help="Skip the reachability analysis.")
    map_p.add_argument("--stats", action="store_true", help=argparse.SUPPRESS)  # on by default
    map_p.add_argument("--reachability", action="store_true", help=argparse.SUPPRESS)  # on by default
    map_p.add_argument("--spawn-x", type=float, default=None, help="Spawn X (default: the player entity).")
    map_p.add_argument("--spawn-y", type=float, default=None, help="Spawn Y (default: the player entity).")

    ast_p = subparsers.add_parser("inspect-asset", help="Show the metadata of a .ppm texture or .wav sound.")
    ast_p.add_argument("asset_path", type=str, help="Path to a .ppm or .wav file.")

    cmap_p = subparsers.add_parser("create-map", help="Create a template map (and, with --level-id, a level stub).")
    cmap_p.add_argument(
        "output", type=str, nargs="?", help="Output .map path (default with --level-id: game/maps/<id>.map)."
    )
    cmap_p.add_argument(
        "--level-id",
        type=str,
        default=None,
        help="Also write game/defs/<id>.entities.json with the player and a door/trigger "
        "entity for every 'D'/'T' of the template.",
    )
    cmap_p.add_argument("--register", action="store_true", help="Append the level to campaign.json (needs --level-id).")
    cmap_p.add_argument("--name", type=str, default=None, help="Level name for --register (default: the id).")
    cmap_p.add_argument(
        "--theme", type=str, default=None, help="Theme for --register (default: the last campaign level's theme)."
    )
    cmap_p.add_argument("--force", action="store_true", help="Overwrite existing files / replace the campaign entry.")
    cmap_p.add_argument("--width", type=int, default=16, help="Map width in cells (at least 4).")
    cmap_p.add_argument("--height", type=int, default=12, help="Map height in cells (at least 4).")
    cmap_p.add_argument("--style", type=str, choices=["box", "arena", "dungeon", "crossroads"], default="arena")

    cast_p = subparsers.add_parser("create-asset", help="Generate a procedural PPM texture or WAV tone.")
    cast_p.add_argument("output", type=str, help="Output asset file path.")
    cast_p.add_argument("--type", type=str, choices=list(ASSET_TYPES), default="checker")
    cast_p.add_argument("--width", type=int, default=64)
    cast_p.add_argument("--height", type=int, default=64)
    cast_p.add_argument("--duration", type=float, default=0.25, help="Seconds (beep).")

    sync_p = subparsers.add_parser(
        "sync-entities",
        help="Export the canonical <level>.entities.json files to <level>.entities.csv (overwrites the CSVs).",
    )
    sync_p.add_argument(
        "--from-csv", action="store_true", help="Reverse direction: overwrite each <level>.entities.json from its CSV."
    )
    sync_p.add_argument(
        "--level",
        type=str,
        default=None,
        metavar="ID|N",
        help="Only this campaign level (its id, e.g. level03, or its 1-based number); "
        "default: every entity file in game/defs/.",
    )
    # JSON -> CSV is the default since Phase 4; the old flag is still accepted and does nothing.
    sync_p.add_argument("--prefer-json", action="store_true", help=argparse.SUPPRESS)

    pkg_p = subparsers.add_parser(
        "package",
        help="Build this OS's release package with CMake: zip (macOS, Windows) or tar.gz (Linux), plus SHA256SUMS.",
    )
    pkg_p.add_argument("--out-dir", type=str, default=None, help="Output folder (default: release/).")
    pkg_p.add_argument(
        "--preset",
        type=str,
        default=None,
        help="CMake configure/build preset (default: package-macos, package-windows-msvc or "
        "package-linux for this OS).",
    )
    pkg_p.add_argument(
        "--build-dir",
        type=str,
        default=None,
        help="Package an already configured build tree instead of a preset (e.g. a "
        "cross-compile); it is built, not configured.",
    )
    pkg_p.add_argument(
        "--no-build", action="store_true", help="Do not configure or build: install and archive the build as it is."
    )
    pkg_p.add_argument(
        "--expect-version",
        type=str,
        default=None,
        metavar="X.Y.Z",
        help="Fail unless CMake's project(VERSION) is X.Y.Z (release tags).",
    )
    pkg_p.add_argument(
        "--clean",
        action="store_true",
        help="Remove earlier neo-wolf-* archives and SHA256SUMS from the output folder first.",
    )
    pkg_p.add_argument(
        "--keep-staging", action="store_true", help="Keep the installed tree in <out-dir>/.staging/ after archiving."
    )
    pkg_p.add_argument("--cmake", type=str, default=None, help="The cmake executable (default: from PATH).")

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

    return handlers[args.command](args)  # argparse accepts only these commands


if __name__ == "__main__":
    sys.exit(main())
