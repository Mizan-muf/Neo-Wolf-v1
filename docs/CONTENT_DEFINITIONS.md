# Content Definitions

Everything the game loads lives under `game/` in the **content root** (the folder found at
startup; see [TROUBLESHOOTING.md](TROUBLESHOOTING.md)). Editing these files changes the game
without recompiling: the next start (or, for a level, the next level load) picks them up.
Test `edited_data_changes_game` proves this for `gameplay.json` and the entity files.

All paths inside the files are relative to the content root and use `/`.

```text
game/
|-- maps/
|   `-- level01.map               level grid (format: MAP_FORMAT.md)
|-- defs/
|   |-- gameplay.json             player, weapon, enemy and pickup values      (required)
|   |-- level01.entities.json     level 1 entity placements, canonical         (required)
|   |-- level01.entities.csv      the same placements as a CSV export          (optional)
|   |-- wall_textures.cfg         wall, door, floor and ceiling textures
|   |-- sprites.cfg               billboard sprite textures
|   `-- audio.cfg                 sound effects and master volume
`-- assets/                       .ppm textures and .wav sounds (ASSET_GUIDE.md)
```

| File | Read by | When | If it is missing or invalid |
|---|---|---|---|
| `defs/gameplay.json` | `engine/assets/GameplayConfig.cpp` | `Engine::Init`, before the window opens | **Fatal**: error message (and an error box, except with `--frames`), exit code 1 |
| `maps/levelNN.map` | `engine/world/MapLoader.cpp` | every level load | The level load fails. For the first level: fatal, exit code 1 |
| `defs/levelNN.entities.json` | `engine/world/EntityPlacementLoader.cpp` | every level load | The level load fails. For the first level: fatal, exit code 1 |
| `defs/wall_textures.cfg` | `engine/assets/ContentConfig.cpp` | `Engine::Init` | Warning; affected textures are drawn as a checkerboard |
| `defs/sprites.cfg` | `engine/assets/ContentConfig.cpp` | `Engine::Init` | Warning; affected sprites are drawn as a checkerboard |
| `defs/audio.cfg` | `engine/assets/AudioConfig.cpp` | `Engine::Init` | Warning; affected sounds are silent |

`--level N` (1–99) selects `game/maps/levelNN.map` and `game/defs/levelNN.entities.json`
(`engine/core/CommandLine.cpp`). Only level 1 exists so far. A level list (`campaign.json`)
comes with Roadmap Phase 5.

Every loader accepts LF and CRLF line endings. The JSON loaders and the entity CSV loader also
skip a UTF-8 byte order mark.

---

## 1. `gameplay.json`

The numbers that define how the game plays. It is parsed with
[nlohmann/json](../third_party/README.md), section by section, so the same key name in two
sections (`damage`, `moveSpeed`, `health`) gives two independent values.

```json
{
  "player":  { "initialHealth": 100, "maxHealth": 100, "moveSpeed": 3.0, "turnSpeed": 1.8, "collisionRadius": 0.22 },
  "weapon":  { "damage": 35, "fireRate": 0.25, "range": 12.0, "initialAmmo": 24, "maxAmmo": 99 },
  "enemy":   { "health": 50, "damage": 12, "moveSpeed": 1.6, "alertRadius": 6.5, "attackRadius": 1.1, "attackRate": 1.0 },
  "pickups": { "health": { "value": 25 }, "ammo": { "value": 12 } }
}
```

(The shipped file has the same values, one key per line.)

### Keys

Every key and every section is optional. A missing key uses the **default** below; the
defaults equal the shipped values and are defined once, in
`engine/world/GameplayDefinitions.h`. "Integer" means a JSON number without a fraction or
exponent (`35`, not `35.0`); "number" accepts both. Units: distances and speeds are in map
cells (one cell = one map character), turn speed in radians per second, times in seconds.

| Key | Type | Range | Default | Applied in |
|---|---|---|---|---|
| `player.initialHealth` | integer | 1–10000, ≤ `player.maxHealth` | 100 | Health at every level load (`Engine::LoadLevel`) |
| `player.maxHealth` | integer | 1–10000 | 100 | Cap for health pickups and the command bridge's `HealPlayer` (`UpdateEntityInteractions`, `CommandBridge`) |
| `player.moveSpeed` | number | > 0, ≤ 20 (cells/s) | 3.0 | Walking and strafing speed (`Player::moveSpeed`, set by `BuildLevelFromPlacements`) |
| `player.turnSpeed` | number | > 0, ≤ 20 (rad/s) | 1.8 | Turning speed (`Player::turnSpeedRadians`) |
| `player.collisionRadius` | number | > 0, < 0.5 (cells) | 0.22 | Player collision with walls and closed doors (`MoveWithSliding`); a door does not close while the player overlaps its cell |
| `weapon.damage` | integer | 1–10000 | 35 | Health removed from an enemy per hit (`Engine::PlayerAttack` → `PerformPlayerAttack`); shown in "Hit enemy! (-N HP)" |
| `weapon.fireRate` | number | 0–60 (s) | 0.25 | **Seconds between two shots** (a cooldown, not shots per second) |
| `weapon.range` | number | > 0, ≤ 1000 (cells) | 12.0 | Maximum hitscan distance; a wall stops the shot earlier |
| `weapon.initialAmmo` | integer | 0–10000, ≤ `weapon.maxAmmo` | 24 | Ammo at every level load |
| `weapon.maxAmmo` | integer | 0–10000 | 99 | Cap for ammo pickups and the command bridge's `AddAmmo` |
| `enemy.health` | integer | 1–10000 | 50 | Health (and maximum health) of every enemy (`BuildLevelFromPlacements`) |
| `enemy.damage` | integer | 0–10000 | 12 | Health removed from the player per enemy attack (`UpdateEnemyAi`); shown in "UNDER ATTACK! (-N HP)" |
| `enemy.moveSpeed` | number | 0–20 (cells/s) | 1.6 | Chase speed (0 = the enemy never moves) |
| `enemy.alertRadius` | number | 0–1000 (cells) | 6.5 | An idle enemy starts chasing when the player is this close (there is no line-of-sight check yet: known issue C10) |
| `enemy.attackRadius` | number | 0–1000 (cells) | 1.1 | The enemy attacks when the player is this close |
| `enemy.attackRate` | number | > 0, ≤ 60 (s) | 1.0 | **Seconds between two attacks** of one enemy |
| `pickups.health.value` | integer | 1–10000 | 25 | Health added by a `health` pickup that has no `value` of its own; shown in "HEALTH RESTORED (+N HP)" |
| `pickups.ammo.value` | integer | 1–10000 | 12 | Ammo added by an `ammo` pickup that has no `value` of its own; shown in "AMMO REPLENISHED (+N AMMO)" |

The HUD messages show the pickup's value even when the maximum caps the amount actually added.
The values are logged at startup, for example
`Gameplay definitions loaded from .../game/defs/gameplay.json: player health 100/100, move 3.00, ...`.

Changed in Phase 4: `maxPlayerHealth` (under `pickups.health`) became `player.maxHealth`, and
`maxPlayerAmmo` (under `pickups.ammo`) became `weapon.maxAmmo`. The old keys are reported as
unknown keys, with a hint where they moved. Before Phase 4 the game did not read this file at
all (known issue C2).

### Validation

`gameplay.json` must be valid. Loading **fails**, and the game exits with code 1 before any
window opens, when:

- the file cannot be read, or is not valid JSON (the message has the line and column);
- a key appears twice in the same object (`duplicate key "damage" inside "weapon"`);
- the top level or a section is not an object;
- a value has the wrong type or is out of range
  (`weapon.damage must be an integer from 1 to 10000, got "35"`);
- `player.initialHealth` > `player.maxHealth`, or `weapon.initialAmmo` > `weapon.maxAmmo`.

All problems are collected, not only the first. Each message starts with the file path. The
log lists all of them; the error box shows the first three.

**Unknown keys** (a typo, or a key from a newer version) are warnings: they are logged and
ignored, and the game starts. Check the log after editing: for a misspelt key the default is
used.

### Planned (Roadmap Phase 6)

Named enemy archetypes will be added as `"enemies": { "guard": { ... }, "brute": { ... } }`,
each with the same keys as `enemy`, selected by an enemy placement's `variant`. The loader is
structured for this (one reader per enemy section). Today an `enemies` key is reported as
unknown.

---

## 2. Entity placements: `<level>.entities.json`

Where the player starts and where the enemies, pickups, doors, the exit and the decorations
are. The JSON file is **canonical**: it is what the game loads. The map's tiles define the
walls; this file defines everything else.

```json
{
  "level": "level01",
  "entities": [
    { "type": "player", "x": 2.5, "y": 2.5, "angle": 0 },
    { "type": "enemy", "x": 6.5, "y": 2.8 },
    { "type": "pickup", "x": 11.8, "y": 6.6, "kind": "health" },
    { "type": "door", "x": 8.5, "y": 7.5, "mapX": 8, "mapY": 7 },
    { "type": "trigger", "x": 13.5, "y": 8.5, "mapX": 13, "mapY": 8 },
    { "type": "decoration", "x": 4.2, "y": 8.8 }
  ]
}
```

### Top level

| Field | Type | Required | Meaning |
|---|---|---|---|
| `level` | string | no (a non-string is a warning) | The level id, for people and tools. The engine does not use it. |
| `entities` | array of objects | **yes** | The placements, in any order. |

Other top-level fields are warnings.

### Entity fields

`x`/`y` are world coordinates in map cells: `(0, 0)` is the top-left corner of the map, the
centre of cell `(cx, cy)` is `(cx + 0.5, cy + 0.5)`, x grows to the right and y downwards.

| Field | Type | Applies to | Required | Meaning |
|---|---|---|---|---|
| `type` | string | all | **yes** | `player`, `enemy`, `pickup`, `door`, `trigger` or `decoration` |
| `x`, `y` | number | all | **yes** | Position. Must lie inside the map (checked when the level is built). |
| `mapX`, `mapY` | integer ≥ 0 | `door`, `trigger` | **yes**, for those two | The map cell the entity controls; must lie inside the map |
| `angle` | number (degrees) | `player` | no (default 0) | Initial view direction: 0 = +x (right on the map), 90 = +y (down on the map) |
| `kind` | `"health"` or `"ammo"` | `pickup` | **yes**, for pickups | What the pickup restores |
| `value` | integer 1–10000 | `pickup` | no | Amount restored; default `pickups.<kind>.value` from `gameplay.json` |
| `variant` | non-empty string | `enemy` | no | Enemy archetype name. **Reserved for Phase 6**: accepted and stored, but today every enemy uses the `enemy` section of `gameplay.json`. |

A field that does not apply to the entity's type (e.g. `kind` on an enemy), and an unknown
field, are warnings: logged and ignored.

### What each type becomes

`BuildLevelFromPlacements` (`engine/world/LevelBuilder.cpp`) turns the placements into the
level:

| Type | Becomes | Behaviour |
|---|---|---|
| `player` | The camera and an invisible `Player` entity | Exactly **one** per level (none or several is an error). Speeds and radius from `gameplay.json`. |
| `enemy` | `Enemy` entity + enemy sprite | Stats from the `enemy` section of `gameplay.json`. Chases and attacks the player; can be shot. |
| `pickup` | `Pickup` entity + pickup sprite | Collected by walking within 0.6 cells of `x`/`y`. Health and ammo pickups share one sprite until Phase 6. |
| `door` | `Door` entity; the map cell `(mapX, mapY)` becomes a closed door | The interact key (`E`) within 1.05 cells of `x`/`y` opens or closes it instantly. It does not close while the player overlaps the cell. |
| `trigger` | `Trigger` entity (the level exit); the cell `(mapX, mapY)` becomes a `T` tile | Walking within 0.8 cells of `x`/`y` completes the level (Victory). |
| `decoration` | `Decoration` entity + decoration sprite | Nothing: not solid, not collectable, no interaction. Its sprite stays visible. |

Entity ids are assigned in file order, starting at 1. The C API reports the entity type as a
number: `Player` 0, `Enemy` 1, `Pickup` 2, `Door` 3, `Trigger` 4, `Decoration` 5.

### Validation

The level load **fails**, and the current level (if any) stays as it was, when:

- the file is missing or unreadable, or its extension is neither `.json` nor `.csv`;
- the JSON is invalid, a key repeats in one object, or `entities` is missing or not an array;
- an entity is not an object, has no or an unknown `type`, lacks `x`/`y` or has a non-number
  there, is a pickup without a valid `kind`, is a door/trigger without `mapX`/`mapY`, or has a
  value of the wrong type or range;
- there is not exactly one player, or a position or a door/trigger cell lies outside the map.

If the first level fails to load, startup fails (error message, exit code 1). There is **no
built-in fallback layout** any more (it hid known issue C1).

Every message names the file, the entity and the field, for example:

```text
.../level01.entities.json: entities[2] (pickup): field "kind" is required ("health" or "ammo")
.../level01.entities.json: entities[4] (door): map cell (16, 7) is outside the 16x12 map
.../level01.entities.csv:5 (entity 3) (door): fields "mapX" and "mapY" are required (...)
```

### The CSV export: `<level>.entities.csv`

A table in the pre-Phase 4 format, kept for backward compatibility. The engine reads a CSV when
the entity path ends in `.csv` (e.g. through the C API `nw_engine_load_level`), with the same
rules and messages as for JSON. The game itself always loads the `.json` file.

```text
# type,x,y,mapX,mapY,kind,value,angle,variant
player,2.5,2.5,,,,,0
enemy,6.5,2.8
pickup,11.8,6.6,,,health
door,8.5,7.5,8,7
```

- Columns, in order: `type,x,y,mapX,mapY,kind,value,angle,variant`. At least `type,x,y`;
  trailing columns may be left out; an empty column means "not given".
- Lines starting with `#` (after leading spaces) and blank lines are skipped. Columns after
  the ninth are ignored with a warning.
