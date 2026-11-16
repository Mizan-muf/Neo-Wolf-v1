"""Map inspection, visualization, reachability analysis, and generation tools for Neo-Wolf."""

import json
import math
from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

from tools.devkit.engine_formats import FLOOR_TILES, SOLID_TILES, WALKABLE_TILES, map_rows_from_bytes

# 'X' is the exit switch (Phase 5): a solid wall cell the player activates with E.



@dataclass
class MapData:
    width: int
    height: int
    grid: List[List[str]] = field(default_factory=list)

    def get_tile(self, x: int, y: int) -> str:
        if 0 <= x < self.width and 0 <= y < self.height:
            return self.grid[y][x]
        return "#"

    def set_tile(self, x: int, y: int, tile: str) -> None:
        if 0 <= x < self.width and 0 <= y < self.height:
            self.grid[y][x] = tile

    def is_solid(self, x: int, y: int) -> bool:
        return self.get_tile(x, y) in SOLID_TILES

    def is_walkable(self, x: int, y: int) -> bool:
        return self.get_tile(x, y) in WALKABLE_TILES


def load_map(file_path: Path) -> MapData:
    """Load a .map file into MapData, reading lines exactly like the engine (engine_formats)."""
    file_path = Path(file_path)
    rows: List[List[str]] = [list(row) for row in map_rows_from_bytes(file_path.read_bytes())]

    if not rows:
        raise ValueError(f"Map {file_path} contains no valid rows.")

    width = len(rows[0])
    height = len(rows)
    return MapData(width=width, height=height, grid=rows)


def save_map(map_data: MapData, file_path: Path, header_comment: Optional[str] = None) -> None:
    """Save MapData to a .map file."""
    file_path = Path(file_path)
    lines: List[str] = []
    if header_comment:
        lines.append(f"; {header_comment}")
    for row in map_data.grid:
        lines.append("".join(row))
    file_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def get_map_stats(map_path: Path) -> Dict[str, Any]:
    """Calculate comprehensive statistics for a map."""
    m = load_map(map_path)
    total_cells = m.width * m.height
    counts: Dict[str, int] = {}
    for y in range(m.height):
        for x in range(m.width):
            t = m.grid[y][x]
            counts[t] = counts.get(t, 0) + 1

    solid_count = sum(counts.get(t, 0) for t in SOLID_TILES)
    walkable_count = sum(counts.get(t, 0) for t in WALKABLE_TILES)

    return {
        "width": m.width,
        "height": m.height,
        "total_cells": total_cells,
        "solid_cells": solid_count,
        "walkable_cells": walkable_count,
        "walkable_percent": round((walkable_count / total_cells) * 100, 1) if total_cells > 0 else 0,
        "walls_standard": counts.get("#", 0),
        "walls_alt_a": counts.get("A", 0),
        "walls_alt_b": counts.get("B", 0),
        "doors": counts.get("D", 0),
        "triggers": counts.get("T", 0),
        "exits": counts.get("X", 0),
        "empty_floor": counts.get(".", 0),
    }


