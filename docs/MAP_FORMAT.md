# Map Format (`.map`)

A level's walls are a plain-text grid in `game/maps/levelNN.map`, one character per cell.
Everything that is not a wall (player start, enemies, pickups, doors, the exit, decorations)
is placed in the level's entity file, `game/defs/levelNN.entities.json`
([CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md)). So are dim and dark areas: the map has no
light layer; the entity file's `lights` array sets the light of cell rectangles
([CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md#light-zones-lights)).

This page describes what the engine's loader (`engine/world/MapLoader.cpp`) accepts. The
DevKit validator is stricter; its extra rules are listed separately below.

## Example (`game/maps/level01.map`)

```text
; level01 - Outpost Gate (bunker)
; Tutorial: walk over a pickup, shoot a guard at range, open a door (E), use the exit switch.
####################
#....####..........#
#..............A...#
#....####..........#
#....####..A.......#
#########..........#
#########ADA###BBBBB
#######........B...B
#######............X
#######............B
#######........B...B
########.######BBBBB
####################
```

The first lines are comments (`;` in column 1): the level's id, name and theme, then its idea
([levels/README.md](levels/README.md#design-rules-every-level), rule 1). The door `D` at
(10, 6) is framed by `A` walls, the exit switch `X` at (19, 8) by `B` walls. Level 1 has no
trigger; level 3's checkpoint `T` at (21, 2) is the first ([level03.md](levels/level03.md)).

## Tiles

| Char | Tile | Solid | Drawn as | Notes |
|---|---|---|---|---|
| `.` | Empty | no | floor and ceiling textures | |
| `#` | Wall | yes | `wall` texture of the level's theme (`game/defs/themes/<id>.cfg`) | `#` is a wall, **not** a comment |
| `A` | Wall, variant A | yes | `wall_alt_a` texture | |
| `B` | Wall, variant B | yes | `wall_alt_b` texture | |
| `D` | Door (closed) | yes | `door` texture | Opens only if a `door` entity has this cell as `mapX`/`mapY`; otherwise it stays a wall for good |
| `T` | Trigger tile | no | nothing in the 3D view; dark green on the minimap | A walk-over marker for a `trigger` **entity** (checkpoint message + interop event). It does **not** end the level. Without a trigger entity it does nothing |
| `X` | Exit switch | yes | `exit` texture of the level's theme (an "EXIT" panel with a lever); a white X cross on a green cell on the minimap | Pressing the interact key (`E`) while facing it within reach **completes the level** (see below). Added in Phase 5 |

Any other character, including lower-case letters, spaces and tabs, is an error.

**Doors.** A `door` entity turns its cell `(mapX, mapY)` into a closed door when the level is
built, whatever the map has there (normally `D`). Pressing the interact key (`E`) within 1.05
cells of the door entity's `x`/`y` toggles it **instantly**: the cell becomes empty (open) or a
door again (closed). There is no sliding animation. A door does not close while the player or
a living enemy overlaps its cell (the HUD shows `BLOCKED`; a corpse does not block). A door
entity with a `lock` colour opens only for a player holding that key and is drawn with the
theme's `door_<colour>` texture; the map itself has no separate tile for locked doors (still
`D`). Enemies never open doors: for their paths, sight and hearing a closed door is a wall.

**Exit switch (`X`).** A solid wall cell drawn with the theme's `exit` texture. The player
completes the level by pressing `E` within **1.1 cells** of its face while roughly facing it:
one of five rays fanned up to about 20° either side of the view direction must hit the switch,
and walls stop the rays, so a switch behind the player, beside the player, behind a wall or out
of reach is not activated ([GAMEPLAY.md](GAMEPLAY.md#winning-and-losing-a-level); test
`exit_switch_activation`). The switch needs no entity. Put it in a wall the player can walk up
to; it may be on the outer wall (it counts as solid there). `level01` has one at `(19, 8)`, at
the east end of row 8.

**Triggers (`T`).** A `trigger` entity turns its cell into a `T` tile. Walking within 0.8 cells
of the trigger entity's `x`/`y` shows `CHECKPOINT REACHED` and sends one `TriggerActivated`
interop event. Before Phase 5 this won the level; now only the exit switch does. A `T` without a
trigger entity does nothing.

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
- Every character must be one of `. # A B D T X` (`invalid tile 'x' at (X,Y)`). A row with the
  wrong length is reported only as a width error; its characters are not checked.
- Any of these errors fails the load, and with it the level (for the first level: the game
  exits with code 1). All errors in the file are reported. Positions in messages are grid
  coordinates (column, row among the grid rows), not file line numbers.

The engine has **no** minimum size and does **not** require a closed outer wall. Outside the
grid, `Map::IsWall` treats every cell as solid, so movement cannot leave the map. Rays are
different: a ray that leaves the grid ends without a hit (`Raycaster::CastRay`). Through an open
edge no wall is drawn in that column (only the background floor/ceiling fill, and no sprites),
and shots fly to their full range. Close the outer wall (the DevKit requires it). There is no maximum size either; the
minimap scales small maps to its panel and follows the player on big ones.

## Coordinates

- Cell `(x, y)` is column `x` (0 = leftmost) of grid row `y` (0 = top).
- World positions are in cells: the centre of cell `(x, y)` is `(x + 0.5, y + 0.5)`. The entity
  file uses these world positions for `x`/`y`, and cell coordinates for `mapX`/`mapY`.
- An angle of 0 looks towards +x (right on the map), 90 degrees towards +y (down).

## DevKit rules (stricter than the engine)

`tools/devkit/validator.py` (`validate`, also run by CTest as `content_validation`) reports
these as **errors** even though the engine would load the map:

- the map is smaller than 3×3;
- an outer cell (first or last row or column) is not a solid wall (`#`, `A`, `B` or `X`); a `D`
  or `T` on the edge counts as a leak;
- the map has no walkable cell (`.`, `D` or `T`);
- in a **campaign level** (listed in `campaign.json`): a `D` without exactly one door entity, a
  door or trigger entity that is not on a `D` / `T` cell, the player or anything else inside a
  solid cell, no exit switch `X`, or no exit switch with a floor cell beside it that the player
  can reach from the spawn (a door entity's cell counts as passable, a locked door only once its
  key can be collected first), a lock colour without a key, or a key that cannot be collected
  before the door it opens.

Warnings: a `T` without a trigger entity, an enemy whose body overlaps a wall, a pickup or enemy
that cannot be reached, a map that no campaign level uses. The full list:
[DEVKIT_GUIDE.md](DEVKIT_GUIDE.md#validate--content-and-campaign-validation).

The DevKit reads map lines **exactly like the engine** (since Phase 7, `tools/devkit/engine_formats.py`):
LF and CRLF, `;` comments in column 1 only, whitespace-only lines and indented comments are
invalid rows. The fixtures in `tests/fixtures/parity/` prove that the two agree.

## DevKit commands

```bash
# Preview a campaign level with its entities, statistics and reachability from the spawn
python3 tools/devkit/devkit.py inspect-map --level level01

# A map file with an entity file of your choice
python3 tools/devkit/devkit.py inspect-map game/maps/level01.map --entities game/defs/level01.entities.csv

# A new level from a template (styles: box, arena, dungeon, crossroads): the map with an exit
# switch X in the middle of the east wall, a stub entity file, and an entry in campaign.json
# (use an id that is not in the campaign yet; --force replaces an existing one). With an 11th level,
# CTest content_validation_campaign_size fails until NEOWOLF_REQUIRED_CAMPAIGN_LEVELS is raised
# (LEVEL_AUTHORING.md, step 9).
python3 tools/devkit/devkit.py create-map --level-id level11 --width 16 --height 12 --style arena --register

# Validate all content (maps, assets, definitions, entity files)
python3 tools/devkit/devkit.py validate --strict
```

`./tools/devkit/neo_devkit` (macOS/Linux) and `tools\devkit\neo_devkit.cmd` (Windows) are
wrappers for `python3 tools/devkit/devkit.py`. See [DEVKIT_GUIDE.md](DEVKIT_GUIDE.md).
