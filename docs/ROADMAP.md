# Neo-Wolf v1.0 Completion Roadmap

**Goal:** Neo-Wolf builds and runs on **macOS and Windows** and ships a **10-level campaign**, with documentation that matches the code.

This roadmap replaces the "complete through Phase 26" claim in [Checklist.md](Checklist.md). An audit on 2026-11-15 found that several items ticked there are not implemented; see [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
Each phase has a **Done when** gate. Only tick an item after it has been verified, by a test, a CI run, or a recorded manual check.

## Decisions

| # | Decision | Choice | Status |
|---|---|---|---|
| 1 | Where work happens | Branch `completion/v1`, one commit per phase, pushed to `origin` (owner approved pushing on 2026-11-16, after Phase 1). | decided |
| 2 | Windows verification | GitHub Actions `windows-latest` (build, tests, bot over all levels, package), plus a manual play-through on a real Windows PC by the owner. | pending push |
| 3 | SDL dependency | SDL2 built from a pinned source release and linked statically on all platforms. `NEOWOLF_SYSTEM_SDL2=ON` uses an installed SDL2 instead. | decided |
| 4 | macOS architectures | Universal binary (arm64 + x86_64). Minimum macOS 10.15. | decided |
| 5 | Licence | Undecided. There is no LICENSE file; the owner must choose. | **pending owner** |
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
- [ ] Later phases add the bot over all levels and package artifacts (Phases 7 and 9)
- [x] Docs: CI section in `TESTING.md`, badge in README
- **Done when:** both jobs pass (requires a push). **Met:** run 43820751721 at `65c26b0`.

## Phase 3 — Runtime robustness
> Status: implemented; ticked items are verified on macOS. Windows: the `game_smoke_*` CTest tests and the `--frames 300` smoke run pass in CI (run 43820751721 at `65c26b0`); the interactive parts still need a real Windows machine (KNOWN_ISSUES B14).
- [x] One content-root resolver: `--data` / `NEOWOLF_DATA_DIR`, the executable's directory, `.app/Contents/Resources`, then the working directory (`engine/core/Paths.{h,cpp}`; explicit `--data`/`NEOWOLF_DATA_DIR` are exclusive; verified from `/`, from a copied folder whose path contains a space, and with temp-directory unit tests)
- [x] Fatal error message box when content is missing (also when the first level fails to load; exit code 1; dialogs suppressed with `--frames`; with the dummy video driver SDL has no message box and the failure is only logged)
- [x] Log file in the per-user preferences folder (`SDL_GetPrefPath`; fresh per launch, previous run kept as `neowolf.prev.log`)
- [x] Audio initialised separately from video; the game runs without an audio device (verified with `SDL_AUDIODRIVER=nonexistent`)
- [x] Vsync or frame cap, single present per frame, delta-time clamp (software renderer fallback fixes B11)
- [ ] HiDPI, fullscreen toggle, pause on focus loss, Quit menu item, Cmd+Q (done and verified on macOS: HiDPI (2048×1536 output for the 1024×768 window), resizable letterboxed window, `F11`/`Alt+Enter`/`Cmd+Ctrl+F` fullscreen, `--fullscreen`, pause on focus loss, `Esc` quits from the menu, clean exit on `SDL_QUIT` (window close / `Cmd+Q`); the toggles, pause and quit were driven with synthetic SDL events, not by hand. Open: the Quit menu item (menu rework, Phase 5))
- [x] CLI: `--level N`, `--data`, `--version`, `--frames N` (plus `--help`, `--windowed`, `--fullscreen`; `engine/core/CommandLine.{h,cpp}`; exit code 2 for bad options)
- [x] CRLF-tolerant loaders
- [x] Docs: `TROUBLESHOOTING.md`
- **Done when:** the game launches from any directory and from Finder; headless smoke runs pass on macOS and in Windows CI (any directory and headless macOS: done; Windows CI: done at `65c26b0`; Finder needs the `.app` from Phase 9)

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
- [ ] `game/defs/campaign.json` with 10 entries
- [ ] States `LevelComplete` and `CampaignComplete`; next level; restart on death; menu with New Game / Continue / Level Select / Quit
- [ ] Health and ammo carry over; death restores the level-entry snapshot
- [ ] Progress saved to the preferences folder
- [ ] Every screen re-laid out for 320×240; level number and name in the HUD
- [ ] Visible exit switch; minimap off by default and scaled
- [ ] Per-level texture themes
- [ ] SDL-free campaign/state logic with tests for every transition
- [ ] Docs: `GAMEPLAY.md`, `CONTROLS.md`
- **Done when:** transition tests pass and the full campaign flow can be played on macOS

## Phase 6 — Gameplay depth
- [ ] Enemy line of sight; `Alert` and `Attack` states used; no waking through walls; doors cannot close on enemies
- [ ] 3 enemy types (guard, brute, fast) defined in `gameplay.json`; separate ammo sprite
- [ ] Key cards and locked doors
- [ ] Tests for AI and enemy types
- [ ] Docs: `GAMEPLAY.md`, `ARCHITECTURE.md`
- **Done when:** AI behaviour is covered by tests and the enemy types are distinct in play

## Phase 7 — Verification tooling
- [ ] Headless autoplay bot: real engine, simulated input, plans a path to the exit, opens doors and unlocks them with keys, shoots enemies; reports win/loss, time and damage per level
- [ ] Validator campaign checks: 10 levels, orphans, one player, exit reachable, D/T/K tile ↔ entity pairing, entities not in walls, `gameplay.json` schema
- [ ] DevKit line-ending handling matches the engine; `create-map --level-id`; `inspect-map` fixes; `doctor` checks SDL, CMake and VS; `neo_devkit.cmd` fixes
- [ ] Tests: multi-level fixtures; packager fails when there is no executable
- [ ] Docs: `DEVKIT_GUIDE.md`, `LEVEL_AUTHORING.md`
- **Done when:** the bot completes level 1 on macOS and in Windows CI

## Phase 8 — The 10 levels
- [ ] Design table in `docs/levels/README.md` (theme, size, enemies, pickups, new mechanic, difficulty)
- [ ] Level 1 reworked as a tutorial; levels 2–10 built (all assets generated by the DevKit)
- [ ] Each level passes `validate --strict` and the bot, plus a screenshot check on macOS
- [ ] Docs: `docs/levels/level01.md` … `level10.md`
- **Done when:** all 10 levels validate and the bot completes all 10 on macOS and Windows

## Phase 9 — Packaging
- [ ] CMake install rules + CPack
- [ ] macOS: `NeoWolf.app` (Info.plist, icon, ad-hoc signature), zipped
- [ ] Windows: zip containing the exe, icon and version resource; static C runtime
- [ ] DevKit `package` drives CMake and fails when the executable is missing; per-OS launchers and README; checksums
- [ ] `package.sh`, `package.cmd`, `package.ps1` updated
- [ ] Bundle smoke test from a clean folder (path containing spaces); CI uploads both packages
- [ ] Docs: `RELEASE.md`; Gatekeeper and SmartScreen notes
- **Done when:** both packages run from a clean folder

## Phase 10 — Documentation pass
- [ ] README, CHANGELOG, CONTRIBUTING
- [ ] `ARCHITECTURE.md` corrected; `C_API.md` replaces `GAMEPLAY_API.md`
- [ ] `Checklist.md` reconciled
- [ ] Stale docs archived to `docs/history/`
- [ ] LICENSE and `ASSET_CREDITS.md` (needs decision 5)
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
