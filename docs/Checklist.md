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
- [ ] Create `Map` class
- [ ] Store:
  - [ ] width
  - [ ] height
  - [ ] cell data
- [ ] Define tile types:
  - [ ] empty
  - [ ] wall
  - [ ] door
  - [ ] trigger
- [ ] Add function `GetCell(x, y)`
- [ ] Add function `IsWall(x, y)`
- [ ] Add function `IsInsideMap(x, y)`
- [ ] Create hardcoded test map first
- [ ] Render top-down debug map using rectangles

### Done When
- [ ] You can see a top-down debug version of the level
- [ ] Map walls are correctly identified

### Test Block (Phases 3-6)
Reference: [test.md](./test.md)
- [ ] Add unit test project and test runner to CMake
- [ ] Add unit tests for `Vec2`, clamp, and angle helpers
- [ ] Add unit tests for `InputState` transitions (`IsDown`, `WasPressed`, `WasReleased`)
- [ ] Add unit tests for map queries (`GetCell`, `IsWall`, `IsInsideMap`)
- [ ] Run tests without requiring an SDL window

---

## Phase 7 — Player Controller and Collision

### Goal
Move the player inside the map correctly.

### Checklist
- [ ] Create `Player` struct/class
- [ ] Store:
  - [ ] position
  - [ ] direction angle
  - [ ] move speed
  - [ ] turn speed
  - [ ] collision radius
- [ ] Implement forward/back movement
- [ ] Implement left/right rotation
- [ ] Implement strafe movement
- [ ] Add collision against wall cells
- [ ] Prevent player from passing through walls
- [ ] Add top-down debug visualization for player

### Done When
- [ ] Player moves smoothly
- [ ] Player rotates
- [ ] Player cannot walk through walls

---

## Phase 8 — Raycasting Core

### Goal
Render walls from first-person view.

### Checklist
- [ ] Create raycaster module
- [ ] For each screen column:
  - [ ] compute ray direction
  - [ ] step through map grid
  - [ ] detect wall hit
  - [ ] calculate hit distance
- [ ] Correct fish-eye distortion
- [ ] Compute projected wall height
- [ ] Draw vertical wall slice
- [ ] Differentiate horizontal/vertical hits for basic shading
- [ ] Add debug toggle to show ray hits in top-down map

### Done When
- [ ] First-person wall rendering works
- [ ] Turning and moving updates the scene correctly

---

## Phase 9 — Texture Mapping for Walls

### Goal
Replace flat wall colors with textures.

### Checklist
- [ ] Create texture loader
- [ ] Load image files into CPU-readable pixel arrays
- [ ] Assign wall textures by map cell type
- [ ] Calculate texture X coordinate from wall hit
- [ ] Sample texture vertically while drawing wall columns
- [ ] Handle different wall types
- [ ] Add missing-texture fallback

### Done When
- [ ] Walls are textured correctly
- [ ] Texture alignment looks stable while moving

---

## Phase 10 — Floor and Ceiling Rendering

### Goal
Make the scene feel complete.

### Checklist
- [ ] Add simple flat-color ceiling
- [ ] Add simple flat-color floor
- [ ] Then upgrade to textured floor/ceiling if desired
- [ ] Add basic depth-based darkening
- [ ] Keep it simple and stable before optimizing

### Done When
- [ ] Screen is no longer just floating walls on empty background

---

## Phase 11 — Sprite System

### Goal
Render enemies, pickups, props as billboard sprites.

### Checklist
- [ ] Create sprite entity type
- [ ] Store:
  - [ ] world position
  - [ ] sprite texture id
  - [ ] type
  - [ ] active/visible state
- [ ] Project sprite into screen space
- [ ] Sort sprites by distance
- [ ] Use wall depth buffer so hidden sprites do not draw through walls
- [ ] Render centered billboard sprite
- [ ] Add at least:
  - [ ] one enemy sprite
  - [ ] one pickup sprite
  - [ ] one decoration sprite

### Done When
- [ ] Sprites appear in correct positions
- [ ] Sprites hide behind walls correctly

---

## Phase 12 — World Entities and Interaction

### Goal
Make the world interactive.

### Checklist
- [ ] Create entity manager
- [ ] Add entity IDs
- [ ] Add entity types:
  - [ ] player
  - [ ] enemy
  - [ ] pickup
  - [ ] door
  - [ ] trigger
- [ ] Add simple update loop for entities
- [ ] Add proximity checks
- [ ] Add item pickup logic
- [ ] Add trigger activation logic
- [ ] Add door open/close state
- [ ] Add door interaction key

### Done When
- [ ] Player can interact with at least one usable object
- [ ] Doors and pickups work

### Test Block (Phases 7-12)
Reference: [test.md](./test.md)
- [ ] Add tests for player movement and wall collision rules
- [ ] Add tests for ray-step/hit-distance math
- [ ] Add tests for sprite distance sorting
- [ ] Add tests for basic entity interactions (pickup, trigger, door state)
- [ ] Keep renderer checks to smoke/integration tests (avoid strict pixel matching)

---

## Phase 13 — Asset Loading and File-Based Content

### Goal
Move from hardcoded data to external files.

### Checklist
- [ ] Create asset manager
- [ ] Load textures from `/game/assets`
- [ ] Load sprite definitions from config files
- [ ] Load wall texture assignments from config files
- [ ] Load map from text/config file instead of hardcoding
- [ ] Load entity placements from file
- [ ] Validate files on startup
- [ ] Print useful file-loading errors

### Suggested Content Types
- [ ] `textures.json`
- [ ] `sprites.json`
- [ ] `entities.json`
- [ ] `level01.map`
- [ ] `level01.entities.json`

### Done When
- [ ] You can change map/content without recompiling engine

---

## Phase 14 — Basic UI Layer

### Goal
Render HUD and debug UI.

### Checklist
- [ ] Add bitmap font or very simple text renderer
- [ ] Add health display
- [ ] Add ammo display
- [ ] Add crosshair
- [ ] Add debug overlay:
  - [ ] FPS
  - [ ] player position
  - [ ] player angle
  - [ ] current map cell
- [ ] Add pause screen placeholder

### Done When
- [ ] Game has visible HUD and debug info

---

## Phase 15 — Basic Lighting

### Goal
Add simple retro-style lighting without making renderer too complex.

### Checklist
- [ ] Add distance-based shading on walls
- [ ] Add distance-based shading on sprites
- [ ] Add per-cell light level support in map
- [ ] Multiply final sampled color by light factor
- [ ] Clamp light values safely
- [ ] Keep only grayscale or scalar darkening for v1

### Done When
- [ ] Distant surfaces are darker
- [ ] Scene has more depth and atmosphere

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

