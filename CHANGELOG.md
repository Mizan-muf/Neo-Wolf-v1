# Changelog

All notable changes to Neo-Wolf are recorded here. The format follows
[Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html) as described in
[docs/RELEASE.md](docs/RELEASE.md#versioning).

No version has been tagged yet. The 0.x entries below were written afterwards, on 2026-11-16,
from the commit history; those versions never existed as releases. The source for every entry
is `git log` (commit messages and diffs) on branch `completion/v1`; the known problems come
from the audit of 2026-11-15 ([docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md)).

## [Unreleased]

The completion work toward 1.0.0 (Roadmap Phases 0–10, [docs/ROADMAP.md](docs/ROADMAP.md)),
commits `224e237` … `87681e1` plus the Phase 10 documentation pass. Builds from this branch
report version 0.9.0; Roadmap Phase 12 sets 1.0.0 and tags it.

Verification status: everything below builds and passes its tests on macOS, and the build, the
tests and the autoplay bot over all 10 levels pass headless in Windows and Linux CI (run
43861234139 at `f76875a`). Not yet done: a play-through by hand (known issue C13), a run on a
real Windows PC, and opening the packages by hand from Finder and Explorer (known issue D5). Open problems are
listed in [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).

### Added

- **Campaign of 10 levels** (Phase 8) listed in `game/defs/campaign.json`, each with its own
  page in `docs/levels/`: a tutorial, key cards, hearing, the brute, runners, ambushes, dark
  wings, a three-key chain, a non-linear hub and a finale.
- **Campaign flow** (Phase 5): main menu (New Game, Continue, Level Select, Quit), level-complete
  screen with kills, pickups and time, game over with restart, credits; health and ammo carried
  between levels; restart from the level-entry health and ammo; saved progress
  (`progress.json`, written atomically); unlocked levels selectable. The state machine
  (`GameFlow`) has no SDL dependency and is unit-tested.
- **Exit switch** (`X` tile, used with `E` while facing it) as the way to finish a level
  (Phase 5).
- **Enemy AI** (Phase 6): line of sight through the raycaster, hearing of gunshots through open
  cells, grid path finding, states Idle, Alert, Chase, Attack, Search and Dead, enemy
  separation, corpses, hit flash. Living enemies block the player (Phase 7).
- **Three enemy types**, guard, brute and runner, defined in `gameplay.json` (Phase 6).
- **Key cards and locked doors** in red, blue and gold, with HUD icons and minimap symbols
  (Phase 6).
- **Light zones**: optional `"lights"` rectangles in a level's entity file for dim and dark
  areas (Phase 8, known issue C21).
- **Texture themes** `bunker`, `stone`, `tech` and a 32×32 `classic` theme, and the sprites,
  all drawn by deterministic generators in `tools/devkit/` (Phases 5, 6 and 8).
- **Autoplay bot** `neo_wolf_autoplay` (Phase 7): plays the real engine headless with simulated
  input and reports per level; `--trace` writes an event log (Phase 8). CTest runs it on every
  level and on the whole campaign.
- **Command line** (Phase 3): `--help`, `--version`, `--data`, `--pref-dir`, `--level`,
  `--frames`, `--windowed`, `--fullscreen`, and the developer tools `--preview` and
  `--screenshot` (Phase 5).
- **Runtime robustness** (Phase 3): content found from any working directory and inside a macOS
  app; a log file in the per-user folder; an error box and exit code 1 when content is missing
  or invalid; the game runs without an audio device; HiDPI, a resizable window and fullscreen
  toggles; pause on focus loss.
- **Release packages** (Phase 9): `NeoWolf.app` for macOS (universal, ad-hoc signed, zipped)
  and a zip with `neo_wolf.exe` for Windows (icon, version resource, static runtime), with a
  generated `README.txt`, `LICENSE.md`, third-party notices and `SHA256SUMS`;
  `tools/ci/package_smoke.py` checks them.
- **Build system** (Phase 1): CMake presets for macOS (universal), Windows (MSVC) and Linux;
  SDL2 2.32.10 built from a SHA-256-pinned source tarball and linked statically;
  `build.sh`, `build.cmd`, `build.ps1`.
- **Continuous integration** (Phase 2): GitHub Actions on macOS, Windows and Linux (build with
  warnings as errors, all tests, headless smoke run, the bot over the campaign), a job for the
  Windows build scripts, and a package job (Phase 9).
- **DevKit** additions: campaign validation rules (Phase 7), engine/DevKit parsing parity
  fixtures, `create-map --level-id`, `sync-entities --level`, a rewritten `doctor`,
  `level_docs.py` (level pages match the shipped maps), the `package` command driving CMake,
  and a Markdown link checker (`doc_links.py`, Phase 10).
- **Licence** (Phase 9): PolyForm Noncommercial 1.0.0 in `LICENSE.md` (source-available, not
  OSI open source).
- **Documentation**: build guides for macOS and Windows, testing, troubleshooting, gameplay,
  controls, level authoring, release, the campaign design, `CONTRIBUTING.md`, this changelog,
  and an in-process C API reference (`docs/C_API.md`, Phase 10).

### Changed

- **Data-driven content** (Phase 4): `gameplay.json` is parsed per section with type, range and
  duplicate-key checks and applied to the player, weapon, enemies and pickups; entity
  placements are JSON (CSV still read); invalid content stops the game with a message naming
  the file, entry and field. Stat literals were removed from the engine.
- **UI** re-laid out for the 320×240 framebuffer (Phase 5); the HUD shows the level number and
  name; the minimap is off by default and scales to any map size.
- **Triggers** (`T`) are checkpoints that show a message; they no longer end the level
  (Phase 5).
- **Frame loop** (Phase 3): one present per frame, vsync or a frame cap at the display's refresh
  rate, frame time clamped to 0.1 s, software-renderer fallback.
- **Packaging** (Phase 9): the DevKit packager now drives CMake install and refuses incomplete
  packages; SDL is static, so no runtime library is copied.
- **Documentation pass** (Phase 10): README, `docs/ARCHITECTURE.md` and `docs/GAMEPLAY.md`
  rewritten or corrected against the code; `docs/Checklist.md` reconciled with the code; stale
  documents moved to `docs/history/`.

### Deprecated

- `GameState::Victory` (value 4) in the C API: never entered; setting it selects
  `LevelComplete` (Phase 5).

### Removed

- The hardcoded fallback level layout, the entity-count rule for pickup types, and the unused
  `textures.json`, `sprites.json` and `entities.json` (Phase 4).
- Hardcoded `/home/mizan/...` SDL paths and `-lpthread -ldl -lm` from the build (Phase 1).
- The packager's raw `g++` fallback and the old launch scripts (Phase 9).
- The original 8×8 textures (Phase 8).
- `docs/GAMEPLAY_API.md`, replaced by `docs/C_API.md` and `docs/GAMEPLAY.md` (Phase 10).

### Fixed

Known-issue numbers refer to [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) ("Resolved").

- Build: hardcoded SDL paths (B1), broken test target (B2), platform-specific link flags (B3),
  `build.sh` hiding test failures (B4); no CI (B5).
- Runtime: content found only from the working directory (B6), audio required for startup
  (B7), frame pacing (B8), CRLF files rejected (B9), no log file (B10), no renderer in headless
  runs and VMs (B11), `Cmd+Ctrl+F` also fired the weapon (B13), tests writing to the player's
  log (B15).
- Content: entity files read with a CSV parser so edits had no effect (C1); `gameplay.json`
  never loaded and parsed wrongly (C2); decorations created as level-ending triggers (C3);
  pickup kind decided by entity count (C4); unused definition files (C5).
- Gameplay: no progression (C6); New Game after game over started with 0 HP (C7); screens
  clipped at 320×240 (C8); invisible exit and unscaled minimap (C9); AI skipped states and woke
  through walls (C10); one enemy type (C11); wall textures mirrored (C14); the player could walk
  through enemies (C15); preview screenshot artefact and squeezed close-up sprites (C18).
- DevKit: packager reported success without an executable (D1); missing campaign validation
  (D2); `inspect-map` crash and exit codes (D3); `doctor` did not check SDL2 (D4).
- Documentation: wrong controls (X2), no macOS build guide (X3), no licence file (X4).

## [0.4.0] - 2026-08-26 (back-filled)

Commits `8015bf1` and `319d7af`. The commit message of `8015bf1` claimed Phases 18–26 of the
original checklist complete, and `docs/Checklist.md` was ticked to the end. **The audit of
2026-11-15 found several of those claims unimplemented or broken** (see below).

### Added

- Game states (menu, gameplay, paused, game over, victory), combat with ammo, pickups, doors,
  a trigger that ended the level, level reload and a one-level "vertical slice".
- `game/defs/gameplay.json` (not actually loaded by the game; see below).
- An in-process C API (`engine/interop/EngineInterop.h`) and a command/event queue
  (`CommandBridge`); no shared library and no C# layer.
- The DevKit (`tools/devkit/`): validator, doctor, map and asset inspectors, generators,
  CSV/JSON sync, packager, and its unit tests.
- `Makefile`, `build.sh`, `package.sh`, `package.cmd`, `package.ps1`.
- Documentation: `ARCHITECTURE.md`, `ASSET_GUIDE.md`, `BUILD_AND_RELEASE.md`,
  `CONTENT_DEFINITIONS.md`, `DEVKIT_GUIDE.md`, `GAMEPLAY_API.md`, `MAP_FORMAT.md`, and (in
  `319d7af`) `SECURITY_AND_ARCHITECTURE_REPORT.md`, which rated the code "production ready".

### Changed

- `319d7af`: range check for the `SetGameState` command, map/entity path parsing for the
  `LoadLevel` command, `nw_engine_spawn_particle` uses its arguments, and the C API header
  became includable from C (with opaque command and event types).

### Known problems (found by the 2026-11-15 audit)

At `319d7af` the code built without warnings and its 23 native and 19 DevKit tests passed, but
the tests did not catch:

- Ticked but not implemented: continuous integration, loading the JSON entity files (they were
  read with a CSV parser and a hardcoded layout was used instead), an exported C API (it was
  only compiled in), a C# layer, the log file, and a test-coverage target.
- The build only worked on the original author's machine (hardcoded SDL paths) and `build.sh`
  hid test failures.
- `gameplay.json` was never loaded, and its parser would have read wrong values; a decoration
  became a trigger that ended the level; pickup kinds came from entity order; after game over,
  New Game started with 0 HP; menus were clipped; the exit was invisible; the enemy AI skipped
  states and woke through walls.
- The packager could report success for a package without an executable.
- The controls were documented wrongly; there was no licence.

## [0.3.0] - 2026-04-13 (back-filled)

Commits `b21c026`, `fdcbbf5`, `7061d86`, `e0a96d0` (2026-04-06 to 2026-04-13): Phases 8–17 of
the original checklist.

### Added

- First-person raycaster with textured walls, textured floor and ceiling, distance shading and
  per-cell light levels (not settable from content files until 2026-11).
- Billboard sprites with a wall depth buffer; entities (player, enemy, pickup, door, trigger)
  with door and pickup interaction.
- PPM texture loader with a checkerboard fallback, asset manager, `.cfg` content files, map
  loader and an entity placement loader; one level, `level01`.
- HUD, crosshair, debug overlay and a pause placeholder.
- Particles (hit sparks, pickup sparkles, dust) and sound effects queued on the SDL audio
  device; four WAV files and `audio.cfg`. The WAV files and the decoration sprite were not made
  by the repository's generators and their origin was not recorded (known issue X5; see
  [docs/ASSET_CREDITS.md](docs/ASSET_CREDITS.md)).

