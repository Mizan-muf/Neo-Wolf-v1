"""Readers for the engine's content formats that behave exactly like the engine's loaders.

The DevKit used to read maps and entity files with Python's ``str.splitlines()`` and
``str.strip()``, which split on more characters (a lone ``\\r``, form feeds, ...) and strip more
(Unicode spaces such as a no-break space) than the engine does, so a file could validate and
still fail in the game (Roadmap Phase 7). The functions here follow the C++ code line by line:

* maps: ``engine/world/MapLoader.cpp`` (``std::getline`` on ``\\n``, one trailing ``\\r``
  removed, empty lines and lines whose *first* character is ``;`` skipped, every other line is
  a grid row as is; a byte order mark is part of the first row);
* entity CSV files: ``ParseEntityPlacementsCsv`` (lines split on ``\\n``, a UTF-8 byte order mark
  removed from line 1, C ``isspace`` trimming, ``#`` comments, ``strtod``/``strtoll`` numbers);
* JSON files: nlohmann/json as used by ``ParseJsonText`` (a byte order mark is skipped, no
  comments, no ``NaN``/``Infinity``, a repeated key in one object is an error).

The parity fixtures in ``tests/fixtures/parity/`` are loaded by both sides (native test
``content_parity_fixtures`` and ``tools/devkit/tests/test_parity.py``) to keep it that way.
"""

import json
import math
import re
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

MAP_TILES = ".#ABDTX"
SOLID_TILES = {"#", "A", "B", "X"}  # always solid; 'D' is solid while the door is closed
WALKABLE_TILES = {".", "D", "T"}
FLOOR_TILES = {".", "T"}  # cells an entity can stand in (a 'D' becomes a closed door)

# C's isspace() in the "C" locale (std::isspace in the engine): ASCII whitespace only.
C_SPACE = " \t\n\v\f\r"
UTF8_BOM = b"\xef\xbb\xbf"

# engine/world/EntityPlacementLoader.cpp
MAX_MAP_COORDINATE = 100000
MAX_GAMEPLAY_STAT = 10000  # kMaxGameplayStat
ENTITY_TYPES = ("player", "enemy", "pickup", "door", "trigger", "decoration")
PICKUP_KINDS = ("health", "ammo", "key")
KEY_COLORS = ("red", "blue", "gold")
ENEMY_VARIANTS = ("guard", "brute", "runner")
ENTITY_FIELDS = {
    "player": {"type", "x", "y", "angle"},
    "enemy": {"type", "x", "y", "variant"},
    "pickup": {"type", "x", "y", "kind", "value", "key"},
    "door": {"type", "x", "y", "mapX", "mapY", "lock"},
    "trigger": {"type", "x", "y", "mapX", "mapY"},
    "decoration": {"type", "x", "y"},
}
KNOWN_ENTITY_FIELDS = {"type", "x", "y", "mapX", "mapY", "angle", "kind", "value", "variant", "key", "lock"}
TOP_LEVEL_FIELDS = {"level", "entities", "lights"}
# Light zones: the optional top-level "lights" array (ReadLightZone, kMin/MaxLightZoneLevel).
LIGHT_FIELDS = ("x", "y", "w", "h", "level")
MIN_LIGHT_LEVEL = 0.2
MAX_LIGHT_LEVEL = 1.0
# CSV column order and kinds (kCsvColumns).
CSV_COLUMNS: List[Tuple[str, str]] = [
    ("type", "text"),
    ("x", "number"),
    ("y", "number"),
    ("mapX", "integer"),
    ("mapY", "integer"),
    ("kind", "text"),
    ("value", "integer"),
    ("angle", "number"),
    ("variant", "text"),
    ("key", "text"),
    ("lock", "text"),
]


def c_strip(text: str) -> str:
    """Trim like the engine's Trim(): ASCII whitespace only (not Unicode spaces)."""
    return text.strip(C_SPACE)


# --- Maps ----------------------------------------------------------------------------------------


