"""Validator for Neo-Wolf content (``neo_devkit validate``).

Two layers:

* **The engine's rules**, implemented to behave exactly like the C++ loaders (shared readers in
  ``engine_formats.py``): map grids, entity placements (JSON and CSV), key/value files,
  ``campaign.json`` and ``gameplay.json``. A file that fails here fails to load in the game.
* **The DevKit's level rules**, stricter than the engine, so that a level that validates can be
  played to the end (Roadmap Phase 7, docs/LEVEL_AUTHORING.md): a closed outer wall, exactly one
  player on a floor cell, every door on a 'D' cell and every 'D' with exactly one door,
  triggers on 'T', nothing inside a solid cell, at least one exit switch 'X' reachable from the
  spawn with every key collected before the door it opens, every pickup and enemy reachable,
  and no map or entity file left out of the campaign.

Errors make ``validate`` fail; warnings fail it only with ``--strict``.
"""

import math
import struct
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

from tools.devkit.engine_formats import (
    ENEMY_VARIANTS,
    ENTITY_FIELDS,
    ENTITY_TYPES,
    FLOOR_TILES,
    KEY_COLORS,
    MAP_TILES,
    PICKUP_KINDS,
    SOLID_TILES,
    WALKABLE_TILES,
    check_light_bounds,
    load_json_strict,
    parse_entities,
    parse_entities_csv,
    parse_entities_json,
    parse_key_value,
    parse_map,
)


@dataclass
class ValidationReport:
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    info: List[str] = field(default_factory=list)

    @property
    def is_valid(self) -> bool:
        return len(self.errors) == 0

    def add_error(self, message: str) -> None:
        self.errors.append(message)

    def add_warning(self, message: str) -> None:
        self.warnings.append(message)

    def add_info(self, message: str) -> None:
        self.info.append(message)

    def merge(self, other: "ValidationReport") -> None:
        self.errors.extend(other.errors)
        self.warnings.extend(other.warnings)
        self.info.extend(other.info)


VALID_TILE_CHARS = {
    ".": "Empty",
    "#": "Wall",
    "A": "WallAltA",
    "B": "WallAltB",
    "D": "Door",
    "T": "Trigger",
    "X": "Exit",
}
THEME_KEYS = {"wall", "wall_alt_a", "wall_alt_b", "door", "floor", "ceiling", "exit", "door_red", "door_blue", "door_gold"}
# sprites.cfg (engine: kSpriteTextureKeys in engine/assets/ContentConfig.h).
SPRITE_KEYS = {
    "guard", "brute", "runner", "guard_dead", "brute_dead", "runner_dead",
    "health", "ammo", "key_red", "key_blue", "key_gold", "decoration",
}
RENAMED_SPRITE_KEYS = {"enemy": "guard", "pickup": "health"}  # Phase 6
AUDIO_REQUIRED_KEYS = {"door_open", "pickup", "enemy_alert"}
AUDIO_OPTIONAL_KEYS = {"weapon_fire", "master_volume"}
CAMPAIGN_LEVEL_FIELDS = ("id", "name", "map", "entities", "theme")
VALID_ENTITY_TYPES = set(ENTITY_TYPES)
MAX_PICKUP_VALUE = 10000  # engine: kMaxGameplayStat
MAX_CAMPAIGN_NAME_BYTES = 32  # engine: kMaxCampaignNameLength (bytes of UTF-8)
ENEMY_RADIUS = 0.25  # engine: kEnemyRadius
DEFAULT_PLAYER_RADIUS = 0.22  # gameplay.json player.collisionRadius default


def validate_map(file_path: Path) -> Tuple[ValidationReport, Optional[List[str]]]:
    """Validate a .map grid file.

    Engine rules (MapLoader, read exactly like the engine: lines split on LF, one trailing CR
    removed, empty lines and lines starting with ';' in column 1 skipped, every other line a grid
    row as is): at least one row, equal row widths, only the tiles . # A B D T X.
    DevKit rules: at least 3x3, a closed outer wall (# A B X), at least one walkable cell.
    Returns the rows when the map is valid."""
    report = ValidationReport()
    file_path = Path(file_path)
    if not file_path.exists():
        report.add_error(f"Map file not found: {file_path}")
        return report, None

    parsed = parse_map(file_path)
    if not parsed.rows:
        report.add_error(f"Map {file_path} is empty or contains only comments.")
        return report, None
    rows = parsed.rows
    width = len(rows[0])
    height = len(rows)
    for y, row in enumerate(rows):
        if len(row) != width:
            report.add_error(f"Map {file_path}: Inconsistent row width at row {y} (expected {width}, got {len(row)})")
            continue
        for x, char in enumerate(row):
            if char not in MAP_TILES:
                report.add_error(f"Map {file_path}: Invalid tile character {char!r} at ({x}, {y})")
    if not report.is_valid:
        return report, None

    if width < 3 or height < 3:
        report.add_error(f"Map {file_path} dimensions ({width}x{height}) must be at least 3x3.")
    for x in range(width):
        if rows[0][x] not in SOLID_TILES:
            report.add_error(
                f"Map {file_path}: North perimeter leak at ({x}, 0) — found '{rows[0][x]}', expected solid wall (#, A, B, X)"
            )
        if height > 1 and rows[-1][x] not in SOLID_TILES:
            report.add_error(
                f"Map {file_path}: South perimeter leak at ({x}, {height - 1}) — found '{rows[-1][x]}', expected solid wall"
            )
    for y in range(height):
        if rows[y][0] not in SOLID_TILES:
            report.add_error(f"Map {file_path}: West perimeter leak at (0, {y}) — found '{rows[y][0]}', expected solid wall")
        if rows[y][-1] not in SOLID_TILES:
            report.add_error(
                f"Map {file_path}: East perimeter leak at ({width - 1}, {y}) — found '{rows[y][-1]}', expected solid wall"
            )
    walkable_count = sum(1 for row in rows for c in row if c in WALKABLE_TILES)
    if walkable_count == 0:
        report.add_error(f"Map {file_path} has no walkable interior tiles.")
    else:
        report.add_info(
            f"Map {file_path.name}: {width}x{height} grid ({walkable_count} walkable cells, {width * height - walkable_count} solid cells)"
        )
    return report, rows if report.is_valid else None


