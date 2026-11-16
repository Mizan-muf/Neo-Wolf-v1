"""Definition and content synchronization tools for Neo-Wolf (CSV/JSON conversions).

The JSON entity files (``<level>.entities.json``) are canonical: the engine loads them.
The CSV files are an optional export in the legacy table format, which the engine also reads:

    type,x,y,mapX,mapY,kind,value,angle,variant,key,lock

Trailing columns may be left out, and an empty column means "not given"
(e.g. ``pickup,11.8,6.6,,,health``). See docs/CONTENT_DEFINITIONS.md.

The CSV is a **lossy** export: it has no place for the JSON's light zones (the top-level
``"lights"`` array, known issue C21). JSON -> CSV drops them with a warning and notes them in a
``#`` comment line; CSV -> JSON keeps the light zones of the JSON it overwrites, and refuses to
overwrite a JSON it cannot read (it could not keep that file's light zones).
"""

import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from tools.devkit.engine_formats import CSV_COLUMNS as ENGINE_CSV_COLUMNS

# CSV column order and value types: the engine's table (engine_formats.CSV_COLUMNS).
_PYTHON_TYPES = {"text": str, "number": float, "integer": int}
CSV_COLUMNS: List[Tuple[str, type]] = [(name, _PYTHON_TYPES[kind]) for name, kind in ENGINE_CSV_COLUMNS]
MIN_CSV_COLUMNS = 3  # type, x, y
CSV_HEADER = "# " + ",".join(name for name, _ in CSV_COLUMNS)


def load_entity_csv(file_path: Path) -> List[Dict[str, Any]]:
    """Parse an entity placement CSV file into dictionaries (empty columns are omitted)."""
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"Entity CSV file not found: {file_path}")

    entities: List[Dict[str, Any]] = []
    lines = file_path.read_text(encoding="utf-8-sig").splitlines()
    for raw in lines:
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < MIN_CSV_COLUMNS:
            continue
        entry: Dict[str, Any] = {}
        for (name, kind), cell in zip(CSV_COLUMNS, parts):
            if cell == "":
                continue
            entry[name] = kind(cell)
        entities.append(entry)
    return entities


def _format_cell(value: Any) -> str:
    return "" if value is None else str(value)


def save_entity_csv(entities: List[Dict[str, Any]], file_path: Path, dropped_lights: int = 0) -> None:
    """Save entity dictionary list to CSV format (fields without a CSV column are dropped).

    `dropped_lights`: the number of light zones of the JSON that the CSV cannot hold; when
    there are any, a comment line says so (the engine and the DevKit skip '#' lines)."""
    file_path = Path(file_path)
    lines: List[str] = [CSV_HEADER]
    if dropped_lights:
        lines.append(
            f"# lights: {dropped_lights} light zone(s) in the JSON are not exported (the CSV cannot hold them)"
        )
    for ent in entities:
        cells = [_format_cell(ent.get(name)) for name, _ in CSV_COLUMNS]
        while len(cells) > MIN_CSV_COLUMNS and cells[-1] == "":
            cells.pop()
        lines.append(",".join(cells))
    file_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _read_entity_json(file_path: Path) -> Dict[str, Any]:
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"Entity JSON file not found: {file_path}")
    data = json.loads(file_path.read_text(encoding="utf-8-sig"))
    if not isinstance(data, dict):
        raise ValueError(f"{file_path}: the top level must be an object with an 'entities' array")
    return data


def load_entity_json(file_path: Path) -> Tuple[str, List[Dict[str, Any]]]:
    """Parse an entity placement JSON file."""
    file_path = Path(file_path)
    data = _read_entity_json(file_path)
    level_id = data.get("level", file_path.stem.replace(".entities", ""))
    entities = data.get("entities", [])
    return level_id, entities


def load_entity_lights(file_path: Path) -> List[Dict[str, Any]]:
    """The light zones (top-level "lights") of an entity JSON file, as written; [] if none."""
    lights = _read_entity_json(file_path).get("lights", [])
    return lights if isinstance(lights, list) else []