### Changed

- `build.cmd` looks for any Visual Studio 2022 edition, not only Professional (`7061d86`).

### Known problems

- The entity JSON file was read with a CSV parser, so the game used a hardcoded layout and
  editing the file had no effect; `textures.json`, `sprites.json` and `entities.json` were
  never read (found by the 2026-11-15 audit).

## [0.2.0] - 2026-03-21 (back-filled)

Commits `214c3da` and `4196cef` (2026-03-20 and 2026-03-21): Phases 3–7 of the original
checklist.

### Added

- A CPU framebuffer presented through an SDL texture; drawing of pixels, lines and rectangles.
- `Vec2`, `Vec2i`, maths and colour helpers, logging.
- Action-based keyboard input (`InputState`) with pressed and released edges.
- A grid map, a player with movement, turning, strafing and wall collision, and a top-down debug
  view.
- The first native unit tests (`tests/LogicTests.cpp`) and learning notes (`docs/Learning.md`,
  now in `docs/history/`).

## [0.1.0] - 2026-03-09 (back-filled)

Commits `4d44c5c` … `2784533` (2026-03-07 to 2026-03-09): project start, Phases 1–2 of the
original checklist.

### Added

- A CMake project that opens an SDL2 window and runs a main loop (SDL2 found through
  `find_package`; Windows build scripts using vcpkg and a fixed Visual Studio 2022
  Professional path).
- The `Engine` class split into `Init`, `Update`, `Render` and `Shutdown` files, an `App`
  host, a `PlatformSDL` wrapper, `InputState` and `TimeState`.
- README and planning documents: the 26-phase `docs/Checklist.md`, `process-flow.md`,
  `structure.md`, `test.md` and an architecture diagram that included a planned C# gameplay
  module (all but the checklist now in `docs/history/`).
