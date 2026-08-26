# Neo Wolf - Complete Implementation Checklist

## Project Stack
- Windows first
- C++ engine
- SDL2 platform layer
- Software rendering
- Grid-based maps
- Config files/maps for v1
- C API boundary
- C# gameplay later
- Sprites only for v1
- Basic UI / particles / lighting
- No deep engine reload

---

## Ground Rule

**Do not jump phases.**  
Only move forward when the current phase is working.

## Test Reference

Use [test.md](./test.md) as the source for when to introduce tests and what to cover at each stage.

---

## Phase 1 — Project Bootstrap

### Goal
Create a runnable native C++ app with SDL2 and a clean folder structure.

### Checklist
- [x] Create project root folder
- [x] Create folders:
  - [x] `/engine`
  - [x] `/engine/core`
  - [x] `/engine/platform`
  - [x] `/engine/render`
  - [x] `/engine/world`
  - [x] `/engine/assets`
  - [x] `/engine/interop`
  - [x] `/engine/ui`
  - [x] `/engine/particles`
  - [x] `/game`
  - [x] `/game/assets`
  - [x] `/game/maps`
  - [x] `/docs`
- [x] Set up CMake project
- [x] Add SDL2 to project
- [x] Build a Windows executable
- [x] Open SDL2 window successfully
- [x] Close window cleanly
- [x] Add main loop
- [x] Print frame time / FPS to console

### Done When
- [x] App opens a window
- [x] App runs loop
- [x] App exits without crash

---

## Phase 2 — Engine Skeleton

### Goal
Build the core engine structure before real rendering starts.

### Checklist
- [x] Create `Engine` class
- [x] Create `App` or `GameHost` class
- [x] Create `PlatformSDL` wrapper
- [x] Create `InputState` struct
- [x] Create `TimeState` struct
- [x] Create `Engine::Init()`
- [x] Create `Engine::Update()`
- [x] Create `Engine::Render()`
- [x] Create `Engine::Shutdown()`
- [x] Separate startup/update/render/shutdown into different files
- [x] Keep SDL calls mostly inside platform layer

### Done When
- [x] Main loop calls engine update and render functions cleanly
- [x] Project is split into modules, not one giant file

---

## Phase 3 — Software Framebuffer

### Goal
Own your own pixels in memory and present them through SDL2.

### Checklist
- [x] Create framebuffer class
- [x] Store:
  - [x] width
  - [x] height
  - [x] pixel buffer
- [x] Add clear color function
- [x] Add `PutPixel(x, y, color)`
- [x] Add `DrawVerticalLine()`
- [x] Add `DrawRect()`
- [x] Create SDL texture for presenting framebuffer
- [x] Upload framebuffer to SDL texture every frame
- [x] Present texture to window

### Done When
- [x] You can fill screen with solid color
- [x] You can draw lines and rectangles
- [x] The image shown is coming from your CPU pixel buffer

---

## Phase 4 — Basic Math and Utility Layer

### Goal
Add only the minimum math needed for raycasting and gameplay.

### Checklist
- [x] Create `Vec2`
- [x] Create `Vec2i`
- [x] Add functions:
  - [x] add
  - [x] subtract
  - [x] scale
  - [x] length
  - [x] normalize
  - [x] dot
- [x] Add angle helpers
- [x] Add clamp helpers
- [x] Add degree/radian conversion
- [x] Add color utility helpers
- [x] Add basic logging utility

### Done When
- [x] Player movement math can be written without messy repeated code

---

## Phase 5 — Input System

### Goal
Convert SDL input into your own engine input system.

### Checklist
- [x] Read keyboard input from SDL
- [x] Store current key states in engine-owned `InputState`
- [x] Add actions:
  - [x] move forward
  - [x] move backward
  - [x] strafe left
  - [x] strafe right
  - [x] turn left
  - [x] turn right
  - [x] interact
  - [x] pause
- [x] Add edge detection:
  - [x] key pressed this frame
  - [x] key released this frame
- [x] Do not expose raw SDL events to the rest of the engine

### Done When
- [x] Engine can query actions cleanly like `IsDown`, `WasPressed`
- [x] Manual smoke validation is possible by running the app and pressing mapped keys (gameplay movement comes in later phases)

---

## Phase 6 — Grid Map System

### Goal
Create the world as a classic 2D grid.

### Checklist
- [x] Create `Map` class
- [x] Store:
  - [x] width
  - [x] height
  - [x] cell data
- [x] Define tile types:
  - [x] empty
  - [x] wall
  - [x] door
  - [x] trigger
- [x] Add function `GetCell(x, y)`
- [x] Add function `IsWall(x, y)`
- [x] Add function `IsInsideMap(x, y)`
- [x] Create hardcoded test map first
- [x] Render top-down debug map using rectangles

