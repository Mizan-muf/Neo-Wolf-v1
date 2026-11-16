# Neo-Wolf-v1

A high-performance **2.5D software-rendered raycasting game engine** written in modern C++17 with SDL2, featuring a data-driven content pipeline, native particle simulation, audio system, and a comprehensive **DevKit CLI toolchain** (`tools/devkit/`).

---

## Features

- **Software Raycasting Engine**: Fast DDA (Digital Differential Analysis) raycaster with fish-eye distortion correction and directional wall shading.
- **Textured Rendering**: Software-textured walls, floors, and ceilings with distance depth fog and per-cell ambient lighting.
- **Billboard Sprites**: Occlusion-culled, distance-sorted 2.5D sprites for enemies, pickups, and decorations with transparent magenta color keying.
- **Particle System**: Real-time particle simulation for wall sparks, pickup sparkles, and floating ambient dust motes.
- **Sound System**: SDL2 16-bit PCM audio mixer with configurable master volume and event-driven SFX triggers (doors, pickups, weapon fire, enemy alerts).
- **GameState Machine**: Robust state separation (`Menu`, `Gameplay`, `Paused`, `GameOver`, `Victory`) with seamless transitions, overlays, and level reload/reset architecture.
- **Gameplay Combat & AI**: Hitscan weapon attacks, ammo management, health pickups, and enemy AI state machine (Idle -> Alert -> Chase -> Attack -> Dead).
- **C API Boundary & Interop Bridge**: Plain C ABI interface and bidirectional Command Bridge / Event Queue for decoupling gameplay scripting.
- **Data-Driven Architecture**: Grid map files (`.map`), texture/sprite configuration files (`.cfg`, `.json`), gameplay definitions (`gameplay.json`), and entity placement definitions (`.csv`, `.json`).
- **DevKit CLI Toolchain**: Complete developer toolkit (`tools/devkit/`) for map inspection, reachability analysis, asset verification, procedural generation, CSV/JSON sync, system doctor diagnostics, and release packaging.
- **Dual-Engine Test Suite**: 100% automated test coverage across native C++ logic/asset loaders and Python DevKit validation suite.
- **Cross-Platform**: macOS (primary, verified), Windows (presets and scripts written, not yet verified), Linux (best effort).

---

## Quick Start (macOS)

Needs the Xcode Command Line Tools, `brew install cmake ninja`, and Python 3. SDL2 is downloaded
and linked statically by CMake, so there is nothing else to install. Details: [docs/BUILD_MACOS.md](docs/BUILD_MACOS.md).

### 1. Build and Run Tests
```bash
./build.sh          # configure + build + run all tests (or: make check)
```

### 2. Launch Game
```bash
./build/macos/neo_wolf            # works from any directory: it finds the repository's game/ folder
./build/macos/neo_wolf --help     # options: --data, --level, --frames, --fullscreen, --version
```

If the game does not start, see [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) (log file
locations, missing content, no audio device, virtual machines).

---

## Quick Start (Linux)

