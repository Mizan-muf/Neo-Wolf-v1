# Neo-Wolf Build & Release Engineering Guide

## 1. Building

The build uses CMake presets (`CMakePresets.json`, CMake 3.25 or newer). SDL2 is downloaded as
a pinned source tarball (SHA-256 checked) and linked statically, so no SDL2 install, vcpkg, Git
or hardcoded library paths are needed.

| Platform | Status | Guide | One command |
|---|---|---|---|
| macOS (universal arm64 + x86_64) | verified | [BUILD_MACOS.md](BUILD_MACOS.md) | `./build.sh` |
| Windows (Visual Studio 2022+) | (not yet run) | [BUILD_WINDOWS.md](BUILD_WINDOWS.md) | `build.cmd` |
| Linux | (not yet run) | see below | `./build.sh` |

**Linux** (best effort, not yet run): install a C++17 compiler, CMake 3.25+, Ninja and Python 3,
plus the development packages SDL builds against for X11/Wayland video and audio. On
Debian/Ubuntu that is for example `libx11-dev libxext-dev libxrandr-dev libxcursor-dev
libxfixes-dev libxi-dev libxss-dev libwayland-dev libxkbcommon-dev libasound2-dev libpulse-dev`
(a subset of the full list in SDL's `docs/README-linux.md`, which is in
`build/<preset>/_deps/sdl2-src/docs/` after the first configure). `libsdl2-dev` is **not**
needed: SDL2 is built from source. Ubuntu 22.04's packaged CMake (3.22) is too old; install a
newer one (for example `pip install cmake`, or Kitware's apt repository). Then run `./build.sh`
or the `linux` presets below.

The underlying commands are the same on every platform:

```bash
cmake --preset <macos|windows-msvc|linux>
cmake --build --preset <macos|windows-msvc|linux>
ctest --preset <macos|windows-msvc|linux>
```

Builds go to `build/<preset>/` (on Windows `build\<preset>\<Config>\`, e.g.
`build\windows-msvc\Release\`). `make` on macOS and Linux is a thin wrapper around these
presets (`make`, `make check`, `make game`, `make tests`, `make clean`). `make clean` removes
`build/` (including the downloaded SDL source, so the next configure downloads it again) and
`release/`.

---

## 2. Running Automated Tests

See [TESTING.md](TESTING.md). In short, `ctest --preset <preset>` runs the native C++ tests
(one CTest entry per test case), the DevKit Python tests and strict content validation.
`make check` does the same on macOS and Linux.

---

## 3. Content & Asset Validation

Before packaging or committing content, run the automated validator:

```bash
make validate-strict
# Or:
./tools/devkit/neo_devkit validate --strict
```

On Windows: `tools\devkit\neo_devkit.cmd validate --strict`. (`make validate` runs the same
checks without `--strict`, so warnings do not fail it.)

---

## 4. Release Packaging (Phase 26)

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