- There is no quoting: values cannot contain commas.

Generate the CSV from the JSON with the DevKit. JSON → CSV is the default direction, and it
overwrites the CSV:

```bash
python3 tools/devkit/devkit.py sync-entities             # every <level>.entities.json -> .csv
python3 tools/devkit/devkit.py sync-entities --from-csv  # the reverse; overwrites the JSON
```

`validate --strict` checks both files with the engine's rules (types, required `kind`,
`mapX`/`mapY` for doors and triggers, value ranges, bounds, exactly one player). Test
`entity_placements_json_csv` checks that the shipped JSON and CSV are equivalent.

---

## 3. `wall_textures.cfg`, `sprites.cfg`, `audio.cfg`

Key/value files: one `key=value` per line; spaces around the key and the value are trimmed;
lines starting with `#` (after leading spaces) and blank lines are skipped. A later duplicate
key replaces an earlier one. Paths are relative to the content root.

A line without `=`, or with an empty key or value, is a syntax error: then **none** of the
file's entries are used (every texture of that file becomes a checkerboard, or every sound is
silent). A missing required key affects only that entry. Problems are logged as warnings and
the game still starts.

### `wall_textures.cfg`

Six required keys: `wall` (`#` tiles), `wall_alt_a` (`A`), `wall_alt_b` (`B`), `door` (door
cells), `floor`, `ceiling`.