def save_entity_json(
    level_id: str, entities: List[Dict[str, Any]], file_path: Path, lights: Optional[List[Dict[str, Any]]] = None
) -> None:
    """Save entity dictionary list (and light zones, if any) to formatted JSON."""
    file_path = Path(file_path)
    data: Dict[str, Any] = {
        "level": level_id,
        "entities": entities,
    }
    if lights:
        data["lights"] = lights
    file_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def csv_to_json(csv_path: Path, json_path: Path, level_id: Optional[str] = None) -> int:
    """Convert CSV entity placements to JSON. Returns the number of light zones kept.

    The CSV has no light zones, so those of an existing JSON are kept. A JSON that exists but
    cannot be read is not overwritten (ValueError): its light zones would be lost."""
    csv_path = Path(csv_path)
    json_path = Path(json_path)
    if not level_id:
        level_id = csv_path.stem.replace(".entities", "")
    lights: List[Dict[str, Any]] = []
    if json_path.exists():
        try:
            lights = load_entity_lights(json_path)
        except (ValueError, OSError) as exc:  # json.JSONDecodeError is a ValueError
            raise ValueError(
                f"{json_path} cannot be read ({exc}); not overwriting it from {csv_path.name}, because its "
                'light zones ("lights") would be lost. Fix the JSON, or delete it to rebuild it from the CSV.'
            ) from exc
    entities = load_entity_csv(csv_path)
    save_entity_json(level_id, entities, json_path, lights)
    return len(lights)


def json_to_csv(json_path: Path, csv_path: Path) -> int:
    """Convert JSON entity placements to CSV. Returns the number of light zones the CSV drops."""
    json_path = Path(json_path)
    csv_path = Path(csv_path)
    _, entities = load_entity_json(json_path)
    dropped = len(load_entity_lights(json_path))
    save_entity_csv(entities, csv_path, dropped)
    return dropped


def entity_file_pair(entities_path: Path) -> Tuple[Path, Path, str]:
    """(JSON path, CSV path, base name) of the entity file pair that `entities_path` (either the
    ``<base>.entities.json`` or the ``<base>.entities.csv``) belongs to; both in its folder."""
    entities_path = Path(entities_path)
    name = entities_path.name
    for suffix in (".entities.json", ".entities.csv"):
        if name.endswith(suffix):
            base_name = name[: -len(suffix)]
            break
    else:
        raise ValueError(f"{entities_path} is not a <level>.entities.json or <level>.entities.csv file")
    folder = entities_path.parent
    return folder / f"{base_name}.entities.json", folder / f"{base_name}.entities.csv", base_name


def sync_entity_definitions(
    project_root: Path, from_csv: bool = False, entities_path: Optional[Path] = None
) -> List[str]:
    """Synchronize the entity files in game/defs/.

    Default: JSON (canonical) -> CSV export, overwriting every <level>.entities.csv.
    from_csv=True: the reverse, CSV -> JSON, overwriting every <level>.entities.json.
    entities_path: sync only the pair this file belongs to (a campaign level's ``entities``,
    sync-entities --level); its source file (the JSON, or the CSV with from_csv) must exist,
    else ValueError and nothing is written.

    Returns one line per file pair; lines starting with "Warning:" report light zones the
    CSV cannot hold. With from_csv, the JSON's light zones are kept (a note says so), and a
    JSON that cannot be read raises ValueError before any file is written.
    """
    defs_dir = Path(project_root) / "game" / "defs"
    synced: List[str] = []

    # (JSON, CSV, base name) of every pair to sync, in a stable order.
    pairs: List[Tuple[Path, Path, str]] = []
    if entities_path is not None:
        json_file, csv_file, base_name = entity_file_pair(entities_path)
        source = csv_file if from_csv else json_file
        if not source.is_file():
            raise ValueError(f"{source} not found; nothing was synced")
        pairs.append((json_file, csv_file, base_name))
    else:
        pattern = "*.entities.csv" if from_csv else "*.entities.json"
        for source in sorted(defs_dir.glob(pattern)):
            pairs.append(entity_file_pair(source))

    if from_csv:
        for json_file, _, _ in pairs:  # refuse before writing anything
            if json_file.exists():
                try:
                    load_entity_lights(json_file)
                except (ValueError, OSError) as exc:
                    raise ValueError(
                        f"{json_file} cannot be read ({exc}); nothing was synced. sync-entities --from-csv keeps "
                        'each JSON\'s light zones ("lights"), so it does not overwrite a JSON it cannot read. '
                        "Fix the JSON, or delete it to rebuild it from the CSV."
                    ) from exc
        for json_file, csv_file, base_name in pairs:
            kept = csv_to_json(csv_file, json_file, level_id=base_name)
            synced.append(
                f"Synced {csv_file.name} -> {json_file.name}"
                + (f" (kept its {kept} light zone(s); the CSV has none)" if kept else "")
            )
    else:
        for json_file, csv_file, _ in pairs:
            dropped = json_to_csv(json_file, csv_file)
            synced.append(f"Synced {json_file.name} -> {csv_file.name}")
            if dropped:
                synced.append(
                    f"Warning: {json_file.name} has {dropped} light zone(s); the CSV cannot hold them, "
                    f"so {csv_file.name} has none (the JSON, which the game loads, keeps them)"
                )

    return synced