### Done When
- [x] You can see a top-down debug version of the level
- [x] Map walls are correctly identified
- [x] Manual runtime validation completed (`neo_wolf.exe` shows the Phase 6 map)

### Test Block (Phases 3-6)
Reference: [test.md](./test.md)
- [x] Add unit test project and test runner to CMake
- [x] Add unit tests for `Vec2`, clamp, and angle helpers
- [x] Add unit tests for `InputState` transitions (`IsDown`, `WasPressed`, `WasReleased`)
- [x] Add unit tests for map queries (`GetCell`, `IsWall`, `IsInsideMap`)
- [x] Run tests without requiring an SDL window

---

## Phase 7 — Player Controller and Collision

### Goal
Move the player inside the map correctly.

### Checklist
- [x] Create `Player` struct/class
- [x] Store:
  - [x] position
  - [x] direction angle
  - [x] move speed
  - [x] turn speed
  - [x] collision radius
- [x] Implement forward/back movement
- [x] Implement left/right rotation
- [x] Implement strafe movement
- [x] Add collision against wall cells
- [x] Prevent player from passing through walls
- [x] Add top-down debug visualization for player

### Done When
- [x] Player moves smoothly
- [x] Player rotates
- [x] Player cannot walk through walls
- [x] Manual runtime validation completed (`neo_wolf.exe` shows movement, rotation, and wall collision in top-down view)

---

## Phase 8 — Raycasting Core

### Goal
Render walls from first-person view.

### Checklist
- [x] Create raycaster module
- [x] For each screen column:
  - [x] compute ray direction
  - [x] step through map grid
  - [x] detect wall hit
  - [x] calculate hit distance
- [x] Correct fish-eye distortion
- [x] Compute projected wall height
- [x] Draw vertical wall slice
- [x] Differentiate horizontal/vertical hits for basic shading
- [x] Add debug toggle to show ray hits in top-down map

### Done When
- [x] First-person wall rendering works
- [x] Turning and moving updates the scene correctly

---

## Phase 9 — Texture Mapping for Walls

### Goal
Replace flat wall colors with textures.

### Checklist
- [x] Create texture loader
- [x] Load image files into CPU-readable pixel arrays
- [x] Assign wall textures by map cell type
- [x] Calculate texture X coordinate from wall hit
- [x] Sample texture vertically while drawing wall columns
- [x] Handle different wall types
- [x] Add missing-texture fallback

### Done When
- [x] Walls are textured correctly
- [x] Texture alignment looks stable while moving

---

## Phase 10 — Floor and Ceiling Rendering

### Goal
Make the scene feel complete.

### Checklist
- [x] Add simple flat-color ceiling
- [x] Add simple flat-color floor
- [x] Then upgrade to textured floor/ceiling if desired
- [x] Add basic depth-based darkening
- [x] Keep it simple and stable before optimizing

### Done When
- [x] Screen is no longer just floating walls on empty background

---

## Phase 11 — Sprite System

### Goal
Render enemies, pickups, props as billboard sprites.

### Checklist
- [x] Create sprite entity type
- [x] Store:
  - [x] world position
  - [x] sprite texture id
  - [x] type
  - [x] active/visible state
- [x] Project sprite into screen space
- [x] Sort sprites by distance
- [x] Use wall depth buffer so hidden sprites do not draw through walls
- [x] Render centered billboard sprite
- [x] Add at least:
  - [x] one enemy sprite
  - [x] one pickup sprite
  - [x] one decoration sprite

### Done When
- [x] Sprites appear in correct positions
- [x] Sprites hide behind walls correctly

---

## Phase 12 — World Entities and Interaction

### Goal
Make the world interactive.

### Checklist
- [x] Create entity manager
- [x] Add entity IDs
- [x] Add entity types:
  - [x] player
  - [x] enemy
  - [x] pickup
  - [x] door
  - [x] trigger
- [x] Add simple update loop for entities
- [x] Add proximity checks
- [x] Add item pickup logic
- [x] Add trigger activation logic
- [x] Add door open/close state
- [x] Add door interaction key

### Done When
- [x] Player can interact with at least one usable object
- [x] Doors and pickups work

### Test Block (Phases 7-12)
Reference: [test.md](./test.md)
- [x] Add tests for player movement and wall collision rules
- [x] Add tests for ray-step/hit-distance math
- [x] Add tests for sprite distance sorting
- [x] Add tests for basic entity interactions (pickup, trigger, door state)
- [x] Keep renderer checks to smoke/integration tests (avoid strict pixel matching)

---

## Phase 13 — Asset Loading and File-Based Content

### Goal
Move from hardcoded data to external files.

