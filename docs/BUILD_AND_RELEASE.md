# Neo-Wolf Build & Release Engineering Guide

## Build Prerequisites

### Linux (Ubuntu / Debian / Fedora / Arch)
- **C++ Compiler**: `g++` or `clang++` supporting C++17
- **Build Tools**: `make` (GNU Make 4.0+) or `cmake` (3.21+)
- **Libraries**: `libsdl2-dev` (SDL2 development headers and runtime)
- **Python**: `python3` (3.8+) for DevKit CLI toolchain

```bash
# Ubuntu / Debian
sudo apt-get install build-essential libsdl2-dev python3
```

### Windows
- **Compiler & IDE**: Visual Studio 2022 (with "Desktop development with C++" workload)
- **Package Manager**: `vcpkg` for SDL2 dynamic libraries (`vcpkg install sdl2:x64-windows`)
- **Python**: Python 3.8+ on system `PATH`

---

## 1. Building on Linux

### Quick Build with Makefile
```bash
# Build test suite and game executable
make all

# Build only game executable
make game

# Build only unit test suite
make tests
```

### Using Build Script
```bash
./build.sh Release
```

### Using CMake
```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## 2. Building on Windows

### Using Windows Batch Script
```cmd
build.cmd Release
```

### Using PowerShell
```powershell
.\build.ps1 -Config Release
```

---

## 3. Running Automated Tests

Neo-Wolf maintains two comprehensive test suites:

### Native C++ Test Suite (`neo_wolf_tests`)
Validates math helpers, input transitions, raycast geometry, sprite depth sorting, collision sliding, entity interactions, PPM/WAV asset loaders, and map perimeter integrity.

```bash
# Build and run native test runner
make test
# Or directly:
./neo_wolf_tests
```

### DevKit Python Test Suite
Validates map syntax, reachability flood-fill, PPM token counts, WAV headers, CSV/JSON synchronizer, and packaging integrity.

```bash
./tools/devkit/neo_devkit test
```

### Unified Test Command
```bash
make check
```

---

## 4. Content & Asset Validation

Before packaging or committing content, run the automated validator:

```bash
make validate
# Or:
./tools/devkit/neo_devkit validate --strict
```

---

## 5. Release Packaging (Phase 26)

Neo-Wolf provides automated packaging tools that create completely self-contained, standalone distribution bundles.

### Generating a Release Bundle
```bash
# Via Makefile:
make package

# Via packaging script:
./package.sh 1.0.0 release

# Via DevKit CLI directly:
./tools/devkit/neo_devkit package --version 1.0.0 --out-dir release
```

### Package Bundle Contents
The generated standalone folder `release/neo-wolf-v1.0.0-<platform>/` includes:

```text
neo-wolf-v1.0.0-linux/
|-- neo_wolf                  # Standalone game executable binary
|-- launch.sh                 # Executable Unix launch script
|-- README.txt                # Player controls, quick start, system requirements
|-- RELEASE_NOTES.md          # Version release notes and features
|-- SHA256SUMS                # Cryptographic file checksums
`-- game/                     # Complete game content bundle
    |-- assets/               # PPM textures and WAV audio
    |-- maps/                 # Grid map files
    `-- defs/                 # Game definitions and entity tables
```

In addition to the directory bundle, DevKit automatically creates compressed archives:
- `release/neo-wolf-v1.0.0-linux.tar.gz`
- `release/neo-wolf-v1.0.0-linux.zip`

### Verifying Standalone Execution
The release package can be run directly from any folder without requiring the development toolchain:

```bash
cd release/neo-wolf-v1.0.0-linux
./launch.sh
```
Relative paths to `game/assets/`, `game/maps/`, and `game/defs/` resolve cleanly relative to the packaged executable.
