"""Definition and content synchronization tools for Neo-Wolf (CSV/JSON conversions)."""

import json
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple


def load_entity_csv(file_path: Path) -> List[Dict[str, Any]]:

    """Parse an entity placement CSV file into dictionaries."""
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"Entity CSV file not found: {file_path}")

    entities: List[Dict[str, Any]] = []
    lines = file_path.read_text(encoding="utf-8").splitlines()
    for line in lines:
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 3:
            continue
        entry: Dict[str, Any] = {
            "type": parts[0],
            "x": float(parts[1]),
            "y": float(parts[2]),
        }
        if len(parts) >= 5:
            entry["mapX"] = int(parts[3])
            entry["mapY"] = int(parts[4])
        entities.append(entry)
    return entities


def save_entity_csv(entities: List[Dict[str, Any]], file_path: Path) -> None:
    """Save entity dictionary list to CSV format."""
    file_path = Path(file_path)
    lines: List[str] = ["# type,x,y,mapX,mapY"]
    for ent in entities:
        t = ent["type"]
        x = ent["x"]
        y = ent["y"]
        if "mapX" in ent and "mapY" in ent:
            lines.append(f"{t},{x},{y},{ent['mapX']},{ent['mapY']}")
        else:
            lines.append(f"{t},{x},{y}")
    file_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def load_entity_json(file_path: Path) -> Tuple[str, List[Dict[str, Any]]]:
    """Parse an entity placement JSON file."""
    file_path = Path(file_path)
    if not file_path.exists():
        raise FileNotFoundError(f"Entity JSON file not found: {file_path}")

    data = json.loads(file_path.read_text(encoding="utf-8"))
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


def sync_entity_definitions(project_root: Path, prefer_csv: bool = True) -> List[str]:
    """Synchronize CSV and JSON entity files in game/defs/."""
    defs_dir = Path(project_root) / "game" / "defs"
    synced: List[str] = []

    if prefer_csv:
        for csv_file in defs_dir.glob("*.entities.csv"):
            base_name = csv_file.name.replace(".entities.csv", "")
            json_file = defs_dir / f"{base_name}.entities.json"
            csv_to_json(csv_file, json_file, level_id=base_name)
            synced.append(f"Synced {csv_file.name} -> {json_file.name}")
    else:
        for json_file in defs_dir.glob("*.entities.json"):
            base_name = json_file.name.replace(".entities.json", "")
            csv_file = defs_dir / f"{base_name}.entities.csv"
            json_to_csv(json_file, csv_file)
            synced.append(f"Synced {json_file.name} -> {csv_file.name}")

    return synced