```ini
wall=game/assets/wall_main.ppm
wall_alt_a=game/assets/wall_alt_a.ppm
wall_alt_b=game/assets/wall_alt_b.ppm
door=game/assets/wall_alt_b.ppm
floor=game/assets/floor.ppm
ceiling=game/assets/ceiling.ppm
```

### `sprites.cfg`

Three required keys: `enemy`, `pickup` (health and ammo), `decoration`.

```ini
enemy=game/assets/sprite_enemy.ppm
pickup=game/assets/sprite_pickup.ppm
decoration=game/assets/sprite_decor.ppm
```

### `audio.cfg`

Required: `door_open`, `pickup`, `enemy_alert`. Optional: `weapon_fire`, and `master_volume`
(a number, clamped to 0.0–1.0; default 0.85). If a required key is missing or `master_volume`
is not a number, the master volume is not applied, but the sounds that are named still load.
Without an audio device no sound is loaded.

```ini
master_volume=0.85
door_open=game/assets/sfx_door_open.wav
pickup=game/assets/sfx_pickup.wav
enemy_alert=game/assets/sfx_enemy_alert.wav
weapon_fire=game/assets/sfx_weapon_fire.wav
```

---

## Removed in Phase 4

`textures.json`, `sprites.json` and `entities.json` were deleted (known issue C5). No part of
the engine read them. `textures.json` and `sprites.json` duplicated the `.cfg` files above.
`entities.json` was a level registry with its own copy of the player spawn; only the DevKit
used it (`validate` checked that its paths existed, `doctor` listed it as a required file).
Those two checks were removed, and `doctor` now requires `gameplay.json` and
`level01.entities.json` instead. A level list returns as `game/defs/campaign.json` in Roadmap
Phase 5, read by the engine.