def analyze_reachability(
    map_path: Path,
    spawn_x: float = 2.5,
    spawn_y: float = 2.5,
    entities: Optional[List[dict]] = None,
) -> Dict[str, Any]:
    """Reachability from the spawn as the player experiences it.

    With `entities`, door entities open their cells (a locked door once its key can be reached
    first) and a 'D' without a door entity is a wall; without entities, every 'D' counts as an
    openable door. The result always has the same keys:

    success            spawn on a floor cell, an exit switch usable, no key/lock problem and
                       every pickup and enemy reachable (the level can be completed)
    all_reachable      spawn on a floor cell and every walkable cell reachable (connectivity)
    spawn, spawn_cell  the spawn position and its cell
    error              why the analysis could not start (spawn outside the map or in a wall), or ""
    total_walkable, reachable_count, unreachable_count, unreachable_cells
    exits, usable_exits, key_order, problems, unreachable_entities
    """
    from tools.devkit.validator import analyse_level_progress  # avoids an import cycle

    m = load_map(map_path)
    rows = ["".join(row) for row in m.grid]
    spawn_cell = (int(math.floor(spawn_x)), int(math.floor(spawn_y)))
    result: Dict[str, Any] = {
        "success": False,
        "all_reachable": False,
        "spawn": (spawn_x, spawn_y),
        "spawn_cell": spawn_cell,
        "error": "",
        "total_walkable": 0,
        "reachable_count": 0,
        "unreachable_count": 0,
        "unreachable_cells": [],
        "exits": 0,
        "usable_exits": 0,
        "key_order": [],
        "problems": [],
        "unreachable_entities": [],
    }
    placed = [e for e in (entities or []) if isinstance(e, dict)]
    if entities is None:
        placed = [{"type": "door", "mapX": x, "mapY": y}
                  for y, row in enumerate(rows) for x, c in enumerate(row) if c == "D"]
    door_cells = {(e.get("mapX"), e.get("mapY")) for e in placed if e.get("type") == "door"}
    trigger_cells = {(e.get("mapX"), e.get("mapY")) for e in placed if e.get("type") == "trigger"}

    def standable(x: int, y: int) -> bool:
        if not (0 <= x < m.width and 0 <= y < m.height) or (x, y) in door_cells:
            return False
        return m.get_tile(x, y) in FLOOR_TILES or (x, y) in trigger_cells

    walkable = {(x, y) for y in range(m.height) for x in range(m.width)
                if m.get_tile(x, y) in FLOOR_TILES or (x, y) in door_cells or (x, y) in trigger_cells}
    result["total_walkable"] = len(walkable)
    if not (0 <= spawn_cell[0] < m.width and 0 <= spawn_cell[1] < m.height):
        result["error"] = f"Spawn ({spawn_x:.2f}, {spawn_y:.2f}) is outside the {m.width}x{m.height} map"
        return result
    if not standable(*spawn_cell):
        tile = m.get_tile(*spawn_cell)
        what = "a door cell" if spawn_cell in door_cells or tile == "D" else f"the solid tile '{tile}'"
        result["error"] = f"Spawn ({spawn_x:.2f}, {spawn_y:.2f}) is in cell {spawn_cell}, inside {what}"
        return result

    progress = analyse_level_progress(rows, placed, spawn_x, spawn_y)
    reached = progress.reached
    unreachable = sorted(walkable - reached, key=lambda c: (c[1], c[0]))
    result.update(
        reachable_count=len(reached & walkable),
        unreachable_count=len(unreachable),
        unreachable_cells=unreachable,
        exits=progress.total_exits,
        usable_exits=progress.usable_exits,
        key_order=progress.key_order,
        problems=list(progress.problems),
    )
    for ent in placed:
        if ent.get("type") in ("pickup", "enemy") and isinstance(ent.get("x"), (int, float)) \
                and isinstance(ent.get("y"), (int, float)):
            if (int(math.floor(ent["x"])), int(math.floor(ent["y"]))) not in reached:
                result["unreachable_entities"].append(ent)
    result["success"] = (progress.usable_exits > 0 and not progress.problems
                         and not result["unreachable_entities"])
    result["all_reachable"] = not unreachable
    return result