def map_rows_from_bytes(data: bytes) -> List[str]:
    """The grid rows of a .map file, exactly as MapLoader reads them (bytes as latin-1 text)."""
    rows: List[str] = []
    for raw in data.split(b"\n"):
        line = raw[:-1] if raw.endswith(b"\r") else raw  # CRLF file
        if not line or line[:1] == b";":
            continue
        rows.append(line.decode("latin-1"))
    return rows


@dataclass
class MapParse:
    rows: List[str] = field(default_factory=list)
    errors: List[str] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.errors

    @property
    def width(self) -> int:
        return len(self.rows[0]) if self.rows else 0

    @property
    def height(self) -> int:
        return len(self.rows)


def parse_map(path: Path) -> MapParse:
    """Loads a .map file with the engine's rules (LoadMapFromFile); errors use its wording."""
    result = MapParse()
    path = Path(path)
    try:
        data = path.read_bytes()
    except OSError as exc:
        result.errors.append(f"Unable to open map file: {path} ({exc})")
        return result
    result.rows = map_rows_from_bytes(data)
    if not result.rows:
        result.errors.append(f"Map file has no rows: {path}")
        return result
    width = len(result.rows[0])
    for y, row in enumerate(result.rows):
        if len(row) != width:
            # The engine reports only the width for such a row; its characters are not checked.
            result.errors.append(f"{path}: inconsistent row width at y={y} (expected {width}, got {len(row)})")
            continue
        for x, char in enumerate(row):
            if char not in MAP_TILES:
                result.errors.append(f"{path}: invalid tile {char!r} at ({x},{y})")
    return result


# --- Numbers like strtod / strtoll ---------------------------------------------------------------

# ASCII digits only: Python's \d also matches other scripts' digits, strtod does not.
_DECIMAL_FLOAT = re.compile(r"[+-]?(?:[0-9]+\.?[0-9]*|\.[0-9]+)(?:[eE][+-]?[0-9]+)?\Z")
_HEX_FLOAT = re.compile(r"[+-]?0[xX](?:[0-9a-fA-F]+\.?[0-9a-fA-F]*|\.[0-9a-fA-F]+)(?:[pP][+-]?[0-9]+)?\Z")
_INTEGER = re.compile(r"[+-]?[0-9]+\Z")


def parse_c_double(text: str) -> Optional[float]:
    """A CSV number cell as the engine reads it: all of it must be a strtod number, finite.

    Python's float() also accepts '1_000', Unicode digits and surrounding whitespace, which
    strtod does not (the engine has trimmed the cell already)."""
    if _DECIMAL_FLOAT.match(text):
        value = float(text)
    elif _HEX_FLOAT.match(text):
        sign = -1.0 if text.startswith("-") else 1.0
        body = text.lstrip("+-")
        if "p" not in body.lower():
            body += "p0"
        try:
            value = sign * float.fromhex(body)
        except (OverflowError, ValueError):
            return None
    else:
        return None  # includes inf/nan: rejected by the engine's isfinite() check
    return value if math.isfinite(value) else None


def parse_c_integer(text: str) -> Optional[int]:
    """A CSV integer cell as strtoll reads it (decimal, optional sign, nothing else)."""
    if not _INTEGER.match(text):
        return None
    value = int(text)
    return value if -(2**63) <= value < 2**63 else None  # strtoll's range (int64); beyond it, ERANGE


# --- JSON like nlohmann/json ---------------------------------------------------------------------


class _DuplicateKeys:
    def __init__(self) -> None:
        self.duplicates: List[str] = []

    def __call__(self, pairs: List[Tuple[str, Any]]) -> Dict[str, Any]:
        result: Dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                self.duplicates.append(key)
            result[key] = value
        return result


def _reject_constant(name: str) -> Any:
    raise ValueError(f"invalid literal {name} (JSON has no NaN or Infinity)")


