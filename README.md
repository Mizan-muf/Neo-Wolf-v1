# Neo-Wolf-v1

Neo Wolf is a C++17 SDL2-based 2.5D raycast game engine built with CMake.

## Current Status

- Phase 1 (Project Bootstrap): complete
- Phase 2 (Engine Skeleton): complete
- Phase 3 (Software Framebuffer): complete
- Phase 4 (Basic Math and Utility Layer): complete
- Phase 5 (Input System): complete
- Phase 6 (Grid Map System): complete
- Phase 3-6 test block: complete
- Phase 7 (Player Controller and Collision): complete
- Phase 8 (Raycasting Core): complete
- Phase 9 (Texture Mapping for Walls): complete
- Phase 10 (Floor and Ceiling Rendering): complete
- Phase 11 (Sprite System): complete
- Phase 12 (World Entities and Interaction): complete
- Phase 13 (Asset Loading and File-Based Content): complete
- Phase 14 (Basic UI Layer): complete
- Phase 15 (Basic Lighting): complete
- Current focus: Phase 16 (Basic Particle System)

## Current Architecture

- `src/main.cpp`: thin entry point
- `engine/core/App.*`: app-level main loop host
- `engine/core/Engine.*`: engine lifecycle split by responsibility
  - `Engine.Init.cpp`
  - `Engine.Update.cpp`
  - `Engine.Render.cpp`
  - `Engine.Shutdown.cpp`
  - `Engine.Core.cpp`
- `engine/platform/PlatformSDL.*`: SDL initialization, event pumping, and presentation
- `engine/render/Framebuffer.*`: CPU-side pixel buffer and primitive drawing helpers
- `engine/render/SpriteRenderer.*`: billboard sprite projection, sorting, and occlusion rendering
- `engine/world/Entity.*`: entity manager with IDs and gameplay entity state
- `engine/assets/AssetManager.*`: texture asset registry and lookup
- `engine/assets/ContentConfig.*`: file-based wall/sprite config parsing
- `engine/world/MapLoader.*`, `EntityPlacementLoader.*`: map and entity placement file loading
- `engine/ui/UiRenderer.*`: HUD, debug text, crosshair, and pause placeholder drawing
- `engine/core/InputState.h`: engine-owned action states with edge detection (`IsDown`, `WasPressed`, `WasReleased`)
- `engine/core/TimeState.h`: per-frame timing state
- `engine/core/Vec2.h`, `Vec2i.h`, `MathUtils.h`: basic math layer
- `engine/core/Color.h`: color packing and color helpers
- `engine/core/Log.*`: minimal logging utility

Rendering path right now:
- Engine renders a first-person raycast scene with textured walls
- Engine renders textured floor and ceiling with depth-based darkening
- Engine renders sorted billboard sprites with wall-depth occlusion
- Engine updates world entities (pickup, trigger, door state) each frame
- Engine draws HUD/debug UI overlays and a pause placeholder
- Optional top-down debug overlay can be toggled with `Tab`
- Engine updates player movement/rotation with wall collision against map walls
- Platform uploads framebuffer pixels to an SDL texture every frame
- SDL presents that texture to the window

## Roadmap (High Level)

1. Build core engine loop and module structure
2. Implement software framebuffer and raycasting pipeline
3. Add map, player controller, collision, and sprites
4. Add assets, UI, particles, lighting, and sound
5. Stabilize native engine and add C API boundary
6. Introduce C# gameplay interop in later phases
7. Package first playable vertical slice

## Requirements

- CMake 3.21+
- A C++17 compiler
- SDL2 development libraries

## Build (Windows, example)

```powershell
.\build.cmd Release
```

## Run

```powershell
.\build\neo_wolf.exe
```

Current window title should show: `Raycast Engine - Phase 15`

## Test / Validation

```powershell
.\build.cmd Debug
ctest --test-dir build -C Debug --output-on-failure
```

Smoke test (Phase 15):
```powershell
.\build\neo_wolf.exe
```

- Launch the app and confirm first-person wall rendering is textured
- Move with `W`, `A`, `S`, `D` and rotate with arrow keys
- Confirm turning/movement update first-person scene correctly
- Confirm floor and ceiling are textured and darken toward the horizon
- Confirm enemy, pickup, and decoration sprites are visible in first-person view
- Confirm sprites disappear correctly behind wall geometry
- Press `E` near the door to open/close it, and verify collision/pathing updates
- Walk over the pickup and confirm it gets collected (disappears)
- Edit `game/maps/level01.map` or `game/defs/level01.entities.csv`, rerun, and confirm scene/layout changes
- Confirm HUD shows health/ammo, crosshair is centered, and debug overlay shows FPS/POS/ANG/CELL
- Press `Esc` and confirm pause placeholder appears and gameplay motion stops
- Confirm distant walls/sprites are darker and map zones show slight per-cell brightness variation
- Press `Tab` to toggle top-down debug overlay with ray hits
- Confirm collision still blocks movement into wall tiles
- Confirm different wall sections use different textures
- Window close (`X`) should exit cleanly

Logic test scope (Phase 3-6):
- `Vec2` operations + clamp/angle helpers
- `InputState` transitions (`IsDown`, `WasPressed`, `WasReleased`)
- `Map` queries (`GetCell`, `IsWall`, `IsInsideMap`)
- Player wall-collision resolution rules
- Ray-step hit-distance math
- Sprite distance sorting order
- Entity interactions (pickup, trigger, door open/close state)
