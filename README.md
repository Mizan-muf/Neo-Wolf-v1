# Neo-Wolf-v1

Neo Wolf is a C++17 SDL2-based 2.5D raycast game engine built with CMake.

## Current Status

- Phase 1 (Project Bootstrap): complete
- Phase 2 (Engine Skeleton): mostly complete
- Current focus: finishing Phase 2 cleanup and moving into Phase 3 (Software Framebuffer)

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
- `engine/core/InputState.h`: engine-owned input state
- `engine/core/TimeState.h`: per-frame timing state

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

## Test / Validation

```powershell
.\build.cmd Debug
.\build.cmd Release
```

Smoke test:

```powershell
.\build\neo_wolf.exe
```
