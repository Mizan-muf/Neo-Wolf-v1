# Known Issues

This is the honest list of open defects and gaps. Each entry names the roadmap phase that fixes it ([ROADMAP.md](ROADMAP.md)).
When an issue is fixed, move it to the "Resolved" section and reference the change.

Baseline: commit `319d7af` (audit of 2026-11-15). That baseline builds with zero warnings, all 23 native tests and 19 DevKit tests pass, and `validate --strict` passes. The issues below are not caught by those tests.

## Open

### Build and platform
| # | Issue | Evidence | Phase |
|---|---|---|---|
| B4 | Partly fixed in Phase 1. `build.sh` no longer hides test failures and runs CTest (verified on macOS). `build.ps1`/`build.cmd` were rewritten (vswhere, any VS 2022+ edition, VS-bundled CMake preferred, generator and VS instance pinned, run CTest, propagate exit codes) but have **not yet been run on Windows** (no PowerShell was available; reviewed by eye only). The CI job `windows-scripts` runs `build.cmd` (Command Prompt) and `build.ps1 -Config Debug` (Windows PowerShell 5.1) to verify them once CI runs. | `build.sh`, `build.ps1`, `build.cmd` | 1 → verify in 2 |
| B5 | CI is written but has never run. `.github/workflows/ci.yml` (macOS, Windows, Linux jobs) first runs when the branch is pushed. Until then nothing has been built or tested on Windows or Linux, including the static MSVC runtime (no `SDL2.dll`, no VC++ redistributable). | `.github/workflows/ci.yml` | 2 |
| B6 | Content is found only relative to the current working directory (or its parent). Launching from Finder, Explorer or another folder loses all content. A failed level load is not fatal. | `Engine.Core.cpp:17-27`, `Engine.Init.cpp:20-29,116-118` | 3 |
| B7 | Audio is initialised together with video, so a machine without an audio device cannot start the game. | `PlatformSDL.cpp:68` | 3 |
| B8 | Frame pacing: uncapped busy loop, frame presented twice, no delta-time clamp. | `App.cpp:22-39`, `Engine.Render.cpp:103,180`, `PlatformSDL.cpp:152` | 3 |
| B9 | The map and entity loaders do not strip `\r`, so CRLF files fail in the engine but pass DevKit validation. | `MapLoader.cpp:41-45` | 3 |
| B10 | The game never enables log-file output (`SetLogFilePath` is only called by tests). | `engine/core/Log.cpp:29` | 3 |
| B11 | The renderer is created with `SDL_RENDERER_ACCELERATED` only. Where no GPU renderer exists (headless runs, `SDL_VIDEODRIVER=dummy`, some VMs/remote desktops) the game exits with `SDL_CreateRenderer failed: Couldn't find matching render driver`. Workaround: `SDL_RENDER_DRIVER=software`. Found during Phase 1 verification. | `PlatformSDL.cpp:86` | 3 |
| B12 | Linux is best effort and has not been built anywhere. The CI Linux job installs only `ninja-build`, so SDL is built there without X11/Wayland/audio backends: it can only show that the code compiles, the tests pass and the game runs headless with the dummy drivers. Ubuntu 22.04's packaged CMake (3.22) is older than the required 3.25. | `.github/workflows/ci.yml`, `CMakeLists.txt` | 2 (CI) / out of scope beyond that |

