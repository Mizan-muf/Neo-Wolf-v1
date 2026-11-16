"""Definition and content synchronization tools for Neo-Wolf (CSV/JSON conversions).

The JSON entity files (``<level>.entities.json``) are canonical: the engine loads them.
The CSV files are an optional export in the legacy table format, which the engine also reads:

    type,x,y,mapX,mapY,kind,value,angle,variant

Trailing columns may be left out, and an empty column means "not given"
(e.g. ``pickup,11.8,6.6,,,health``). See docs/CONTENT_DEFINITIONS.md.
"""

import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

# CSV column order and value types (the engine's EntityPlacementLoader uses the same table).
CSV_COLUMNS: List[Tuple[str, type]] = [
    ("type", str),
    ("x", float),
    ("y", float),
    ("mapX", int),
    ("mapY", int),
    ("kind", str),
    ("value", int),
    ("angle", float),
    ("variant", str),
    ("key", str),
    ("lock", str),
]
CSV_HEADER = "# " + ",".join(name for name, _ in CSV_COLUMNS)


def load_entity_csv(file_path: Path) -> List[Dict[str, Any]]:
    """Parse an entity placement CSV file into dictionaries (empty columns are omitted)."""
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"Entity CSV file not found: {file_path}")

    entities: List[Dict[str, Any]] = []
    lines = file_path.read_text(encoding="utf-8-sig").splitlines()
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 3:
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


def save_entity_csv(entities: List[Dict[str, Any]], file_path: Path) -> None:
    """Save entity dictionary list to CSV format (fields without a CSV column are dropped)."""
    file_path = Path(file_path)
    lines: List[str] = [CSV_HEADER]
    for ent in entities:
        cells = [_format_cell(ent.get(name)) for name, _ in CSV_COLUMNS]
        while len(cells) > 3 and cells[-1] == "":
            cells.pop()
        lines.append(",".join(cells))
    file_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def load_entity_json(file_path: Path) -> Tuple[str, List[Dict[str, Any]]]:
    """Parse an entity placement JSON file."""
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"Entity JSON file not found: {file_path}")

    data = json.loads(file_path.read_text(encoding="utf-8-sig"))
    level_id = data.get("level", file_path.stem.replace(".entities", ""))
    entities = data.get("entities", [])
    return level_id, entities


def save_entity_json(level_id: str, entities: List[Dict[str, Any]], file_path: Path) -> None:
    """Save entity dictionary list to formatted JSON."""
    file_path = Path(file_path)
    data = {
        "level": level_id,
        "entities": entities,
    }
    file_path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def csv_to_json(csv_path: Path, json_path: Path, level_id: Optional[str] = None) -> None:
    """Convert CSV entity placements to JSON."""
    csv_path = Path(csv_path)
    json_path = Path(json_path)
    if not level_id:
        level_id = csv_path.stem.replace(".entities", "")
    entities = load_entity_csv(csv_path)
    save_entity_json(level_id, entities, json_path)


def json_to_csv(json_path: Path, csv_path: Path) -> None:
    """Convert JSON entity placements to CSV."""
    json_path = Path(json_path)
    csv_path = Path(csv_path)
    _, entities = load_entity_json(json_path)
    save_entity_csv(entities, csv_path)


def sync_entity_definitions(project_root: Path, from_csv: bool = False) -> List[str]:
    """Synchronize the entity files in game/defs/.

    Default: JSON (canonical) -> CSV export, overwriting every <level>.entities.csv.
    from_csv=True: the reverse, CSV -> JSON, overwriting every <level>.entities.json.
    """
    defs_dir = Path(project_root) / "game" / "defs"
    synced: List[str] = []

    if from_csv:
        for csv_file in sorted(defs_dir.glob("*.entities.csv")):
            base_name = csv_file.name.replace(".entities.csv", "")
            json_file = defs_dir / f"{base_name}.entities.json"
            csv_to_json(csv_file, json_file, level_id=base_name)
            synced.append(f"Synced {csv_file.name} -> {json_file.name}")
    else:
        for json_file in sorted(defs_dir.glob("*.entities.json")):
            base_name = json_file.name.replace(".entities.json", "")
            csv_file = defs_dir / f"{base_name}.entities.csv"
            json_to_csv(json_file, csv_file)
            synced.append(f"Synced {json_file.name} -> {csv_file.name}")

    return synced
