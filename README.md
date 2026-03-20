# Neo-Wolf-v1

Neo Wolf is a C++17 SDL2-based 2.5D raycast game engine built with CMake.

## Current Status

- Phase 1 (Project Bootstrap): complete
- Phase 2 (Engine Skeleton): complete
- Phase 3 (Software Framebuffer): complete
- Phase 4 (Basic Math and Utility Layer): complete
- Phase 5 (Input System): complete
- Current focus: Phase 6 (Grid Map System) and Phase 3-6 test setup

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
- Engine clears and draws primitives into a CPU framebuffer
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

Current window title should show: `Raycast Engine - Phase 5`

## Test / Validation

```powershell
.\build.cmd Debug
.\build.cmd Release
```

Smoke test:

```powershell
.\build\neo_wolf.exe
```

Input smoke test (no debugger required):
- Focus the game window and press `W`, `A`, `S`, `D`, arrow keys, `E`, `Esc`
- The app should remain responsive and process events
- Window close (`X`) should exit cleanly
- Visible movement is expected in later phases, not Phase 5