def validate_ppm(file_path: Path, expected_dim: Optional[int] = None) -> ValidationReport:
    """Validate a Netpbm PPM image file (P3 ASCII format as expected by engine).

    Checks:
    - Magic identifier is P3 (plain text PPM)
    - Valid positive width and height
    - Max color value > 0 and <= 255
    - Exact number of RGB pixel tokens (3 * width * height)
    - Pixel values within [0, maxval]
    """
    report = ValidationReport()
    file_path = Path(file_path)

    if not file_path.exists():
        report.add_error(f"PPM file not found: {file_path}")
        return report

    try:
        with open(file_path, "r", encoding="latin-1") as f:
            text = f.read()
    except Exception as exc:
        report.add_error(f"Failed to read PPM file {file_path}: {exc}")
        return report

    # Tokenize ignoring comments starting with #
    tokens: List[str] = []
    for line in text.splitlines():
        line = line.split("#", 1)[0].strip()
        if line:
            tokens.extend(line.split())

    if len(tokens) < 4:
        report.add_error(f"PPM {file_path}: Incomplete header. Expected magic, width, height, maxval.")
        return report

    magic = tokens[0]
    if magic != "P3":
        if magic == "P6":
            report.add_error(
                f"PPM {file_path}: Found binary P6 PPM format, but Neo-Wolf engine requires ASCII P3 format."
            )
        else:
            report.add_error(
                f"PPM {file_path}: Unsupported format '{magic}'. Expected 'P3'."
            )
        return report

    try:
        width = int(tokens[1])
        height = int(tokens[2])
        maxval = int(tokens[3])
    except ValueError:
        report.add_error(f"PPM {file_path}: Non-integer values in header: {tokens[1:4]}")
        return report

    if width <= 0 or height <= 0:
        report.add_error(f"PPM {file_path}: Invalid dimensions {width}x{height}")
        return report

    if maxval <= 0 or maxval > 255:
        report.add_error(f"PPM {file_path}: Unsupported max color value {maxval} (must be 1..255)")

    if expected_dim is not None and (width != expected_dim or height != expected_dim):
        report.add_warning(
            f"PPM {file_path}: Dimensions {width}x{height} differ from standard {expected_dim}x{expected_dim}"
        )

    # Check power-of-two dimensions (recommended for raycaster wall textures)
    if (width & (width - 1)) != 0 or (height & (height - 1)) != 0:
        report.add_warning(
            f"PPM {file_path}: Dimensions {width}x{height} are not power-of-two (64x64 recommended)."
        )

    pixel_tokens = tokens[4:]
    expected_token_count = width * height * 3

    if len(pixel_tokens) != expected_token_count:
        report.add_error(
            f"PPM {file_path}: Token count mismatch. Expected {expected_token_count} RGB tokens ({width}x{height}x3), got {len(pixel_tokens)}"
        )
        return report

    # Sample pixel value range
    for i in range(0, min(len(pixel_tokens), 300), 3):
        try:
            r, g, b = int(pixel_tokens[i]), int(pixel_tokens[i + 1]), int(pixel_tokens[i + 2])
            if not (0 <= r <= maxval and 0 <= g <= maxval and 0 <= b <= maxval):
                report.add_error(
                    f"PPM {file_path}: Pixel value out of bounds [0, {maxval}]: ({r},{g},{b})"
                )
                break
        except ValueError:
            report.add_error(f"PPM {file_path}: Non-integer pixel value encountered.")
            break

    report.add_info(f"PPM {file_path.name}: {width}x{height} P3 ASCII format (maxval {maxval}) OK")
    return report


