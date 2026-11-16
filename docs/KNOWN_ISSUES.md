# Known Issues

This is the honest list of open defects and gaps. Each entry names the roadmap phase that fixes it ([ROADMAP.md](ROADMAP.md)).
When an issue is fixed, move it to the "Resolved" section and reference the change.

Baseline: commit `319d7af` (audit of 2026-11-15). That baseline builds with zero warnings, all 23 native tests and 19 DevKit tests pass, and `validate --strict` passes. The issues below are not caught by those tests.

## Open

### Build and platform
| # | Issue | Evidence | Phase |
|---|---|---|---|
| B4 | Mostly fixed. `build.sh` propagates test failures (verified on macOS). `build.cmd` (Command Prompt, Release) and `build.ps1 -Config Debug` (Windows PowerShell 5.1) built with MSVC and passed the full test suite in CI job `windows-scripts` on the first run (commit `1b10a57`, `windows-latest`). Remaining: not yet run on a real (non-CI) Windows PC. | `build.sh`, `build.ps1`, `build.cmd` | 11 (owner run) |
| B5 | CI is not yet green. First run (commit `1b10a57`): Linux and `windows-scripts` passed; the Windows preset job failed on MSVC warning C4459 (fixed in Phase 3); the macOS job never started because the `macos-14` image is retired (now `macos-latest`). | `.github/workflows/ci.yml` | 2 |
| B12 | Linux is best effort and has not been built anywhere. The CI Linux job installs only `ninja-build`, so SDL is built there without X11/Wayland/audio backends: it can only show that the code compiles, the tests pass and the game runs headless with the dummy drivers. Ubuntu 22.04's packaged CMake (3.22) is older than the required 3.25. | `.github/workflows/ci.yml`, `CMakeLists.txt` | 2 (CI) / out of scope beyond that |
| B13 | `Cmd+Ctrl+F` (the macOS fullscreen shortcut, added in Phase 3) also fires the weapon during play, because `Ctrl` is a fire key. `F11` and `Alt+Enter` do not have this side effect. | `PlatformSDL.cpp` (`MapScancodeToActions`, `IsFullscreenShortcut`) | 5 (controls/menu rework) |
| B14 | The Phase 3 Windows-specific code has only been cross-compiled (MinGW), not run: the WIN32 subsystem for Release builds with `/ENTRY:mainCRTStartup`, attaching to the parent console for `--help`/`--version`, UTF-16 command line and `NEOWOLF_DATA_DIR` conversion, `SDL_HINT_WINDOWS_DPI_SCALING`, and the error box. Windows CI runs `game_smoke_frames`, `game_smoke_missing_data` and the `--frames 300` smoke run, which cover startup, the content search, the software-renderer fallback and the exit codes, but not the console attach, HiDPI scaling, the message box or non-ASCII paths. | `src/main.cpp`, `CMakeLists.txt`, `engine/core/Paths.cpp`, `PlatformSDL.cpp` | 3 → verify on Windows |
| B15 | Automated runs (`ctest`'s `game_smoke_*` tests, `smoke_run.py`) write to the same per-user log file as normal play, so running the tests replaces the player's `neowolf.log` / `neowolf.prev.log`. | `App.cpp` (`StartLogFile`) | 9 or later (option for a log path) |

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
| X4 | There is no LICENSE file, and the provenance of the existing assets is undocumented. | 10 (needs owner decision) |

## Resolved

| # | Issue | Fix | Phase |
|---|---|---|---|
| B1 | Hardcoded `/home/mizan/deps/...` SDL paths in `Makefile` and `CMakeLists.txt`. | CMake now builds a pinned SDL2 (official 2.32.10 source tarball, SHA-256 checked) from source via FetchContent, or uses `find_package(SDL2 CONFIG)` with `NEOWOLF_SYSTEM_SDL2=ON`. The Makefile is a thin wrapper around the CMake presets. Verified on macOS with both SDL modes. | 1 |
| B2 | Test target linked SDL2 only via the hardcoded paths; `CommandBridge.cpp` listed twice. | Every engine source is listed once in the `neowolf_engine` static library, which the game and tests link. SDL2 comes through that library. | 1 |
| B3 | `-lpthread -ldl -lm` on every platform; empty `TARGET_RUNTIME_DLLS`. | The literal flags are gone (SDL's CMake targets supply their own link dependencies). SDL is static, so there is no DLL to copy, and the MSVC `POST_BUILD` copy step was removed. | 1 |
| B6 | Content was found only relative to the working directory (or its parent); a failed level load was not fatal. | `engine/core/Paths.{h,cpp}` resolves one content root at startup (`--data`, `NEOWOLF_DATA_DIR`, the executable's folder and up to 3 parents, `<exe>/../Resources`, then the working directory and its parent; sentinel `game/maps` + `game/defs`); the duplicated helpers are gone. No content root, or a first level that fails to load, logs the searched folders, shows an error box and exits with code 1. Verified on macOS: started from `/`, from a copy in `/tmp/nw-p3 copy dir/`, and with `--data <empty dir>` (exit 1); tests `content_root_resolution`, `game_smoke_frames`, `game_smoke_missing_data`. Launch from Finder (a `.app`) is not verified yet (no bundle until Phase 9). Windows: pending CI. | 3 |
| B7 | Audio was initialised together with video. | `SDL_Init(SDL_INIT_VIDEO)` only; `AudioSystem::Init` calls `SDL_InitSubSystem(SDL_INIT_AUDIO)` and the game runs silently with a warning if that or opening the device fails; the subsystem is shut down before video. Verified on macOS with `SDL_AUDIODRIVER=nonexistent` (runs, exit 0). | 3 |
| B8 | Frame pacing: uncapped busy loop, frame presented twice, no delta-time clamp. | One `SDL_RenderPresent` per frame (in `PlatformSDL::PresentFramebuffer`); renderer created with vsync; without vsync (software renderer, `SDL_RENDER_VSYNC=0`) or while minimised the loop sleeps to the display refresh rate (60 Hz if unknown); dt clamped to 0.1 s; FPS statistics are logged every 10 s instead of printed every second. Verified on macOS: 60.4 FPS headless (cap), about 119 FPS with vsync on a 120 Hz display, 118.6 FPS with `SDL_RENDER_VSYNC=0` (cap at the 120 Hz refresh rate). | 3 |
| B9 | The map and entity loaders did not strip `\r`, so CRLF files failed in the engine. | `MapLoader`, `EntityPlacementLoader` and both key/value parsers (`AudioConfig.cpp`, `ContentConfig.cpp`) strip a trailing `\r`. Test `crlf_content_loading` loads CRLF copies of the shipped map, entity, audio and wall-texture files and compares them with the LF originals; it fails if the `MapLoader` fix is removed (checked). | 3 |
| B10 | The game never enabled log-file output. | `App` starts `<SDL_GetPrefPath("NeoWolf","NeoWolf")>/neowolf.log` at every launch (previous run kept as `neowolf.prev.log`) and logs its path and the content root; see [TROUBLESHOOTING.md](TROUBLESHOOTING.md). Test `log_file_rotation`. Verified on macOS (`~/Library/Application Support/NeoWolf/NeoWolf/`). | 3 |
| B11 | The renderer was created with `SDL_RENDERER_ACCELERATED` only, so headless runs, VMs and remote sessions failed with `Couldn't find matching render driver`. | Accelerated + vsync first, then SDL's software renderer; the choice is logged. `game_smoke_frames` runs with `SDL_VIDEODRIVER=dummy` and `SDL_RENDER_DRIVER` removed from the environment, and `smoke_run.py` no longer sets it. Verified on macOS; Windows/Linux: pending CI. | 3 |
| X2 | The controls were documented incorrectly in `README.md`, `GAMEPLAY_API.md` and the packaged README. | All three now match the bindings in `PlatformSDL.cpp` (including the new fullscreen keys and Esc-to-quit on the menu). | 3 |
| X3 | No macOS build documentation. | [BUILD_MACOS.md](BUILD_MACOS.md), plus [BUILD_WINDOWS.md](BUILD_WINDOWS.md) and [TESTING.md](TESTING.md). | 1 |