def load_json_strict(path: Path) -> Tuple[Any, List[str]]:
    """Parses a JSON file like the engine: BOM allowed, no NaN/Infinity, no repeated keys.

    Returns (document, errors); the document is None when the file cannot be parsed."""
    path = Path(path)
    try:
        data = path.read_bytes()
    except OSError as exc:
        return None, [f"Unable to open {path}: {exc}"]
    if data.startswith(UTF8_BOM):
        data = data[len(UTF8_BOM) :]
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError as exc:
        return None, [f"{path}: invalid JSON: not UTF-8 ({exc})"]
    hook = _DuplicateKeys()
    try:
        document = json.loads(text, object_pairs_hook=hook, parse_constant=_reject_constant)
    except ValueError as exc:
        return None, [f"{path}: invalid JSON: {exc}"]
    errors = [f'{path}: duplicate key "{key}" (each key may appear only once per object)' for key in hook.duplicates]
    return document, errors


# --- Entity placements ---------------------------------------------------------------------------


def is_number(value: object) -> bool:
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def is_int(value: object) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


@dataclass
class EntityParse:
    entities: List[dict] = field(default_factory=list)  # the placements that are valid
    lights: List[dict] = field(default_factory=list)  # the light zones that are valid (JSON only)
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    count: int = 0  # placements in the file, valid or not

    @property
    def ok(self) -> bool:
        return not self.errors


def check_placement(item: dict, label: str, result: EntityParse) -> bool:
    """One placement with the engine's rules (ReadPlacement). False on an error."""
    etype = item.get("type")
    if etype not in ENTITY_TYPES:
        if "type" not in item:
            result.errors.append(f"{label}: type is required ({', '.join(ENTITY_TYPES)})")
        else:
            result.errors.append(f"{label}: invalid type {etype!r}. Valid types: {', '.join(sorted(ENTITY_TYPES))}")
        return False

    ok = True
    x = item.get("x")
    y = item.get("y")
    if not is_number(x) or not is_number(y):
        result.errors.append(f"{label} ({etype}): x and y must be numbers, got {x!r}, {y!r}")
        ok = False

    for key in sorted(set(item) - ENTITY_FIELDS[etype]):
        if key in KNOWN_ENTITY_FIELDS:
            result.warnings.append(f"{label} ({etype}): field '{key}' is not used by a {etype}; ignored")
        else:
            result.warnings.append(f"{label} ({etype}): unknown field '{key}' ignored")

    if etype in ("door", "trigger"):
        map_x = item.get("mapX")
        map_y = item.get("mapY")
        if not all(is_int(v) and 0 <= v <= MAX_MAP_COORDINATE for v in (map_x, map_y)):
            result.errors.append(
                f"{label} ({etype}): mapX and mapY are required integers from 0 to {MAX_MAP_COORDINATE}, "
                f"got {map_x!r}, {map_y!r}"
            )
            ok = False

    if etype == "pickup" and not _check_pickup(item, label, result):
        ok = False

    if etype == "player" and "angle" in item and not is_number(item["angle"]):
        result.errors.append(f"{label} (player): angle must be a number (degrees), got {item['angle']!r}")
        ok = False
    if etype == "enemy" and "variant" in item and item["variant"] not in ENEMY_VARIANTS:
        result.errors.append(f"{label} (enemy): variant must be 'guard', 'brute' or 'runner', got {item['variant']!r}")
        ok = False
    if etype == "door" and "lock" in item and item["lock"] not in KEY_COLORS:
        result.errors.append(f"{label} (door): lock must be 'red', 'blue' or 'gold', got {item['lock']!r}")
        ok = False
    return ok


def _check_pickup(item: dict, label: str, result: EntityParse) -> bool:
    """The pickup fields: kind, and the key colour of a key or the value of the others."""
    ok = True
    kind = item.get("kind")
    if kind not in PICKUP_KINDS:
        result.errors.append(f"{label} (pickup): kind is required and must be 'health', 'ammo' or 'key', got {kind!r}")
        ok = False
    if kind == "key":
        if item.get("key") not in KEY_COLORS:
            result.errors.append(
                f"{label} (pickup): a key pickup needs 'key': 'red', 'blue' or 'gold', got {item.get('key')!r}"
            )
            ok = False
        if "value" in item:
            result.warnings.append(f"{label} (pickup): field 'value' is not used by a key pickup; ignored")
        return ok
    if "key" in item:
        result.warnings.append(f"{label} (pickup): field 'key' only applies to pickups of kind 'key'; ignored")
    if "value" in item:
        value = item["value"]
        if not is_int(value) or not (1 <= value <= MAX_GAMEPLAY_STAT):
            result.errors.append(
                f"{label} (pickup): value must be an integer from 1 to {MAX_GAMEPLAY_STAT}, got {value!r}"
            )
            ok = False
    return ok