def validate_wav(file_path: Path) -> ValidationReport:
    """Validate a RIFF WAVE audio file.

    Checks:
    - RIFF header format
    - Format code is PCM (1)
    - Channels is 1 (mono) or 2 (stereo)
    - Valid sample rate and bit depth
    - Data chunk present and non-empty
    """
    report = ValidationReport()
    file_path = Path(file_path)

    if not file_path.exists():
        report.add_error(f"WAV file not found: {file_path}")
        return report

    try:
        data = file_path.read_bytes()
    except Exception as exc:
        report.add_error(f"Failed to read WAV file {file_path}: {exc}")
        return report

    if len(data) < 44:
        report.add_error(f"WAV {file_path}: File size too small ({len(data)} bytes, min 44 bytes).")
        return report

    riff_header = data[:4]
    wave_header = data[8:12]

    if riff_header != b"RIFF" or wave_header != b"WAVE":
        report.add_error(f"WAV {file_path}: Missing RIFF/WAVE header marker.")
        return report

    # Locate 'fmt ' chunk and 'data' chunk
    offset = 12
    fmt_found = False
    data_found = False
    audio_format = 0
    num_channels = 0
    sample_rate = 0
    bits_per_sample = 0
    data_size = 0

    while offset + 8 <= len(data):
        chunk_id = data[offset : offset + 4]
        chunk_size = struct.unpack("<I", data[offset + 4 : offset + 8])[0]
        chunk_data_offset = offset + 8

        if chunk_id == b"fmt ":
            fmt_found = True
            if chunk_size >= 16:
                audio_format, num_channels, sample_rate, byte_rate, block_align, bits_per_sample = struct.unpack(
                    "<HHIIHH", data[chunk_data_offset : chunk_data_offset + 16]
                )
        elif chunk_id == b"data":
            data_found = True
            data_size = chunk_size

        offset += 8 + chunk_size
        # Account for word alignment padding
        if chunk_size % 2 != 0:
            offset += 1

    if not fmt_found:
        report.add_error(f"WAV {file_path}: Missing 'fmt ' chunk.")
        return report

    if not data_found or data_size <= 0:
        report.add_error(f"WAV {file_path}: Missing or empty 'data' chunk.")
        return report

    if audio_format != 1 and audio_format != 0xFFFE:  # PCM or WAVE_FORMAT_EXTENSIBLE
        report.add_warning(
            f"WAV {file_path}: Audio format code is {audio_format} (non-PCM compression). Standard PCM is 1."
        )

    if num_channels not in (1, 2):
        report.add_error(f"WAV {file_path}: Unsupported channel count {num_channels} (must be 1 or 2)")

    if bits_per_sample not in (8, 16, 24, 32):
        report.add_warning(
            f"WAV {file_path}: Unusual bit depth {bits_per_sample} bits (8 or 16 recommended)."
        )

    duration_sec = 0.0
    bytes_per_sample = (bits_per_sample // 8) * num_channels
    if bytes_per_sample > 0 and sample_rate > 0:
        duration_sec = data_size / (sample_rate * bytes_per_sample)

    report.add_info(
        f"WAV {file_path.name}: {num_channels}ch, {sample_rate}Hz, {bits_per_sample}-bit PCM, duration: {duration_sec:.2f}s"
    )
    return report


def validate_cfg(file_path: Path, required_keys: Optional[Set[str]] = None) -> Tuple[ValidationReport, Dict[str, str]]:
    """Validate a key=value file (theme, sprites.cfg, audio.cfg) like the engine reads it."""
    report = ValidationReport()
    file_path = Path(file_path)
    if not file_path.exists():
        report.add_error(f"Config file not found: {file_path}")
        return report, {}
    parsed = parse_key_value(file_path)
    for error in parsed.errors:
        report.add_error(error + " (the engine ignores every entry of this file)")
    if required_keys:
        for key in sorted(required_keys - set(parsed.values)):
            report.add_error(f"CFG {file_path}: Missing required key '{key}'")
    report.add_info(f"CFG {file_path.name}: Loaded {len(parsed.values)} key-value entries.")
    return report, parsed.values


# --- Entity files ------------------------------------------------------------------------------


def _check_bounds(entities: List[dict], label: str, map_dimensions: Optional[Tuple[int, int]],
                  report: ValidationReport) -> None:
    if not map_dimensions:
        return
    mw, mh = map_dimensions
    for index, ent in enumerate(entities):
        x, y = ent["x"], ent["y"]
        if not (0 <= x < mw and 0 <= y < mh):
            report.add_error(f"{label} entity {index} ({ent['type']}): position ({x}, {y}) is outside map bounds ({mw}x{mh})")
        if ent["type"] in ("door", "trigger"):
            if not (0 <= ent["mapX"] < mw and 0 <= ent["mapY"] < mh):
                report.add_error(
                    f"{label} entity {index} ({ent['type']}): map cell ({ent['mapX']}, {ent['mapY']}) is outside map bounds ({mw}x{mh})"
                )


def _check_player_count(entities: List[dict], label: str, report: ValidationReport) -> None:
    players = sum(1 for ent in entities if ent.get("type") == "player")
    if players != 1:
        report.add_error(
            f"{label}: {players} 'player' entities; the engine requires exactly one (the level fails to load)."
        )


def _validate_entity_file(file_path: Path, kind: str, map_dimensions: Optional[Tuple[int, int]]
                          ) -> Tuple[ValidationReport, List[dict]]:
    report = ValidationReport()
    file_path = Path(file_path)
    if not file_path.exists():
        report.add_error(f"Entity {kind} file not found: {file_path}")
        return report, []
    parsed = parse_entities_json(file_path) if kind == "JSON" else parse_entities_csv(file_path)
    report.errors.extend(parsed.errors)
    report.warnings.extend(parsed.warnings)
    _check_bounds(parsed.entities, f"{kind} {file_path}:", map_dimensions, report)
    if map_dimensions:
        for error in check_light_bounds(parsed.lights, map_dimensions[0], map_dimensions[1], f"{kind} {file_path}:"):
            report.add_error(error)
    if parsed.ok or parsed.count:
        _check_player_count(parsed.entities, f"{kind} {file_path}", report)
    report.add_info(f"{kind} {file_path.name}: Found {len(parsed.entities)} entity placements"
                    + (f" and {len(parsed.lights)} light zone(s)." if parsed.lights else "."))
    return report, parsed.entities


def validate_entity_csv(file_path: Path, map_dimensions: Optional[Tuple[int, int]] = None
                        ) -> Tuple[ValidationReport, List[dict]]:
    """Validate an entity CSV export (type,x,y,mapX,mapY,kind,value,angle,variant,key,lock)."""
    return _validate_entity_file(file_path, "CSV", map_dimensions)


def validate_entity_json(file_path: Path, map_dimensions: Optional[Tuple[int, int]] = None
                         ) -> Tuple[ValidationReport, List[dict]]:
    """Validate an entity JSON file (level, entities array)."""
    return _validate_entity_file(file_path, "JSON", map_dimensions)


# --- gameplay.json (engine/assets/GameplayConfig.cpp) -------------------------------------------

# (key, kind, minimum, maximum, minimum exclusive, maximum exclusive, default)
_INT = "integer"
_NUM = "number"
GAMEPLAY_SCHEMA: Dict[str, List[Tuple[str, str, float, float, bool, bool, float]]] = {
    "player": [
        ("initialHealth", _INT, 1, 10000, False, False, 100),
        ("maxHealth", _INT, 1, 10000, False, False, 100),
        ("moveSpeed", _NUM, 0, 20, True, False, 3.0),
        ("turnSpeed", _NUM, 0, 20, True, False, 1.8),
        ("collisionRadius", _NUM, 0, 0.5, True, True, 0.22),
    ],
    "weapon": [
        ("damage", _INT, 1, 10000, False, False, 35),
        ("fireRate", _NUM, 0, 60, False, False, 0.25),
        ("range", _NUM, 0, 1000, True, False, 12.0),
        ("initialAmmo", _INT, 0, 10000, False, False, 24),
        ("maxAmmo", _INT, 0, 10000, False, False, 99),
        ("noiseRadius", _NUM, 0, 1000, False, False, 10.0),
    ],
    "enemy": [
        ("health", _INT, 1, 10000, False, False, 50),
        ("damage", _INT, 0, 10000, False, False, 12),
        ("moveSpeed", _NUM, 0, 20, False, False, 1.6),
        ("alertRadius", _NUM, 0, 1000, False, False, 6.5),
        ("attackRadius", _NUM, 0, 1000, False, False, 1.1),
        ("attackRate", _NUM, 0, 60, True, False, 1.0),
        ("reactionTime", _NUM, 0, 60, False, False, 0.5),
        ("score", _INT, 0, 10000, False, False, 100),
    ],
    "pickup": [("value", _INT, 1, 10000, False, False, 0)],
}
_ENEMY_DEFAULTS = {
    "guard": {},
    "brute": {"health": 140, "damage": 25, "moveSpeed": 1.0, "alertRadius": 5.5, "attackRadius": 1.3,
              "attackRate": 1.6, "reactionTime": 0.8, "score": 300},
    "runner": {"health": 25, "damage": 6, "moveSpeed": 2.8, "alertRadius": 8.0, "attackRadius": 1.0,
               "attackRate": 0.6, "reactionTime": 0.2, "score": 150},
}
_PICKUP_DEFAULTS = {"health": 25, "ammo": 12}


def _describe(value: Any) -> str:
    if isinstance(value, dict):
        return "an object"
    if isinstance(value, list):
        return "an array"
    if value is None:
        return "null"
    return json_dump(value)


def json_dump(value: Any) -> str:
    import json
    return json.dumps(value)


def _format_range(kind: str, lo: float, hi: float, lo_ex: bool, hi_ex: bool) -> str:
    if kind == _INT:
        return f"an integer from {int(lo)} to {int(hi)}"
    def fmt(v: float) -> str:
        return f"{v:g}"
    return f"a number {'>' if lo_ex else '>='} {fmt(lo)} and {'<' if hi_ex else '<='} {fmt(hi)}"


def _read_section(section: dict, path: str, schema: list, defaults: Dict[str, Any], report: ValidationReport,
                  label: str) -> Dict[str, Any]:
    values: Dict[str, Any] = {key: default for key, _, _, _, _, _, default in schema}
    values.update(defaults)
    known = set()
    for key, kind, lo, hi, lo_ex, hi_ex, _ in schema:
        known.add(key)
        if key not in section:
            continue
        value = section[key]
        name = f"{path}.{key}"
        expected = _format_range(kind, lo, hi, lo_ex, hi_ex)
        is_int = isinstance(value, int) and not isinstance(value, bool)
        is_num = (is_int or isinstance(value, float)) and math.isfinite(value)
        if (kind == _INT and not is_int) or (kind == _NUM and not is_num):
            report.add_error(f"{label}: {name} must be {expected}, got {_describe(value)}")
            continue
        above = value > lo if lo_ex else value >= lo
        below = value < hi if hi_ex else value <= hi
        if not (above and below):
            report.add_error(f"{label}: {name} must be {expected}, got {_describe(value)}")
            continue
        values[key] = value
    hints = {"pickups.health.maxPlayerHealth": " (moved to player.maxHealth)",
             "pickups.ammo.maxPlayerAmmo": " (moved to weapon.maxAmmo)"}
    for key in section:
        if key not in known:
            report.add_warning(f"{label}: unknown key {path}.{key} ignored{hints.get(path + '.' + key, '')}")
    return values


def validate_gameplay(file_path: Path) -> Tuple[ValidationReport, Dict[str, Any]]:
    """gameplay.json with the engine's rules: types, ranges, duplicate keys, sections, the
    enemies section (and the deprecated 'enemy'), cross-field rules; unknown keys are warnings.
    Returns the effective values (defaults for missing keys), e.g. values['player']['collisionRadius']."""
    report = ValidationReport()
    file_path = Path(file_path)
    label = str(file_path)
    values: Dict[str, Any] = {
        "player": {k: d for k, _, _, _, _, _, d in GAMEPLAY_SCHEMA["player"]},
        "weapon": {k: d for k, _, _, _, _, _, d in GAMEPLAY_SCHEMA["weapon"]},
        "enemies": {name: {**{k: d for k, _, _, _, _, _, d in GAMEPLAY_SCHEMA["enemy"]}, **over}
                    for name, over in _ENEMY_DEFAULTS.items()},
        "pickups": {name: {"value": v} for name, v in _PICKUP_DEFAULTS.items()},
    }
    if not file_path.is_file():
        report.add_error(f"Missing gameplay definitions: {file_path} (the game does not start without it)")
        return report, values
    document, errors = load_json_strict(file_path)
    for error in errors:
        report.add_error(error)
    if document is None or errors:
        return report, values
    if not isinstance(document, dict):
        report.add_error(f"{label}: the top level must be an object, got {_describe(document)}")
        return report, values

    def section(parent: dict, path: str, key: str) -> Optional[dict]:
        if key not in parent:
            return None
        if not isinstance(parent[key], dict):
            report.add_error(f"{label}: {path + '.' if path else ''}{key} must be an object, got {_describe(parent[key])}")
            return None
        return parent[key]

    for name in ("player", "weapon"):
        sec = section(document, "", name)
        if sec is not None:
            values[name] = _read_section(sec, name, GAMEPLAY_SCHEMA[name], {}, report, label)
    legacy = section(document, "", "enemy")
    enemies = section(document, "", "enemies")
    if legacy is not None and enemies is not None:
        report.add_error(f'{label}: use either "enemies" or the old "enemy" section, not both ("enemy" is now "enemies.guard")')
    elif legacy is not None:
        report.add_warning(f'{label}: section "enemy" is deprecated: it is read as "enemies.guard" (move it there)')
        values["enemies"]["guard"] = _read_section(legacy, "enemy", GAMEPLAY_SCHEMA["enemy"], {}, report, label)
    elif enemies is not None:
        for name in ENEMY_VARIANTS:
            sec = section(enemies, "enemies", name)
            if sec is not None:
                values["enemies"][name] = _read_section(sec, f"enemies.{name}", GAMEPLAY_SCHEMA["enemy"],
                                                        _ENEMY_DEFAULTS[name], report, label)
        for key in enemies:
            if key not in ENEMY_VARIANTS:
                report.add_warning(f"{label}: unknown key enemies.{key} ignored")
    pickups = section(document, "", "pickups")
    if pickups is not None:
        for name in ("health", "ammo"):
            sec = section(pickups, "pickups", name)
            if sec is not None:
                schema = [("value", _INT, 1, 10000, False, False, _PICKUP_DEFAULTS[name])]
                values["pickups"][name] = _read_section(sec, f"pickups.{name}", schema, {}, report, label)
        for key in pickups:
            if key not in ("health", "ammo"):
                report.add_warning(f"{label}: unknown key pickups.{key} ignored")
    for key in document:
        if key not in ("player", "weapon", "enemy", "enemies", "pickups"):
            report.add_warning(f"{label}: unknown key {key} ignored")
    if report.is_valid:
        player, weapon = values["player"], values["weapon"]
        if player["initialHealth"] > player["maxHealth"]:
            report.add_error(f"{label}: player.initialHealth ({player['initialHealth']}) must not exceed player.maxHealth ({player['maxHealth']})")
        if weapon["initialAmmo"] > weapon["maxAmmo"]:
            report.add_error(f"{label}: weapon.initialAmmo ({weapon['initialAmmo']}) must not exceed weapon.maxAmmo ({weapon['maxAmmo']})")
    if report.is_valid:
        report.add_info(f"gameplay.json: valid ({len(document)} section(s))")
    return report, values


# --- Level layout and progress -----------------------------------------------------------------


def _cell_of(x: float, y: float) -> Tuple[int, int]:
    return int(math.floor(x)), int(math.floor(y))


def _body_overlaps(x: float, y: float, radius: float, solid) -> Optional[Tuple[int, int]]:
    """The first solid cell a circle overlaps (the engine's IsBlockedByWall test), or None."""
    for cy in range(int(math.floor(y - radius)), int(math.floor(y + radius)) + 1):
        for cx in range(int(math.floor(x - radius)), int(math.floor(x + radius)) + 1):
            if not solid(cx, cy):
                continue
            nx = min(max(x, cx), cx + 1.0)
            ny = min(max(y, cy), cy + 1.0)
            if (x - nx) ** 2 + (y - ny) ** 2 < radius * radius:
                return cx, cy
    return None


def check_level_layout(rows: List[str], entities: List[dict], label: str, report: ValidationReport,
                       player_radius: float = DEFAULT_PLAYER_RADIUS) -> None:
    """Placement rules of a campaign level (all errors unless noted):

    * exactly one player, standing on a floor cell ('.' or 'T') with its body clear of walls;
    * every door entity on a 'D' cell, positioned inside that cell; every 'D' cell controlled by
      exactly one door entity (a 'D' without one is a wall for good);
    * every trigger entity on a 'T' cell, positioned inside it ('T' without a trigger: warning);
    * enemies, pickups and decorations on floor cells, not inside walls, the exit or a door;
      an enemy whose body overlaps a wall is a warning (it starts wedged)."""
    height = len(rows)
    width = len(rows[0]) if rows else 0

    def tile(x: int, y: int) -> str:
        return rows[y][x] if 0 <= y < height and 0 <= x < width else "#"

    door_cells: Dict[Tuple[int, int], int] = {}
    trigger_cells: Dict[Tuple[int, int], int] = {}
    for ent in entities:
        if ent["type"] == "door":
            door_cells[(ent["mapX"], ent["mapY"])] = door_cells.get((ent["mapX"], ent["mapY"]), 0) + 1
        elif ent["type"] == "trigger":
            trigger_cells[(ent["mapX"], ent["mapY"])] = trigger_cells.get((ent["mapX"], ent["mapY"]), 0) + 1

    def solid(x: int, y: int) -> bool:  # at level start: walls, the exit, and every closed door
        return tile(x, y) in SOLID_TILES or tile(x, y) == "D" or (x, y) in door_cells

    players = [e for e in entities if e["type"] == "player"]
    if len(players) != 1:
        report.add_error(f"{label}: {len(players)} player entities; exactly one is required")

    for index, ent in enumerate(entities):
        etype = ent["type"]
        where = f"{label}: {etype} {index} at ({ent['x']}, {ent['y']})"
        cell = _cell_of(ent["x"], ent["y"])
        if etype in ("door", "trigger"):
            target = (ent["mapX"], ent["mapY"])
            wanted = "D" if etype == "door" else "T"
            if tile(*target) != wanted:
                report.add_error(f"{where}: its cell {target} is '{tile(*target)}' on the map, not '{wanted}' "
                                 f"(a {etype} must sit on a '{wanted}' cell)")
            if cell != target:
                report.add_error(f"{where}: the position is in cell {cell}, not in its own cell {target}")
            continue
        t = tile(*cell)
        if t not in FLOOR_TILES or cell in door_cells:
            what = "a door cell" if (t == "D" or cell in door_cells) else f"a solid cell '{t}'"
            report.add_error(f"{where}: it is inside {what} {cell}; place it on a floor cell ('.' or 'T')")
            continue
        if etype == "player":
            blocked = _body_overlaps(ent["x"], ent["y"], player_radius, solid)
            if blocked:
                report.add_error(f"{where}: the player's body (radius {player_radius}) overlaps the solid cell "
                                 f"{blocked}, so it cannot move; move it towards the centre of its cell")
        elif etype == "enemy":
            blocked = _body_overlaps(ent["x"], ent["y"], ENEMY_RADIUS, solid)
            if blocked:
                report.add_warning(f"{where}: the enemy's body (radius {ENEMY_RADIUS}) overlaps the solid cell "
                                   f"{blocked}; it starts wedged against it")

    for y in range(height):
        for x in range(width):
            if tile(x, y) == "D":
                count = door_cells.get((x, y), 0)
                if count == 0:
                    report.add_error(f"{label}: 'D' at ({x}, {y}) has no door entity, so it never opens "
                                     "(add a door with mapX/mapY, or make it a wall)")
                elif count > 1:
                    report.add_error(f"{label}: 'D' at ({x}, {y}) has {count} door entities; exactly one is allowed")
            elif tile(x, y) == "T" and (x, y) not in trigger_cells:
                report.add_warning(f"{label}: 'T' at ({x}, {y}) has no trigger entity; it does nothing")


def find_reachable_exits(rows: List[str], spawn_x: float, spawn_y: float) -> Tuple[int, int]:
    """Returns (exit switches in the map, exit switches usable from the spawn), counting every
    'D' as passable. A map-only check (no entities); campaign levels use analyse_level_progress."""
    progress = analyse_level_progress(
        rows,
        [{"type": "door", "mapX": x, "mapY": y} for y, row in enumerate(rows) for x, c in enumerate(row) if c == "D"],
        spawn_x, spawn_y,
    )
    return progress.total_exits, progress.usable_exits


@dataclass
class LevelProgress:
    """What analyse_level_progress found for one level."""
    total_exits: int = 0
    usable_exits: int = 0
    locks: Dict[Tuple[int, int], str] = field(default_factory=dict)  # door cell -> lock colour
    key_order: List[str] = field(default_factory=list)  # colours in the order they become reachable
    problems: List[str] = field(default_factory=list)  # errors (lock without key, key behind its lock)
    reached: Set[Tuple[int, int]] = field(default_factory=set)  # cells reachable at the end


def analyse_level_progress(rows: List[str], entities: List[dict], spawn_x: float, spawn_y: float) -> LevelProgress:
    """Key-before-lock reachability, as the player experiences it.

    Breadth-first search (4-connected, like the exit switch rule) from the spawn cell over floor
    cells ('.', 'T', and every trigger entity's cell, which the engine turns into 'T'). A door
    entity's cell (mapX/mapY) is passable; a locked one only once a key of its colour lies in a
    reached cell. A 'D' without a door entity never opens. The search repeats until no new key is
    reached; an exit switch 'X' counts as usable when one of its four neighbours was reached.
    Problems: a lock colour without a key in the level, and a key that cannot be collected
    before the door it opens."""
    result = LevelProgress()
    height = len(rows)
    width = len(rows[0]) if rows else 0
    door_cells: Set[Tuple[int, int]] = set()
    trigger_cells: Set[Tuple[int, int]] = set()
    keys: List[Tuple[Tuple[int, int], str]] = []
    for e in entities:
        if e.get("type") in ("door", "trigger") and isinstance(e.get("mapX"), int) and isinstance(e.get("mapY"), int):
            cell = (e["mapX"], e["mapY"])
            if e["type"] == "door":
                door_cells.add(cell)
                if e.get("lock") in KEY_COLORS:
                    result.locks[cell] = e["lock"]
            else:
                trigger_cells.add(cell)
        elif e.get("type") == "pickup" and e.get("kind") == "key" and e.get("key") in KEY_COLORS:
            if isinstance(e.get("x"), (int, float)) and isinstance(e.get("y"), (int, float)):
                keys.append((_cell_of(e["x"], e["y"]), e["key"]))

    for cell, color in sorted(result.locks.items()):
        if not any(key_color == color for _, key_color in keys):
            result.problems.append(f"the door at {cell} needs the {color} key, but the level has no {color} key")

    def tile(x: int, y: int) -> str:
        return rows[y][x] if 0 <= y < height and 0 <= x < len(rows[y]) else "#"

    held: Set[str] = set()

    def passable(x: int, y: int) -> bool:
        if not (0 <= x < width and 0 <= y < height):
            return False
        if (x, y) in door_cells:
            return (x, y) not in result.locks or result.locks[(x, y)] in held
        return (x, y) in trigger_cells or tile(x, y) in FLOOR_TILES

    start = _cell_of(spawn_x, spawn_y)
    seen: Set[Tuple[int, int]] = set()
    while True:
        seen = set()
        if passable(*start):
            seen.add(start)
            queue = [start]
            for cx, cy in queue:
                for nx, ny in ((cx + 1, cy), (cx - 1, cy), (cx, cy + 1), (cx, cy - 1)):
                    if (nx, ny) not in seen and passable(nx, ny):
                        seen.add((nx, ny))
                        queue.append((nx, ny))
        new_keys = sorted({color for cell, color in keys if cell in seen} - held)
        if not new_keys:
            break
        held.update(new_keys)
        result.key_order.extend(new_keys)
    result.reached = seen

    for color in sorted(set(result.locks.values()) - held):
        if any(key_color == color for _, key_color in keys):
            result.problems.append(
                f"the {color} key cannot be reached from the spawn before the door it opens "
                f"(it is behind a {color} door, or behind another locked door that cannot be opened)"
            )

    for y in range(height):
        for x in range(width):
            if tile(x, y) != "X":
                continue
            result.total_exits += 1
            if any(n in seen for n in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1))):
                result.usable_exits += 1
    return result


