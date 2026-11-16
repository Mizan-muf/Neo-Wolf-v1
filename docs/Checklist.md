# Neo Wolf - Complete Implementation Checklist

> **History, reconciled with the code on 2026-11-16 (Roadmap Phase 10).** This was the original
> 26-phase plan, and every item in it was ticked. The audit of 2026-11-15 found that several
> ticked items were not implemented ([KNOWN_ISSUES.md](KNOWN_ISSUES.md), issue X1).
> **[ROADMAP.md](ROADMAP.md) is now the source of truth** for what is done and what is left.
>
> Nothing has been deleted from this page. Instead, every item that was ticked but is false or
> only partly true has been unticked or annotated in place:
> **not done** (unticked), **partial**, **no longer applies**, *false when ticked* (fixed
> later), *replaced by Phase N of ROADMAP*, or *out of scope for v1* (the
> [decisions](ROADMAP.md#decisions)). Ticks without a note were spot-checked against the
> current code and hold, some only in a later, reworked form; they were not re-verified one by
> one. Issue codes such as C1 or B10 refer to [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

## Project Stack
- Windows first *(changed: macOS and Windows are equal targets, and macOS is the primary verified platform; [ROADMAP.md](ROADMAP.md))*
- C++ engine
- SDL2 platform layer
- Software rendering
- Grid-based maps
- Config files/maps for v1
- C API boundary
- C# gameplay later *(out of scope for v1: [ROADMAP.md, decisions](ROADMAP.md#decisions))*
- Sprites only for v1
- Basic UI / particles / lighting
- No deep engine reload

---

## Ground Rule

**Do not jump phases.**  
Only move forward when the current phase is working.

## Test Reference

Use [test.md](history/test.md) as the source for when to introduce tests and what to cover at each stage. *(Archived: [TESTING.md](TESTING.md) and [CONTRIBUTING.md](../CONTRIBUTING.md) replace it.)*

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
- [x] Build a Windows executable *(today: built and tested headless in Windows CI since [Phase 2 of ROADMAP](ROADMAP.md#phase-2--ci-macos--windows); not yet run on a Windows PC, [Phase 11 of ROADMAP](ROADMAP.md#phase-11--final-sign-off))*
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
- [x] Keep SDL calls mostly inside platform layer — **partial**: SDL is also used in `App.cpp`, `Paths.cpp` and `AudioSystem.cpp` ([ARCHITECTURE.md](ARCHITECTURE.md#dependencies)); [Phase 12 of ROADMAP](ROADMAP.md#phase-12--clean-code-review) reviews it

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
- [x] Manual runtime validation completed (`neo_wolf.exe` shows the Phase 6 map) *(historical record, not re-checked; the debug map became the minimap)*

### Test Block (Phases 3-6)
Reference: [test.md](history/test.md) (archived)
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
- [x] Manual runtime validation completed (`neo_wolf.exe` shows movement, rotation, and wall collision in top-down view) *(historical record, not re-checked)*

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
- [ ] Add debug toggle to show ray hits in top-down map — **not in the current code**: `F3` shows frame rate, position, angle and cell; the minimap draws no rays

### Done When
- [x] First-person wall rendering works
- [x] Turning and moving updates the scene correctly

---

## Phase 9 — Texture Mapping for Walls

### Goal
Replace flat wall colors with textures.

### Checklist
- [x] Create texture loader
- [x] Load image files into CPU-readable pixel arrays *(ASCII PPM `P3` only)*
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
Reference: [test.md](history/test.md) (archived)
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
- [x] Load entity placements from file *(false when ticked: the JSON file was read with a CSV parser and a hardcoded layout was used, known issue C1; true since [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects))*
- [x] Validate files on startup *(replaced by [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects): invalid `gameplay.json`, campaign or level files now stop the game with the file, entry and field)*
- [x] Print useful file-loading errors *(replaced by [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects))*

### Suggested Content Types
- [ ] `textures.json` — **never read by the engine; deleted** in [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects) (C5). Wall textures come from `game/defs/themes/<id>.cfg`
- [ ] `sprites.json` — **never read; deleted** in [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects) (C5). The engine reads `sprites.cfg`
- [ ] `entities.json` — **never read by the engine; deleted** in [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects) (C5). The level list is `campaign.json` ([Phase 5 of ROADMAP](ROADMAP.md#phase-5--campaign--progression))
- [x] `level01.map`
- [x] `level01.entities.json`

### Done When
- [x] You can change map/content without recompiling engine *(false when ticked: entity files and `gameplay.json` had no effect, C1 and C2; true since [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects), test `edited_data_changes_game`)*

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
- [x] Add per-cell light level support in map *(content files could not set it until light zones, known issue C21, [Phase 8 of ROADMAP](ROADMAP.md#phase-8--the-10-levels))*
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
  - [x] sprite/frame — **partial**: `Particle::spriteFrame` is set but nothing draws by it
  - [x] size
- [x] Create particle manager
- [x] Add spawn function
- [x] Update particles every frame
- [x] Render particles as simple sprites *(drawn as coloured squares, not textured sprites)*
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
- [x] Load WAV/OGG for effects — **partial**: WAV only (no OGG); sounds are queued one after another, not mixed
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
- [ ] Remove obvious memory leaks — **not verified**: no leak checker or sanitizer has been run; ownership is reviewed in [Phase 12 of ROADMAP](ROADMAP.md#phase-12--clean-code-review)
- [x] Add safe null checks
- [ ] Add debug assertions — **not done**: the engine has no `assert` calls (only one `static_assert`)
- [x] Add log file output *(false when ticked: the game never enabled the log file, B10; replaced by [Phase 3 of ROADMAP](ROADMAP.md#phase-3--runtime-robustness): `neowolf.log` in the per-user folder)*
- [x] Test map reload by restarting the level

### Done When
- [x] Native-only engine feels stable and reusable *(replaced by [Phase 3 of ROADMAP](ROADMAP.md#phase-3--runtime-robustness) (runtime robustness) and [Phase 11 of ROADMAP](ROADMAP.md#phase-11--final-sign-off) (sign-off); the audit found the defects in [KNOWN_ISSUES.md](KNOWN_ISSUES.md))*

### Test Block (Phase 18+)
Reference: [test.md](history/test.md) (archived)
- [x] Make all existing tests run in CI/local gate before merge *(false when ticked: there was no CI, B5; replaced by [Phase 2 of ROADMAP](ROADMAP.md#phase-2--ci-macos--windows): GitHub Actions on macOS, Windows and Linux)*
- [x] Add integration tests for map reload and state transitions
- [x] Add regression tests for bugs fixed during stabilization
- [ ] Track and maintain a minimum logic-test coverage target — **not done**: coverage is not measured and there is no target

---

## Phase 19 — C API Boundary

### Goal
Expose native engine functions through a clean C interface.

### Checklist
- [x] Create `/engine/interop`
- [x] Add exported C API header
- [x] Export functions using C ABI — **partial**: the functions have C linkage, but they live in the static library `neowolf_engine`; no shared library is built or exported (out of scope for v1 ([ROADMAP.md, decisions](ROADMAP.md#decisions)); [C_API.md](C_API.md))
- [x] Keep interface plain:
  - [x] ints
  - [x] floats
  - [x] enums
  - [x] simple structs — **partial**: `InteropCommand` / `InteropEvent` are opaque to C callers, so commands and events only work from C++ ([C_API.md](C_API.md))
  - [x] handles / IDs
- [x] Do not expose C++ classes *(for C callers; in C++ the header also includes `CommandBridge.h`)*
- [x] Do not expose STL containers
- [x] Export minimal functions first:
  - [x] init gameplay bridge *(`nw_engine_init` initialises the whole engine; there is no separate bridge init)*
  - [x] shutdown gameplay bridge
  - [x] create entity
  - [x] destroy entity
  - [x] get/set entity position — **partial**: entities can be read (`nw_engine_get_entity`) but not moved; only the player has a setter
  - [ ] read input state — **not done**: no C API function exposes input
  - [x] send event/log — **partial**: the engine sends events (`nw_engine_poll_event`); there is no logging function
- [ ] Version the API — **not done**: there is no API version ([C_API.md](C_API.md))

### Done When
- [x] A separate program could theoretically call your engine functions through the C API — **partial**: only code linked into the same executable can; nothing is exported ([C_API.md](C_API.md))

---

## Phase 20 — C# Interop Layer / Command Bridge

### Goal
Connect C# gameplay code to native engine via bidirectional command & event bridge.

> **Out of scope for v1** ([ROADMAP.md, decisions](ROADMAP.md#decisions)): there is no C# or other managed code in the repository. Only the native half (the command and event queues) exists; see [C_API.md](C_API.md).

### Checklist
- [x] Create separate interop bridge module (`engine/interop/CommandBridge.h`, `engine/interop/EngineInterop.h`)
- [x] Add C ABI declarations for exported native functions
- [ ] Create wrapper functions over raw imports and command buffer queue — **not done**, out of scope for v1 ([ROADMAP.md, decisions](ROADMAP.md#decisions))
- [ ] Add engine startup callback from native to managed or managed bootstrap into native — **not done**, out of scope for v1 ([ROADMAP.md, decisions](ROADMAP.md#decisions))
- [x] Add test feature for gameplay rules over bridge: — **partial**
  - [x] command queue & event polling for combat, healing, state changes *(the queues are tested on their own, `command_bridge_events`; no test runs a command through the engine)*
- [ ] Confirm data marshaling works correctly with POD types — **not done**: there is no managed side to marshal to, out of scope for v1 ([ROADMAP.md, decisions](ROADMAP.md#decisions))
- [x] Keep ownership on native side unless absolutely necessary

### Done When
- [x] External callers can successfully call native engine functions — **partial**: in-process only (test `engine_interop_c_api`)
- [ ] Real gameplay behavior runs across the bridge — **not done**: the game's rules run in C++; the bridge only reports events and accepts test commands

---

## Phase 21 — Move Gameplay Rules to Scripting / State Layer

### Goal
Make gameplay rules modular, decoupled, and event-driven.

### Checklist
- [x] Move pickup logic to gameplay interaction layer *(a C++ module, `engine/world/InteractionRules.cpp`, not a scripting layer)*
- [x] Move enemy behavior to gameplay interaction layer *(C++: `engine/world/EnemyAi.cpp`, rewritten in [Phase 6 of ROADMAP](ROADMAP.md#phase-6--gameplay-depth))*
- [x] Move trigger reactions to gameplay interaction layer *(C++: `InteractionRules.cpp`)*
- [x] Move weapon state logic to gameplay interaction layer — **partial**: the shot is in `InteractionRules.cpp`; ammo and cooldown stay in `Engine`
- [x] Keep rendering, collision, map loading, and engine loop in C++
- [ ] Add gameplay lifecycle methods: — **not done**: none of these functions exists (the nearest equivalents are the C API events `LevelLoaded` and `StateChanged`)
  - [ ] `OnGameStart`
  - [ ] `OnLevelLoaded`
  - [ ] `OnUpdate`
  - [ ] `OnEntitySpawned`
  - [ ] `OnShutdown`

### Done When
- [x] Core engine is native
- [x] Game rules are cleanly separated into gameplay layer — **partial**: separate C++ modules in `engine/world/`, coordinated by `Engine` ([ARCHITECTURE.md](ARCHITECTURE.md))

---

## Phase 22 — File-Driven Gameplay Definitions

### Goal
Make content editing easy without recompiling.

### Checklist
- [x] Create config for enemy definitions (`game/defs/gameplay.json`) *(three types since [Phase 6 of ROADMAP](ROADMAP.md#phase-6--gameplay-depth))*
- [x] Create config for pickup definitions
- [x] Create config for weapon definitions
- [ ] Create config for trigger definitions — **not done**: `gameplay.json` has no trigger section; triggers have fixed behaviour
- [x] Load these from files at startup *(false when ticked: the game never loaded `gameplay.json`, C2; replaced by [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects))*
- [x] Connect config data to gameplay layer *(replaced by [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects))*
- [x] Validate broken configs clearly *(replaced by [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects))*

### Done When
- [x] Most game behavior can be tuned through files + config logic *(true since [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects))*

---

## Phase 23 — Controlled Reload Preparation

### Goal
Prepare architecture for future reload support without implementing deep reload.

### Checklist
- [ ] Separate engine core from gameplay assembly loading logic — **not done**: there is no assembly loading, out of scope for v1 ([ROADMAP.md, decisions](ROADMAP.md#decisions))
- [ ] Separate static engine state from gameplay-owned state — **not done**: `Engine` holds both as public members of one class
- [x] Keep asset/data loading modular
- [x] Add ability to unload and reload content files on full level restart
- [x] Add gameplay state serialization placeholder *(replaced by [Phase 5 of ROADMAP](ROADMAP.md#phase-5--campaign--progression): `progress.json` saves the unlocked levels only)*
- [ ] Add clear API boundary for future gameplay DLL reload — **not done**, out of scope for v1 ([ROADMAP.md, decisions](ROADMAP.md#decisions))
- [x] Do not attempt renderer/core live reload

### Done When
- [x] Project structure is ready for later controlled gameplay/data reload — **partial**: levels reload from their files on every start and restart; nothing else is reloadable

---

## Phase 24 — First Playable Vertical Slice

### Goal
Produce a tiny but complete game sample.

### Checklist
- [x] One full level *(replaced by [Phase 8 of ROADMAP](ROADMAP.md#phase-8--the-10-levels): ten levels)*
- [x] Spawn point
- [x] Walls and textures
- [x] At least one door
- [x] At least one pickup
- [x] At least one enemy
- [x] HUD
- [x] Sound
- [x] Simple win condition *(then a trigger tile; the exit switch since [Phase 5 of ROADMAP](ROADMAP.md#phase-5--campaign--progression))*
- [x] Simple restart flow *(was broken: Menu → Start kept 0 HP, C7; fixed in [Phase 5 of ROADMAP](ROADMAP.md#phase-5--campaign--progression))*
- [x] No crash during normal play session — **partial**: verified by headless bot runs, not yet by a person playing (C13, [Phase 11 of ROADMAP](ROADMAP.md#phase-11--final-sign-off))

### Done When
- [x] Someone can launch the game and complete a small level — **partial**: the autoplay bot completes all ten levels; a play-through by hand is [Phase 11 of ROADMAP](ROADMAP.md#phase-11--final-sign-off)

---

## Phase 25 — Polish and Cleanup

### Goal
Make the project maintainable.

### Checklist
- [x] Remove dead code — **partial**: unused definition files remained until [Phase 4 of ROADMAP](ROADMAP.md#phase-4--data-pipeline-defects); [Phase 12 of ROADMAP](ROADMAP.md#phase-12--clean-code-review) reviews the rest
- [x] Rename unclear classes/functions *(not re-checked; [Phase 12 of ROADMAP](ROADMAP.md#phase-12--clean-code-review))*
- [x] Split oversized files *(not re-checked; [Phase 12 of ROADMAP](ROADMAP.md#phase-12--clean-code-review))*
- [x] Add comments only where useful *(not re-checked; [Phase 12 of ROADMAP](ROADMAP.md#phase-12--clean-code-review))*
- [x] Add README
- [x] Add build instructions *(the instructions did not work on another machine, hardcoded SDL paths B1; replaced by [Phase 1 of ROADMAP](ROADMAP.md#phase-1--portable-build-system): BUILD_MACOS.md, BUILD_WINDOWS.md)*
- [x] Add asset folder instructions
- [x] Add map format documentation
- [x] Add gameplay API documentation *(`GAMEPLAY_API.md` was inaccurate; replaced in [Phase 10 of ROADMAP](ROADMAP.md#phase-10--documentation-pass) by [C_API.md](C_API.md) and [GAMEPLAY.md](GAMEPLAY.md))*
- [x] Add debug controls documentation *(was wrong, X2; replaced by [CONTROLS.md](CONTROLS.md), [Phase 3 of ROADMAP](ROADMAP.md#phase-3--runtime-robustness) and [Phase 5 of ROADMAP](ROADMAP.md#phase-5--campaign--progression))*

### Done When
- [x] A new developer can clone and understand the project *(false when ticked: a fresh clone did not build, B1; replaced by [Phase 1 of ROADMAP](ROADMAP.md#phase-1--portable-build-system) and [Phase 10 of ROADMAP](ROADMAP.md#phase-10--documentation-pass))*

---

## Phase 26 — Packaging

### Goal
Make it runnable outside your dev machine.

### Checklist
- [x] Build release configuration
- [x] Copy executable *(false when ticked: the packager could produce a bundle without the executable, D1; replaced by [Phase 9 of ROADMAP](ROADMAP.md#phase-9--packaging))*
- [ ] Copy SDL2 runtime — **no longer applies**: SDL2 is linked statically ([ROADMAP.md, decision 3](ROADMAP.md#decisions)), so there is no runtime to copy
- [ ] Copy native DLLs / binaries — **no longer applies**: no DLLs (static SDL2 and C runtime)
- [x] Copy assets/maps/config files *(replaced by [Phase 9 of ROADMAP](ROADMAP.md#phase-9--packaging))*
- [x] Test on a clean standalone package directory *(replaced by [Phase 9 of ROADMAP](ROADMAP.md#phase-9--packaging): `package_smoke.py` passes for the macOS and Windows x64 packages, locally and in CI run 43864756039, from a path with spaces)*
- [x] Confirm relative asset paths work *(content-root search since [Phase 3 of ROADMAP](ROADMAP.md#phase-3--runtime-robustness))*
- [x] Confirm game launches without IDE *(headless from the macOS and Windows x64 packages, CI run 43864756039; Finder and Explorer launches by hand are open, D5)*

### Done When
- [x] Game runs from packaged folder — **partial**: verified headless for the macOS and Windows x64 packages (CI run 43864756039); not yet launched by hand from Finder or Explorer (D5, [Phase 11 of ROADMAP](ROADMAP.md#phase-11--final-sign-off))

---

## DevKit CLI Toolchain & Test Automation

### Goal
Provide professional CLI developer tools and automated verification for all content, maps, and assets.

### Checklist
- [x] DevKit CLI toolchain in `tools/devkit/` (`neo_devkit`, `devkit.py`)
- [x] Map inspector with terminal ASCII rendering and BFS reachability analysis (`inspect-map`) *(crashed on a spawn in a wall and always exited 0, D3; fixed in [Phase 7 of ROADMAP](ROADMAP.md#phase-7--verification-tooling))*
- [x] Asset metadata inspector for PPM textures and WAV audio (`inspect-asset`)
- [x] Procedural map and asset generators (`create-map`, `create-asset`)
- [x] Definition synchronizer for entity CSV and JSON (`sync-entities`)
- [x] Environment and repository health doctor (`doctor`) *(did not check SDL2, D4; fixed in [Phase 7 of ROADMAP](ROADMAP.md#phase-7--verification-tooling))*
- [x] Content and asset integrity validator (`validate`) *(campaign rules added in [Phase 7 of ROADMAP](ROADMAP.md#phase-7--verification-tooling), D2)*
- [x] Automated release packager with checksum generation (`package`) *(was broken, D1; rebuilt in [Phase 9 of ROADMAP](ROADMAP.md#phase-9--packaging))*
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



