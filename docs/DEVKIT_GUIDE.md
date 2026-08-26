# Neo-Wolf DevKit CLI Toolchain Guide

## Introduction

The **Neo-Wolf DevKit** (`tools/devkit/`) is a comprehensive suite of developer tools designed for inspecting, authoring, validating, testing, and packaging Neo-Wolf engine content and release distributions.

---

## Quick Start & Invocation

You can invoke DevKit in several convenient ways:

```bash
# Via native launcher script (Linux / macOS):
./tools/devkit/neo_devkit <command> [options]

# Via batch script (Windows):
tools\devkit\neo_devkit.cmd <command> [options]

# Via Python module:
python3 -m tools.devkit <command> [options]

# Via direct script entry:
python3 tools/devkit/devkit.py <command> [options]
```

---

## Command Reference

### 1. `doctor` — Environment & Project Diagnostics
Performs health checks on your compiler (`g++`, `clang++`, `cl.exe`), build systems (`make`, `cmake`), SDL2 dependencies, directory layout, and file integrity.

```bash
./tools/devkit/neo_devkit doctor
```

---

### 2. `validate` — Content & Asset Integrity Verification
Performs multi-layer verification on all project content:
- Enforces closed boundaries and consistent line lengths on all `.map` files.
- Verifies Netpbm PPM (`P3`) syntax, headers, dimensions, and pixel counts.
- Verifies RIFF WAVE (`.wav`) headers, bit depth, channel configuration, and PCM data.
- Enforces consistency between entity placement files (`.csv` and `.json`) and map dimensions.
- Cross-references all file paths in `.cfg` and `.json` definitions against disk.

```bash
# Validate entire repository content:
./tools/devkit/neo_devkit validate

# Strict mode (treats warnings as failures):
./tools/devkit/neo_devkit validate --strict

# Validate a specific map file:
./tools/devkit/neo_devkit validate --map game/maps/level01.map

# Validate a specific texture or audio asset:
./tools/devkit/neo_devkit validate --asset game/assets/wall_main.ppm
```

---

### 3. `inspect-map` — Map Terminal Viewer & Reachability Analysis
Renders an ASCII visualization of your map directly in the terminal, overlaying entity placements, calculating tile breakdown statistics, and running a breadth-first search (BFS) flood-fill reachability analysis.

```bash
# Render map with entity overlays:
./tools/devkit/neo_devkit inspect-map game/maps/level01.map --entities game/defs/level01.entities.csv

# Print detailed tile statistics and reachability report:
./tools/devkit/neo_devkit inspect-map game/maps/level01.map --stats --reachability
```

---

### 4. `inspect-asset` — Asset Metadata Inspector
Inspects and prints detailed metadata for PPM textures and WAV audio files.

```bash
# Inspect texture:
./tools/devkit/neo_devkit inspect-asset game/assets/sprite_enemy.ppm

# Inspect sound effect:
./tools/devkit/neo_devkit inspect-asset game/assets/sfx_door_open.wav
```

---

### 5. `create-map` — Procedural Map Generator
Generates a starter map pre-configured with solid outer walls, walkable floor, doors, and triggers.

```bash
# Generate a 20x16 arena:
./tools/devkit/neo_devkit create-map game/maps/level02.map --width 20 --height 16 --style arena

# Available procedural styles:
#   arena      - Open hall with central pillars and alt-walls
#   dungeon    - Corridors with interactive doors and switches
#   crossroads - 4-way cross corridor layout
#   pillars    - Symmetrical column hall
#   box        - Simple rectangular room
```

---

### 6. `create-asset` — Procedural Asset Generator
Creates procedural test textures or test audio tones.

```bash
# Generate a 64x64 magenta/black checkerboard texture:
./tools/devkit/neo_devkit create-asset --type checker --out game/assets/test_checker.ppm --width 64 --height 64

# Generate a 64x64 stone brick texture:
./tools/devkit/neo_devkit create-asset --type brick --out game/assets/test_brick.ppm --width 64 --height 64

# Generate a 0.2s 440 Hz test beep WAV:
./tools/devkit/neo_devkit create-asset --type beep --out game/assets/sfx_test.wav
```

---

### 7. `sync-entities` — Entity Format Synchronizer
Maintains bi-directional synchronization between `.entities.csv` and `.entities.json` files in `game/defs/`.

```bash
./tools/devkit/neo_devkit sync-entities
```

---

### 8. `package` — Release Distribution Builder
Assembles an isolated, standalone release distribution folder and generates `.zip` and `.tar.gz` archive bundles with checksums (`SHA256SUMS`).

```bash
# Create release bundle in release/ folder:
./tools/devkit/neo_devkit package --version 1.0.0 --out-dir release
```

---

### 9. `test` — DevKit Automated Unit Test Suite
Runs the internal test suite verifying all DevKit validator, analysis, generation, and packaging algorithms.

```bash
./tools/devkit/neo_devkit test
```

---

## Step-by-Step Level Creation Workflow

1. **Create the Map Grid**:
   ```bash
   ./tools/devkit/neo_devkit create-map game/maps/level02.map --width 16 --height 12 --style dungeon
   ```
2. **Inspect and Verify Reachability**:
   ```bash
   ./tools/devkit/neo_devkit inspect-map game/maps/level02.map --stats --reachability
   ```
3. **Create Entity Placements** (`game/defs/level02.entities.csv`):
   ```csv
   # type,x,y,mapX,mapY
   player,2.5,2.5
   enemy,7.5,3.5
   pickup,12.5,8.5
   door,8.5,5.5,8,5
   trigger,13.5,8.5,13,8
   ```
4. **Sync JSON Placement Files**:
   ```bash
   ./tools/devkit/neo_devkit sync-entities
   ```
5. **Run Full Project Validation**:
   ```bash
   ./tools/devkit/neo_devkit validate --strict
   ```
6. **Package the Game**:
   ```bash
   ./tools/devkit/neo_devkit package
   ```
