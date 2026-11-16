# Neo-Wolf Architecture Specification

## Overview

Neo-Wolf is a modern, modular **2.5D software-rendered raycasting engine** written in C++17 with an SDL2 platform abstraction layer. The architecture is engineered around strict subsystem isolation, deterministic software rasterization, data-driven content definitions, and standalone portability.

```
+-----------------------------------------------------------------------+
|                              App / Main Loop                          |
+-----------------------------------------------------------------------+
       |                                |                       |
       v                                v                       v
+------------------+         +--------------------+   +-----------------+
|   PlatformSDL    |         |       Engine       |   |   AudioSystem   |
| (Events / Window)|         | (Tick Coordinator) |   | (SDL2 Audio)    |
+------------------+         +--------------------+   +-----------------+
       |                                |
       | InputState                     | DeltaTime
       v                                v
+------------------+         +--------------------+   +-----------------+
|  InputSystem     |         |  World Simulation  |   | ParticleManager |
| (Key / Actions)  |         | (Player / Entities)|   | (Sparks / Dust) |
+------------------+         +--------------------+   +-----------------+
                                        |
                                        | World State / Camera
                                        v
                             +--------------------+
                             |  Software Renderer |
                             | (Raycaster / Frame)|
                             +--------------------+
                                        |
                                        | Framebuffer (ARGB32)
                                        v
                             +--------------------+
                             |  Platform Present  |
                             | (SDL Texture Blit) |
                             +--------------------+
```

---

## Core Subsystems

### 1. Engine Core (`engine/core/`)
- **`Engine`**: The central coordinator owning the lifecycle (`Init`, `Update`, `Render`, `Shutdown`).
- **`App`**: Application shell managing timing, target framerate pacing (60 FPS default), and main loop dispatch.
- **`TimeState`**: High-precision frame timing holding `deltaSeconds`, `totalSeconds`, and `frameIndex`.
- **`InputState`**: Digital input action mapping supporting edge triggers (`WasPressed`, `WasReleased`) and continuous state (`IsDown`). `Engine::SetInputInjector` lets tests (and the Phase 7 bot) feed it instead of the keyboard.
- **`GameFlow`** (Phase 5): SDL-free state machine for menus, level progression, death/restart, pause and the end screens. Input snapshot + simulation events in, requests (load level, restart, save progress, quit) out; the engine carries them out ([GAMEPLAY.md](GAMEPLAY.md)).
- **`Progress`** (Phase 5): `progress.json` read/atomic write. **`CampaignConfig`** (`engine/assets/`): the `campaign.json` loader.
- **`MathUtils` & `Vec2`**: Vector operations (`Add`, `Subtract`, `Scale`, `Dot`, `Length`, `Normalize`), distance calculations, angle normalization (`NormalizeAngleRadians`), and scalar clamp utilities.

### 2. Platform Layer (`engine/platform/`)
- **`PlatformSDL`**: Encapsulates all SDL2 subsystem initializations (`SDL_INIT_VIDEO | SDL_INIT_AUDIO`), window creation, SDL texture creation, event polling (`SDL_PollEvent`), keyboard scancode translation, and screen presentation (`SDL_UpdateTexture` + `SDL_RenderCopy`).
- The rest of the engine remains 100% free of SDL headers, enabling headless testing and clean porting.

