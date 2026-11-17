# Changelog

All notable changes to Neo-Wolf are recorded here. The format follows
[Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html) as described in
[docs/RELEASE.md](docs/RELEASE.md#versioning).

1.0.0 is the first tagged release. The 0.x entries below were written afterwards, on
2026-11-16, from the commit history; those versions never existed as releases. The source for every entry
is `git log` (commit messages and diffs) on branch `completion/v1`; the known problems come
from the audit of 2026-11-15 ([docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md)).

## [Unreleased]

### Added

- **The Test Chamber** ([docs/levels/testchamber.md](docs/levels/testchamber.md)): a development
  level outside the campaign, on the main menu, that the game can open straight into. A hub
  with signposted areas: an enemy gallery (a cell per enemy type behind its own door, and a
  mixed group), a pickups row (health, ammo, the three key cards), a lock test (red, blue and
  gold doors in a row; the gold room holds the exit switch), a props row (seven vases), an
  18-cell weapon range with guards at 9, 11 and 15 cells from a firing booth (out of their sight
  and hearing until hit; the weapon reaches 12) and wall markers every 3 cells, and a dark room
  at light level 0.2. Its exit switch restarts it; it never writes `progress.json`
  or unlocks a level; the HUD shows `TEST CHAMBER` instead of a level number. Files:
  `game/maps/testchamber.map`, `game/defs/testchamber.entities.json` (and its CSV export).
- **`campaign.json` `"testLevel"` and `"bootIntoTestLevel"`**: a level outside the campaign (the
  fields of a campaign level) and a flag to start the game in it. Both are validated by the
  engine and by `validate` (bad definitions are errors; the test level gets the campaign
  level rules), and documented in [CONTENT_DEFINITIONS.md](docs/CONTENT_DEFINITIONS.md#test-level).
  `--level N` and `--preview` still start where they say; the C API, the native tests and the
  autoplay bot ignore the flag (`EngineOptions::testLevelBoot`), so the bot's campaign report is
  unchanged. The main menu gets a **TEST CHAMBER** item (the test level's name) whenever a test
  level is defined.
- **A trigger's `"message"`**: the HUD text a walk-over trigger shows instead of
  `CHECKPOINT REACHED` (1–40 bytes of well-formed UTF-8; no comma, control character or space
  at either end, so that the CSV export holds it unchanged), used for the Test Chamber's signs. The
  entity CSV export gained a twelfth column, `message` (the ten campaign CSVs were regenerated:
  only their header comment changed).
- The DevKit finds the test level by id (`inspect-map --level testchamber`,
  `sync-entities --level testchamber`) and checks its page with `level_docs`;
  `create-map --register` refuses the test level's id. `package --expect-version` (release tags)
  fails while `"bootIntoTestLevel"` is `true`, and other packages warn. `neo_wolf --help` says
  when the game starts in the test level.
- Tests: native `game_flow_test_level` and `test_level`; `campaign_manifest_validation`,
  `entity_placements_json_csv` and the parity fixtures cover bad test-level definitions and
  trigger messages; game tests `game_test_level_boot`, `game_test_level_not_with_level` and
  `game_test_level_not_with_preview` (`cmake/RunGame.cmake` gained `EXPECT_NO_REGEX` and
  `EXPECT_NO_FILE`; the boot test checks that no `progress.json` is written); DevKit
  `test_campaign_rules.TestTestLevel` (119 CTest tests).
- **`--render-size <W>x<H>`**: the size of the image the game draws (even width 320–1920, even
  height 200–1080, width:height from 5:4 to 64:27, the shape of 2560×1080; anything else is a usage error with exit
  code 2). `EngineOptions::renderSize`
  carries it; the simulation never reads it, and the autoplay bot's report is byte-identical at
  every size.
- Tests `render_size_option` and `projection_square_pixels`; `ui_layout_bounds` now checks every
  screen at eleven render sizes from 320×200 to 1920×1080, including the edges of the 5:4 and
  64:27 shape limits, and that the minimap shows every
  shipped level as at 320×240, the level banner stays clear of the debug overlay's text and the
  crosshair, and the overlay panels end above the HUD bar. Game tests `game_render_size` and
  `game_render_size_invalid`; the `game_preview_*` tests check the screenshot's size (114 CTest
  tests).

### Changed

- **Neo-Wolf is presented as an engine with a demo.** The README and docs describe the engine
  first and the 10-level campaign as its demo; [docs/ROADMAP_NEXT.md](docs/ROADMAP_NEXT.md) is
  now the engine's own roadmap (games built on the engine keep their plans in their own
  repositories). The demo starts on the main menu again (`"bootIntoTestLevel": false`); the
  Test Chamber stays on the main menu, so CTest registers 118 tests (`game_test_level_boot`
  only runs while the flag is on).
- **Enemy types are defined by the game, not the engine** (engine/game boundary,
  [ROADMAP_NEXT.md](docs/ROADMAP_NEXT.md)). `gameplay.json` `enemies` may define any types (ids of lower case letters, digits and
  `_`), each with optional `sprite` and `corpseSprite` keys (default `<id>` and `<id>_dead`), and
  `defaultEnemy` names the type of a placement without `"variant"`. The engine's fixed
  `EnemyVariant` enum (guard, brute, runner) and its fixed sprite-slot enum are gone: entities
  hold an index into the loaded types, and the sprite slots are the engine's six role sprites
  (pickups, key cards, decoration) followed by the sprites the enemy types name, each loaded
  from `sprites.cfg` (`SpriteSlotNames`). An unknown `variant` is now an error when the level
  is built rather than when the entity file is read. The sample game's `gameplay.json` gained
  `"defaultEnemy": "guard"`; gameplay is unchanged (the autoplay bot's report is identical).
  Enemies spawned through the C API get the default type's `gameplay.json` stats. The old
  `enemy` section is now an error, and `validate` checks every placement's type and the
  `sprites.cfg` keys the types need ([CONTENT_DEFINITIONS.md](docs/CONTENT_DEFINITIONS.md#changed-after-10)).
  The `--preview enemies` screen shows the default type first, then the others alphabetically.
- The game starts in the Test Chamber while `campaign.json` sets `"bootIntoTestLevel": true`
  (shipped as `false`: the demo starts on the main menu). With five
  menu items (saved progress and a test level) the main menu at 16:9 uses a tighter vertical
  spacing so that its three-line controls summary still fits (`ui_layout_bounds`).
- **Default internal resolution 640×360** (16:9) instead of a fixed 320×240. The initial window is
  the largest whole multiple of the render size that fits in 1280×960 (1280×720 by default),
  shrunk with the same shape on a smaller desktop; letterboxing and fullscreen work as before.
- **Projection with square pixels** ("Hor+"): the vertical field of view of 1.0.0 is kept (a
  wall one unit high fills the screen height at distance 1, about 53°) and the horizontal one
  follows from the image's shape (about 67° at 4:3, 83° at 16:9). Up to 1.0.0 it was a fixed
  60°, which stretched the 320×240 image 1.15 times horizontally; at 320×240 the view is now
  wider and walls are square. Sprites and particles are projected with the same camera, so they
  keep their size relative to the walls at every size.
- **UI scale**: the UI is laid out on a canvas of at least 320×180 UI pixels and drawn at a
  whole-number scale (1 at 320×240, where it is unchanged pixel for pixel; 2 at 640×360 and
  640×480; 3 at 960×540; 4 at 1280×720). On a 16:9 canvas the main menu and the credits use
  tighter vertical spacing, the level list uses closer rows so that all ten levels fit, the
  level-complete panel is shorter so that it ends above the HUD bar, the level banner is a
  little more compact so that it ends clear of the crosshair, and the minimap panel keeps its
  320×240 size. `--screenshot` saves the frame at the render size.
- The reference screenshots in `docs/images/screens/` and the level start views in
  `docs/images/levels/` are now 640×360.
- On the development Mac (Release build, headless, no frame cap) a frame of `Update` + `Render`
  takes about 0.45 ms at 320×240, 1.3 ms at 640×360 and 5.2 ms at 1280×720
  ([docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) C28).

### Fixed

- A byte that is not UTF-8 in a text cell of an entity CSV (for example `lock`) made the engine
  abort while it wrote the error message; the message now shows the byte as U+FFFD. In a trigger
  `message` such a byte is an error in the engine and in the DevKit (whose `validate` crashed on
  it).
- A level that fails to load from the menu or a restart (an invalid entity file is only found
  at load time) left the game flow on the failed level while the engine kept the previous one:
  through the C API the previous level's exit switch then completed the failed level. The flow
  now goes back to the level that is still loaded (`GameFlow::CancelLevelStart`), so the Test
  Chamber stays the Test Chamber and saves nothing.

## [1.0.0] - 2026-11-16

The completion work (Roadmap Phases 0–10 and 12, [docs/ROADMAP.md](docs/ROADMAP.md)), commits
`eb14c78` … `38c9573`, the Phase 10 audit fixes and the Phase 12 clean-code review. Tagged
`v1.0.0`; development builds before it reported version 0.9.0.

Verification status: everything below builds and passes its tests on macOS, and the build, the
tests and the autoplay bot over all 10 levels pass headless in Windows and Linux CI, and CI
builds and smoke-tests the macOS and Windows packages and runs the lint checks (CI run
43879209370, all seven jobs green; the first all-green run with the package jobs was
43864756039). The macOS package's Intel slice
has only been run under Rosetta. Not yet done: a play-through by hand on any OS (known issue
C13), a run on a real Windows PC, and opening the packages by hand from Finder and Explorer
(known issue D5). Open problems are
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
  all drawn by deterministic generators in `tools/devkit/` (Phases 5, 6 and 8); since Phase 10
  also the four sound effects (`gen_sounds.py`) and the decoration sprite (a floor vase).
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
  and, in Phase 10, a Markdown link checker (`doc_links.py`) and `asset_provenance.py` (every
  file under `game/assets` must be reproduced byte for byte by a generator).
- **Licence** (Phase 9): PolyForm Noncommercial 1.0.0 in `LICENSE.md` (source-available, not
  OSI open source). Phase 10 audited the SDL code actually linked
  ([docs/THIRD_PARTY_LICENSES.md](docs/THIRD_PARTY_LICENSES.md)): the packages also ship SDL's
  HIDAPI and yuv2rgb notices; asset provenance is recorded in
  [docs/ASSET_CREDITS.md](docs/ASSET_CREDITS.md).
- **Documentation**: build guides for macOS and Windows, testing, troubleshooting, gameplay,
  controls, level authoring, release, the campaign design, `CONTRIBUTING.md`, this changelog,
  and an in-process C API reference (`docs/C_API.md`, Phase 10).
- **Documentation accuracy audit** (Phase 10): every claim in the documentation checked against
  the code, the files and CI runs; the corrections are listed under Fixed, and claims that could
  not be checked are marked as unverified in the docs.

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
- **Clean-code review** (Phase 12): coding standards in `docs/CODING_STANDARDS.md`; all C++
  formatted with clang-format and all Python with ruff (both formatting commits are listed in
  `.git-blame-ignore-revs`); clang-tidy (374 diagnostics) and ruff (261 findings) brought to
  zero; every module reviewed by hand against the ten principles, 297 findings (244 fixed,
  53 kept with a reason), recorded in `docs/CLEAN_CODE_REVIEW.md`. No behaviour change: the
  autoplay bot's report is byte-identical before and after. CI has a new `lint` job (pinned
  clang-format, clang-tidy and ruff) and uses `actions/checkout@v7`, `actions/setup-python@v7`
  and `actions/upload-artifact@v7`. Engine headers no longer include SDL.

### Deprecated

- `GameState::Victory` (value 4) in the C API: never entered; setting it selects
  `LevelComplete` (Phase 5).

### Removed

- The hardcoded fallback level layout, the entity-count rule for pickup types, and the unused
  `textures.json`, `sprites.json` and `entities.json` (Phase 4).
- Hardcoded `/home/mizan/...` SDL paths and `-lpthread -ldl -lm` from the build (Phase 1).
- The packager's raw `g++` fallback and the old launch scripts (Phase 9).
- The original 8×8 textures (Phase 8).
- `game/assets/sprite_decor.ppm` and the four sound files of unknown origin, replaced by
  generated ones (Phase 10).
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
- Assets: four sounds and a sprite of unrecorded origin (X5, Phase 10).
- CI and packaging: packager tests stored the game executable without its exec bit when a macOS
  or Linux archive was assembled on the Windows runner; executables are now always stored as
  0755 (commit `781fd42`). The Windows package-scripts step ended with exit code 2 although
  every check passed (cmd's `echo` does not reset `ERRORLEVEL`); it now ends with `exit /b 0`
  (commit `3cf95a8`).
- Phase 10 audit: over 100 documentation corrections (stale CI status, Windows behaviour stated
  as fact before it was verified, outdated messages and examples, level-page distances and
  captions, the level-authoring steps for a new campaign level); DevKit `create-map --register
  --force` reported the wrong campaign position for a replaced level.
- CI: the actions ran on the deprecated Node.js 20 (B17, Phase 12).

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