**Not yet run on Linux** (best effort; Linux CI is written but has not run yet). Needs a C++17
compiler, CMake 3.25+, Ninja and Python 3. Ubuntu 22.04's packaged CMake (3.22) is too old for
the presets: get a newer CMake (for example `pip install cmake`, or Kitware's apt repository).
SDL2 is built from source by CMake (no `libsdl2-dev` needed), but that needs the X11/Wayland and
audio development packages SDL builds against; see [docs/BUILD_AND_RELEASE.md](docs/BUILD_AND_RELEASE.md).

### 1. Build and Run Tests
```bash
./build.sh          # configure + build + run all tests (or: make check)
```

### 2. Launch Game
```bash
./build/linux/neo_wolf
```

---

## Quick Start (Windows)

Needs Visual Studio 2022 or newer (any edition) with the *Desktop development with C++* workload,
and Python 3. No vcpkg or SDL2 install is needed. Details: [docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md)
(**not yet built or tested on Windows**; Windows CI is written but has not run yet).

### 1. Build and Run Tests
```cmd
build.cmd Release
```

### 2. Launch Game
```cmd
build\windows-msvc\Release\neo_wolf.exe
```

Running a single test, test labels and adding tests: [docs/TESTING.md](docs/TESTING.md).

---

## DevKit CLI Toolchain (`tools/devkit/`)

The DevKit CLI provides developer tools for map inspection, asset validation, level creation, and distribution packaging.

```bash
# Run system doctor to check environment & compilers
./tools/devkit/neo_devkit doctor

# Validate all project maps, assets, and definitions
./tools/devkit/neo_devkit validate

# Inspect and preview a map with ANSI colors & reachability analysis
./tools/devkit/neo_devkit inspect-map game/maps/level01.map --entities game/defs/level01.entities.csv

# Inspect texture or audio asset metadata
./tools/devkit/neo_devkit inspect-asset game/assets/sprite_enemy.ppm
./tools/devkit/neo_devkit inspect-asset game/assets/sfx_door_open.wav

# Create a new starter map template
./tools/devkit/neo_devkit create-map game/maps/level02.map --width 20 --height 15 --style dungeon

# Synchronize CSV and JSON entity definitions
./tools/devkit/neo_devkit sync-entities

# Assemble standalone release package
./tools/devkit/neo_devkit package --version 1.0.0 --out-dir release

# Run DevKit unit test suite
./tools/devkit/neo_devkit test
```

---

## Automated Test Suites

Neo-Wolf maintains two complementary automated test suites:

| Test Suite | Implementation | Target | Coverage |
| :--- | :--- | :--- | :--- |
| **Native Engine Tests** | C++17 (`tests/LogicTests.cpp`) | `build/macos/neo_wolf_tests` (Windows: `build\windows-msvc\Release\neo_wolf_tests.exe`) | Math helpers, raycaster DDA, collision sliding, entity interactions, PPM/WAV asset loaders, map perimeter enclosure, reachability flood-fill |
| **DevKit Test Suite** | Python 3 (`tools/devkit/tests/`) | `neo_devkit test` | Map parser, reachability BFS, PPM token parser, WAV RIFF headers, CSV/JSON synchronizer, release packager |

Run both test suites with one command:
```bash
make check
```

---

## Release Packaging (Phase 26)

Create a completely self-contained, standalone distribution bundle:

```bash
make package
# Or:
./package.sh 1.0.0 release
```

The output directory `release/neo-wolf-v1.0.0-<platform>/` is fully portable and includes the executable, game content (`assets/`, `maps/`, `defs/`), launch scripts (`launch.sh`, `launch.cmd`), documentation, and cryptographic checksums (`SHA256SUMS`). Compressed `.tar.gz` and `.zip` archives are generated alongside.

---

## Controls & Debug Features

The bindings live in `engine/platform/PlatformSDL.cpp` (`MapScancodeToActions`); this table matches it.

| Action | Keys | Notes |
| :--- | :--- | :--- |
| **Move forward / back** | `W` / `S`, or Up / Down arrows | |
| **Strafe left / right** | `A` / `D` | |
| **Turn left / right** | Left / Right arrows; `Q` also turns left | `E` is *not* a turn key (it is Interact) |
| **Fire** | `Space`, `F`, left or right `Ctrl` | Also starts the game from the menu |
| **Interact** (open/close doors) | `E` | Also starts the game from the menu. Pickups and the exit trigger work by walking into them. |
| **Pause / resume** | `Esc` or `P` | `Enter` also resumes. The game pauses by itself when its window loses focus. |
| **Restart level** | `R` | In play, paused, or on the game-over / victory screen |
| **Confirm** | `Enter` (or keypad `Enter`) | Starts the game from the menu; returns to the menu from game-over / victory (`Esc`/`P` too) |
| **Minimap and debug overlay** | `Tab` | On by default |
| **Fullscreen on/off** | `F11` or `Alt+Enter`; on macOS also `Cmd+Ctrl+F` | Desktop fullscreen; the image keeps its 4:3 shape with black bars. `--fullscreen` starts in fullscreen. |
| **Quit** | `Esc` on the main menu; close the window, `Cmd+Q` (macOS) or `Alt+F4` (Windows) | A Quit menu item comes with the menu rework (Roadmap Phase 5) |

The window can be resized freely; the 320×240 image is scaled with sharp (nearest-neighbour) pixels.

---

## Project Structure

```text
/neo-wolf
|-- engine/                 # Core engine subsystems
|   |-- core/               # Engine lifecycle, App host, Math, Color, InputState, Time
|   |-- platform/           # SDL2 platform abstraction and windowing
|   |-- render/             # Software Framebuffer, Raycaster DDA, Sprite/Particle renderers
|   |-- world/              # Map representation, Player, Entities, Collision & Interaction rules
|   |-- assets/             # PPM TextureLoader, AssetManager, Content/Audio configs
|   |-- audio/              # AudioSystem SDL audio device and playback
|   |-- particles/          # Particle manager, emitters, and physics
|   `-- ui/                 # HUD rendering, font glyphs, crosshair, pause screen
|-- game/                   # Game content and definitions
|   |-- assets/             # PPM textures/sprites and WAV sound effects
|   |-- maps/               # ASCII grid map files (.map)
|   `-- defs/               # Texture/sprite/audio configs and entity placement tables
|-- tools/                  # Developer toolchain
|   `-- devkit/             # DevKit CLI: validator, doctor, map/asset tools, packager
|-- docs/                   # Technical documentation
|-- tests/                  # Native C++ test runner
|-- src/                    # Main executable entry point
|-- Makefile                # Linux build & automation targets
|-- build.sh / package.sh   # Unix build and packaging scripts
|-- build.cmd / package.cmd # Windows build and packaging scripts
`-- CMakeLists.txt          # CMake build configuration
```

---

## Documentation Index

- [Architecture Specification](docs/ARCHITECTURE.md)
- [Map Format Specification](docs/MAP_FORMAT.md)
- [Asset Pipeline Guide](docs/ASSET_GUIDE.md)
- [Content Definitions Guide](docs/CONTENT_DEFINITIONS.md)
- [DevKit CLI Toolchain Guide](docs/DEVKIT_GUIDE.md)
- [Gameplay API & Mechanics](docs/GAMEPLAY_API.md)
- [Build & Release Engineering](docs/BUILD_AND_RELEASE.md)
- [Implementation Checklist](docs/Checklist.md)
