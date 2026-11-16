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
- **`Map`**: 2D grid containing discrete `TileType` cells (`Empty`, `Wall`, `Door`, `Trigger`, `WallAltA`, `WallAltB`, `Exit`) and floating-point per-tile `lightLevel` values.
- **`Player`**: Camera and player state holding floating-point `position` (`Vec2`), `directionAngleRadians`, `moveSpeed`, `turnSpeedRadians`, and collision bounding radius (`radius = 0.28f`).
- **`Entity` & `EntityManager`**: Entity tracking system with stable IDs, entity types (`Player`, `Enemy`, `Pickup`, `Door`, `Trigger`, `Decoration`), state flags (`collected`, `doorOpen`, `triggerActivated`), and spatial coordinates.
- **`MovementRules`**: Bounding circle vs axis-aligned grid wall collision resolution with sliding velocity vectors along obstacle planes.
- **`InteractionRules`**: Raycast proximity interaction for doors, triggers, pickups, and enemy alert radius checks.

### 4. Software Renderer (`engine/render/`)
- **`Framebuffer`**: Directly owns pixel buffer memory (`std::vector<std::uint32_t>` in packed `0xAARRGGBB` format) with fast memory fill operations and bounds-checked pixel writes.
- **`Raycaster`**: Digital Differential Analysis (DDA) raycaster:
  - Iterates across screen horizontal columns (`x = 0` to `width - 1`).
  - Computes camera plane vector orthogonal to player direction.
  - Traces DDA ray steps across grid boundaries until hitting a solid wall or interactive door/trigger.
  - Eliminates fish-eye distortion by projecting distance along player view normal (`perpWallDist = dist * cos(angleDelta)`).
  - Samples vertical texture stripes with floating-point interpolation.
  - Applies distance attenuation and directional wall shading (North/South vs East/West contrast).
  - Casts textured or shaded horizontal scanlines for floor and ceiling planes.
- **`SpriteRenderer`**: Billboard sprite projection:
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
