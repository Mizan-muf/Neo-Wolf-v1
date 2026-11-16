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

## 4. Release packages

Packaging is described in full in [RELEASE.md](RELEASE.md) (versioning, package contents,
checksums, the smoke test, the release checklist, and what is not done: no notarization, no
Windows code signing). In short:

```bash
./package.sh                                  # macOS / Linux (or: make package)
python3 tools/ci/package_smoke.py release     # check the result (or: make package-smoke)
```

```cmd
package.cmd                                   rem Windows (or .\package.ps1)
```

The DevKit `package` command behind these scripts configures and builds the package preset
for the OS (`package-macos`, `package-windows-msvc`, `package-linux`), installs it with
`cmake --install` and writes `release/neo-wolf-<version>-<platform>.zip` (macOS: `NeoWolf.app`;
Windows: `neo_wolf.exe`) or `.tar.gz` (Linux), plus `release/SHA256SUMS`. The version comes
from `project(VERSION)` in `CMakeLists.txt`. It fails, without writing an archive, when the
content does not validate, the build fails, or the package lacks the executable, the content,
the README, the licence or the third-party notices.
