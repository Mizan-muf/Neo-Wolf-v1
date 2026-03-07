# Neo-Wolf-v1

Neo Wolf is a C++17 SDL2-based 2.5D raycast game engine built with CMake.

## Current Status

- Phase 1 (Project Bootstrap): complete
- Current focus: Phase 2 (Engine Skeleton)

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
cmake -S . -B build
cmake --build build --config Release
```

## Run

```powershell
.\build\neo_wolf.exe
```
