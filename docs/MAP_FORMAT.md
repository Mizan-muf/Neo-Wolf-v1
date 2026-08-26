# Neo-Wolf Map Format Specification

## File Overview

Neo-Wolf level maps are stored in plain text ASCII files with the `.map` extension under the `game/maps/` directory (e.g. `game/maps/level01.map`).

The format is human-readable, easily editable in any text editor, and fully verifiable using the **DevKit CLI** (`tools/devkit/`).

---

## Format Syntax

A `.map` file consists of a grid of single-character tokens representing cells in the 2D world.

### Comment Syntax
- Lines starting with `;` or `#` (when formatted as whole-line comments) are ignored during loading.
- Blank or whitespace-only lines are ignored.

### Grid Rules
1. **Rectangularity**: Every active line must contain exactly the same number of characters. Inconsistent line lengths will fail validation and engine loading.
2. **Perimeter Enclosure**: The outer boundary of the map (top row, bottom row, left column, right column) **must** consist exclusively of solid wall characters (`#`, `A`, `B`). This prevents raycasting rays and player physics from leaking into undefined memory.
3. **Minimum Dimensions**: Maps must be at least `3x3` cells in size (standard playable maps are typically `16x12` to `64x64`).

---

## Tile Character Reference

| Character | Name | Type | Description | Default Texture |
| :--- | :--- | :--- | :--- | :--- |
| `.` | Empty Floor | Walkable | Open floor space where player and entities can move. | `floor.ppm` / `ceiling.ppm` |
| `#` | Solid Wall | Impassable | Standard opaque stone wall. Blocks movement and vision. | `wall_main.ppm` |
| `A` | Wall Alt A | Impassable | Alternative wall variant A (e.g., banner/tech wall). | `wall_alt_a.ppm` |
| `B` | Wall Alt B | Impassable | Alternative wall variant B (e.g., rusted/brick wall). | `wall_alt_b.ppm` |
| `D` | Door | Interactive | Interactive sliding door. Starts closed, opens on interaction. | `wall_alt_b.ppm` / `sfx_door_open.wav` |
| `T` | Trigger | Interactive | Interactive terminal or switch tile. Activates objectives. | `wall_main.ppm` |

---

## Example Level Map (`level01.map`)

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

---

## Coordinate Mapping

- Cell `(0, 0)` is located at the **top-left** corner (Row 0, Column 0).
- Row index `y` maps to vertical axis `(0 <= y < height)`.
- Column index `x` maps to horizontal axis `(0 <= x < width)`.
- The floating-point world coordinates for the center of cell `(x, y)` is `(x + 0.5, y + 0.5)`.

---

## Entity Placements (`.entities.csv` / `.entities.json`)

While structural tiles are defined in the `.map` file, dynamic objects, enemies, pickups, and player spawn points are defined in matching entity placement files:

```csv
# type,x,y,mapX,mapY
player,2.5,2.5
enemy,6.5,2.8
pickup,11.8,6.6
door,8.5,7.5,8,7
trigger,13.5,8.5,13,8
decoration,4.2,8.8
```

For `door` and `trigger` entities, the optional `mapX` and `mapY` fields bind the entity logic directly to the corresponding map grid coordinate.

---

## Map Authoring with DevKit CLI

The DevKit CLI toolchain provides dedicated commands for creating, inspecting, and validating maps:

### 1. Preview Map in Terminal
```bash
# Preview map with color rendering and entity overlay
./tools/devkit/neo_devkit inspect-map game/maps/level01.map --entities game/defs/level01.entities.csv
```

### 2. Check Map Reachability & Statistics
```bash
# Calculate walkable percentage and verify all items are reachable from spawn
./tools/devkit/neo_devkit inspect-map game/maps/level01.map --stats --reachability
```

### 3. Generate a New Starter Map
```bash
# Generate a template arena map (16x12)
./tools/devkit/neo_devkit create-map game/maps/level02.map --width 16 --height 12 --style arena

# Available styles: arena, dungeon, crossroads, pillars, box
```

### 4. Validate All Maps
```bash
./tools/devkit/neo_devkit validate --strict
```