# --- campaign.json (engine/assets/CampaignConfig.cpp) and its levels -----------------------------


def _is_content_id(value: str) -> bool:
    return bool(value) and all(c.isascii() and (c.islower() or c.isdigit() or c in "_-") for c in value)


@dataclass
class CampaignLevelInfo:
    index: int
    id: str
    map: str
    entities: str
    theme: str


def read_campaign(project_root: Path, report: ValidationReport) -> Optional[List[CampaignLevelInfo]]:
    """campaign.json with the engine's rules. Returns the levels whose fields are all valid, or
    None when the manifest itself cannot be used."""
    root = Path(project_root)
    path = root / "game" / "defs" / "campaign.json"
    label = path.name
    if not path.is_file():
        report.add_error(f"Missing campaign manifest: {path}")
        return None
    document, errors = load_json_strict(path)
    for error in errors:
        report.add_error(error)
    if document is None:
        return None
    if not isinstance(document, dict):
        report.add_error(f"{path}: the top level must be an object, got {_describe(document)}")
        return None
    title = document.get("title")
    if not isinstance(title, str) or not title:
        report.add_error(f"{label}: 'title' must be a non-empty string")
    elif len(title.encode("utf-8")) > MAX_CAMPAIGN_NAME_BYTES:
        report.add_warning(f"{label}: title is longer than {MAX_CAMPAIGN_NAME_BYTES} characters and is shortened on screen")
    for key in document:
        if key not in ("title", "levels"):
            report.add_warning(f'{label}: unknown key "{key}" ignored')
    levels = document.get("levels")
    if not isinstance(levels, list) or not levels:
        report.add_error(f"{label}: 'levels' must be a non-empty array")
        return None

    result: List[CampaignLevelInfo] = []
    seen_ids: Set[str] = set()
    for index, level in enumerate(levels):
        entry_label = f"{label}: levels[{index}]"
        if not isinstance(level, dict):
            report.add_error(f"{entry_label} must be an object")
            continue
        missing = [k for k in CAMPAIGN_LEVEL_FIELDS if not isinstance(level.get(k), str) or not level.get(k)]
        if missing:
            report.add_error(f"{entry_label}: missing or empty field(s): {', '.join(missing)}")
            continue
        entry_label += f" ({level['id']})"
        ok = True
        if not _is_content_id(level["id"]):
            report.add_error(f"{entry_label}: id must use only a-z, 0-9, '_' and '-'")
            ok = False
        elif level["id"] in seen_ids:
            report.add_error(f"{entry_label}: id is used by an earlier level (ids must be unique)")
            ok = False
        seen_ids.add(level["id"])
        if len(level["name"].encode("utf-8")) > MAX_CAMPAIGN_NAME_BYTES:
            report.add_warning(f"{entry_label}: name is longer than {MAX_CAMPAIGN_NAME_BYTES} characters and is shortened on screen")
        if not _is_content_id(level["theme"]):
            report.add_error(f"{entry_label}: theme must use only a-z, 0-9, '_' and '-', got '{level['theme']}'")
            ok = False
        elif not (root / "game" / "defs" / "themes" / f"{level['theme']}.cfg").is_file():
            report.add_error(f"{entry_label}: theme '{level['theme']}' not found (game/defs/themes/{level['theme']}.cfg)")
            ok = False
        for key in ("map", "entities"):
            if not (root / level[key]).is_file():
                report.add_error(f"{entry_label}: {key} file not found: {level[key]}")
                ok = False
        for key in level:
            if key not in CAMPAIGN_LEVEL_FIELDS:
                report.add_warning(f'{entry_label}: unknown key "{key}" ignored')
        if ok:
            result.append(CampaignLevelInfo(index, level["id"], level["map"], level["entities"], level["theme"]))
    return result