def check_light_zone(item: dict, label: str, result: EntityParse) -> bool:
    """One entry of "lights" with the engine's rules (ReadLightZone). False on an error.

    x and y are integers from 0, w and h integers from 1 (all up to MAX_MAP_COORDINATE), and
    level a number from MIN_LIGHT_LEVEL to MAX_LIGHT_LEVEL. Map bounds are checked separately (check_light_bounds)."""
    ok = True
    for key, low in (("x", 0), ("y", 0), ("w", 1), ("h", 1)):
        if key not in item:
            result.errors.append(f"{label}: field '{key}' is required (an integer from {low} to {MAX_MAP_COORDINATE})")
            ok = False
        elif not (is_int(item[key]) and low <= item[key] <= MAX_MAP_COORDINATE):
            result.errors.append(
                f"{label}: field '{key}' must be an integer from {low} to {MAX_MAP_COORDINATE}, got {item[key]!r}"
            )
            ok = False
    if "level" not in item:
        result.errors.append(
            f"{label}: field 'level' is required (a number from {MIN_LIGHT_LEVEL} to {MAX_LIGHT_LEVEL})"
        )
        ok = False
    elif not (is_number(item["level"]) and MIN_LIGHT_LEVEL <= item["level"] <= MAX_LIGHT_LEVEL):
        result.errors.append(
            f"{label}: field 'level' must be a number from {MIN_LIGHT_LEVEL} to {MAX_LIGHT_LEVEL}, got {item['level']!r}"
        )
        ok = False
    for key in sorted(set(item) - set(LIGHT_FIELDS)):
        result.warnings.append(f"{label}: unknown field '{key}' ignored")
    return ok


def check_light_bounds(lights: List[dict], width: int, height: int, label: str) -> List[str]:
    """The zones that are not fully inside a width x height map (BuildLevelFromPlacements)."""
    errors: List[str] = []
    for index, zone in enumerate(lights):
        right, bottom = zone["x"] + zone["w"], zone["y"] + zone["h"]
        if right > width or bottom > height:
            errors.append(
                f"{label} lights[{index}]: cells ({zone['x']}, {zone['y']}) to ({right - 1}, {bottom - 1}) "
                f"are not all inside the {width}x{height} map"
            )
    return errors


def light_grid(width: int, height: int, lights: List[dict]) -> List[List[float]]:
    """The light level of every cell once the zones are applied in order (1.0 where none)."""
    grid = [[1.0] * width for _ in range(height)]
    for zone in lights:
        for y in range(zone["y"], min(zone["y"] + zone["h"], height)):
            for x in range(zone["x"], min(zone["x"] + zone["w"], width)):
                grid[y][x] = float(zone["level"])
    return grid


def parse_entities_json(path: Path) -> EntityParse:
    result = EntityParse()
    path = Path(path)
    document, errors = load_json_strict(path)
    result.errors.extend(errors)
    if document is None:
        return result
    if not isinstance(document, dict):
        result.errors.append(f"JSON {path}: Root must be an object with an 'entities' array.")
        return result
    if "level" in document and not isinstance(document["level"], str):
        result.warnings.append(f"JSON {path}: field 'level' should be a string (the level id)")
    for key in sorted(set(document) - TOP_LEVEL_FIELDS):
        result.warnings.append(f"JSON {path}: unknown top-level field '{key}' ignored")
    if "lights" in document:
        lights = document["lights"]
        if not isinstance(lights, list):
            result.errors.append(f"JSON {path}: field 'lights' must be an array of light zones, got {lights!r}")
        else:
            for index, item in enumerate(lights):
                label = f"JSON {path}: lights[{index}]"
                if not isinstance(item, dict):
                    result.errors.append(f"{label}: each light zone must be an object, got {item!r}")
                elif check_light_zone(item, label, result):
                    result.lights.append(item)
    entities = document.get("entities")
    if not isinstance(entities, list):
        result.errors.append(f"JSON {path}: Missing 'entities' array.")
        return result
    for index, item in enumerate(entities):
        label = f"JSON {path}: entities[{index}]"
        result.count += 1
        if not isinstance(item, dict):
            result.errors.append(f"{label} is not an object.")
            continue
        if check_placement(item, label, result):
            result.entities.append(item)
    return result


