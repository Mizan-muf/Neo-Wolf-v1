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
- Current focus: Phase 8 (Raycasting Core)

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
- `engine/core/InputState.h`: engine-owned action states with edge detection (`IsDown`, `WasPressed`, `WasReleased`)
- `engine/core/TimeState.h`: per-frame timing state
- `engine/core/Vec2.h`, `Vec2i.h`, `MathUtils.h`: basic math layer
- `engine/core/Color.h`: color packing and color helpers
- `engine/core/Log.*`: minimal logging utility

Rendering path right now:
- Engine clears and draws a top-down grid map into a CPU framebuffer
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

Current window title should show: `Raycast Engine - Phase 7`

## Test / Validation

```powershell
.\build.cmd Debug
ctest --test-dir build -C Debug --output-on-failure
```

Smoke test (Phase 7):
```powershell
.\build\neo_wolf.exe
```

- Launch the app and confirm the top-down map is visible with a player marker + facing line
- Move with `W`, `A`, `S`, `D` and rotate with arrow keys
- Confirm collision blocks movement into wall tiles
- Window close (`X`) should exit cleanly

Logic test scope (Phase 3-6):
- `Vec2` operations + clamp/angle helpers
- `InputState` transitions (`IsDown`, `WasPressed`, `WasReleased`)
- `Map` queries (`GetCell`, `IsWall`, `IsInsideMap`)