def validate_level(project_root: Path, level: CampaignLevelInfo, report: ValidationReport,
                   player_radius: float = DEFAULT_PLAYER_RADIUS) -> None:
    """Everything one campaign level needs to load and to be completable."""
    root = Path(project_root)
    label = f"campaign.json: levels[{level.index}] ({level.id})"
    map_report, rows = validate_map(root / level.map)
    if rows is None:
        for error in map_report.errors:
            report.add_error(f"{label}: {error}")
        return
    parsed = parse_entities(root / level.entities)
    if not parsed.ok:
        report.add_error(f"{label}: the entity file {level.entities} does not load ({len(parsed.errors)} error(s); "
                         f"first: {parsed.errors[0]})")
        return
    entities = parsed.entities
    bounds = ValidationReport()
    _check_bounds(entities, f"{label}: {level.entities}", (len(rows[0]), len(rows)), bounds)
    bounds.errors.extend(check_light_bounds(parsed.lights, len(rows[0]), len(rows), f"{label}: {level.entities}:"))
    if bounds.errors:
        report.errors.extend(bounds.errors)
        return

    before = len(report.errors)
    check_level_layout(rows, entities, label, report, player_radius)
    players = [e for e in entities if e["type"] == "player"]
    if len(players) != 1:
        return
    spawn_x, spawn_y = players[0]["x"], players[0]["y"]
    progress = analyse_level_progress(rows, entities, spawn_x, spawn_y)
    for problem in progress.problems:
        report.add_error(f"{label}: {problem}")
    if progress.total_exits == 0:
        report.add_error(f"{label}: the map has no exit switch 'X'; the level cannot be completed")
    elif progress.usable_exits == 0:
        report.add_error(
            f"{label}: no exit switch 'X' can be reached from the player spawn ({spawn_x}, {spawn_y})"
            + (" with the keys that can be collected first" if progress.locks else "")
        )
    for index, ent in enumerate(entities):
        if ent["type"] in ("pickup", "enemy") and _cell_of(ent["x"], ent["y"]) not in progress.reached:
            what = f"{ent.get('kind', '')} pickup" if ent["type"] == "pickup" else f"{ent.get('variant', 'guard')} enemy"
            report.add_warning(f"{label}: the {what} (entity {index}) at ({ent['x']}, {ent['y']}) cannot be reached "
                               "from the spawn")
    if len(report.errors) == before:
        keys = f"; keys in order: {', '.join(progress.key_order)}" if progress.key_order else ""
        lights = f"; {len(parsed.lights)} light zone(s)" if parsed.lights else ""
        report.add_info(f"{label}: {progress.usable_exits} of {progress.total_exits} exit switch(es) reachable{keys}{lights}")