def parse_entities_csv(path: Path) -> EntityParse:
    result = EntityParse()
    path = Path(path)
    try:
        data = path.read_bytes()
    except OSError as exc:
        result.errors.append(f"Unable to open entity placement file: {path} ({exc})")
        return result
    index = 0
    for line_number, raw in enumerate(data.split(b"\n"), start=1):
        text = raw[len(UTF8_BOM) :] if line_number == 1 and raw.startswith(UTF8_BOM) else raw
        line = c_strip(text.decode("utf-8", errors="surrogateescape"))
        if not line or line.startswith("#"):
            continue
        label = f"CSV {path}:{line_number} (entity {index})"
        index += 1
        result.count += 1
        cells = [c_strip(cell) for cell in line.split(",")]
        if len(cells) < 3:
            result.errors.append(f"{label}: Expected at least 3 fields (type,x,y), got {len(cells)}")
            continue
        if len(cells) > len(CSV_COLUMNS):
            result.warnings.append(f"{label}: {len(cells) - len(CSV_COLUMNS)} extra column(s) after 'lock' ignored")
        item: dict = {}
        converted = True
        for (name, kind), cell in zip(CSV_COLUMNS, cells):
            if cell == "":
                continue  # an empty column means "not given"
            if kind == "text":
                item[name] = cell
                continue
            value = parse_c_double(cell) if kind == "number" else parse_c_integer(cell)
            if value is None:
                expected = "a number" if kind == "number" else "an integer"
                result.errors.append(f"{label}: column '{name}' must be {expected}, got {cell!r}")
                converted = False
            else:
                item[name] = value
        if converted and check_placement(item, label, result):
            result.entities.append(item)
    return result


def parse_entities(path: Path) -> EntityParse:
    """An entity file chosen by its extension, like LoadEntityPlacementsFromFile."""
    path = Path(path)
    suffix = path.suffix.lower()
    if suffix == ".json":
        return parse_entities_json(path)
    if suffix == ".csv":
        return parse_entities_csv(path)
    result = EntityParse()
    result.errors.append(f"{path}: unsupported entity file type (expected .json, or legacy .csv)")
    return result


# --- Key/value files (themes, sprites.cfg, audio.cfg) --------------------------------------------


@dataclass
class KeyValueParse:
    values: Dict[str, str] = field(default_factory=dict)
    errors: List[str] = field(default_factory=list)


def parse_key_value(path: Path) -> KeyValueParse:
    """engine/assets/ContentConfig.cpp: key=value lines, '#' comments, trimmed; a line without
    '=' or with an empty key or value is a syntax error (the engine then ignores the whole file)."""
    result = KeyValueParse()
    path = Path(path)
    try:
        data = path.read_bytes()
    except OSError as exc:
        result.errors.append(f"Config file not found or unreadable: {path} ({exc})")
        return result
    for line_number, raw in enumerate(data.split(b"\n"), start=1):
        line = c_strip(raw.decode("utf-8", errors="surrogateescape"))
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            result.errors.append(f"CFG {path}:{line_number}: Missing '=' delimiter in '{line}'")
            continue
        key, value = (c_strip(part) for part in line.split("=", 1))
        if not key or not value:
            result.errors.append(f"CFG {path}:{line_number}: Empty key or value in '{line}'")
            continue
        result.values[key] = value
    return result