def render_ascii_preview(
    map_path: Path,
    entity_placements: Optional[List[dict]] = None,
    color: bool = True,
    show_legend: bool = True,
) -> str:
    """Render a colored/formatted ASCII preview of the map and placed entities."""
    m = load_map(map_path)

    # ANSI color codes
    C_RESET = "\033[0m" if color else ""
    C_WALL = "\033[37;1m" if color else ""      # Bright White
    C_WALL_A = "\033[36m" if color else ""      # Cyan
    C_WALL_B = "\033[34m" if color else ""      # Blue
    C_DOOR = "\033[33;1m" if color else ""      # Bright Yellow
    C_TRIGGER = "\033[35;1m" if color else ""   # Bright Magenta
    C_EXIT = "\033[42;30m" if color else ""     # Black on green
    C_PLAYER = "\033[32;1m" if color else ""    # Bright Green
    C_ENEMY = "\033[31;1m" if color else ""     # Bright Red
    C_PICKUP = "\033[33m" if color else ""      # Yellow
    C_DECOR = "\033[36;1m" if color else ""     # Bright Cyan
    C_EMPTY = "\033[90m" if color else ""       # Dark Gray

    # Create overlay grid
    overlay = [[m.grid[y][x] for x in range(m.width)] for y in range(m.height)]

    if entity_placements:
        for ent in entity_placements:
            if not isinstance(ent, dict) or not isinstance(ent.get("x"), (int, float)) \
                    or not isinstance(ent.get("y"), (int, float)):
                continue
            ex = int(math.floor(ent["x"]))
            ey = int(math.floor(ent["y"]))
            if 0 <= ex < m.width and 0 <= ey < m.height:
                etype = ent.get("type")
                if etype == "player":
                    overlay[ey][ex] = "P"
                elif etype == "enemy":
                    overlay[ey][ex] = "E"
                elif etype == "pickup":
                    overlay[ey][ex] = "$"
                elif etype == "decoration":
                    overlay[ey][ex] = "*"
                elif etype == "door":
                    overlay[ey][ex] = "D"
                elif etype == "trigger":
                    overlay[ey][ex] = "T"

    lines: List[str] = []
    header = f"=== Map Preview: {Path(map_path).name} ({m.width}x{m.height}) ==="
    lines.append(header)

    # Column numbers header (every 5)
    col_header = "   " + "".join(str(x % 10) for x in range(m.width))
    lines.append(col_header)

    for y in range(m.height):
        row_str = f"{y:2d} "
        for x in range(m.width):
            c = overlay[y][x]
            if c == "#":
                row_str += f"{C_WALL}#{C_RESET}"
            elif c == "A":
                row_str += f"{C_WALL_A}A{C_RESET}"
            elif c == "B":
                row_str += f"{C_WALL_B}B{C_RESET}"
            elif c == "D":
                row_str += f"{C_DOOR}D{C_RESET}"
            elif c == "T":
                row_str += f"{C_TRIGGER}T{C_RESET}"
            elif c == "X":
                row_str += f"{C_EXIT}X{C_RESET}"
            elif c == "P":
                row_str += f"{C_PLAYER}P{C_RESET}"
            elif c == "E":
                row_str += f"{C_ENEMY}E{C_RESET}"
            elif c == "$":
                row_str += f"{C_PICKUP}${C_RESET}"
            elif c == "*":
                row_str += f"{C_DECOR}*{C_RESET}"
            elif c == ".":
                row_str += f"{C_EMPTY}.{C_RESET}"
            else:
                row_str += c
        lines.append(row_str)

    if show_legend:
        lines.append("-" * len(header))
        legend = [
            f"{C_WALL}#{C_RESET}:Wall",
            f"{C_WALL_A}A{C_RESET}:WallA",
            f"{C_WALL_B}B{C_RESET}:WallB",
            f"{C_DOOR}D{C_RESET}:Door",
            f"{C_TRIGGER}T{C_RESET}:Trigger",
            f"{C_EXIT}X{C_RESET}:Exit",
            f"{C_PLAYER}P{C_RESET}:Player",
            f"{C_ENEMY}E{C_RESET}:Enemy",
            f"{C_PICKUP}${C_RESET}:Pickup",
            f"{C_DECOR}*{C_RESET}:Decor",
        ]
        lines.append("Legend: " + " | ".join(legend))

    return "\n".join(lines)


