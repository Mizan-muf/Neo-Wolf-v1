# Neo-Wolf v1.0 Completion Roadmap

**Goal:** Neo-Wolf builds and runs on **macOS and Windows** and ships a **10-level campaign**, with documentation that matches the code.

This roadmap replaces the "complete through Phase 26" claim in [Checklist.md](Checklist.md). An audit on 2026-11-15 found that several items ticked there are not implemented; see [KNOWN_ISSUES.md](KNOWN_ISSUES.md).
Each phase has a **Done when** gate. Only tick an item after it has been verified, by a test, a CI run, or a recorded manual check.

## Decisions

| # | Decision | Choice | Status |
|---|---|---|---|
| 1 | Where work happens | Branch `completion/v1`, one commit per phase. Pushing to GitHub requires owner approval. | push pending |
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
- [ ] CMake: a `neowolf_engine` static library linked by the game and the tests; each source listed once
- [ ] Remove `/home/mizan` paths and `-lpthread -ldl -lm`
- [ ] SDL2 from a pinned source release via FetchContent, linked statically; `NEOWOLF_SYSTEM_SDL2` option
- [ ] `CMakePresets.json` with `macos`, `windows-msvc` and `linux` presets in debug and release
- [ ] Per-compiler warning flags, warnings-as-errors in CI, MSVC portability fixes
- [ ] Test harness: failures are recorded instead of aborting; non-zero exit on failure; one CTest entry per test; fixed repo root
- [ ] `build.sh` (macOS/Linux) and `build.cmd`/`build.ps1` (vswhere, any VS edition); no `|| true`; Makefile becomes a thin wrapper
- [ ] Docs: `BUILD_MACOS.md`, `BUILD_WINDOWS.md`, `TESTING.md`
- **Done when:** a fresh clone passes `cmake --preset macos && cmake --build --preset macos && ctest --preset macos`

## Phase 2 — CI (macOS + Windows)
- [ ] GitHub Actions on `macos-14` and `windows-latest`: configure, build, ctest, DevKit tests, `validate --strict`, headless smoke run
- [ ] Later phases add the bot over all levels and package artifacts
- [ ] Docs: CI section in `TESTING.md`, badge in README
- **Done when:** both jobs pass (requires a push)

## Phase 3 — Runtime robustness
- [ ] One content-root resolver: `--data` / `NEOWOLF_DATA_DIR`, the executable's directory, `.app/Contents/Resources`, then the working directory
- [ ] Fatal error message box when content is missing
- [ ] Log file in the per-user preferences folder (`SDL_GetPrefPath`)
- [ ] Audio initialised separately from video; the game runs without an audio device
- [ ] Vsync or frame cap, single present per frame, delta-time clamp
- [ ] HiDPI, fullscreen toggle, pause on focus loss, Quit menu item, Cmd+Q
- [ ] CLI: `--level N`, `--data`, `--version`, `--frames N`
- [ ] CRLF-tolerant loaders
- [ ] Docs: `TROUBLESHOOTING.md`
- **Done when:** the game launches from any directory and from Finder; headless smoke runs pass on macOS and in Windows CI

## Phase 4 — Data pipeline defects
- [ ] JSON entity placements loaded properly (vendored MIT JSON library); JSON is canonical, CSV is an optional export
- [ ] Decoration entity type fixed in the same change (no longer a trigger; sprite persists)
- [ ] `gameplay.json` parsed per section and applied; hardcoded stats removed
- [ ] Pickup kind and value come from data; HUD messages show real values
- [ ] Hardcoded fallback level removed; load errors logged loudly
- [ ] Dead definition files removed or wired in
- [ ] Exact-value tests; a test proving that edited data changes the game
- [ ] Docs: `CONTENT_DEFINITIONS.md`, `MAP_FORMAT.md` rewritten
- **Done when:** changing level or gameplay files changes the game without recompiling, and a test proves it

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
- [ ] Tag `v1.0.0`