def validate_campaign(project_root: Path, require_levels: Optional[int] = None,
                      player_radius: Optional[float] = None) -> ValidationReport:
    """campaign.json like the engine (fields, ids, files, themes), the level count when
    `require_levels` is given, and every level's layout and progress (validate_level)."""
    report = ValidationReport()
    root = Path(project_root)
    if player_radius is None:
        gameplay_path = root / "game" / "defs" / "gameplay.json"
        player_radius = DEFAULT_PLAYER_RADIUS
        if gameplay_path.is_file():
            gameplay_report, values = validate_gameplay(gameplay_path)
            if gameplay_report.is_valid:
                player_radius = values["player"]["collisionRadius"]
    levels = read_campaign(root, report)
    if levels is None:
        return report
    count = _campaign_level_count(root)
    if require_levels is not None and count != require_levels:
        report.add_error(f"campaign.json: the campaign has {count} level(s); {require_levels} are required (--require-levels)")
    for level in levels:
        validate_level(root, level, report, player_radius)
    report.add_info(f"Campaign: {count} level(s)")
    return report


def _campaign_level_count(project_root: Path) -> int:
    document, _ = load_json_strict(Path(project_root) / "game" / "defs" / "campaign.json")
    if isinstance(document, dict) and isinstance(document.get("levels"), list):
        return len(document["levels"])
    return 0