def create_template_map(
    file_path: Path, width: int = 16, height: int = 12, style: str = "arena"
) -> MapData:
    """Generate a valid template map."""
    if width < 4 or height < 4:
        raise ValueError(f"Dimensions {width}x{height} too small. Minimum is 4x4.")

    grid = [["." for _ in range(width)] for _ in range(height)]

    # Solid perimeter
    for x in range(width):
        grid[0][x] = "#"
        grid[height - 1][x] = "#"
    for y in range(height):
        grid[y][0] = "#"
        grid[y][width - 1] = "#"

    if style == "arena":
        # Four internal pillars and alt walls
        cx, cy = width // 2, height // 2
        if width > 8 and height > 8:
            grid[cy - 2][cx - 3] = "A"
            grid[cy - 2][cx + 3] = "A"
            grid[cy + 2][cx - 3] = "B"
            grid[cy + 2][cx + 3] = "B"
            grid[cy][cx] = "T"
            grid[cy][cx - 1] = "D"
    elif style == "dungeon":
        # Partition wall in center with door
        cx = width // 2
        for y in range(1, height - 1):
            grid[y][cx] = "A"
        grid[height // 2][cx] = "D"
        grid[height - 2][width - 2] = "T"
    elif style == "crossroads":
        # Cross corridors
        cx, cy = width // 2, height // 2
        for y in range(1, height - 1):
            for x in range(1, width - 1):
                if x not in (cx - 1, cx, cx + 1) and y not in (cy - 1, cy, cy + 1):
                    grid[y][x] = "B"
        grid[cy][cx] = "T"

    # Every template gets an exit switch in the east wall (campaign levels need one).
    grid[height // 2][width - 1] = "X"
    if grid[height // 2][width - 2] not in WALKABLE_TILES:
        grid[height // 2][width - 2] = "."

    map_data = MapData(width=width, height=height, grid=grid)
    save_map(map_data, file_path, header_comment=f"Neo-Wolf map template (style: {style})")
    return map_data


def create_level_stub_entities(map_data: MapData, level_id: str) -> Dict[str, Any]:
    """Entity placements that make a template map a valid campaign level: the player on the
    first floor cell (row by row) from which an exit switch can be reached, a door entity on
    every 'D' and a trigger entity on every 'T'."""
    from tools.devkit.validator import analyse_level_progress  # avoids an import cycle

    rows = ["".join(row) for row in map_data.grid]
    doors = [{"type": "door", "x": x + 0.5, "y": y + 0.5, "mapX": x, "mapY": y}
             for y, row in enumerate(rows) for x, c in enumerate(row) if c == "D"]
    triggers = [{"type": "trigger", "x": x + 0.5, "y": y + 0.5, "mapX": x, "mapY": y}
                for y, row in enumerate(rows) for x, c in enumerate(row) if c == "T"]
    player: Optional[Dict[str, Any]] = None
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c != "." or player is not None:
                continue
            progress = analyse_level_progress(rows, doors + triggers, x + 0.5, y + 0.5)
            if progress.usable_exits > 0:
                player = {"type": "player", "x": x + 0.5, "y": y + 0.5, "angle": 0}
    if player is None:
        raise ValueError("the map has no floor cell from which an exit switch can be reached")
    return {"level": level_id, "entities": [player] + doors + triggers}


def save_entities_json(document: Dict[str, Any], file_path: Path) -> None:
    """Writes an entity file in the shipped layout: one placement per line."""
    lines = ["{", f'  "level": {json.dumps(document["level"])},', '  "entities": [']
    entities = document["entities"]
    for index, ent in enumerate(entities):
        body = ", ".join(f"{json.dumps(k)}: {json.dumps(v)}" for k, v in ent.items())
        lines.append(f"    {{ {body} }}" + ("," if index + 1 < len(entities) else ""))
    lines += ["  ]", "}"]
    Path(file_path).write_text("\n".join(lines) + "\n", encoding="utf-8")
