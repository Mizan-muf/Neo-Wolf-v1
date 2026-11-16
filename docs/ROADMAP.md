# Neo-Wolf v1.0 Completion Roadmap

**Goal:** Neo-Wolf builds and runs on **macOS and Windows** and ships a **10-level campaign**, with documentation that matches the code.

This roadmap replaces the "complete through Phase 26" claim in [Checklist.md](Checklist.md). An audit on 2026-11-15 found that several items ticked there are not implemented; see [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
Each phase has a **Done when** gate. Only tick an item after it has been verified, by a test, a CI run, or a recorded manual check.

## Decisions

| # | Decision | Choice | Status |
|---|---|---|---|
| 1 | Where work happens | Branch `completion/v1`, one commit per phase, pushed to `origin` (owner approved pushing on 2026-11-16, after Phase 1). | decided |
| 2 | Windows verification | GitHub Actions `windows-latest` (build, tests, bot over all levels, package), plus a manual play-through on a real Windows PC by the owner. | CI build, tests and bot verified (run 43861234139 at `f76875a`); package job (Phase 9) and the owner's Windows play-through pending |
| 3 | SDL dependency | SDL2 built from a pinned source release and linked statically on all platforms. `NEOWOLF_SYSTEM_SDL2=ON` uses an installed SDL2 instead. | decided |
| 4 | macOS architectures | Universal binary (arm64 + x86_64). Minimum macOS 10.15. | decided |
| 5 | Licence | PolyForm Noncommercial 1.0.0 (owner, 2026-11-16): `LICENSE.md` with the Required Notice `Copyright (c) 2026 Mizan-muf`; source-available, not OSI open source. Shipped in every package. | decided |
| 6 | Gameplay scope | Health and ammo carry over between levels. Death restores the stats from level entry. Progress is saved and unlocked levels are selectable. 3 enemy types. Key cards and locked doors. | decided |
| 7 | Toolchain on the dev Mac | `brew install cmake ninja` | done |

**Out of scope for v1** (documented as such): a C# gameplay layer and an exported shared library (the C API stays in-process), Apple notarization and Windows code signing, music, and Linux support beyond keeping its build working.

---

## Phase 0 — Setup & baseline
- [x] Working copy on branch `completion/v1`
- [x] CMake and Ninja installed on the dev Mac
- [x] Baseline reproduced on macOS: 23/23 native tests, 19/19 DevKit tests, `validate --strict` passes
- [x] `.gitattributes` added (LF for content, CRLF for `.cmd`)
- [x] Docs: [KNOWN_ISSUES.md](KNOWN_ISSUES.md) and this roadmap

## Phase 1 — Portable build system
- [x] CMake: a `neowolf_engine` static library linked by the game and the tests; each source listed once
- [x] Remove `/home/mizan` paths and `-lpthread -ldl -lm`
- [x] SDL2 from a pinned source release via FetchContent, linked statically; `NEOWOLF_SYSTEM_SDL2` option (official SDL2 2.32.10 source tarball, SHA-256 pinned, no Git needed; a wrong hash fails configure; both modes verified on macOS)
- [x] `CMakePresets.json` with `macos`, `windows-msvc` and `linux` presets in debug and release (`macos`, `macos-debug`, `ci-macos` verified locally; `ci-windows-msvc` and `ci-linux` pass in CI, and `build.ps1 -Config Debug` builds the Windows Debug configuration there; `linux-debug` not run)
- [x] Per-compiler warning flags, warnings-as-errors in CI, MSVC portability fixes (`-Werror` on macOS and Linux, `/W4 /WX` with MSVC, all passing in CI at `65c26b0`)
- [x] Test harness: failures are recorded instead of aborting; non-zero exit on failure; one CTest entry per test; fixed repo root
- [x] `build.sh` (macOS/Linux) and `build.cmd`/`build.ps1` (vswhere, any VS edition); no `|| true`; Makefile becomes a thin wrapper (`build.sh` and `make check` verified on macOS; `build.cmd` and `build.ps1 -Config Debug` pass in CI job `windows-scripts`; a run on a real Windows PC is part of Phase 11)
- [x] Docs: `BUILD_MACOS.md`, `BUILD_WINDOWS.md`, `TESTING.md`
- **Done when:** a fresh clone passes `cmake --preset macos && cmake --build --preset macos && ctest --preset macos`

## Phase 2 — CI (macOS + Windows)
> Status: CI is green. GitHub Actions run 43820751721 on commit `65c26b0` passed all four jobs: macOS (`ci-macos`, universal), Windows MSVC x64 (`ci-windows-msvc`), Linux (`ci-linux`) and `windows-scripts`. (History: on the first run, at `1b10a57`, the Windows preset job failed on MSVC C4459 and the macOS job did not start because the `macos-14` image is retired; both fixed in Phase 3.)
- [x] GitHub Actions on `macos-latest` and `windows-latest`: configure, build, ctest, DevKit tests, `validate --strict`, headless smoke run (`.github/workflows/ci.yml`; the DevKit tests and `validate --strict` run as CTest tests; smoke run `--frames 300`, exit code 0)
- [ ] Later phases add the bot over all levels and package artifacts (Phases 7 and 9: the bot step runs and passes on every OS since run 43861234139 at `f76875a`; the `package` job is added in Phase 9 and has not run yet)
- [x] Docs: CI section in `TESTING.md`, badge in README
- **Done when:** both jobs pass (requires a push). **Met:** run 43820751721 at `65c26b0`.

## Phase 3 — Runtime robustness
> Status: implemented; ticked items are verified on macOS. Windows: the `game_smoke_*` CTest tests and the `--frames 300` smoke run pass in CI (run 43820751721 at `65c26b0`); the interactive parts still need a real Windows machine (KNOWN_ISSUES B14).
- [x] One content-root resolver: `--data` / `NEOWOLF_DATA_DIR`, the executable's directory, `.app/Contents/Resources`, then the working directory (`engine/core/Paths.{h,cpp}`; explicit `--data`/`NEOWOLF_DATA_DIR` are exclusive; verified from `/`, from a copied folder whose path contains a space, and with temp-directory unit tests)
- [x] Fatal error message box when content is missing (also when the first level fails to load; exit code 1; dialogs suppressed with `--frames`; with the dummy video driver SDL has no message box and the failure is only logged)
- [x] Log file in the per-user preferences folder (`SDL_GetPrefPath`; fresh per launch, previous run kept as `neowolf.prev.log`)
- [x] Audio initialised separately from video; the game runs without an audio device (verified with `SDL_AUDIODRIVER=nonexistent`)
- [x] Vsync or frame cap, single present per frame, delta-time clamp (software renderer fallback fixes B11)
- [x] HiDPI, fullscreen toggle, pause on focus loss, Quit menu item, Cmd+Q (done and verified on macOS: HiDPI (2048×1536 output for the 1024×768 window), resizable letterboxed window, `F11`/`Alt+Enter`/`Cmd+Ctrl+F` fullscreen, `--fullscreen`, pause on focus loss, `Esc` quits from the menu, clean exit on `SDL_QUIT` (window close / `Cmd+Q`); the toggles, pause and quit were driven with synthetic SDL events, not by hand. The Quit menu item came with Phase 5 and is covered by `game_flow_menu` and `new_game_resets_world`; it has not been clicked by hand)
- [x] CLI: `--level N`, `--data`, `--version`, `--frames N` (plus `--help`, `--windowed`, `--fullscreen`; `engine/core/CommandLine.{h,cpp}`; exit code 2 for bad options)
- [x] CRLF-tolerant loaders
- [x] Docs: `TROUBLESHOOTING.md`
- **Done when:** the game launches from any directory and from Finder; headless smoke runs pass on macOS and in Windows CI (any directory and headless macOS: done; Windows CI: done at `65c26b0`; the `.app` exists since Phase 9 and its executable finds the content inside the bundle headless; a Finder launch by hand is still open, KNOWN_ISSUES D5)

## Phase 4 — Data pipeline defects
> Status: implemented; ticked items are verified on macOS (build with 0 warnings in our code, `ci-macos` with `-Werror`, 40 CTest tests, 24 DevKit tests, `validate --strict`, headless `--frames 300` run) and by a MinGW cross-compile. Windows (MSVC) and Linux: pending the CI run for this commit. Known issues C1–C5 are resolved.
- [x] JSON entity placements loaded properly (vendored MIT JSON library); JSON is canonical, CSV is an optional export (nlohmann/json 3.12.0 in `third_party/`, SYSTEM include; `EntityPlacementLoader` picks the parser by extension; DevKit `sync-entities` now exports JSON → CSV by default)
- [x] Decoration entity type fixed in the same change (no longer a trigger; sprite persists) (`EntityType::Decoration` appended; sprites are synced, not rebuilt)
- [x] `gameplay.json` parsed per section and applied; hardcoded stats removed (validated: types, ranges, duplicate keys; invalid file = exit code 1; `player.maxHealth` and `weapon.maxAmmo` moved out of `pickups`)
- [x] Pickup kind and value come from data; HUD messages show real values
- [x] Hardcoded fallback level removed; load errors logged loudly (file, entity index and field in every message; placement wiring extracted to `BuildLevelFromPlacements`)
- [x] Dead definition files removed or wired in (`textures.json`, `sprites.json`, `entities.json` deleted; DevKit references removed)
- [x] Exact-value tests; a test proving that edited data changes the game (`gameplay_definitions_exact`, `gameplay_definitions_invalid`, `entity_placements_json_csv`, `decorations_and_sprite_sync`, `pickup_kind_from_data`, `edited_data_changes_game`, `level_load_failures`, CTest `game_smoke_invalid_gameplay`)
- [x] Docs: `CONTENT_DEFINITIONS.md`, `MAP_FORMAT.md` rewritten (plus `THIRD_PARTY_LICENSES.md`, `third_party/README.md`)
- **Done when:** changing level or gameplay files changes the game without recompiling, and a test proves it. **Met on macOS:** `edited_data_changes_game` writes a modified `gameplay.json` and entity file into a temporary content root and checks the new values through `Engine::LoadGameplay`, `Engine::LoadLevel` and `Engine::Update`.

## Phase 5 — Campaign & progression
> Status: implemented; ticked items are verified on macOS (build with 0 warnings in our code, `ci-macos` with `-Werror`, 66 CTest tests: 47 native + registry, 16 headless game runs, DevKit (31 tests) + `validate --strict`, `validate --strict`), by a MinGW cross-compile with 0 warnings and a clang `-Wshadow-all` check of our sources. The flow was played by a headless scripted bot (`campaign_playthrough`), not yet by hand in a window (KNOWN_ISSUES C13). Windows (MSVC) and Linux: pending the CI run for this commit. Known issues C6–C9, B13 and B15 are resolved; C14 (mirrored wall textures) was found and fixed on the way.
- [x] `game/defs/campaign.json` with 10 entries (the manifest, its fail-fast loader and `--level N` = campaign index are done and tested (`campaign_manifest_validation`). Since the Phase 8 design it has the 10 final ids, names and themes; all ten were built in Phase 8; CTest `content_validation_campaign_size` requires exactly 10)
- [x] States `LevelComplete` and `CampaignComplete`; next level; restart on death; menu with New Game / Continue / Level Select / Quit (`Victory` kept as a deprecated C API alias of `LevelComplete`; tests `game_flow_menu`, `game_flow_progression`, `game_flow_death_pause`, `new_game_resets_world`, `campaign_playthrough`)
- [x] Health and ammo carry over; death restores the level-entry snapshot (clamped to the `gameplay.json` maxima; `game_flow_progression`, `game_flow_death_pause`, and through the engine in `campaign_playthrough`)
- [x] Progress saved to the preferences folder (`progress.json`, schema version 1, atomic write, corrupt file = no progress with a warning; `--pref-dir` / `NEOWOLF_PREF_DIR` for tests and CI; `progress_file_roundtrip`, `pref_dir_resolution`)
- [x] Every screen re-laid out for 320×240; level number and name in the HUD (`ui_layout_bounds`; screenshots of all 8 preview screens reviewed, `docs/images/screens/`; `--preview` and `--screenshot` developer tools)
- [x] Visible exit switch; minimap off by default and scaled (`X` tile with a themed "EXIT" texture, `E` within 1.1 cells while facing it; minimap `M`/`Tab`, exit drawn as an X cross; `exit_switch_activation`, `ui_layout_bounds`, `key_bindings`)
- [x] Per-level texture themes (`bunker`, `stone`, `tech` generated by `tools/devkit/gen_textures.py`, plus `classic`, the old `wall_textures.cfg`, regenerated as a 32×32 castle theme in Phase 8; `theme_loading_and_swap`, DevKit `test_gen_textures`)
- [x] SDL-free campaign/state logic with tests for every transition (`engine/core/GameFlow.{h,cpp}`)
- [x] Docs: `GAMEPLAY.md`, `CONTROLS.md` (plus `CONTENT_DEFINITIONS.md`, `MAP_FORMAT.md`, `ASSET_GUIDE.md`, `TROUBLESHOOTING.md`, `TESTING.md`, `GAMEPLAY_API.md`, README)
- **Done when:** transition tests pass and the full campaign flow can be played on macOS. **Partly met:** the transition tests pass and the whole campaign is completed through the real engine by the scripted bot on macOS; a manual play-through in a window is still open (C13).

## Phase 6 — Gameplay depth
> Status: implemented; ticked items are verified on macOS (build with 0 warnings in our code, `ci-macos` with `-Werror`, 79 CTest tests: 58 native + registry, 18 headless game runs, DevKit (39 tests) + `validate --strict`), by a MinGW cross-compile with 0 warnings and a clang `-Wshadow-all` check of our sources. Played by the headless scripted bot (`campaign_playthrough`: at the time it fetched level 2's blue key and fought level 3's brute and runner; those Phase 7 levels are now frozen in `tests/fixtures/reference/`) and checked in screenshots (`docs/images/screens/enemies.png`, `items.png`), not yet by hand in a window (KNOWN_ISSUES C13). Windows (MSVC) and Linux: pending the CI run for this commit. Known issues C10 and C11 are resolved; C15–C17 were recorded.
- [x] Enemy line of sight; `Alert` and `Attack` states used; no waking through walls; doors cannot close on enemies (sight through the raycaster's DDA, hearing through open cells, grid path finding with cell-centre steering, separation, `Search` state; `BLOCKED` message; tests `line_of_sight`, `enemy_ai_behavior`, `enemy_hearing`, `enemy_pathfinding`, `enemy_separation`, `door_close_blocked`, `ai_stress_20_enemies`)
- [x] 3 enemy types (guard, brute, fast) defined in `gameplay.json`; separate ammo sprite (`enemies.guard`/`brute`/`runner`, the fast one is the runner; `variant` validated; generated sprites and corpses for each, separate health, ammo and key sprites, `tools/devkit/gen_sprites.py`; tests `enemy_archetypes_from_data`, `entity_variant_validation`, `corpse_and_hit_flash`, DevKit `test_gen_sprites`)
- [x] Key cards and locked doors (red/blue/gold; per-colour door textures in every theme; HUD key cards; key shapes on the minimap; keys reset per level; DevKit key-before-lock reachability; tests `locked_doors_and_keys`, `hud_key_icons_layout`, DevKit `test_validator`; level 2 uses a blue key and door)
- [x] Tests for AI and enemy types (the tests above, all registered in `NEOWOLF_NATIVE_TESTS`)
- [x] Docs: `GAMEPLAY.md`, `ARCHITECTURE.md` (plus `CONTENT_DEFINITIONS.md`, `MAP_FORMAT.md`, `ASSET_GUIDE.md`, `DEVKIT_GUIDE.md`, `TESTING.md`, `KNOWN_ISSUES.md`)
- **Done when:** AI behaviour is covered by tests and the enemy types are distinct in play. **Partly met:** the AI is covered by tests and the three types differ in stats, silhouette and colour in screenshots and in the bot's play-through; they have not yet been seen in play by a person (C13).

## Phase 7 — Verification tooling
- [x] Headless autoplay bot: real engine, simulated input, plans a path to the exit, opens doors and unlocks them with keys, shoots enemies; reports win/loss, time and damage per level (`engine/autoplay/`, `neo_wolf_autoplay`, CTest `autoplay.level.N` / `autoplay.campaign`; verified on macOS: levels 1–3 and the campaign complete, deterministic JSON)
- [x] Validator campaign checks: level count (`--require-levels 10`), orphans, one player, exit reachable, D/T tile ↔ entity pairing, keys before locks, entities not in walls, `gameplay.json` schema
- [x] DevKit line-ending handling matches the engine (shared parity fixtures); `create-map --level-id`; `inspect-map` fixes; `doctor` checks SDL, CMake, Ninja, compilers and VS (vswhere)
- [ ] `neo_devkit.cmd` fixes (`setlocal`, `py -3` fallback, exit code): written and checked by a DevKit test, **not yet run on Windows**
- [x] Tests: multi-level fixtures (DevKit), autoplay fixture levels and C15 collision (native)
- [x] Tests: packager fails when there is no executable (known issue D1, resolved in Phase 9: DevKit tests `test_fails_without_an_executable` and `test_fails_without_an_executable_for_every_os`)
- [x] Docs: `DEVKIT_GUIDE.md`, `LEVEL_AUTHORING.md`
- [x] Gameplay fixes before level authoring: C15 (enemies block the player), C18 (preview screenshots)
- **Done when:** the bot completes level 1 on macOS (**done**) and in Windows CI (**done**: run 43861234139 at `f76875a`, `neo_wolf_autoplay --all` completes all 10 levels on Windows MSVC x64 and Linux)

## Phase 8 — The 10 levels
> Status: built and integrated; ticked items are verified **on macOS only** (2026-11-16, integration of `1cfa749` plus the integration changes): clean build with 0 warnings in our code, `ci-macos` with `-Werror` (0 warnings), 110 CTest tests all green (65 native + registry, 25 headless game runs, 16 autoplay, 3 DevKit: 108 DevKit unit tests, `validate --strict`, `validate --strict --require-levels 10`), a MinGW cross-compile from a clean copy with 0 warnings, and clang `-Wshadow-all` on the touched C++ sources. The bot's JSON reports are byte-identical for arm64, x86_64 (Rosetta) and `ci-macos`. **Windows and Linux: verified headless in CI**, run 43861234139 at `f76875a` (all four jobs green): 110/110 CTest tests on Windows MSVC x64, and `neo_wolf_autoplay --all` completes all 10 levels with carry-over, with per-level frames, damage, HP and ammo identical to macOS (campaign damage 0, 0, 12, 24, 24, 37, 49, 43, 62, 81). Not yet played by hand (C13), and not yet on a real Windows PC (Phase 11).
- [x] Design table in `docs/levels/README.md` (theme, size, enemies, pickups, new mechanic, difficulty; plus the resource economy, bot difficulty targets per level and the design rules; the builders' per-level briefs are kept as history in `docs/levels/BUILD_NOTES.md`). The tests no longer depend on the shipped levels' details (frozen Phase 7 levels in `tests/fixtures/reference/`, campaign-wide tests read `campaign.json`), and the `enemies`/`items` previews use a built-in stage instead of level 1
- [x] Light zones authorable from content (known issue C21): optional `"lights"` rectangles in the entity JSON, mirrored by the DevKit (`validate`, `inspect-map`, parity fixtures); cosmetic only (AI and bot ignore light); the CSV export is lossy and `sync-entities` warns or keeps them. Used by levels 5, 7 and 10
- [x] Level 1 reworked as a tutorial; levels 2–10 built to the design (counts, keys, themes; known issue C12 resolved). Textures and sprites are generated by the DevKit (`gen_textures.py`, now all four themes including a new 32×32 `classic` castle theme for level 10, and `gen_sprites.py`; both `--check` clean); the decoration sprite and the sound effects are the earlier hand-made assets (provenance: X5; replaced by generated ones in Phase 10)
- [x] Each level passes `validate --strict` and the bot, plus a screenshot check on macOS (`validate --strict --require-levels 10`; `autoplay.level.1`–`.10` and `autoplay.campaign` inside every target of the levels README, [Measured results](levels/README.md#measured-results-phase-8-integration); a jitter check of every level; start views in `docs/images/levels/`, level 10 re-checked with the new theme)
- [x] Design-rule review of all 10 levels (16 rules and the economy rules); level 3's hearing lesson written into the arc rule; justified exceptions recorded in each level page; level 4's fuel-store guard moved one cell for balance margin
- [x] Tooling: `sync-entities --level <id|N>`; `neo_wolf_autoplay --trace <file>` (deterministic event log: enemy states, hits taken and dealt, pickups, doors, keys, triggers, the exit; CTests `autoplay.trace*`); the bot no longer stalls on an enemy behind a corner (CTest `autoplay.fixture_aim_blocked`)
- [x] Docs: `docs/levels/level01.md` … `level10.md` in one structure, each showing its shipped map byte for byte and a generated annotated grid (DevKit `level_docs.py`, test `test_level_docs`); `BUILD_NOTES.md` condensed into history, its lessons in `LEVEL_AUTHORING.md`; `GAMEPLAY.md`, `MAP_FORMAT.md`, `CONTENT_DEFINITIONS.md`, `ASSET_GUIDE.md`, `TESTING.md`, `KNOWN_ISSUES.md`, README and the preview screenshots updated
- [x] The bot completes all 10 levels on Windows (and Linux) CI (run 43861234139 at `f76875a`: the autoplay step of every job passed; Windows results identical to macOS level by level)
- **Done when:** all 10 levels validate and the bot completes all 10 on macOS and Windows. **Met:** macOS (arm64 and x86_64) locally, Windows MSVC x64 (and Linux) in CI run 43861234139. A play-through by hand is still open (C13, Phase 11).

## Phase 9 — Packaging
> Status: implemented; ticked items are verified **on macOS** (2026-11-16): clean build with 0 warnings in our code, `ci-macos` with `-Werror`, 110 CTest tests and 133 DevKit tests green; the macOS package built with `devkit package` (`neo-wolf-0.9.0-macos-universal.zip`) passes `tools/ci/package_smoke.py` (extracted to a path with spaces, level 10 for 300 frames headless from there and with cwd `/`, content root `NeoWolf.app/Contents/Resources`, `codesign --verify --deep --strict`, `lipo` arm64 + x86_64, `otool -L` system libraries only, `plutil -lint`). Windows: the full project including the `.rc` resource cross-compiles with MinGW, and a MinGW zip (`-windows-x64-mingw`, not the release) passes the static checks (GUI subsystem, icon and version resource, no runtime DLLs). **Pending:** the MSVC package and its smoke run in CI (the `package` job is new and has not run yet, KNOWN_ISSUES D5); opening the app from Finder and the exe from Explorer by hand (D5, Phase 11). Details: [RELEASE.md](RELEASE.md).
- [x] CMake install rules + CPack (`cmake/Packaging.cmake`: install layout per OS, `cmake --install` default prefix `build/<preset>/install`, CPack ZIP/TGZ with the same name; package presets `package-macos`, `package-windows-msvc`, `package-linux`)
- [x] macOS: `NeoWolf.app` (Info.plist, icon, ad-hoc signature), zipped (bundle id `io.github.mizan-muf.neowolf`, macOS 10.15+, universal; icon drawn by `tools/devkit/gen_icon.py` and converted by `iconutil` at build time; `codesign --force --deep -s -` in the install step; the content root inside the app is logged as `app bundle Resources`)
- [ ] Windows: zip containing the exe, icon and version resource; static C runtime (written: `packaging/windows/neo_wolf.rc.in` with the version and a generated `.ico`, GUI subsystem, `/MT`; MinGW cross-build checked, MSVC build pending CI)
- [x] DevKit `package` drives CMake and fails when the executable is missing; per-OS launchers and README; checksums (old raw-g++ path removed; `README.txt` per OS from `tools/devkit/release_readme.py`, controls converted from `CONTROLS.md`; `SHA256SUMS`; `--expect-version`; known issue D1 resolved. The old `launch.sh`/`launch.cmd` were dropped: the game is cwd-independent, so the app or exe is started directly)
- [x] `package.sh`, `package.cmd`, `package.ps1` updated (call the DevKit, pass exit codes on, `py -3` fallback in the Windows scripts; `package.sh` verified on macOS, the Windows scripts by a DevKit test and the CI job `windows-scripts`, not yet run)
- [ ] Bundle smoke test from a clean folder (path containing spaces); CI uploads both packages (`tools/ci/package_smoke.py`, opt-in CTest `package.build`/`package.smoke` (`-DNEOWOLF_PACKAGE_TESTS=ON`), CI job `package`; passes for the macOS package; Windows and the uploads pending CI)
- [x] Docs: `RELEASE.md`; Gatekeeper and SmartScreen notes (`TROUBLESHOOTING.md` "Packaged builds", package `README.txt`, README "Download and run", `BUILD_AND_RELEASE.md`, `DEVKIT_GUIDE.md`, `TESTING.md`)
- **Done when:** both packages run from a clean folder. **Partly met:** the macOS package does (headless, from a path with spaces and from `/`); the Windows package waits for CI.

## Phase 10 — Documentation pass
- [x] README, CHANGELOG, CONTRIBUTING (README rewritten as the front door with a docs index; `CHANGELOG.md` in Keep a Changelog format with back-filled 0.x entries; `CONTRIBUTING.md`; their macOS commands run in a fresh copy of the repository, Windows commands taken from the CI-verified build docs)
- [x] `ARCHITECTURE.md` corrected; `C_API.md` replaces `GAMEPLAY_API.md` (ARCHITECTURE rewritten from the code, its module diagram checked against every `#include`; `C_API.md` documents all 27 functions, the enums, threading, ownership, no API version, no shared library; a C client was compiled and run against the header; the still-useful gameplay content moved to `GAMEPLAY.md`)
- [x] `Checklist.md` reconciled (banner: ROADMAP is the source of truth; false or partial ticks unticked or annotated in place, nothing deleted; known issue X1)
- [x] Stale docs archived to `docs/history/` (`Learning.md`, `process-flow.md`, `structure.md`, `test.md`, `SECURITY_AND_ARCHITECTURE_REPORT.md`, `mermaid-diagram.png`, each with a "superseded" banner, plus an index; every relative link and anchor in the repository's Markdown is checked by `tools/devkit/doc_links.py`, DevKit test `test_doc_links`)
- [x] LICENSE and `ASSET_CREDITS.md` (`LICENSE.md`: decision 5, PolyForm Noncommercial 1.0.0, shipped in the packages since Phase 9. [ASSET_CREDITS.md](ASSET_CREDITS.md): every asset family, its generator, licence and how to regenerate it; the five non-generated assets were replaced by generated ones (new `gen_sounds.py`, the vase in `gen_sprites.py`; known issue X5 resolved) and `test_asset_provenance` fails if any file under `game/assets` is not reproduced byte for byte by a generator. Third-party audit: SDL's bundled components linked into the macOS executable were checked with `nm`; the packages now also ship the HIDAPI and yuv2rgb BSD notices ([THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md)). Verified on macOS 2026-11-16: 110 CTest tests, DevKit tests, `validate --strict --require-levels 10`, bot 10/10 with identical results, `devkit package` + `package_smoke.py` pass with all five notices in `LICENSES/`. Windows binary not inspected)
- [ ] Doc-vs-code accuracy audit with zero failures
- **Done when:** the audit passes

## Phase 11 — Final sign-off
- [ ] Fresh clone on macOS: build, all tests, bot completes 10/10, manual full play-through
- [ ] Windows CI: build, tests, bot completes 10/10, package
- [ ] Owner: manual play-through on a real Windows PC
- **Done when:** all of the above pass (the release tag waits for Phase 12)

## Phase 12 — Clean-code review
Checks the finished code against clean-code principles and fixes what falls short. Runs last so it reviews the final code; behaviour must not change, and every test, the bot run and CI must still pass after each fix.
- [ ] Agree the review criteria and record them in `docs/CODING_STANDARDS.md`:
  - meaningful names
  - small functions with one responsibility
  - no duplication (DRY)
  - no dead or commented-out code
  - no magic numbers
  - const-correctness
  - RAII and clear ownership (no raw `new`/`delete`)
  - consistent error handling
  - comments that explain *why*
  - clear module boundaries and dependencies (no SDL leaking out of the platform layer)
- [ ] Automated checks: `.clang-format` and `clang-tidy` (readability, modernize, bugprone, cppcoreguidelines subsets) for C++, `ruff` for the Python DevKit; all three added to CI
- [ ] Manual review of every module (core, platform, render, world, assets, audio, ui, interop, DevKit), with findings recorded in `docs/CLEAN_CODE_REVIEW.md` (finding → principle → fix → status)
- [ ] Fix every finding, or record why it was kept; one commit per module
- [ ] Re-run the full verification from Phase 11 (macOS + Windows CI, bot 10/10)
- [ ] Tag `v1.0.0`
- **Done when:** formatting, clang-tidy and ruff checks pass in CI; every review finding is fixed or justified; Phase 11 verification passes again
