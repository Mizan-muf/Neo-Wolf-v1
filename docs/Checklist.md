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
- [ ] Create particle struct
- [ ] Store:
  - [ ] position
  - [ ] velocity
  - [ ] lifetime
  - [ ] sprite/frame
  - [ ] size
- [ ] Create particle manager
- [ ] Add spawn function
- [ ] Update particles every frame
- [ ] Render particles as simple sprites
- [ ] Add at least:
  - [ ] hit spark
  - [ ] pickup sparkle
  - [ ] ambient dust

### Done When
- [ ] At least one gameplay event spawns visible particles

---

## Phase 17 — Sound Layer

### Goal
Add enough sound to make the engine feel alive.

### Checklist
- [ ] Initialize SDL audio or your chosen simple audio path
- [ ] Load WAV/OGG for effects
- [ ] Add play sound function
- [ ] Add sounds for:
  - [ ] door open
  - [ ] pickup
  - [ ] enemy alert
  - [ ] weapon fire later
- [ ] Add volume config
- [ ] Keep music optional for now

### Done When
- [ ] Gameplay events trigger sound successfully

---

## Phase 18 — Native Game Loop Stabilization

### Goal
Make the engine solid before introducing C#.

### Checklist
- [ ] Add state separation:
  - [ ] menu state
  - [ ] gameplay state
  - [ ] paused state
- [ ] Add level reset
- [ ] Add clean shutdown
- [ ] Remove obvious memory leaks
- [ ] Add safe null checks
- [ ] Add debug assertions
- [ ] Add log file output
- [ ] Test map reload by restarting the level

### Done When
- [ ] Native-only engine feels stable and reusable

### Test Block (Phase 18+)
Reference: [test.md](./test.md)
- [ ] Make all existing tests run in CI/local gate before merge
- [ ] Add integration tests for map reload and state transitions
- [ ] Add regression tests for bugs fixed during stabilization
- [ ] Track and maintain a minimum logic-test coverage target

---

## Phase 19 — C API Boundary

### Goal
Expose native engine functions through a clean C interface.

### Checklist
- [ ] Create `/engine/interop`
- [ ] Add exported C API header
- [ ] Export functions using C ABI
- [ ] Keep interface plain:
  - [ ] ints
  - [ ] floats
  - [ ] enums
  - [ ] simple structs
  - [ ] handles / IDs
- [ ] Do not expose C++ classes
- [ ] Do not expose STL containers
- [ ] Export minimal functions first:
  - [ ] init gameplay bridge
  - [ ] shutdown gameplay bridge
  - [ ] create entity
  - [ ] destroy entity
  - [ ] get/set entity position
  - [ ] read input state
  - [ ] send event/log
- [ ] Version the API

### Done When
- [ ] A separate program could theoretically call your engine functions through the C API

---

## Phase 20 — C# Interop Layer

### Goal
Connect C# gameplay code to native engine.

### Checklist
- [ ] Create separate C# project
- [ ] Add P/Invoke declarations for exported native functions
- [ ] Create managed wrapper classes over raw imports
- [ ] Add engine startup callback from native to managed or managed bootstrap into native
- [ ] Add one test feature in C#:
  - [ ] move one simple gameplay rule into C#
- [ ] Confirm data marshaling works correctly
- [ ] Keep ownership on native side unless absolutely necessary

### Done When
- [ ] C# can successfully call native engine functions
- [ ] One real gameplay behavior runs from C#

---

## Phase 21 — Move Gameplay Rules to C#

### Goal
Make C# the gameplay brain.

### Checklist
- [ ] Move pickup logic to C#
- [ ] Move enemy behavior to C#
- [ ] Move trigger reactions to C#
- [ ] Move weapon state logic to C#
- [ ] Keep rendering, collision, map loading, and engine loop in C++
- [ ] Add gameplay lifecycle methods:
  - [ ] `OnGameStart`
  - [ ] `OnLevelLoaded`
  - [ ] `OnUpdate`
  - [ ] `OnEntitySpawned`
  - [ ] `OnShutdown`

### Done When
- [ ] Core engine is native
- [ ] Game rules are mostly managed

---

## Phase 22 — File-Driven Gameplay Definitions

### Goal
Make content editing easy without recompiling.

### Checklist
- [ ] Create config for enemy definitions
- [ ] Create config for pickup definitions
- [ ] Create config for weapon definitions
- [ ] Create config for trigger definitions
- [ ] Load these from files at startup
- [ ] Connect config data to C# gameplay layer
- [ ] Validate broken configs clearly

### Done When
- [ ] Most game behavior can be tuned through files + C# logic

---

## Phase 23 — Controlled Reload Preparation

### Goal
Prepare architecture for future reload support without implementing deep reload.

### Checklist
- [ ] Separate engine core from gameplay assembly loading logic
- [ ] Separate static engine state from gameplay-owned state
- [ ] Keep asset/data loading modular
- [ ] Add ability to unload and reload content files on full level restart
- [ ] Add gameplay state serialization placeholder
- [ ] Add clear API boundary for future gameplay DLL reload
- [ ] Do not attempt renderer/core live reload

### Done When
- [ ] Project structure is ready for later controlled gameplay/data reload

---

## Phase 24 — First Playable Vertical Slice

### Goal
Produce a tiny but complete game sample.

### Checklist
- [ ] One full level
- [ ] Spawn point
- [ ] Walls and textures
- [ ] At least one door
- [ ] At least one pickup
- [ ] At least one enemy
- [ ] HUD
- [ ] Sound
- [ ] Simple win condition
- [ ] Simple restart flow
- [ ] No crash during normal play session

### Done When
- [ ] Someone can launch the game and complete a small level

---

## Phase 25 — Polish and Cleanup

### Goal
Make the project maintainable.

### Checklist
- [ ] Remove dead code
- [ ] Rename unclear classes/functions
- [ ] Split oversized files
- [ ] Add comments only where useful
- [ ] Add README
- [ ] Add build instructions
- [ ] Add asset folder instructions
- [ ] Add map format documentation
- [ ] Add gameplay API documentation
- [ ] Add debug controls documentation

### Done When
- [ ] A new developer can clone and understand the project

---

## Phase 26 — Packaging

### Goal
Make it runnable outside your dev machine.

### Checklist
- [ ] Build release configuration
- [ ] Copy executable
- [ ] Copy SDL2 runtime
- [ ] Copy native DLLs
- [ ] Copy C# gameplay DLL
- [ ] Copy assets/maps/config files
- [ ] Test on a clean Windows machine/folder
- [ ] Confirm relative asset paths work
- [ ] Confirm game launches without IDE

### Done When
- [ ] Game runs from packaged folder

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
    /interop
    /ui
    /particles
  /gameplay-csharp
  /game
    /assets
    /maps
    /defs
  /docs
  CMakeLists.txt
```