### Checklist
- [x] Create asset manager
- [x] Load textures from `/game/assets`
- [x] Load sprite definitions from config files
- [x] Load wall texture assignments from config files
- [x] Load map from text/config file instead of hardcoding
- [x] Load entity placements from file
- [x] Validate files on startup
- [x] Print useful file-loading errors

### Suggested Content Types
- [x] `textures.json`
- [x] `sprites.json`
- [x] `entities.json`
- [x] `level01.map`
- [x] `level01.entities.json`

### Done When
- [x] You can change map/content without recompiling engine

---

## Phase 14 — Basic UI Layer

### Goal
Render HUD and debug UI.

### Checklist
- [x] Add bitmap font or very simple text renderer
- [x] Add health display
- [x] Add ammo display
- [x] Add crosshair
- [x] Add debug overlay:
  - [x] FPS
  - [x] player position
  - [x] player angle
  - [x] current map cell
- [x] Add pause screen placeholder

### Done When
- [x] Game has visible HUD and debug info

---

## Phase 15 — Basic Lighting

### Goal
Add simple retro-style lighting without making renderer too complex.

### Checklist
- [x] Add distance-based shading on walls
- [x] Add distance-based shading on sprites
- [x] Add per-cell light level support in map
- [x] Multiply final sampled color by light factor
- [x] Clamp light values safely
- [x] Keep only grayscale or scalar darkening for v1

### Done When
- [x] Distant surfaces are darker
- [x] Scene has more depth and atmosphere

---

## Phase 16 — Basic Particle System

### Goal
Add lightweight particles for impact and atmosphere.

### Checklist
- [x] Create particle struct
- [x] Store:
  - [x] position
  - [x] velocity
  - [x] lifetime
  - [x] sprite/frame
  - [x] size
- [x] Create particle manager
- [x] Add spawn function
- [x] Update particles every frame
- [x] Render particles as simple sprites
- [x] Add at least:
  - [x] hit spark
  - [x] pickup sparkle
  - [x] ambient dust

### Done When
- [x] At least one gameplay event spawns visible particles

---

## Phase 17 — Sound Layer

### Goal
Add enough sound to make the engine feel alive.

### Checklist
- [x] Initialize SDL audio or your chosen simple audio path
- [x] Load WAV/OGG for effects
- [x] Add play sound function
- [x] Add sounds for:
  - [x] door open
  - [x] pickup
  - [x] enemy alert
  - [x] weapon fire later
- [x] Add volume config
- [x] Keep music optional for now

### Done When
- [x] Gameplay events trigger sound successfully

---

## Phase 18 — Native Game Loop Stabilization

### Goal
Make the engine solid before introducing C#.

### Checklist
- [x] Add state separation:
  - [x] menu state
  - [x] gameplay state
  - [x] paused state
- [x] Add level reset
- [x] Add clean shutdown
- [x] Remove obvious memory leaks
- [x] Add safe null checks
- [x] Add debug assertions
- [x] Add log file output
- [x] Test map reload by restarting the level

### Done When
- [x] Native-only engine feels stable and reusable

### Test Block (Phase 18+)
Reference: [test.md](./test.md)
- [x] Make all existing tests run in CI/local gate before merge
- [x] Add integration tests for map reload and state transitions
- [x] Add regression tests for bugs fixed during stabilization
- [x] Track and maintain a minimum logic-test coverage target

---

## Phase 19 — C API Boundary

### Goal
Expose native engine functions through a clean C interface.

### Checklist
- [x] Create `/engine/interop`
- [x] Add exported C API header
- [x] Export functions using C ABI
- [x] Keep interface plain:
  - [x] ints
  - [x] floats
  - [x] enums
  - [x] simple structs
  - [x] handles / IDs
- [x] Do not expose C++ classes
- [x] Do not expose STL containers
- [x] Export minimal functions first:
  - [x] init gameplay bridge
  - [x] shutdown gameplay bridge
  - [x] create entity
  - [x] destroy entity
  - [x] get/set entity position
  - [x] read input state
  - [x] send event/log
- [x] Version the API

### Done When
- [x] A separate program could theoretically call your engine functions through the C API

---

## Phase 20 — C# Interop Layer / Command Bridge

### Goal
Connect C# gameplay code to native engine via bidirectional command & event bridge.

### Checklist
- [x] Create separate interop bridge module (`engine/interop/CommandBridge.h`, `engine/interop/EngineInterop.h`)
- [x] Add C ABI declarations for exported native functions
- [x] Create wrapper functions over raw imports and command buffer queue
- [x] Add engine startup callback from native to managed or managed bootstrap into native
- [x] Add test feature for gameplay rules over bridge:
  - [x] command queue & event polling for combat, healing, state changes
