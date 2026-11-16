"""Builds small, complete content roots for the DevKit tests (a multi-level campaign that passes
`validate --strict`); each test then breaks one thing. Not a test module itself."""

import json
import struct
from pathlib import Path
from typing import Dict, List, Optional, Tuple

THEME_KEYS = [
    "wall",
    "wall_alt_a",
    "wall_alt_b",
    "door",
    "door_red",
    "door_blue",
    "door_gold",
    "floor",
    "ceiling",
    "exit",
]
SPRITE_KEYS = [
    "guard",
    "brute",
    "runner",
    "guard_dead",
    "brute_dead",
    "runner_dead",
    "health",
    "ammo",
    "key_red",
    "key_blue",
    "key_gold",
    "decoration",
]

# Three levels that use every rule: a trigger, keys before locks (blue, then red), enemies of
# each type, pickups and a decoration.
GOOD_LEVELS: Dict[str, Tuple[str, List[dict]]] = {
    "alpha": (
        "#######\n#.....#\n#..T..X\n#######\n",
        [
            {"type": "player", "x": 1.5, "y": 1.5, "angle": 0},
            {"type": "trigger", "x": 3.5, "y": 2.5, "mapX": 3, "mapY": 2},
            {"type": "pickup", "x": 4.5, "y": 1.5, "kind": "health"},
            {"type": "enemy", "x": 5.5, "y": 1.5, "variant": "guard"},
        ],
    ),
    "beta": (
        "; keys before locks: blue, then red\n##########\n#..D..D..X\n##########\n",
        [
            {"type": "player", "x": 1.5, "y": 1.5},
            {"type": "door", "x": 3.5, "y": 1.5, "mapX": 3, "mapY": 1, "lock": "blue"},
            {"type": "door", "x": 6.5, "y": 1.5, "mapX": 6, "mapY": 1, "lock": "red"},
            {"type": "pickup", "x": 2.5, "y": 1.5, "kind": "key", "key": "blue"},
            {"type": "pickup", "x": 5.5, "y": 1.5, "kind": "key", "key": "red"},
        ],
    ),
    "gamma": (
        "########\n#......#\n#.####.#\n#......X\n########\n",
        [
            {"type": "player", "x": 1.5, "y": 1.5, "angle": 90},
            {"type": "enemy", "x": 5.5, "y": 1.5, "variant": "brute"},
            {"type": "enemy", "x": 5.5, "y": 3.5, "variant": "runner"},
            {"type": "pickup", "x": 1.5, "y": 3.5, "kind": "ammo", "value": 5},
            {"type": "decoration", "x": 3.5, "y": 3.5},
        ],
    ),
}


def tiny_wav() -> bytes:
    samples = bytes([128] * 64)
    return (
        b"RIFF"
        + struct.pack("<I", 36 + len(samples))
        + b"WAVEfmt "
        + struct.pack("<IHHIIHH", 16, 1, 1, 8000, 8000, 1, 8)
        + b"data"
        + struct.pack("<I", len(samples))
        + samples
    )


def write_entities(path: Path, level_id: str, entities: List[dict]) -> None:
    path.write_text(json.dumps({"level": level_id, "entities": entities}, indent=2) + "\n", encoding="utf-8")


def write_campaign(
    root: Path, level_ids: List[str], title: str = "Fixture campaign", extra: Optional[Dict[str, dict]] = None
) -> None:
    levels = []
    for level_id in level_ids:
        entry = {
            "id": level_id,
            "name": level_id.title(),
            "map": f"game/maps/{level_id}.map",
            "entities": f"game/defs/{level_id}.entities.json",
            "theme": "fx",
        }
        entry.update((extra or {}).get(level_id, {}))
        levels.append(entry)
    (root / "game" / "defs" / "campaign.json").write_text(
        json.dumps({"title": title, "levels": levels}, indent=2) + "\n", encoding="utf-8"
    )


def write_content_root(
    root: Path,
    levels: Optional[Dict[str, Tuple[str, List[dict]]]] = None,
    gameplay: str = '{ "player": { "initialHealth": 80 } }\n',
) -> Path:
    """A content root with the given levels (default GOOD_LEVELS) in the campaign, in order."""
    levels = GOOD_LEVELS if levels is None else levels
    (root / "game" / "maps").mkdir(parents=True, exist_ok=True)
    (root / "game" / "defs" / "themes").mkdir(parents=True, exist_ok=True)
    (root / "game" / "assets").mkdir(parents=True, exist_ok=True)
    (root / "game" / "assets" / "fx.ppm").write_text(
        "P3\n2 2\n255\n9 9 9  200 200 200\n200 200 200  9 9 9\n", encoding="ascii"
    )
    (root / "game" / "assets" / "fx.wav").write_bytes(tiny_wav())
    (root / "game" / "defs" / "themes" / "fx.cfg").write_text(
        "".join(f"{key}=game/assets/fx.ppm\n" for key in THEME_KEYS), encoding="utf-8"
    )
    (root / "game" / "defs" / "sprites.cfg").write_text(
        "".join(f"{key}=game/assets/fx.ppm\n" for key in SPRITE_KEYS), encoding="utf-8"
    )
    (root / "game" / "defs" / "audio.cfg").write_text(
        "master_volume=0.5\n"
        + "".join(f"{key}=game/assets/fx.wav\n" for key in ("door_open", "pickup", "enemy_alert")),
        encoding="utf-8",
    )
    (root / "game" / "defs" / "gameplay.json").write_text(gameplay, encoding="utf-8")
    for level_id, (grid, entities) in levels.items():
        (root / "game" / "maps" / f"{level_id}.map").write_text(grid, encoding="utf-8")
        write_entities(root / "game" / "defs" / f"{level_id}.entities.json", level_id, entities)
    write_campaign(root, list(levels))
    return root