### Content and gameplay
| # | Issue | Evidence | Phase |
|---|---|---|---|
| C1 | The engine loads `level01.entities.json` with a CSV-only parser. Every line fails, and the game silently falls back to a hardcoded entity layout, so editing level files has no effect. | `Engine.h:78`, `EntityPlacementLoader.cpp`, `Engine.Core.cpp:139-156` | 4 |
| C2 | `gameplay.json` is never loaded by the game (only by a test). Its parser takes the first match of a key, so duplicate keys collide: weapon damage reads as 12 (should be 35) and player move speed as 1.6 (should be 3.0). The test only asserts `> 0`. | `ContentConfig.cpp:79-176`, `LogicTests.cpp:352-356` | 4 |
| C3 | A `decoration` placement creates an `EntityType::Trigger`, so touching a decoration wins the level. This is latent today only because of C1. Decoration sprites are also dropped after the first frame. | `Engine.Core.cpp:134-136`, `Engine.Update.cpp:217-228` | 4 |
| C4 | Pickup type (health or ammo) is decided by entity-count parity, not by data. HUD pickup messages are hardcoded. | `Engine.Core.cpp:116`, `Engine.Update.cpp:154-156` | 4 |
| C5 | `textures.json`, `sprites.json` and `entities.json` are not read by anything. | grep | 4 |
| C6 | There is no level list or progression. Victory only allows replaying or returning to the menu. | `Engine.Update.cpp:70-78` | 5 |
| C7 | Menu → Start does not reset the world. After Game Over → Menu → Start, the player has 0 HP. | `Engine.Update.cpp:51-57` | 5 |
| C8 | Menu, pause and end screens are drawn larger than the 320×240 framebuffer and are clipped. | `UiRenderer.cpp:188-235` | 5 |
| C9 | The exit trigger is invisible in the 3D view. The minimap is on by default and does not scale. | `Map.cpp:60-61`, `Engine.h:62`, `Engine.Render.cpp:56` | 5 |
| C10 | Enemy AI skips the `Alert` and `Attack` states, wakes up through walls, and has no line-of-sight check. A door can close on an enemy. | `InteractionRules.cpp:97-120,67-75` | 6 |
| C11 | There is only one enemy type, and its stats are hardcoded. | `Engine.Core.cpp:110-112` | 6 |

### DevKit and packaging
| # | Issue | Evidence | Phase |
|---|---|---|---|
| D1 | The packager's fallback compile omits `engine/interop/*.cpp` and fails to link. It then reports success with a bundle that contains no executable, and never bundles the SDL runtime. | `packager.py:61-105,158-167,279` | 9 |
| D2 | The validator does not check campaign structure, a single player, exit reachability, or door/trigger tile pairing. | `validator.py` | 7 |
| D3 | `inspect-map` crashes (KeyError) when the spawn point is inside a wall, and always exits 0. | `cli.py:134-138` | 7 |
| D4 | `doctor` does not check SDL2, despite the docs. | `doctor.py` | 7 |

### Documentation
| # | Issue | Phase |
|---|---|---|
| X1 | `Checklist.md` ticks items that are not implemented (CI, JSON content loading, exported C API, C# layer, log file, coverage target). | 10 |
| X2 | The controls are documented incorrectly in `README.md`, `GAMEPLAY_API.md` and the packaged README. | 3 / 10 |
| X4 | There is no LICENSE file, and the provenance of the existing assets is undocumented. | 10 (needs owner decision) |

## Resolved

| # | Issue | Fix | Phase |
|---|---|---|---|
| B1 | Hardcoded `/home/mizan/deps/...` SDL paths in `Makefile` and `CMakeLists.txt`. | CMake now builds a pinned SDL2 (official 2.32.10 source tarball, SHA-256 checked) from source via FetchContent, or uses `find_package(SDL2 CONFIG)` with `NEOWOLF_SYSTEM_SDL2=ON`. The Makefile is a thin wrapper around the CMake presets. Verified on macOS with both SDL modes. | 1 |
| B2 | Test target linked SDL2 only via the hardcoded paths; `CommandBridge.cpp` listed twice. | Every engine source is listed once in the `neowolf_engine` static library, which the game and tests link. SDL2 comes through that library. | 1 |
| B3 | `-lpthread -ldl -lm` on every platform; empty `TARGET_RUNTIME_DLLS`. | The literal flags are gone (SDL's CMake targets supply their own link dependencies). SDL is static, so there is no DLL to copy, and the MSVC `POST_BUILD` copy step was removed. | 1 |
| X3 | No macOS build documentation. | [BUILD_MACOS.md](BUILD_MACOS.md), plus [BUILD_WINDOWS.md](BUILD_WINDOWS.md) and [TESTING.md](TESTING.md). | 1 |