### 3. World & Simulation (`engine/world/`)
- **`Map`**: 2D grid containing discrete `TileType` cells (`Empty`, `Wall`, `Door`, `Trigger`, `WallAltA`, `WallAltB`, `Exit`), floating-point per-tile `lightLevel` values, and per-cell door locks (`GetDoorLock`, Phase 6: only for drawing; the door entity decides whether it opens).
- **`Player`**: Camera and player state holding floating-point `position` (`Vec2`), `directionAngleRadians`, `moveSpeed`, `turnSpeedRadians`, and collision bounding radius (`radius = 0.28f`).
- **`Entity` & `EntityManager`**: Entity tracking system with stable IDs, entity types (`Player`, `Enemy`, `Pickup`, `Door`, `Trigger`, `Decoration`), state flags (`collected`, `doorOpen`, `triggerActivated`), and spatial coordinates. Enemies carry their archetype (`variant`), stats and AI working state (state timers, last known player position, current path); pickups their kind (`Health`, `Ammo`, `Key`) and key colour; doors their lock (`KeyColor`, `engine/world/KeyColor.h`).
- **`MovementRules`**: Bounding circle vs axis-aligned grid wall collision resolution with sliding velocity vectors along obstacle planes.
- **`InteractionRules`**: Pickups (health, ammo, key cards into the level's `KeyRing`), triggers, doors (locked doors need their key; a door does not close on the player or a living enemy: `IsDoorwayOccupied`), the exit switch (it takes an Interact press before any door does), and the player's hitscan shot (`PerformPlayerAttack`, which alerts and flashes the enemy it hits and leaves a corpse).
- **`Navigation`** (Phase 6): grid queries shared by the AI. `HasLineOfSight` reuses the raycaster's DDA (`Raycaster::CastRay`), so exactly what blocks the view in 3D (walls, the exit switch, closed doors) blocks sight; `HasClearCorridor` checks the centre line and both edges of a moving body. `FindPath` is an 8-connected breadth-first search over walkable cells that never cuts a wall corner; `ComputeSoundDistances` is a 4-connected breadth-first search of path lengths, so sound goes round walls and stops at closed doors. Everything works on `Map::IsWall`, so a door is part of the graph exactly while it is open.
- **`EnemyAi`** (Phase 6): the per-enemy state machine `Idle → Alert → Chase ⇄ Attack`, `Chase → Search → Idle`, `Dead` ([GAMEPLAY.md](GAMEPLAY.md#enemy-ai)). Per frame `UpdateEnemyAi` computes the sound distances once if the player fired (the engine keeps the shot as a `NoiseEvent` until the next AI step), updates each living enemy (perception, state, movement with `MoveWithSliding`, attacks), then `SeparateEnemies` pushes overlapping living enemies apart. Paths are recomputed at most every `kEnemyRepathInterval` (0.3 s) per enemy, or at once when an enemy stops making progress; a straight move is used whenever the body fits along the line. Cost with 20 enemies on a 48×48 map: about 0.5 ms per frame for update and render together on the development Mac (test `ai_stress_20_enemies`).
- **`LevelBuilder`**: turns entity placements into entities and sprites, applies each enemy's archetype from `gameplay.json` (`ApplyEnemyArchetype`), and keeps the sprites in step with the entities every frame (`SyncSpritesWithEntities`: positions, visibility, the corpse texture after death, the hit flash).

### 4. Software Renderer (`engine/render/`)
- **`Framebuffer`**: Directly owns pixel buffer memory (`std::vector<std::uint32_t>` in packed `0xAARRGGBB` format) with fast memory fill operations and bounds-checked pixel writes.
- **`Raycaster`**: Digital Differential Analysis (DDA) raycaster:
  - Iterates across screen horizontal columns (`x = 0` to `width - 1`).
  - Computes camera plane vector orthogonal to player direction.
  - Traces DDA ray steps across grid boundaries until hitting a solid wall or interactive door/trigger.
  - Eliminates fish-eye distortion by projecting distance along player view normal (`perpWallDist = dist * cos(angleDelta)`).
  - Samples vertical texture stripes with floating-point interpolation.
  - Draws a closed door whose cell has a lock (`Map::GetDoorLock`) with the theme's `door_red`, `door_blue` or `door_gold` texture.
  - `CastRay` is also the line-of-sight test of the enemy AI (`Navigation`).
  - Applies distance attenuation and directional wall shading (North/South vs East/West contrast).
  - Casts textured or shaded horizontal scanlines for floor and ceiling planes.
- **`SpriteRenderer`**: Billboard sprite projection (one texture per `SpriteTextureId` slot from `sprites.cfg`; sprites flagged `flash` are drawn brightened):
  - Sorts active sprites from furthest to closest (Painter's Algorithm).
  - Transforms world positions into camera view space.
  - Clips vertical and horizontal bounding boxes against the software depth buffer (`ZBuffer`).
  - Samples texture pixels with alpha color keying (Magenta `#FF00FF` is discarded).
- **`ParticleRenderer`**: Renders camera-facing spark and dust particles into the framebuffer.

### 5. Asset Pipeline (`engine/assets/`)
- **`Texture`**: 2D pixel buffer (`ARGB32`) with nearest-neighbor UV sampling (`SampleNearest(u, v)`).
- **`TextureLoader`**: Netpbm PPM (`P3` ASCII) parser with automatic color normalization and procedural checkerboard fallback generators.
- **`AssetManager`**: Central asset repository storing named textures and sprites with graceful missing-asset fallback.
- **`ContentConfig`**: Config parser for Key-Value `.cfg` files (theme files `themes/<id>.cfg`, `sprites.cfg`, `audio.cfg`) and JSON definitions.

### 6. Audio Subsystem (`engine/audio/`)
- **`AudioSystem`**: SDL audio mixer managing sound effect caching and playback:
  - Loads 16-bit PCM WAV sound effects into memory buffers (`sfx_door_open`, `sfx_pickup`, `sfx_enemy_alert`, `sfx_weapon_fire`).
  - Mixes active audio channels with master volume scaling during SDL audio callback.

### 7. Particle System (`engine/particles/`)
- **`ParticleManager`**: Lightweight pool of simulated particles with lifetime, velocity, drag, color fade, and gravity.
- Generates weapon hit sparks, pickup collection sparkles, door movement sparks, and ambient floating dust.

### 8. UI Subsystem (`engine/ui/`)
- **`UiRenderer`**: The menu, level select, HUD, pause, level-complete, game-over and credits screens, the minimap and the debug overlay, laid out for the 320×240 framebuffer from its width and height (Phase 5; test `ui_layout_bounds`).

---

## Coordinate Systems & Math Model

1. **Grid Space (Integer)**:
   - Origin `(0, 0)` is at the top-left corner of the map.
   - `x` increases horizontally to the right (`0` to `width - 1`).
   - `y` increases vertically downward (`0` to `height - 1`).

2. **World Space (Floating-Point)**:
   - Continuous 2D coordinates `(worldX, worldY)`.
   - Center of grid cell `(x, y)` is `(x + 0.5f, y + 0.5f)`.
   - Angles are measured in radians where `0` points along `+X` (East), `PI/2` points along `+Y` (South), `PI` points along `-X` (West), and `3PI/2` points along `-Y` (North).

3. **Screen / Framebuffer Space**:
   - Software framebuffer resolution defaults to `320x240` or `640x480` scaled up to the presentation window.
   - Column `0` is the left edge of the screen; column `width - 1` is the right edge.
