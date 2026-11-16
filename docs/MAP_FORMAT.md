# Map Format (`.map`)

A level's walls are a plain-text grid in `game/maps/levelNN.map`, one character per cell.
Everything that is not a wall (player start, enemies, pickups, doors, the exit, decorations)
is placed in the level's entity file, `game/defs/levelNN.entities.json`
([CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md)).

This page describes what the engine's loader (`engine/world/MapLoader.cpp`) accepts. The
DevKit validator is stricter; its extra rules are listed separately below.

## Example (`game/maps/level01.map`)

```text
################
#..............#
#..............#
#..##A####B##..#
#..............#
#.......A......#
#..............#
#.......D......#
#............T.#
#.......B......#
#..............#
################
```

## Tiles

| Char | Tile | Solid | Drawn as | Notes |
|---|---|---|---|---|
| `.` | Empty | no | floor and ceiling textures | |
| `#` | Wall | yes | `wall` texture (`wall_textures.cfg`) | `#` is a wall, **not** a comment |
| `A` | Wall, variant A | yes | `wall_alt_a` texture | |
| `B` | Wall, variant B | yes | `wall_alt_b` texture | |
| `D` | Door (closed) | yes | `door` texture | Opens only if a `door` entity has this cell as `mapX`/`mapY`; otherwise it stays a wall for good |
| `T` | Exit tile | no | nothing in the 3D view; green on the minimap | Purely a marker: walkable and invisible. The level is won by the `trigger` **entity**, not by the tile |

Any other character, including lower-case letters, spaces and tabs, is an error.

**Doors.** A `door` entity turns its cell `(mapX, mapY)` into a closed door when the level is
built, whatever the map has there (normally `D`). Pressing the interact key (`E`) within 1.05
cells of the door entity's `x`/`y` toggles it **instantly**: the cell becomes empty (open) or a
door again (closed). There is no sliding animation. A door does not close while the player
overlaps its cell. (Doors can still close on enemies: known issue C10.)

**Exit.** A `trigger` entity turns its cell into a `T` tile. Walking within 0.8 cells of the
trigger entity's `x`/`y` wins the level. The exit is invisible in the 3D view (known issue C9,
Roadmap Phase 5). A `T` without a trigger entity does nothing.

## Lines

The loader reads the file line by line:

- A trailing `\r` is removed, so LF and CRLF files load the same (test `crlf_content_loading`).
- An **empty** line is skipped.
- A line whose **first character** is `;` is a comment and is skipped. Only column 1 counts:
  `  ; text` (with leading spaces) is not a comment but a grid row, so the map fails to load.
- Every other line is a row of the grid, **as is**: nothing is trimmed. A whitespace-only line,
  leading or trailing spaces, or tabs make the map invalid (`invalid tile ' '`, or
  `inconsistent row width` when the row's length differs). A UTF-8 byte order mark at the start
  of the file is also invalid.

## Grid rules

- The first row sets the width; every row must have the same length
  (`inconsistent row width at y=N`).
- There must be at least one row (`Map file has no rows`).
- Every character must be one of `. # A B D T` (`invalid tile 'x' at (X,Y)`). A row with the
  wrong length is reported only as a width error; its characters are not checked.
- Any of these errors fails the load, and with it the level (for the first level: the game
  exits with code 1). All errors in the file are reported. Positions in messages are grid
  coordinates (column, row among the grid rows), not file line numbers.

The engine has **no** minimum size and does **not** require a closed outer wall. Outside the
grid, the engine treats every cell as solid wall (`Map::GetCell`, `Map::IsWall`), so rays and
movement cannot leave the map even if an edge is open. There is no maximum size either; the
renderer's minimap is designed for small maps (known issue C9).

## Coordinates

- Cell `(x, y)` is column `x` (0 = leftmost) of grid row `y` (0 = top).
- World positions are in cells: the centre of cell `(x, y)` is `(x + 0.5, y + 0.5)`. The entity
  file uses these world positions for `x`/`y`, and cell coordinates for `mapX`/`mapY`.
- An angle of 0 looks towards +x (right on the map), 90 degrees towards +y (down).

## DevKit rules (stricter than the engine)

`tools/devkit/validator.py` (`validate`, also run by CTest as `content_validation`) reports
these as **errors** even though the engine would load the map:

- the map is smaller than 3×3;
- an outer cell (first or last row or column) is not a solid wall (`#`, `A` or `B`); a `D` or
  `T` on the edge counts as a leak;
- the map has no walkable cell (`.`, `D` or `T`).

The DevKit also handles lines differently: it strips spaces from every line, so it skips
whitespace-only lines and indented `;` comments that the engine rejects (Roadmap Phase 7 aligns
the two).

## DevKit commands

```bash
# Preview the map with the entities on top
python3 tools/devkit/devkit.py inspect-map game/maps/level01.map --entities game/defs/level01.entities.json

# Walkable-cell statistics and reachability from a spawn cell
python3 tools/devkit/devkit.py inspect-map game/maps/level01.map --stats --reachability

# A new map from a template (styles: box, arena, dungeon, crossroads)
python3 tools/devkit/devkit.py create-map game/maps/level02.map --width 16 --height 12 --style arena

# Validate all content (maps, assets, definitions, entity files)
python3 tools/devkit/devkit.py validate --strict
```

`./tools/devkit/neo_devkit` (macOS/Linux) and `tools\devkit\neo_devkit.cmd` (Windows) are
wrappers for `python3 tools/devkit/devkit.py`. See [DEVKIT_GUIDE.md](DEVKIT_GUIDE.md).
