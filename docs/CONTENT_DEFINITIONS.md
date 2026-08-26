# Neo-Wolf Content Definitions & Configuration Guide

## Overview

Neo-Wolf relies on a clean, data-driven content pipeline located in `game/defs/`. This allows designers to add maps, bind textures, reconfigure audio, and place entities without touching engine source code or recompiling.

```text
game/defs/
|-- wall_textures.cfg       # Key-Value texture mapping for walls/floors
|-- textures.json           # JSON mirror of texture mappings
|-- sprites.cfg             # Key-Value sprite slot mapping
|-- sprites.json            # JSON mirror of sprite mappings
|-- audio.cfg               # Key-Value audio effects and volume config
|-- entities.json           # Global levels manifest and player spawn registry
|-- level01.entities.csv    # Level 01 entity placement table (CSV format)
`-- level01.entities.json   # Level 01 entity placement table (JSON format)
```

---

## 1. Texture Definitions (`wall_textures.cfg` / `textures.json`)

Binds internal texture IDs to relative file paths on disk:

### `wall_textures.cfg` (CFG Format)
```ini
# Relative paths from repository root
wall=game/assets/wall_main.ppm
wall_alt_a=game/assets/wall_alt_a.ppm
wall_alt_b=game/assets/wall_alt_b.ppm
door=game/assets/wall_alt_b.ppm
floor=game/assets/floor.ppm
ceiling=game/assets/ceiling.ppm
```

### `textures.json` (JSON Format)
```json
{
  "wall": "game/assets/wall_main.ppm",
  "wall_alt_a": "game/assets/wall_alt_a.ppm",
  "wall_alt_b": "game/assets/wall_alt_b.ppm",
  "door": "game/assets/wall_alt_b.ppm",
  "floor": "game/assets/floor.ppm",
  "ceiling": "game/assets/ceiling.ppm"
}
```

---

## 2. Sprite Definitions (`sprites.cfg` / `sprites.json`)

Binds billboard sprite categories to texture files:

### `sprites.cfg`
```ini
enemy=game/assets/sprite_enemy.ppm
pickup=game/assets/sprite_pickup.ppm
decoration=game/assets/sprite_decor.ppm
```

### `sprites.json`
```json
{
  "enemy": "game/assets/sprite_enemy.ppm",
  "pickup": "game/assets/sprite_pickup.ppm",
  "decoration": "game/assets/sprite_decor.ppm"
}
```

---

## 3. Audio Definitions (`audio.cfg`)

Configures global volume and sound effect file associations:

```ini
# Master volume scalar (0.0 to 1.0)
master_volume=0.85

# Sound effect slots
door_open=game/assets/sfx_door_open.wav
pickup=game/assets/sfx_pickup.wav
enemy_alert=game/assets/sfx_enemy_alert.wav
weapon_fire=game/assets/sfx_weapon_fire.wav
```

---

## 4. Entity Manifest & Placements

### `entities.json` (Global Level Registry)
```json
{
  "version": 1,
  "levels": [
    {
      "id": "level01",
      "map": "game/maps/level01.map",
      "entities": "game/defs/level01.entities.json",
      "player_spawn": {
        "x": 2.5,
        "y": 2.5
      }
    }
  ]
}
```

### Level Entity Placements (`<level>.entities.csv`)
CSV header format: `# type,x,y,mapX,mapY`

```csv
# type,x,y,mapX,mapY
player,2.5,2.5
enemy,6.5,2.8
pickup,11.8,6.6
door,8.5,7.5,8,7
trigger,13.5,8.5,13,8
decoration,4.2,8.8
```

### Entity Types Reference

| Type | Visual Representation | Behavior |
| :--- | :--- | :--- |
| `player` | None (Camera) | Player initial spawn position and orientation |
| `enemy` | `sprite_enemy.ppm` | Hostile agent with distance detection and alert sound |
| `pickup` | `sprite_pickup.ppm` | Collectible item that disappears on contact and spawns sparkle particles |
| `door` | `wall_alt_b.ppm` | Interactive door tied to grid cell `(mapX, mapY)` |
| `trigger` | `wall_main.ppm` | Objective terminal/switch tied to grid cell `(mapX, mapY)` |
| `decoration` | `sprite_decor.ppm` | Static world decoration billboard |

---

## Synchronizing Definitions with DevKit CLI

To ensure both CSV and JSON definitions remain in sync:

```bash
# Sync all entity definitions in game/defs/
./tools/devkit/neo_devkit sync-entities
```
