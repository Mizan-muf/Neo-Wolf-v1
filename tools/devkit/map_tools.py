"""Map inspection, visualization, reachability analysis, and generation tools for Neo-Wolf."""

from collections import deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

SOLID_TILES = {"#", "A", "B"}
WALKABLE_TILES = {".", "D", "T"}



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
    """Load a .map file into MapData."""
    file_path = Path(file_path)
    content = file_path.read_text(encoding="utf-8")
    rows: List[List[str]] = []
    for line in content.splitlines():
        line = line.strip()
        if not line or line.startswith(";"):
            continue
        rows.append(list(line))

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
        "empty_floor": counts.get(".", 0),
    }


def analyze_reachability(
    map_path: Path,
    spawn_x: float = 2.5,
    spawn_y: float = 2.5,
    entities: Optional[List[dict]] = None,
) -> Dict[str, Any]:
    """Perform BFS reachability analysis starting from the spawn point."""
    m = load_map(map_path)
    start_ix = int(spawn_x)
    start_iy = int(spawn_y)

    if not m.is_walkable(start_ix, start_iy):
        return {
            "success": False,
            "error": f"Spawn location ({spawn_x:.2f}, {spawn_y:.2f}) cell ({start_ix},{start_iy}) is inside solid tile '{m.get_tile(start_ix, start_iy)}'",
            "reachable_cells": 0,
            "unreachable_cells": [],
            "unreachable_entities": [],
        }

    visited: Set[Tuple[int, int]] = set()
    queue: deque = deque([(start_ix, start_iy)])
    visited.add((start_ix, start_iy))

    directions = [(1, 0), (-1, 0), (0, 1), (0, -1)]

    while queue:
        cx, cy = queue.popleft()
        for dx, dy in directions:
            nx, ny = cx + dx, cy + dy
            if 0 <= nx < m.width and 0 <= ny < m.height:
                if (nx, ny) not in visited and m.is_walkable(nx, ny):
                    visited.add((nx, ny))
                    queue.append((nx, ny))

    all_walkable: Set[Tuple[int, int]] = set()
    for y in range(m.height):
        for x in range(m.width):
            if m.is_walkable(x, y):
                all_walkable.add((x, y))

    unreachable = all_walkable - visited
    unreachable_list = sorted(list(unreachable))

    unreachable_entities = []
    if entities:
        for ent in entities:
            ex, ey = int(ent["x"]), int(ent["y"])
            if (ex, ey) not in visited:
                unreachable_entities.append(ent)

    return {
        "success": len(unreachable) == 0 and len(unreachable_entities) == 0,
        "spawn": (spawn_x, spawn_y),
        "total_walkable": len(all_walkable),
        "reachable_count": len(visited),
        "unreachable_count": len(unreachable),
        "unreachable_cells": unreachable_list,
        "unreachable_entities": unreachable_entities,
    }


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
    C_PLAYER = "\033[32;1m" if color else ""    # Bright Green
    C_ENEMY = "\033[31;1m" if color else ""     # Bright Red
    C_PICKUP = "\033[33m" if color else ""      # Yellow
    C_DECOR = "\033[36;1m" if color else ""     # Bright Cyan
    C_EMPTY = "\033[90m" if color else ""       # Dark Gray

    # Create overlay grid
    overlay = [[m.grid[y][x] for x in range(m.width)] for y in range(m.height)]

    if entity_placements:
        for ent in entity_placements:
            ex = int(ent["x"])
            ey = int(ent["y"])
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

    map_data = MapData(width=width, height=height, grid=grid)
    save_map(map_data, file_path, header_comment=f"Neo-Wolf map template (style: {style})")
    return map_data