def find_orphans(project_root: Path) -> List[str]:
    """Maps and entity files under game/ that campaign.json does not use."""
    root = Path(project_root)
    scratch = ValidationReport()
    levels = read_campaign(root, scratch) or []
    used = {(root / level.map).resolve() for level in levels} | {(root / level.entities).resolve() for level in levels}
    orphans: List[str] = []
    for map_file in sorted((root / "game" / "maps").glob("*.map")):
        if map_file.resolve() not in used:
            orphans.append(f"orphan map: game/maps/{map_file.name} is not used by campaign.json")
    defs = root / "game" / "defs"
    for entity_file in sorted(defs.glob("*.entities.json")):
        if entity_file.resolve() not in used:
            orphans.append(f"orphan entity file: game/defs/{entity_file.name} is not used by campaign.json")
    for csv_file in sorted(defs.glob("*.entities.csv")):
        sibling = csv_file.with_name(csv_file.name.replace(".entities.csv", ".entities.json"))
        if csv_file.resolve() not in used and sibling.resolve() not in used:
            orphans.append(f"orphan entity file: game/defs/{csv_file.name} (no campaign level uses it or its JSON)")
    return orphans


def validate_project(project_root: Path, require_levels: Optional[int] = None) -> ValidationReport:
    """A complete pass over game/: maps, assets, theme/sprite/audio files, gameplay.json, entity
    files, campaign.json and its levels, and orphans (warnings)."""
    project_root = Path(project_root).resolve()
    report = ValidationReport()
    report.add_info(f"Starting complete project validation for {project_root}")

    maps_dir = project_root / "game" / "maps"
    assets_dir = project_root / "game" / "assets"
    defs_dir = project_root / "game" / "defs"

    map_dimensions: Dict[str, Tuple[int, int]] = {}
    if not maps_dir.exists():
        report.add_error(f"Missing maps directory: {maps_dir}")
    else:
        map_files = sorted(maps_dir.glob("*.map"))
        if not map_files:
            report.add_error(f"No .map files found in {maps_dir}")
        for map_file in map_files:
            map_rep, rows = validate_map(map_file)
            report.merge(map_rep)
            if rows:
                map_dimensions[map_file.stem] = (len(rows[0]), len(rows))

    if not assets_dir.exists():
        report.add_error(f"Missing assets directory: {assets_dir}")
    else:
        ppm_files = sorted(assets_dir.rglob("*.ppm"))  # includes the theme folders
        if not ppm_files:
            report.add_error(f"No .ppm files found in {assets_dir}")
        for ppm_file in ppm_files:
            report.merge(validate_ppm(ppm_file, expected_dim=None))
        wav_files = sorted(assets_dir.glob("*.wav"))
        if not wav_files:
            report.add_error(f"No .wav files found in {assets_dir}")
        for wav_file in wav_files:
            report.merge(validate_wav(wav_file))

    if not defs_dir.exists():
        report.add_error(f"Missing defs directory: {defs_dir}")
        return report

    def check_referenced_file(owner: str, key: str, rel_path: str) -> None:
        target = project_root / rel_path
        if not target.is_file():
            report.add_error(f"{owner} references non-existent file for '{key}': {rel_path}")
        elif target.suffix.lower() == ".ppm" and assets_dir not in target.resolve().parents:
            report.merge(validate_ppm(target))  # outside game/assets: not validated above

    # Texture themes: game/defs/themes/<id>.cfg, ten keys each, every file present.
    themes_dir = defs_dir / "themes"
    theme_files = sorted(themes_dir.glob("*.cfg")) if themes_dir.is_dir() else []
    if not theme_files:
        report.add_error(f"No texture themes found in {themes_dir}")
    for theme_cfg in theme_files:
        cfg_rep, theme_vals = validate_cfg(theme_cfg, THEME_KEYS)
        report.merge(cfg_rep)
        for key, rel_path in sorted(theme_vals.items()):
            if key not in THEME_KEYS:
                report.add_warning(f"{theme_cfg.name}: unknown key '{key}' ignored by the engine")
                continue
            check_referenced_file(f"themes/{theme_cfg.name}", key, rel_path)

    sprites_cfg = defs_dir / "sprites.cfg"
    if sprites_cfg.exists():
        cfg_rep, spr_vals = validate_cfg(sprites_cfg, SPRITE_KEYS)
        report.merge(cfg_rep)
        for key, rel_path in sorted(spr_vals.items()):
            if key in RENAMED_SPRITE_KEYS:
                report.add_error(
                    f"sprites.cfg: key '{key}' was renamed to '{RENAMED_SPRITE_KEYS[key]}' (Phase 6); the engine ignores it"
                )
            elif key not in SPRITE_KEYS:
                report.add_warning(f"sprites.cfg: unknown key '{key}' ignored by the engine")
            else:
                check_referenced_file("sprites.cfg", key, rel_path)
    else:
        report.add_error(f"Missing sprites.cfg in {defs_dir}")

    audio_cfg = defs_dir / "audio.cfg"
    if audio_cfg.exists():
        cfg_rep, audio_vals = validate_cfg(audio_cfg, AUDIO_REQUIRED_KEYS)
        report.merge(cfg_rep)
        for key, value in sorted(audio_vals.items()):
            if key == "master_volume":
                try:
                    volume = float(value)
                    if not (0.0 <= volume <= 1.0):
                        report.add_warning(f"audio.cfg master_volume ({volume}) is outside [0.0, 1.0] (clamped)")
                except ValueError:
                    report.add_error(f"audio.cfg invalid master_volume: {value}")
            elif key not in AUDIO_REQUIRED_KEYS | AUDIO_OPTIONAL_KEYS:
                report.add_warning(f"audio.cfg: unknown key '{key}' ignored by the engine")
            else:
                check_referenced_file("audio.cfg", key, value)
    else:
        report.add_error(f"Missing audio.cfg in {defs_dir}")

    gameplay_report, gameplay = validate_gameplay(defs_dir / "gameplay.json")
    report.merge(gameplay_report)

    # Every entity file on its own (the campaign check below adds the level rules).
    for csv_file in sorted(defs_dir.glob("*.entities.csv")):
        dims = map_dimensions.get(csv_file.name.replace(".entities.csv", ""))
        report.merge(validate_entity_csv(csv_file, dims)[0])
    for json_file in sorted(defs_dir.glob("*.entities.json")):
        dims = map_dimensions.get(json_file.name.replace(".entities.json", ""))
        report.merge(validate_entity_json(json_file, dims)[0])

    report.merge(validate_campaign(project_root, require_levels,
                                   gameplay["player"]["collisionRadius"] if gameplay_report.is_valid else None))
    for orphan in find_orphans(project_root):
        report.add_warning(orphan)
    return report
