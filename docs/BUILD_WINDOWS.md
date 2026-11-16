# Building Neo-Wolf on Windows

> **Status: not yet built or tested on Windows.** Presets and scripts were written and reviewed
> on macOS (and the sources cross-compile with MinGW); Windows CI (Phase 2) will verify them.
> CI runs both the CMake presets and the scripts (`build.cmd`, `build.ps1 -Config Debug`, job
> `windows-scripts`). Until CI has passed, please report any problem you hit.

For macOS see [BUILD_MACOS.md](BUILD_MACOS.md); for the test suites see [TESTING.md](TESTING.md).

## Prerequisites

| Tool | Notes |
|---|---|
| **Visual Studio 2022 or newer** (any edition: Community, Professional, Enterprise, or *Build Tools for Visual Studio*) | Install the **Desktop development with C++** workload. Make sure the **C++ CMake tools for Windows** component is selected. It is normally part of that workload, but check in the installer. |
| **CMake 3.25 or newer** | `build.ps1` prefers the CMake bundled with the Visual Studio it selects (the *C++ CMake tools for Windows* component). A CMake on `PATH` (for example from cmake.org) also works, but must be new enough to know your Visual Studio: Visual Studio 2026 needs CMake 4.2 or newer (the first release with the `Visual Studio 18 2026` generator). `CMakePresets.json` uses preset schema version 6, and `CMakeLists.txt` needs 3.25. |
| **Python 3** | Needed for the DevKit tests and content validation. Install from python.org and tick "Add python.exe to PATH". The Microsoft Store `python` stub is not enough. |

**You do not need vcpkg, an SDL2 install or Git** (Git is only needed to clone the repository
itself). CMake downloads the official SDL2 2.32.10 source tarball, checks its SHA-256 and links
SDL2 statically. Internet access is needed the first time a build directory is configured.

**No runtime installers should be needed for players.** The build is configured to link SDL2 and
the Microsoft C/C++ runtime statically (`/MT`), so `neo_wolf.exe` should need neither `SDL2.dll`
nor the Visual C++ Redistributable (to be confirmed by Windows CI).

## Quick start

From a normal Command Prompt in the repository root:

```cmd
rem Configure, build and run all tests (Release):
build.cmd
rem The same in Debug:
build.cmd Debug
```

From PowerShell in the repository root:

```powershell
# Configure, build and run all tests (Release); either of:
.\build.cmd
.\build.ps1
# The same in Debug; either of:
.\build.cmd Debug
.\build.ps1 -Config Debug
```

Run these from a console window: `build.cmd` does not pause at the end, so a double-clicked
window would close before you can read the result.

`build.cmd` just runs `build.ps1`. The script:

1. uses `vswhere.exe` to find the newest Visual Studio 2022 or newer (any edition) with the C++
   x64 tools, and explains what to install if none is found;
2. uses the CMake bundled with that Visual Studio, or else `cmake` from `PATH`, and checks that
   it is 3.25+ and has the generator for that Visual Studio;
3. configures with the `windows-msvc` preset pinned to that Visual Studio (it passes
   `-G "Visual Studio 17 2022"` or `-G "Visual Studio 18 2026"` and
   `-DCMAKE_GENERATOR_INSTANCE=<install path>`, and ignores any `CMAKE_GENERATOR` environment
   variable), then builds and tests with the `windows-msvc` (Release) or `windows-msvc-debug`
   (Debug) build and test presets, stopping with the failing step's exit code if anything
   fails (including any test).

If PowerShell refuses to run the script, use `build.cmd`, which passes `-ExecutionPolicy Bypass`
for that one invocation.

## Using the CMake presets directly

From a **Developer PowerShell for VS** (or any shell where `cmake` is on `PATH`):

