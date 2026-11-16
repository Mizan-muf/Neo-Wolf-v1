# Contributing to Neo-Wolf

Thanks for helping. This page is the short version of how the project is built, tested and
changed. The plan and its status are in [docs/ROADMAP.md](docs/ROADMAP.md), open problems in
[docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md), and how the code fits together in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Prerequisites

| | macOS (primary) | Windows (CI-verified) | Linux (best effort) |
|---|---|---|---|
| Compiler | Apple clang from the Xcode Command Line Tools (`xcode-select --install`) | Visual Studio 2022 or newer, any edition, with *Desktop development with C++* | a C++17 compiler (gcc or clang) |
| CMake | 3.25+ (`brew install cmake`) | 3.25+ (the one bundled with Visual Studio is used if present) | 3.25+ (Ubuntu 22.04's 3.22 is too old) |
| Ninja | `brew install ninja` | not needed (Visual Studio generator) | `ninja-build` |
| Python 3 | `/usr/bin/python3` from the Command Line Tools | python.org install, on `PATH` | system Python 3 |
| SDL2 | nothing to install: CMake downloads the pinned SDL2 2.32.10 source (SHA-256 checked) and links it statically | same | same, but SDL needs the X11/Wayland and audio development packages ([BUILD_AND_RELEASE.md](docs/BUILD_AND_RELEASE.md)) |

Details and troubleshooting: [BUILD_MACOS.md](docs/BUILD_MACOS.md),
[BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md). The first configure needs internet access for the
SDL2 download ([offline builds](docs/BUILD_MACOS.md#offline-builds)). The Python DevKit uses
the standard library only (Python 3.8+); nothing needs `pip install`.

## Build and test

macOS (and Linux), from the repository root:

```bash
./build.sh                         # configure, build and run every test (Release); ./build.sh Debug
# or step by step:
cmake --preset macos
cmake --build --preset macos
ctest --preset macos               # all tests: native, headless game runs, autoplay, DevKit
```

Windows (Command Prompt or PowerShell in the repository root; taken from
[BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md), verified in CI only, not yet on a Windows PC):

```cmd
build.cmd
rem or, in a Developer PowerShell for VS:
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
```

Useful while working (macOS paths; on Windows the executables are in
`build\windows-msvc\Release\`):

```bash
ctest --preset macos -R native.map_queries     # one test
ctest --preset macos -L devkit                 # a label: native, game, autoplay, devkit
python3 tools/devkit/devkit.py test            # only the DevKit's Python tests
python3 tools/devkit/devkit.py validate --strict   # content validation
build/macos/neo_wolf_autoplay --all            # the bot plays the whole campaign
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/macos/neo_wolf --frames 120   # headless run
```

Before you push, run the same strict build CI uses (warnings are errors):

```bash
cmake --preset ci-macos && cmake --build --preset ci-macos && ctest --preset ci-macos
```

Everything about the test layers, filters and the bot: [TESTING.md](docs/TESTING.md).

## Adding a test

**Native (C++) test** ([TESTING.md](docs/TESTING.md#adding-a-native-test)):

1. Write `void TestSomething()` in the anonymous namespace of `tests/LogicTests.cpp`, using
   `TEST_ASSERT` / `TEST_ASSERT_NEAR`, and `ResolveRepoPath("game/...")` for repository files.
2. Add it to the `kTests` table at the bottom of that file:
   `{"something", "Something works", &TestSomething},`.
3. Add the same id to `NEOWOLF_NATIVE_TESTS` in `CMakeLists.txt`.

The **registry rule**: every test in `kTests` must have a CTest entry and the other way round.
The CTest test `native_test_registry` fails, naming the id, if the two lists drift apart.
Tests that need exact level details use the frozen levels in `tests/fixtures/reference/`, not
the shipped campaign, which may change ([TESTING.md](docs/TESTING.md#test-layers)).

**DevKit (Python) test**: add a `test_*.py` file (standard `unittest`) under
`tools/devkit/tests/`. It is found automatically and runs inside the CTest test
`devkit_tests`.

## Adding a level

Follow [docs/LEVEL_AUTHORING.md](docs/LEVEL_AUTHORING.md) step by step: plan it against the
campaign design ([docs/levels/README.md](docs/levels/README.md)), create the stub with
`create-map --level-id`, draw the map, place the entities, then `validate --strict`, the
autoplay bot, a look at it, and its level page (kept in step with the map by
`python3 tools/devkit/level_docs.py`). The campaign must keep exactly 10 levels unless the
roadmap changes (`content_validation_campaign_size`).

## Adding an asset

Formats, sizes and the generators are in [docs/ASSET_GUIDE.md](docs/ASSET_GUIDE.md); every
asset's origin and licence is recorded in [docs/ASSET_CREDITS.md](docs/ASSET_CREDITS.md). The
shipped assets are made by deterministic generators in `tools/devkit/` (`gen_*.py`, listed in
`ASSET_CREDITS.md`); change the generator and regenerate rather than editing the files by hand.
Only add an asset you made yourself or whose licence allows shipping it with the game, and
record it in `ASSET_CREDITS.md` in the same change. Third-party code goes in `third_party/`
with its licence ([THIRD_PARTY_LICENSES.md](docs/THIRD_PARTY_LICENSES.md)).

## Coding conventions (as they are today)

There is no formatter or linter configuration yet. Roadmap Phase 12 will agree the review
criteria and record them in `docs/CODING_STANDARDS.md`, add `clang-format`, `clang-tidy` and
`ruff`, and run them in CI ([ROADMAP.md](docs/ROADMAP.md#phase-12--clean-code-review)). Until
then, match the surrounding code:

- **C++17**, no compiler extensions. Builds must stay free of warnings with `-Wall -Wextra
  -Wpedantic` (clang, gcc) and `/W4 /permissive-` (MSVC); CI treats warnings as errors.
- **Names:** `PascalCase` for types and functions, `camelCase` for locals and parameters,
  `trailingUnderscore_` for class members, `kPascalCase` for constants, `enum class` for
  enums. Files are named after their main type (`Raycaster.cpp`).
- **Layout:** 4-space indentation, opening braces on the same line, lines mostly within 120
  columns, `#pragma once`.
- **Includes:** the file's own header first, then standard headers, then project headers by
  their full path from the repository root (`#include "engine/world/Map.h"`).
- **SDL** only where [ARCHITECTURE.md](docs/ARCHITECTURE.md#dependencies) lists it; game logic
  must stay SDL-free so it can be tested headless.
- **Errors:** loaders return `bool` and append readable messages (naming the file, entry and
  field) to a `std::vector<std::string>`; the caller logs them with `LogError` /
  `LogWarning`. Invalid content fails loudly; there are no silent fallbacks for level data.
- **Data, not literals:** gameplay numbers belong in `game/defs/gameplay.json`; engine-side
  constants are named (`kExitReach`, `kEnemyRepathInterval`).
- **Values exposed through the C API** (`GameState`, `EntityType`, the interop enums) are
  append-only ([C_API.md](docs/C_API.md)).
- **Comments** explain why, not what.
- **Python (DevKit):** standard library only, Python 3.8+, type hints, `unittest`.
- **Docs:** describe what the code does now and how it was verified; update the docs that a
  change affects in the same commit. `python3 tools/devkit/doc_links.py` (also a DevKit test)
  checks every relative link and anchor.

## Commit messages

The commits on `completion/v1` use one commit per roadmap phase, in this form:

```text
feat: phase 6 - enemy AI, archetypes, key cards and locked doors

- Line of sight via the raycaster DDA; 8-connected BFS pathfinding ...
- Archetypes guard / brute / runner in gameplay.json "enemies" ...

Verified on macOS: 79/79 tests, ci-macos -Werror, validate --strict, ...
```

- Subject: a type (`feat`, `build`, `ci`, `chore`; `fix` and `docs` fit the same scheme),
  then `phase N - ` when the change belongs to a roadmap phase, then a short summary in
  lower case.
- Body: one bullet per change, naming the known-issue numbers it resolves (`fixes C1-C5`).
- A final "Verified on ..." paragraph saying what was run and where; say what was **not**
  verified.
- The phase commits on this branch end with a `Co-Authored-By:` trailer naming the AI
  assistant that co-wrote them.

## Continuous integration

`.github/workflows/ci.yml` runs on pushes to `main` and `completion/**`, on `v*` tags, on pull
requests and on demand ([TESTING.md](docs/TESTING.md#continuous-integration)):

- macOS, Windows (MSVC x64) and Linux: configure and build with the `ci-*` presets (warnings
  as errors), run every CTest test, a 300-frame headless smoke run, and the autoplay bot over
  the whole campaign;
- `windows-scripts`: `build.cmd` and `build.ps1 -Config Debug`, and the exit codes of the
  package scripts;
- `package` (macOS and Windows): builds, smoke-tests and uploads the release packages (added in
  Phase 9; status in [KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md), D5).

A change is ready when all jobs are green. Do not tick a roadmap item until it has been
verified by a test, a CI run or a recorded manual check, and record what is still unverified in
[KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

## Licence of contributions

Neo-Wolf is licensed under the [PolyForm Noncommercial License 1.0.0](LICENSE.md): it is
source-available, not OSI open source, and commercial use needs the copyright holder's
permission. The project has no contributor licence agreement or sign-off process yet. Only
contribute code and assets you have the right to license under these terms, and say in the
pull request if any part comes from elsewhere. Check with the copyright holder before
contributing anything large.
