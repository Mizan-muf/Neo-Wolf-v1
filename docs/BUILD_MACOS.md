# Building Neo-Wolf on macOS

This guide covers building the game and running the tests on macOS (Apple Silicon or Intel).
For Windows see [BUILD_WINDOWS.md](BUILD_WINDOWS.md); for the test suites see [TESTING.md](TESTING.md).

## Prerequisites

| Tool | How to get it | Notes |
|---|---|---|
| Apple clang (Xcode Command Line Tools) | `xcode-select --install` | Full Xcode is not required. |
| Homebrew | see [brew.sh](https://brew.sh) | Needed for the `brew install` commands below (or install CMake and Ninja another way, e.g. the cmake.org installer). |
| CMake 3.25 or newer | `brew install cmake` | `CMakeLists.txt` and `CMakePresets.json` (preset schema version 6) both need CMake 3.25. CMake 4.x works (verified with 4.5). |
| Ninja | `brew install ninja` | The macOS presets use the Ninja generator. |
| Python 3 | already there: the Command Line Tools provide `/usr/bin/python3` | Needed for the DevKit tests and content validation. `brew install python` or a python.org install also works but is optional. |

Git is only needed to clone the repository; the build itself does not use it.

**You do not need to install SDL2.** By default CMake downloads the official SDL2 2.32.10 source
tarball, checks its SHA-256 and links SDL2 statically into the game. Internet access is needed
the first time each preset's build directory is configured (and again after `make clean` or
`rm -rf build`, which delete the downloaded copy). See [Offline builds](#offline-builds) if you
have no network access.

## Quick start

From the repository root:

```bash
./build.sh            # configure + build + run all tests (Release)
./build.sh Debug      # the same in Debug
```

`build.sh` stops with a non-zero exit code if any step fails, including any test.

Or use `make`, which wraps the same presets:

```bash
make            # configure + build (Release)
make check      # build, then run every test with CTest
make CONFIG=Debug check
```

## Using the CMake presets directly

```bash
cmake --preset macos            # configure into build/macos/
cmake --build --preset macos    # build neo_wolf and neo_wolf_tests
ctest --preset macos            # run all tests
```

| Preset | What it is |
|---|---|
| `macos` | Release, universal binary (arm64 + x86_64), Ninja |
| `macos-debug` | Debug, universal binary, Ninja |
| `ci-macos` | `macos` with `NEOWOLF_WERROR=ON` (warnings are errors). Used by CI. |

Each preset builds into `build/<preset-name>/`. List all presets with `cmake --list-presets`.
Presets for other operating systems are hidden on macOS.

### Universal binary

The `macos` presets set `CMAKE_OSX_ARCHITECTURES` to `arm64;x86_64`, so the game and SDL2 are
built for both architectures in one go:

```bash
lipo -info build/macos/neo_wolf
# Architectures in the fat file: build/macos/neo_wolf are: x86_64 arm64
```

The minimum macOS version is 10.15 (`CMAKE_OSX_DEPLOYMENT_TARGET`), because the engine uses
`std::filesystem`. The arm64 slice reports 11.0, the first macOS that runs on Apple Silicon.

For faster local builds of the native architecture only, override the architectures:

```bash
cmake --preset macos -DCMAKE_OSX_ARCHITECTURES=arm64
```

## Running the game

```bash
./build/macos/neo_wolf
```

The game works from **any working directory**. At startup it looks for its content root (the
folder that contains `game/maps` and `game/defs`) in this order and logs the one it uses:

1. `--data <dir>` if given, otherwise the `NEOWOLF_DATA_DIR` environment variable if set. An
   explicit choice is the only place searched: if it has no `game/` folder the game stops with
   an error instead of quietly using another copy.
2. The executable's folder and up to 3 folders above it (so `build/macos/neo_wolf` finds the
   repository's `game/`), then `<executable folder>/../Resources` (a macOS `.app` bundle).
3. The current working directory and its parent.

If no content root is found, the game logs the folders it searched, shows an error dialog and
exits with code 1. An invalid `gameplay.json`, or a first level whose map or entity file is
missing or invalid, also ends startup with an error dialog and exit code 1; the log names the
file and the problem.

Command-line options (`./build/macos/neo_wolf --help` lists them):

| Option | Meaning |
|---|---|
| `--data <dir>` | Use the content in `<dir>` (the folder that contains `game/`). |
| `--pref-dir <dir>` | Keep the log and `progress.json` in `<dir>` instead of the per-user folder (overrides `NEOWOLF_PREF_DIR`; used by every automated run). |
| `--level <N>` | Start campaign level N (1-based index into `game/defs/campaign.json`, 10 levels) directly, skipping the menu and the unlocked-level check. For development and tests. |
| `--preview <screen>` / `--screenshot <file.bmp>` | Developer tools: show one screen with sample data / save the last frame as a BMP on exit ([TESTING.md](TESTING.md#screen-previews)). |
| `--frames <N>` | Run N frames, then exit with code 0. For automated smoke tests; error dialogs are suppressed. |
| `--windowed` / `--fullscreen` | Start in a window (default) or in desktop fullscreen. |
| `--version` / `--help` | Print the version or the usage and exit. |

Exit codes: `0` normal exit, `1` startup failed (for example content not found), `2` bad command line.

The log goes to the terminal and to `~/Library/Application Support/NeoWolf/NeoWolf/neowolf.log`
(the previous run's log is kept as `neowolf.prev.log`). See [TROUBLESHOOTING.md](TROUBLESHOOTING.md).

### Headless runs

With SDL's dummy drivers no window or sound device is needed. There is no GPU renderer then, so
the game falls back to SDL's software renderer by itself (no `SDL_RENDER_DRIVER` needed any more):

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/macos/neo_wolf --frames 120
```

Frame pacing: the game uses vsync when the renderer supports it; otherwise (software renderer,
dummy driver, or `SDL_RENDER_VSYNC=0`) it caps the frame rate at the display's refresh rate
(60 FPS if unknown). 120 frames therefore take about 2 seconds headless.

## Build options

| CMake option | Default | Meaning |
|---|---|---|
| `NEOWOLF_SYSTEM_SDL2` | `OFF` | `ON` uses an installed SDL2 (`find_package(SDL2 CONFIG)`) instead of building the pinned source release. |
| `NEOWOLF_WERROR` | `OFF` | Treat compiler warnings as errors for Neo-Wolf's own targets (never for SDL). |
| `NEOWOLF_BUILD_TESTS` | `ON` | Build `neo_wolf_tests` and register the CTest tests. |

Pass options on the configure command line, for example `cmake --preset macos -DNEOWOLF_WERROR=ON`.

### Using a system SDL2 (`NEOWOLF_SYSTEM_SDL2=ON`)

```bash
brew install sdl2
cmake -B build/system-sdl -G Ninja -DNEOWOLF_SYSTEM_SDL2=ON -DCMAKE_OSX_ARCHITECTURES=$(uname -m)
cmake --build build/system-sdl
ctest --test-dir build/system-sdl
```

Caveats:

- On Homebrew, `sdl2` is now **sdl2-compat**: the SDL2 API implemented on top of SDL3. It works
  (the full test suite passes against sdl2-compat 2.32.72), but it is not the same code as the
  pinned SDL2 release.
- The game then links `/opt/homebrew/opt/sdl2-compat/lib/libSDL2-2.0.0.dylib` (on Intel Macs
  Homebrew lives in `/usr/local`, so `/usr/local/opt/sdl2-compat/...`), so the binary only runs
  on machines with that Homebrew package installed. Do not ship it.
- Homebrew libraries are built for your macOS version, so the linker warns
  `building for macOS-11.0, but linking with dylib ... built for newer version`. The warning is
  harmless for local builds; you can silence it with `-DCMAKE_OSX_DEPLOYMENT_TARGET=<your macOS version>`.
- Homebrew bottles are single-architecture, so build for your native architecture only
  (`$(uname -m)` above gives `arm64` or `x86_64`).

## Offline builds

SDL2 is fetched with CMake's `FetchContent` from the URL pinned in `CMakeLists.txt`
(`NEOWOLF_SDL2_URL`, checked against `NEOWOLF_SDL2_SHA256`). To build without network access,
point CMake at a local copy of the same SDL2 release with the standard
`FETCHCONTENT_SOURCE_DIR_SDL2` variable:

```bash
# once, while online:
mkdir -p ~/src && cd ~/src
curl -LO https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz
shasum -a 256 SDL2-2.32.10.tar.gz   # must match NEOWOLF_SDL2_SHA256 in CMakeLists.txt
tar -xzf SDL2-2.32.10.tar.gz

# any time, offline (from the repository root):
cmake --preset macos -DFETCHCONTENT_SOURCE_DIR_SDL2=$HOME/src/SDL2-2.32.10
```

The local copy must be the release pinned in `CMakeLists.txt` (`NEOWOLF_SDL2_VERSION`); a Git
checkout of tag `release-2.32.10` has the same sources. CMake does not check the hash of a
local copy. An existing build directory keeps its downloaded copy in
`build/<preset>/_deps/sdl2-src/` and does not download it again when it re-configures.

## Troubleshooting

| Symptom | Fix |
|---|---|
| `xcrun: error: invalid active developer path` | Install the Command Line Tools: `xcode-select --install`. |
| CMake rejects `CMakePresets.json` (an error about the presets file version) | CMake is older than 3.25. `brew upgrade cmake`. |
| `Cannot use disabled configure preset ... "windows-msvc"` | That preset is for another OS (presets have a host condition). Use `macos`. |
| `No such configure preset ...` | Typo in the preset name, or you are not in the repository root. `cmake --list-presets` shows the valid names. |
| `CMake was unable to find a build program corresponding to "Ninja"` | `brew install ninja`. |
| SDL download fails (an error from the `sdl2-populate` download step) | Check network or proxy settings, or use [Offline builds](#offline-builds). |
| `SHA256 hash of ... SDL2-2.32.10.tar.gz does not match expected value` / `Hash mismatch, removing...` | The downloaded file is not the pinned release (for example a proxy error page). CMake deletes the bad file; fix the network problem and configure again. |
| Strange errors after changing options or upgrading CMake | Reconfigure from scratch: `cmake --preset macos --fresh`, or `rm -rf build/macos`. |
| `devkit_tests` / `content_validation` are missing from `ctest` | CMake did not find Python 3. Check `python3 --version`, then reconfigure with `--fresh`. |
| `Game content not found` (error dialog or log) | The `game/` folder is not next to/above the executable. Pass `--data <folder that contains game/>` (see [Running the game](#running-the-game)). |
| Other problems when running the game | See [TROUBLESHOOTING.md](TROUBLESHOOTING.md). |