```powershell
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
# Debug uses the same configure (the Visual Studio generator is multi-config):
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

| Preset | Kind | What it is |
|---|---|---|
| `windows-msvc` | configure, build, test | x64; configures both Debug and Release into `build\windows-msvc\`. The build/test presets select Release. |
| `windows-msvc-debug` | build, test | Debug, using the `windows-msvc` configure preset (there is no separate Debug configure). |
| `ci-windows-msvc` | configure, build, test | `windows-msvc` with `NEOWOLF_WERROR=ON` (warnings are errors), Release. Used by CI. |

### Generator choice

The Windows presets do **not** name a generator, so CMake uses its default on Windows: the
**Visual Studio generator for the newest Visual Studio installed**. They only set the platform
to x64. That means:

- No particular Visual Studio version is hard-coded. VS 2022 and newer work, and the preset does
  not need to change when you upgrade.
- A Developer prompt is **not** required. The Visual Studio generator finds the compiler itself.
  A plain `cmd` or PowerShell works as long as `cmake` is on `PATH`, and `build.cmd`/`build.ps1`
  handle that too.
- The CMake you use must know your Visual Studio version. The CMake bundled with Visual Studio
  always does. A standalone CMake may not: Visual Studio 2026 needs CMake 4.2 or newer.
- If you set the `CMAKE_GENERATOR` environment variable (for example to `Ninja`), running the
  presets by hand will not work. Unset it for this project. `build.ps1` clears it for its own
  process.
- With several Visual Studio versions installed, plain `cmake --preset windows-msvc` picks the
  newest. `build.ps1` pins the one vswhere selected (also the newest) explicitly.

Output goes to `build\<configure preset>\<Config>\`, for example
`build\windows-msvc\Release\neo_wolf.exe`, `build\windows-msvc\Debug\neo_wolf.exe` (Debug) or
`build\ci-windows-msvc\Release\neo_wolf.exe` (CI).

## Running the game

Run the game **from the repository root**, because the engine currently finds `game\` relative
to the working directory (fixed in Roadmap Phase 3):

```cmd
build\windows-msvc\Release\neo_wolf.exe
rem or, after a Debug build:
build\windows-msvc\Debug\neo_wolf.exe
```

The game currently uses the console subsystem, so a console window opens next to the game
window. Phase 3 switches to a normal windowed (WIN32 subsystem) application.

## Compiler settings

- MSVC flags for Neo-Wolf's own code: `/W4 /permissive- /utf-8 /EHsc /Zc:__cplusplus`.
  With `NEOWOLF_WERROR=ON` (the `ci-*` presets) `/WX` is added. SDL is never built with these flags.
- C runtime: configured as `CMAKE_MSVC_RUNTIME_LIBRARY = MultiThreaded$<$<CONFIG:Debug>:Debug>`
  (static CRT, `/MT` or `/MTd`) for every target, SDL included (`SDL_FORCE_STATIC_VCRT=ON`,
  `SDL_LIBC=ON`; SDL 2.32.10 enables policy CMP0091 itself, so it honours this setting). A value
  passed with `-DCMAKE_MSVC_RUNTIME_LIBRARY=...` takes precedence. Not yet confirmed on Windows
  (Windows CI will check it).
- SDL is added as a `SYSTEM` dependency, so its headers do not trigger our warning flags.
- `SDL_MAIN_HANDLED` is defined, so `<SDL.h>` never renames `main`, and `SDL2main` is not used.

## Build options

| CMake option | Default | Meaning |
|---|---|---|
| `NEOWOLF_SYSTEM_SDL2` | `OFF` | `ON` uses an installed SDL2 (`find_package(SDL2 CONFIG)`, e.g. from vcpkg) instead of the pinned source release. A shared SDL2 then needs `SDL2.dll` next to the exe, which this build does not copy. |
| `NEOWOLF_WERROR` | `OFF` | Warnings are errors for Neo-Wolf's targets. |
| `NEOWOLF_BUILD_TESTS` | `ON` | Build the tests and register them with CTest. |

Offline builds work as on macOS: download and extract the pinned SDL2 source tarball once, then
configure with `-DFETCHCONTENT_SOURCE_DIR_SDL2=C:\path\to\SDL2-2.32.10`. See
[BUILD_MACOS.md](BUILD_MACOS.md#offline-builds).

## Troubleshooting

| Symptom | Fix |
|---|---|
| `vswhere.exe not found` or `No Visual Studio 2022+ installation ...` | Install Visual Studio 2022 or newer, or Build Tools, with *Desktop development with C++*. |
| `Visual Studio's bundled CMake is missing and cmake is not on PATH` | Add the *C++ CMake tools for Windows* component, or install CMake from cmake.org. |
| `... is too old: CMakePresets.json needs CMake 3.25` | Update Visual Studio, or install a current CMake. |
| `... has no 'Visual Studio 18 2026' generator` | The CMake in use predates Visual Studio 2026. Use CMake 4.2 or newer (or the CMake bundled with Visual Studio 2026). |
| `... is newer than this script knows` | A Visual Studio major version newer than 18. Update `build.ps1` (its `VsGenerators` map) and use a CMake that has the matching generator. |
| `Generator ... does not match the generator used previously` or a `CMAKE_GENERATOR_INSTANCE` mismatch | `build\windows-msvc\` was configured with a different Visual Studio (or by hand with another generator). Delete `build\windows-msvc\` and run again. |
| `Generator ... does not support platform specification` | `CMAKE_GENERATOR` is set in your environment (when running presets by hand). Unset it. |
| SDL download fails, or `... does not match expected value` / `Hash mismatch` | Check network and proxy settings, or use an offline SDL copy (above). A hash mismatch means the downloaded file is not the pinned SDL2 2.32.10 tarball (for example a proxy error page); CMake deletes it, so just configure again once the network is fixed. |
| `SDL_CreateRenderer failed: Couldn't find matching render driver` | No GPU renderer is available (virtual machine, Remote Desktop session, no GPU driver). Set `SDL_RENDER_DRIVER=software` before starting the game (`set SDL_RENDER_DRIVER=software` in cmd, `$env:SDL_RENDER_DRIVER="software"` in PowerShell). Known issue B11, fixed in Roadmap Phase 3. |
| `devkit_tests` / `content_validation` are missing | CMake did not find Python 3. Install it from python.org (on `PATH`), then run `cmake --preset windows-msvc --fresh`. |
| The game starts with no content | Start it from the repository root. |