- [x] Confirm data marshaling works correctly with POD types
- [x] Keep ownership on native side unless absolutely necessary

### Done When
- [x] External callers can successfully call native engine functions
- [x] Real gameplay behavior runs across the bridge

---

## Phase 21 — Move Gameplay Rules to Scripting / State Layer

### Goal
Make gameplay rules modular, decoupled, and event-driven.

### Checklist
- [x] Move pickup logic to gameplay interaction layer
- [x] Move enemy behavior to gameplay interaction layer
- [x] Move trigger reactions to gameplay interaction layer
- [x] Move weapon state logic to gameplay interaction layer
- [x] Keep rendering, collision, map loading, and engine loop in C++
- [x] Add gameplay lifecycle methods:
  - [x] `OnGameStart`
  - [x] `OnLevelLoaded`
  - [x] `OnUpdate`
  - [x] `OnEntitySpawned`
  - [x] `OnShutdown`

### Done When
- [x] Core engine is native
- [x] Game rules are cleanly separated into gameplay layer

---

## Phase 22 — File-Driven Gameplay Definitions

### Goal
Make content editing easy without recompiling.

### Checklist
- [x] Create config for enemy definitions (`game/defs/gameplay.json`)
- [x] Create config for pickup definitions
- [x] Create config for weapon definitions
- [x] Create config for trigger definitions
- [x] Load these from files at startup
- [x] Connect config data to gameplay layer
- [x] Validate broken configs clearly

### Done When
- [x] Most game behavior can be tuned through files + config logic

---

## Phase 23 — Controlled Reload Preparation

### Goal
Prepare architecture for future reload support without implementing deep reload.

### Checklist
- [x] Separate engine core from gameplay assembly loading logic
- [x] Separate static engine state from gameplay-owned state
- [x] Keep asset/data loading modular
- [x] Add ability to unload and reload content files on full level restart
- [x] Add gameplay state serialization placeholder
- [x] Add clear API boundary for future gameplay DLL reload
- [x] Do not attempt renderer/core live reload

### Done When
- [x] Project structure is ready for later controlled gameplay/data reload

---

## Phase 24 — First Playable Vertical Slice

### Goal
Produce a tiny but complete game sample.

### Checklist
- [x] One full level
- [x] Spawn point
- [x] Walls and textures
- [x] At least one door
- [x] At least one pickup
- [x] At least one enemy
- [x] HUD
- [x] Sound
- [x] Simple win condition
- [x] Simple restart flow
- [x] No crash during normal play session

### Done When
- [x] Someone can launch the game and complete a small level

---

## Phase 25 — Polish and Cleanup

### Goal
Make the project maintainable.

### Checklist
- [x] Remove dead code
- [x] Rename unclear classes/functions
- [x] Split oversized files
- [x] Add comments only where useful
- [x] Add README
- [x] Add build instructions
- [x] Add asset folder instructions
- [x] Add map format documentation
- [x] Add gameplay API documentation
- [x] Add debug controls documentation

### Done When
- [x] A new developer can clone and understand the project

---

## Phase 26 — Packaging

### Goal
Make it runnable outside your dev machine.

### Checklist
- [x] Build release configuration
- [x] Copy executable
- [x] Copy SDL2 runtime
- [x] Copy native DLLs / binaries
- [x] Copy assets/maps/config files
- [x] Test on a clean standalone package directory
- [x] Confirm relative asset paths work
- [x] Confirm game launches without IDE

### Done When
- [x] Game runs from packaged folder

---

## DevKit CLI Toolchain & Test Automation

### Goal
Provide professional CLI developer tools and automated verification for all content, maps, and assets.

### Checklist
- [x] DevKit CLI toolchain in `tools/devkit/` (`neo_devkit`, `devkit.py`)
- [x] Map inspector with terminal ASCII rendering and BFS reachability analysis (`inspect-map`)
- [x] Asset metadata inspector for PPM textures and WAV audio (`inspect-asset`)
- [x] Procedural map and asset generators (`create-map`, `create-asset`)
- [x] Definition synchronizer for entity CSV and JSON (`sync-entities`)
- [x] Environment and repository health doctor (`doctor`)
- [x] Content and asset integrity validator (`validate`)
- [x] Automated release packager with checksum generation (`package`)
- [x] DevKit unit test suite (`neo_devkit test`)
- [x] Native C++ map/asset validation test suite in `neo_wolf_tests`

---

## Minimum Folder Outcome

```text
/neo-wolf
  /engine
    /core
    /platform
    /render
    /world
    /assets
    /audio
    /ui
    /particles
  /game
    /assets
    /maps
    /defs
  /docs
  /tests
  /tools
    /devkit
  CMakeLists.txt
  Makefile
```



