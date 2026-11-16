# Clean-code review

The Phase 12 review of the whole code base against [CODING_STANDARDS.md](CODING_STANDARDS.md).
Each finding records where it was, which principle it breaks, the fix, and its status
(**Fixed**, or **Kept** with the reason). Line numbers in the *Where* column are from commit
`094742a`, before the clean-up.

**Status: complete.** 297 findings: 244 fixed, 53 kept with a reason.
clang-format, clang-tidy and ruff report nothing on the whole tree and run in CI (job *Lint*).

## Scope

| Area | Modules |
|---|---|
| Engine core | `engine/core`, `src/` |
| Platform and audio | `engine/platform`, `engine/audio`, `engine/particles` |
| Rendering and UI | `engine/render`, `engine/ui` |
| Game world and content | `engine/world`, `engine/assets` |
| C API and autoplay bot | `engine/interop`, `engine/autoplay` |
| Tests | `tests/` |
| Python tools | `tools/devkit`, `tools/ci` |

## Baseline and result

| Check | Before | After |
|---|---|---|
| clang-format | 74 of 92 C++ files needed formatting | 0 (fixed in one formatting-only commit, `8c4c9ee`) |
| ruff format | 34 of 81 Python files needed formatting | 0 (fixed in one formatting-only commit, `9351bbb`) |
| clang-tidy ([.clang-tidy](../.clang-tidy)) | 374 diagnostics | 0 |
| ruff check ([ruff.toml](../ruff.toml)) | 261 findings | 0 |

Both formatting commits are listed in [.git-blame-ignore-revs](../.git-blame-ignore-revs), so
`git blame` (with `blame.ignoreRevsFile` set) and GitHub's blame view skip them.

## Summary by module

Each module was reviewed by reading every file against the ten principles, as well as by the
tools. The clang-tidy counts are per module (diagnostics in a module's `.cpp` files and its own
headers); they add up to 372 of the 374 whole-tree diagnostics.

| Module | Findings | Fixed | Kept | clang-tidy |
|---|---|---|---|---|
| `engine/core` | 42 | 37 | 5 | 66 → 0 |
| `src/` | 10 | 7 | 3 | 7 → 0 |
| `engine/platform` | 13 | 12 | 1 | 8 → 0 |
| `engine/audio` | 12 | 9 | 3 | 4 → 0 |
| `engine/particles` | 8 | 7 | 1 | 2 → 0 |
| `engine/render` | 18 | 14 | 4 | 44 → 0 |
| `engine/ui` | 18 | 13 | 5 | 12 → 0 |
| `engine/world` | 28 | 24 | 4 | 41 → 0 |
| `engine/assets` | 15 | 12 | 3 | 16 → 0 |
| `engine/interop` | 22 | 16 | 6 | 62 → 0 |
| `engine/autoplay` | 26 | 23 | 3 | 27 → 0 |
| `tests/` | 31 | 23 | 8 | 83 → 0 |
| `tools/devkit` | 48 | 42 | 6 | n/a (Python) |
| `tools/ci` | 6 | 5 | 1 | n/a (Python) |
| **Total** | **297** | **244** | **53** | **372 → 0** (whole tree: 374 → 0) |

## How behaviour was kept the same

The rule for every change was *no behaviour change*: same output, logs, file formats, command-line
flags and exit codes. Each module was checked on its own branch, and the merged tree was checked
again (see [Verification of the merged tree](#verification-of-the-merged-tree)):

- `ctest` (110 tests) and the DevKit suite (145 tests) pass.
- The autoplay bot plays the whole campaign (`neo_wolf_autoplay --all`), and its JSON report is
  byte-identical to the one recorded before the review. The bot is deterministic, so any change to
  movement, AI, combat, pickups, doors or level loading would show up there.

Where the bot cannot see a change, the module was compared against the code from before the
review with extra checks. The scratch tools used for this are not part of the repository.

| Module | Extra check |
|---|---|
| `engine/core`, `src/` | 6 `--preview` screenshots byte-identical; 17 good and bad `neo_wolf` command lines give the same output and exit codes |
| `engine/render`, `engine/ui` | 10 `--preview` screens and all 10 levels as headless screenshots, byte-identical; a framebuffer dump of walls, floors, doors, sprites, particles, every UI screen, the HUD and the minimap at 320×240, 97×61 and 64×48 is byte-identical on arm64 and x86_64 |
| `engine/particles` | old and new particle code run side by side for 60 frames of every preset; every particle field identical |
| `engine/interop`, `engine/autoplay` | the bot's JSON, trace and summary table, the time-out paths, and 15 command lines (help, version, every usage error) byte-identical |
| `engine/world`, `engine/assets` | every error and log message in the changed parsers compared with the old text |
| `tests/` | the same 110 CTest names; assertion counts unchanged (`TEST_ASSERT` 1022, `TEST_ASSERT_NEAR` 94, `FailTest` 14), each changed line checks the same value |
| `tools/devkit`, `tools/ci` | the output of every DevKit command (validate, inspect, create-map, sync-entities, create-asset, doctor, …) identical; all 56 game assets still reproduced byte for byte |

Not checked by the review: a real window and real audio output (all runs use SDL's `dummy`
drivers). These are part of the Phase 11 hands-on play test.

## Suppressions

Each suppression has its reason on the same line, as [CODING_STANDARDS.md](CODING_STANDARDS.md)
requires.

| Where | Check | Why it is kept |
|---|---|---|
| `engine/core/Engine.Update.cpp` `RandomDustOffset` | `misc-predictable-rand` | Decoration only; another generator would change the dust layout (CORE-10) |
| `engine/platform/PlatformSDL.cpp` `SaveFramebufferBmp` | `cppcoreguidelines-pro-type-const-cast` | SDL takes `void*`; `SDL_SaveBMP` only reads the pixels (PLAT-4) |
| `engine/interop/CommandBridge.h` `InteropCommand::strArg`, `InteropEvent::strArg` | `*-avoid-c-arrays` | C API struct layout (INTEROP-9) |
| `engine/interop/EngineInterop.cpp` `nw_engine_create`, `nw_engine_destroy` | `cppcoreguidelines-owning-memory` | The C caller owns the opaque handle (INTEROP-15) |
| `engine/autoplay/AutoplayBot.h` `engine_` | `cppcoreguidelines-avoid-const-or-ref-data-members` | The bot borrows its engine for its whole life (AUTO-24) |
| `src/main.cpp`, `src/autoplay_main.cpp` `main` | `bugprone-exception-escape` | An exception there is a bug; terminating keeps it visible, and a catch-all would change exit codes (SRC-7) |
| `tests/LogicTests.cpp` `FailTest` | `bugprone-std-exception-baseclass` | `TestFailure` is deliberately not a `std::exception`, so code under test cannot swallow a failed check (TEST-2) |
| `tests/LogicTests.cpp` `main` | `bugprone-exception-escape` | Test cases run inside `RunTestCase`'s catch blocks; only output can throw there (TEST-4) |
| `tools/devkit/devkit.py` | ruff `E402` | The import must follow the `sys.path` set-up when run as a script (DEVKIT-42) |
| `tools/devkit/tests/test_gen_icon.py`, `test_release_readme.py` | ruff `E402` | Stand-alone scripts imported by path |

## Changes made while merging

The module branches were applied in the order core, platform, world, render, interop, tests,
devkit. Then:

- **TEST-3:** the one merge conflict, in `TestKeyBindings`. The platform change (PLAT-5) replaced
  the `InputAction[2]` parameter of `MapKeyToActions` with a `std::array`, so the test uses that
  and its `NOLINT` is gone.
- **TEST-26:** the ui module now exports `UiLayout::kHudAmmoX` and `kTextInset` (UI-14), so
  `TestHudKeyIconsLayout` uses them instead of repeating `6` and `80`.
- **CORE-26:** fixed by the audio module (AUD-1). No engine header includes SDL any more.
- **SDL in `core`:** `App.cpp` (version log, `SDL_Delay`) and `Paths.cpp` (`SDL_GetBasePath`,
  `SDL_GetPrefPath`) keep their direct SDL calls. They are thin uses of SDL services, and moving
  them behind `platform` would add an interface with one caller. This closes the item that
  [ARCHITECTURE.md](ARCHITECTURE.md) listed for Phase 12.
- Two DevKit tests suppressed ruff `E731` (a lambda assigned to a name) without a reason; the
  lambdas are now small local functions.
- Some kept findings are real defects whose fix would change behaviour. They are recorded in
  [KNOWN_ISSUES.md](KNOWN_ISSUES.md): C25 (master volume on non-S16 audio devices, AUD-10), C26
  (particle damping per frame, PART-7), C27 (`std::stof` accepts trailing text in `audio.cfg`,
  ASSET-14) and V6 (`neo_wolf_autoplay` aborts on a bad `TMPDIR`, SRC-5). ASSET-3 (the return
  value of `LoadKeyValueConfig` when the error list is not empty) affects no current caller and
  stays here only.

## Verification of the merged tree

VERIFY_PLACEHOLDER

## Engine core

### `engine/core`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| CORE-1 | engine/core/CommandLine.cpp:42 `ParseCommandLine` | Cognitive complexity 78: one long `if` chain for every option, two lambdas, two flags for the window mode | 2 Small functions | Table of value options (`kValueOptions`: name, parser, "needs ..." text) plus `SplitArgument`, `TakeValue`, `ParseFlag`, `ParseArgument`; the window-mode conflict is derived from the current mode instead of two booleans. Messages and exit codes unchanged (compared against the base binary for 17 argument sets) | Fixed |
| CORE-2 | engine/core/CommandLine.cpp:12 `ParsePositiveInt` | `9` digit limit unexplained | 5 Magic numbers | `kMaxNumberDigits` with the reason (no int overflow) | Fixed |
| CORE-3 | engine/core/Engine.Init.cpp:43 `Engine::Init` | Cognitive complexity 44; ten numbered steps in one 175-line function | 2 Small functions | One private method per step: `InitContentRoot`, `InitDefinitions`, `InitWindow`, `InitAssets` (+ `LoadSpriteTextures`, `LoadSoundEffects`), `InitFirstLevel`; same order and messages | Fixed |
| CORE-4 | engine/core/Engine.Init.cpp:68-96 `Engine::Init` | The gameplay.json and campaign.json error dialogs were built by two copies of the same loop (with two copies of `kShownErrors`) | 3 DRY | `DescribeErrors(heading, errors)`; content-not-found text moved to `DescribeMissingContent` | Fixed |
| CORE-5 | engine/core/Engine.Init.cpp:111 `Engine::Init`, Engine.Shutdown.cpp:18, Engine.h:115 | `platform_` was an owning raw pointer (`new` / `delete`) | 7 RAII | `std::unique_ptr<PlatformSDL>`; `Engine()` defined out of line where `PlatformSDL` is complete. Only core used `platform_` | Fixed |
| CORE-6 | engine/core/Engine.Init.cpp:158, Engine.Campaign.cpp:176 | Fallback checkerboard size `64, 64` repeated | 5 Magic numbers / 3 DRY | `Engine::kMissingTextureSize` | Fixed |
| CORE-7 | engine/core/Engine.Update.cpp:79 `Engine::UpdateGameplay` | Cognitive complexity 30: movement, event reporting, level completion, damage and dust in one function | 2 Small functions | `MovePlayer`, `ReportInteractionEvents`, `CompleteLevel`, `ReportPlayerDamage`, `UpdateAmbientDust`; the frame order is unchanged | Fixed |
| CORE-8 | engine/core/Engine.Update.cpp:211, Engine.Core.cpp:203 | The enemy-alert sound cooldown was written out twice | 3 DRY | `PlayEnemyAlertSound()` (returns whether it played; only the AI path pushes the EnemyAlerted event, as before) | Fixed |
| CORE-9 | engine/core/Engine.Update.cpp:154-233, Engine.Core.cpp:53,213,233 | Nine copies of "InteropEvent e{}; e.type = ...; e.intArg1 = ...; bridge_.PushEvent(e)" | 3 DRY | `PushInteropEvent(type, intArg1, intArg2)`; the events with float or string arguments stay explicit | Fixed |
| CORE-10 | engine/core/Engine.Update.cpp:250-251 `UpdateGameplay` | `std::rand()` (misc-predictable-rand) | 2 / tooling | Moved into `RandomDustOffset`; kept `rand()` with `NOLINTNEXTLINE(misc-predictable-rand)`: the dust is decoration and another generator would change the dust layout (behaviour) | Kept: behaviour |
| CORE-11 | engine/core/Engine.Update.cpp:126, 248, 250 | `1e-6f`, `0.25f`, `4.0f` (and the `% 100`) unexplained | 5 Magic numbers | `kMinWishLengthSquared`, `kSpawnIntervalSeconds`, `kSpreadTiles`, comment on the offset's range | Fixed |
| CORE-12 | engine/core/Engine.Update.cpp:21 `Engine::Update` | Comment restated the call ("Process incoming interop commands") | 9 Comments | Says why the commands run first | Fixed |
| CORE-13 | engine/core/Engine.Core.cpp:18 `LogMessages`, Engine.Campaign.cpp:79-86, 379-382 | Indented error/warning logging written three ways | 3 DRY | `LogIndented(LogLevel, messages)` in Log.h, used by all of them | Fixed |
| CORE-14 | engine/core/Engine.Core.cpp:146-162 `LoadLevel`, Engine.Shutdown.cpp:46-68 | The per-level state reset (stats, keys, timers, counters) duplicated | 3 DRY | `ResetLevelState()` used by both | Fixed |
| CORE-15 | engine/core/Engine.Core.cpp:210 `PlayerAttack` | `0.4f` muzzle offset | 5 Magic numbers | `kMuzzleFlashDistance` | Fixed |
| CORE-16 | engine/core/Engine.Core.cpp:89, Engine.Campaign.cpp:88 | Error vectors copied out instead of moved (performance-use-std-move) | 6 / tooling | `std::move` | Fixed |
| CORE-17 | engine/core/Engine.Campaign.cpp:286-445 `SetupPreview`, `SetupShowcase` | About 20 unexplained numbers: sample health/ammo 75/18 and 82/31, message time 60 s (x4), sample unlock level 2, showcase player/door/trigger positions | 5 Magic numbers | `kPreviewStats`, `kShowcaseStats`, `kPreviewMessageSeconds`, `kPreviewHighestUnlocked`, `kShowcasePlayerStart`, `kShowcaseDoorCell`, `kShowcaseTriggerCell` (+ `ShowcaseCellPlacement`). Enemy and item line-ups are named data tables. Reference screenshots unchanged (6 previews compared byte for byte) | Fixed |
| CORE-18 | engine/core/Engine.Campaign.cpp:408, 425 `SetupShowcase` | Anonymous structs with uninitialised members in C arrays | 6 / tooling | `ShowcaseEnemy` / `ShowcaseItem` with member initialisers in `std::array` tables (`kShowcaseEnemies`, `kShowcaseItems`) | Fixed |
| CORE-19 | engine/core/Engine.Campaign.cpp:24, 37 | `kThemeSlots`, `kShowcaseRows` C arrays | tooling | `std::array` (rows as `std::string_view`) | Fixed |
| CORE-20 | engine/core/Engine.Campaign.cpp:138, 190, 294, 371 | `SaveProgress` not const; redundant parentheses; empty lambda parameter list; `placements` not const | 6 Const | Fixed as reported | Fixed |
| CORE-21 | engine/core/Engine.Render.cpp:19 `RenderCeilingAndFloor` | Ceiling and floor loops duplicated; colours `0xFF2A2A2A` / `0xFF3F3F3F` unexplained | 3 DRY / 5 | `FillRows` + `kPlainCeilingColor`, `kPlainFloorColor` | Fixed |
| CORE-22 | engine/core/Engine.Render.cpp:17, 160 | `1.04719755f` and `57.2957795f` literals | 5 Magic numbers | FOV is `DegreesToRadians(60.0f)` (checked: same float). The degrees factor became `kDegreesPerRadian`, not `RadiansToDegrees`, because `180/kPi` differs in the last bit (debug overlay text could change) | Fixed |
| CORE-23 | engine/core/Engine.Render.cpp:7-8, Engine.Core.cpp:5 | Unused includes (`Color.h`, `Log.h`, `<string_view>`) | 4 Dead code | Removed | Fixed |
| CORE-24 | engine/core/Engine.h:57 | Destructor and deleted copy, but no move declarations; 14 redundant `{}` member initialisers (also `EngineOptions`) | tooling | Moves deleted explicitly; initialisers removed | Fixed |
| CORE-25 | engine/core/Engine.h:115-170 | All data members public | 10 Boundaries | none | Kept: the C API, command bridge, autoplay bot and tests use them directly (ARCHITECTURE.md documents it); hiding them is a cross-module redesign |
| CORE-26 | engine/core/Engine.h:12 | `Engine.h` includes `AudioSystem.h`, so SDL reaches every includer of Engine.h | 10 Boundaries | Fixed in the audio module (AUD-1): `AudioSystem.h` no longer includes SDL, so `Engine.h` does not pass it on | Fixed (by AUD-1) |
| CORE-27 | engine/core/Engine.Update.cpp, Engine.Core.cpp, Engine.Campaign.cpp `ShowMessage(..., 1.0f / 1.5f / 2.0f / 3.0f)` | HUD message durations as literals | 5 Magic numbers | none | Kept: each is the duration argument of `ShowMessage`, next to the message it times; naming each would only rename the number |
| CORE-28 | engine/core/App.cpp:63 `App::Run` | Cognitive complexity 27; could be static | 2 Small functions | `LogStartupInfo`, `MakeEngineOptions`, `FrameStats`, `FramePacer`, `RunMainLoop`, `SaveScreenshot`; `Run` is static. Log order and pacing unchanged | Fixed |
| CORE-29 | engine/core/App.cpp:59 `CapRefreshRate`, 137 | `30`, `500`, `1000.0` unexplained | 5 Magic numbers | `kMinPlausibleRefreshRate`, `kMaxPlausibleRefreshRate`, `kMillisecondsPerSecond` | Fixed |
| CORE-30 | engine/core/App.cpp:36, 50; 94; 105 | C arrays; `platform` not const; repeated type in `duration_cast` | tooling / 6 | `std::array`, `const PlatformSDL&`, `auto` | Fixed |
| CORE-31 | engine/core/App.cpp:42 `CurrentDate`, Log.cpp:76 `LogMessage` | The `localtime_s` / `localtime_r` shim is written twice | 3 DRY | none | Kept: two 5-line shims with different formats; a shared time helper for them would be a new public API for little gain |
| CORE-32 | engine/core/Log.cpp:16-18 | Mutable globals (`g_logFilePath`, `g_logMutex`, `g_consoleMinimum`) | 7 Ownership | One `LogState` behind a function-local static; `const std::scoped_lock` | Fixed |
| CORE-33 | engine/core/Log.cpp:21 `Severity`, 85-96 `LogMessage` | Nested conditional; the line format was written twice (console and file) | 3 DRY | `switch`; `FormatLogLine` builds the line once (same bytes) | Fixed |
| CORE-34 | engine/core/MathUtils.h:18, 35; Vec2.h:14, 46 | `RadiansToDegrees`, `NormalizeAngleDegrees`, `Subtract`, `Normalize` never called | 4 Dead code | Removed | Fixed |
| CORE-35 | engine/core/MathUtils.h:31; Vec2.h:10, 18 | `NormalizeAngle` is an alias of `NormalizeAngleRadians`; `Add` / `Scale` duplicate `operator+` / `operator*` | 3 DRY | none | Kept: all are used by render, world and particles; removing them means editing those modules |
| CORE-36 | engine/core/MathUtils.h:10-11 `Clamp` | `is_arithmetic<T>::value`; nested conditional | tooling | `is_arithmetic_v`; two `if`s (same results, NaN included) | Fixed |
| CORE-37 | engine/core/Color.h:13-14 `PackARGB32` | Shifts by signed literals | tooling | `24U`, `16U`, `8U` | Fixed |
| CORE-38 | engine/core/CommandLine.h:37, PreviewScreen.h:53, JsonDocument.cpp:69 | Redundant member initialiser; C array `kPreviewScreens`; temporary strings in a loop | tooling | Removed; `std::array` (deduced); `append` | Fixed |
| CORE-39 | engine/core/Paths.cpp:153-157, 199-201 | Working-directory lookup written twice; `"NeoWolf", "NeoWolf"` unexplained | 3 DRY / 5 | `WorkingDirUtf8()`; `kPrefOrganisation`, `kPrefApplication` | Fixed |
| CORE-40 | engine/core/GameFlow.cpp:157-166, 184-186, 194-199 | Menu-cursor wrapping and the Quit request each written twice | 3 DRY | `MoveCursor`, `RequestQuit` | Fixed |
| CORE-41 | engine/core/Progress.cpp:92 `SerializeProgress` | `dump(2)` | 5 Magic numbers | `kJsonIndent` | Fixed |
| CORE-42 | engine/core/App.h, Color.h, Engine.h, GameState.h, InputState.h, Log.h, MathUtils.h, TimeState.h, Vec2.h, Vec2i.h | Header without the short "what this is for" comment | 9 Comments | Added | Fixed |

### `src/`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| SRC-1 | src/autoplay_main.cpp:85 `main` | Cognitive complexity 27 | 2 Small functions | Helpers below; `main` reads as parse, prepare, play, write | Fixed |
| SRC-2 | src/autoplay_main.cpp:135, 174 `main` | The temporary per-user folder was deleted by hand on two exit paths | 7 RAII | `FolderRemover` guard (same removal points: trace-open failure and the end) | Fixed |
| SRC-3 | src/autoplay_main.cpp:126-172 `main` | Create-parent-folders / open / write / close / report written twice (trace, JSON) | 3 DRY | `OpenOutputFile`, `WriteAndClose`; output and exit codes compared with the base binary (success, bad trace path, bad JSON path, bad option) | Fixed |
| SRC-4 | src/autoplay_main.cpp:73 `MakeTemporaryPrefDir` | `100` attempts unexplained | 5 Magic numbers | `kTemporaryFolderAttempts` | Fixed |
| SRC-5 | src/autoplay_main.cpp:72 `MakeTemporaryPrefDir` | `temp_directory_path()` without `error_code` throws on a bad TMPDIR, so the tool terminates instead of printing its "cannot create a temporary folder" message | 8 Error handling | none | Kept: the fix changes the failure behaviour (abort becomes exit 1 with a message); suggested as a follow-up |
| SRC-6 | src/main.cpp:75, src/autoplay_main.cpp:64 `CommandLineArgs` | C-array parameter; return type repeated | tooling | `char**`; braced return | Fixed |
| SRC-7 | src/main.cpp:83, src/autoplay_main.cpp:85 `main` | bugprone-exception-escape | 8 Error handling | `NOLINTNEXTLINE(bugprone-exception-escape)`: an exception there is a bug, and terminating keeps it visible; a catch-all would change exit codes | Kept: behaviour |
| SRC-8 | src/main.cpp:42-57, src/autoplay_main.cpp:39-62 `CommandLineArgs` (Windows) | The UTF-16 to UTF-8 argument conversion is written twice, in two styles | 3 DRY | none | Kept: Windows-only code that cannot be built here; sharing it needs a new shared source and Windows CI to verify |
| SRC-9 | src/main.cpp:23, src/autoplay_main.cpp:32 | `shellapi.h` sorted before `windows.h` (breaks MSVC) | 10 / build | Same change as be60ee8 on completion/v1: `windows.h` first, then a comment, then `shellapi.h` in its own block | Fixed |
| SRC-10 | src/main.cpp:104 `main` | `App app; app.Run(...)` on a stateless class | 2 | `App::Run(...)` (static, CORE-28) | Fixed |

## Platform and audio

### `engine/platform`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| PLAT-1 | engine/platform/PlatformSDL.cpp:208 `PumpEvents` | Cognitive complexity 28 (limit 25): event dispatch, the fullscreen chord, the Cmd/Ctrl B13 handling and key mapping all in one loop | 2 Small functions | Key handling moved to private `HandleKeyEvent(const SDL_KeyboardEvent&, InputState&)` plus `IsGuiKey` helper. Focus handling (`hasFocus_`, `headlessVideo_`) left exactly as it was, still in `PumpEvents` | Fixed |
| PLAT-2 | engine/platform/PlatformSDL.h:61-63; PlatformSDL.cpp:341 `EnsureFramebufferTexture`, :369 `Shutdown` | Window, renderer and texture were raw owning pointers released by hand | 7 RAII | `std::unique_ptr` with `WindowDeleter`/`RendererDeleter`/`TextureDeleter` (defined in the .cpp so the header does not include SDL). Members are declared in creation order; `Shutdown` still releases texture, then renderer, then window, then quits the video subsystem | Fixed |
| PLAT-3 | PlatformSDL.cpp:297 `SaveFramebufferBmp` | `SDL_Surface` freed by hand after the save | 7 RAII | `std::unique_ptr<SDL_Surface, SurfaceDeleter>`; same error strings | Fixed |
| PLAT-4 | PlatformSDL.cpp:298 `SaveFramebufferBmp` | `const_cast` on the framebuffer pixels (cppcoreguidelines-pro-type-const-cast) | 6 Const | none | Kept: `SDL_CreateRGBSurfaceWithFormatFrom` takes `void*` and `SDL_SaveBMP` only reads the pixels. The alternatives (copying the framebuffer, or taking a non-const `Framebuffer&`) are worse. `NOLINTNEXTLINE` with that reason |
| PLAT-5 | PlatformSDL.h:53, PlatformSDL.cpp:27 `MapKeyToActions`, :243 `PumpEvents` | C arrays `InputAction[2]` (3 tidy diagnostics); the 2 was unnamed | 5 Magic numbers / tidy | `std::array<InputAction, PlatformSDL::kMaxActionsPerKey>`; caller in tests/LogicTests.cpp updated | Fixed |
| PLAT-6 | PlatformSDL.h:12-15, :66-67 `PlatformConfig`, `logicalWidth_` | Unnamed 1024/768/320/240 defaults. They duplicated `kWindowWidth`/`kRenderWidth`… in Engine.Init.cpp, and nothing used them: the only caller sets every field | 3 DRY / 5 Magic numbers | Size defaults are 0, with a comment that the caller (Engine::Init) sets them; the sizes are now defined only in Engine.Init.cpp | Fixed |
| PLAT-7 | PlatformSDL.cpp:118 `Init` | Did several jobs (hints, subsystem, driver, desktop fitting, window, renderer); `SDL_GetCurrentVideoDriver` was called twice (:133, :162) | 2 Small functions / 3 DRY | Split into `OpenWindow`, `FitWindowToDesktop` and `IsHeadlessVideoDriver`. The driver is queried once. `headlessVideo_` uses the same driver names (dummy, offscreen); `hasFocus_` is still read from the window flags right after creation | Fixed |
| PLAT-8 | PlatformSDL.cpp:140, 142, 176/180, 299, 331 | Unexplained 0 (display), 0.9 (desktop fraction), -1 (render driver index), 32 (bits per pixel), 255 (alpha) | 5 Magic numbers | `kPrimaryDisplay`, `kMaxDesktopFraction`, `kFirstMatchingRenderDriver`, `kFramebufferBitsPerPixel`, `SDL_ALPHA_OPAQUE` | Fixed |
| PLAT-9 | PlatformSDL.cpp:281 `IsMinimized` | bugprone-signed-bitwise on `(SDL_WINDOW_MINIMIZED \| SDL_WINDOW_HIDDEN)` | tidy | Named `Uint32` constant `kNotVisibleWindowFlags` | Fixed |
| PLAT-10 | PlatformSDL.cpp:99 `IsFullscreenShortcut` | modernize-use-auto | tidy | `const auto code = static_cast<SDL_Scancode>(…)` | Fixed |
| PLAT-11 | PlatformSDL.h:19 | special-member-functions: no move operations declared | tidy | Move operations explicitly deleted (the class was already not movable) | Fixed |
| PLAT-12 | PlatformSDL.h:35 `ToggleFullscreen` | Public, but only the key handler calls it | 10 Boundaries | Made private | Fixed |
| PLAT-13 | PlatformSDL.h:1 | No header comment saying what the module is for | 9 Comments | Added | Fixed |

### `engine/audio`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| AUD-1 | engine/audio/AudioSystem.h:3 | The header included `<SDL.h>` for one `SDL_AudioDeviceID` member, so every file that includes Engine.h (interop, autoplay, tests) saw SDL. ARCHITECTURE.md listed this as a Phase 12 item | 10 Boundaries | Device id and format are stored as `std::uint32_t`/`std::uint16_t`, with `static_assert`s in the .cpp that they match the SDL types; `SDL_AudioSpec` is forward-declared. Checked that no Engine.h includer used SDL through this header (only comments matched). `SDL_MAIN_HANDLED` is set globally, so `main` is unaffected. ARCHITECTURE.md (SDL section) and Checklist.md:94 updated | Fixed |
| AUD-2 | AudioSystem.cpp:105, :117, :122 `LoadEffectFromWav` | The `SDL_malloc` conversion buffer (cppcoreguidelines-owning-memory) and the WAV buffer were raw pointers freed by hand on every path | 7 RAII | `std::unique_ptr` with `SdlFreeDeleter` / `WavDeleter` | Fixed |
| AUD-3 | AudioSystem.cpp:78 `LoadEffectFromWav` | Loading and conversion in one function, tracked with an `ok` flag and three levels of nesting | 2 Small functions | Conversion moved to `ConvertToDeviceFormat` with early returns. Error messages and their order are unchanged | Fixed |
| AUD-4 | AudioSystem.cpp:161 `PlayEffect` | `reinterpret_cast<int16_t*>` over the byte buffer (tidy) | tidy | `ScaleS16Samples` reads and writes each sample with `memcpy`; same arithmetic | Fixed |
| AUD-5 | AudioSystem.cpp:154, 165, 169-171 `PlayEffect` | Unexplained 0.999, -32768/32767, 128, 255 | 5 Magic numbers | `kFullVolume`, `numeric_limits<int16_t>`, `kU8Silence`, `numeric_limits<uint8_t>`; U8 scaling moved to `ScaleU8Samples` | Fixed |
| AUD-6 | AudioSystem.h:24 `HasEffect`, :29 `MasterVolume` | Never called (engine, interop, tests, docs) | 4 Dead code | Removed | Fixed |
| AUD-7 | AudioSystem.h:10 | special-member-functions | tidy | Move operations explicitly deleted | Fixed |
| AUD-8 | AudioSystem.h:36 | readability-redundant-member-init `effects_{}` | tidy | Removed | Fixed |
| AUD-9 | AudioSystem.h:20 `Init(44100, 2, 1024)` | Literal default arguments | 5 Magic numbers | none | Kept: the parameter names (`sampleRate`, `channels`, `samples`) already name them, and the only call uses the defaults |
| AUD-10 | AudioSystem.cpp:160-173 `PlayEffect` | Master volume (0.85 in audio.cfg) only scales S16 and U8 devices. The device is opened with `SDL_AUDIO_ALLOW_ANY_CHANGE`, so another format (such as float32) would silently play at full volume. `AUDIO_S16MSB` is scaled as host-order samples, which is wrong on little-endian hosts. Not verified which drivers actually return a non-S16 format (WASAPI might) | 8 Errors | A comment now states that other formats play at full volume | Kept: fixing it changes behaviour (a bug fix, not a clean-up). Worth a KNOWN_ISSUES entry |
| AUD-11 | AudioSystem.cpp:155/175 `SDL_QueueAudio` | Effects are queued, not mixed (KNOWN_ISSUES C23) | n/a | none | Kept: by design for v1; changing it is a feature. The header comment now points to C23 |
| AUD-12 | AudioSystem.h:1 | No header comment | 9 Comments | Added | Fixed |

### `engine/particles`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| PART-1 | engine/particles/Particle.cpp:7 | Local `kPi` copy of MathUtils.h's `kPi` | 3 DRY | Uses `engine/core/MathUtils.h`, which ARCHITECTURE allows (particles → core foundation) | Fixed |
| PART-2 | Particle.cpp:32 `SpawnHitSpark`, :49 `SpawnPickupSparkle`, :66 `SpawnAmbientDust` | Two near-identical ring loops, plus the same seven field assignments in three places | 3 DRY | `SpawnRing` driven by `RingBurst` presets (`kHitSpark`, `kPickupSparkle`); `MakeParticle` builds every preset particle | Fixed |
| PART-3 | Particle.cpp:33-74, :96 | Unexplained counts, speeds, lifetimes, sizes, colours and the 0.98 damping | 5 Magic numbers | Named presets/constants with units (`kDust*`, `kVelocityDampingPerUpdate`); values unchanged | Fixed |
| PART-4 | Particle.h:14 `Particle::spriteFrame` | Set by the presets but never read (renderer, interop, tests) | 4 Dead code | Removed; docs/Checklist.md:449 note updated | Fixed |
| PART-5 | Particle.cpp:21 `Spawn` | misc-const-correctness on `it` | 6 Const | `const auto it` | Fixed |
| PART-6 | Particle.h:33 | readability-redundant-member-init `particles_{}` | tidy | Removed | Fixed |
| PART-7 | Particle.cpp:96 `Update` | Velocity damping is applied once per `Update` call, so it depends on the frame rate | 2 / correctness | Named, with a comment saying it is per update, not per second | Kept: making it time-based changes particle motion (behaviour) |
| PART-8 | Particle.h:1, :9-18, :23 | No header comment, no units on fields, the `Spawn` pool policy was not documented | 9 Comments | Added header comment, field units and the `Spawn` policy (reuse an expired slot, else replace the oldest) | Fixed |

## Rendering and UI

### `engine/render`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| REND-1 | engine/render/Raycaster.cpp:152 `RenderWalls` | Cognitive complexity 54: it did the camera set-up, the floor/ceiling pass, texture choice, texture column, shading and the column loop. | 2 Small functions | Split into `RenderFloorAndCeiling`, `ResolveWallTexture`, `WallTextureU`, `WallShade`, `DrawWallColumn`; `RenderWalls` keeps set-up and the column loop. | Fixed |
| REND-2 | engine/render/SpriteRenderer.cpp:71 `Render` | Cognitive complexity 26. The per-sprite light and shade (lines 145-148) were recomputed for every pixel. | 2 Small functions | Split into `ComputeSpriteScreenRect` and `DrawSpriteColumns`. Light and shade are now computed once per sprite (same values, fewer operations in the hot loop). | Fixed |
| REND-3 | SpriteRenderer.cpp:85-112, ParticleRenderer.cpp:38-62, Raycaster.cpp:164-168 | The camera basis (forward, camera plane), the billboard inverse (det, invDet), the camera-space transform and the screen-column formula were copied in two or three places. | 3 No duplication | New header-only `engine/render/RenderMath.h`: `CameraBasis`/`MakeCameraBasis`, `MakeBillboardProjection` (std::optional), `ToViewSpace`, `ScreenColumn`. The expressions are unchanged, so floating-point results are the same. | Fixed |
| REND-4 | Raycaster.cpp:40 `DarkenColor`, ParticleRenderer.cpp:13 `ShadeParticle`, SpriteRenderer.cpp:149-156, Framebuffer.cpp:108-111 | Two identical "scale RGB by a factor" functions; the ARGB unpack/pack code was written out four times. | 3 No duplication | `UnpackARGB32` + `ScaleRgb` in RenderMath.h, packing through core `PackARGB32`; sprite shading uses `ShadeSpritePixel`; `Darken` uses the unpack too. | Fixed |
| REND-5 | Raycaster.cpp:51,195,253; SpriteRenderer.cpp:145-147; ParticleRenderer.cpp:75-78 | The distance falloff `Clamp(1/(1+d*k), min, 1)` appeared three times; the "light of the cell under a world point" lookup appeared three times. | 3 No duplication | `DistanceShade`, `LightAt` in RenderMath.h. | Fixed |
| REND-6 | Raycaster.cpp:23-37,172-179,236; SpriteRenderer.cpp:98 | The `t != nullptr && Width() > 0 && Height() > 0` check was repeated, and so was the `x != nullptr ? x : missing` ternary five times. | 3 No duplication | `IsDrawable` (RenderMath.h), `OrMissing` (Raycaster.cpp). | Fixed |
| REND-7 | Raycaster.cpp:17,67,147,195,199,252,253,260; SpriteRenderer.cpp:40,107,148; ParticleRenderer.cpp:43,49,57,78 | Unnamed shading factors, falloffs and minimums, epsilons, near plane, minimum lifetime, and the magenta colour key. | 5 No magic numbers | Named constants with comments (`kFloorFalloff`, `kHorizontalFaceShade`, `kWallMinLight`, `kSpriteColorKey`, `kBillboardNearPlane`, ...). | Fixed |
| REND-8 | engine/render/Framebuffer.cpp:57 / Framebuffer.h:34 `DrawVerticalLine` | No caller anywhere (engine, src, tests). | 4 No dead code | Deleted. docs/Checklist.md:115 and docs/history/* still mention it as history; I left those files alone. | Fixed |
| REND-9 | Raycaster.cpp:185 and :204 (floor loop) | `p <= 0.0001f` can never be true (p = y - horizon >= 1), and `mirroredY >= 0 && mirroredY < height` is always true for y in (horizon, height). | 4 No dead code | Removed both checks. | Fixed |
| REND-10 | SpriteRenderer.cpp:134 | `texU < 0 \|\| texU > 1` can never be true: the stripe range lies inside [left, left + size], so texU is in [0, 1]. | 4 No dead code | Removed. | Fixed |
| REND-11 | Framebuffer.h:42 `PackRGBA` | Re-implemented core `PackARGB32` by hand (and caused 3 signed-bitwise diagnostics). | 3 No duplication | Now returns `PackARGB32(MakeColor(r, g, b, a))`. The public signature is unchanged (tests use it). | Fixed |
| REND-12 | Framebuffer.cpp:94-95 `DrawRect` | Recomputed `x + width - 1` / `y + height - 1`, which are already in `right` / `bottom`. | 3 No duplication | Reuses `right` / `bottom`. | Fixed |
| REND-13 | Framebuffer.h, Raycaster.h, SpriteRenderer.h, ParticleRenderer.h | No header comment saying what the module is for. | 9 Comments | Added. The Raycaster comments on wall orientation and face lighting moved onto `WallTextureU` / `WallShade`, wording kept. | Fixed |
| REND-14 | all render files (clang-tidy) | 24 signed-bitwise, 13 use-auto, 3 narrowing, 1 integer-division (SpriteRenderer.cpp:133), 2 const-correctness, 1 redundant member init (Framebuffer.h:50), plus REND-1/2. | 6 Const / tidy | Unsigned shifts (through REND-4), `auto` for casts, `const` locals, the sprite's left edge kept as an int before the float division. | Fixed |
| REND-15 | engine/render/Raycaster.cpp:23 vs :172 | Inconsistent fallback: an empty (0x0) floor or ceiling texture falls back to `missingTexture`, but an empty wall texture skips the column. | 8 Consistent handling | None: unifying would change output for empty wall textures. | Kept: behaviour change; the texture loader does not produce such textures in practice (not verified for every path) |
| REND-16 | SpriteRenderer.cpp:148 | The sprite shade clamps after multiplying by the light level, unlike walls, floor and particles (clamp, then multiply). So it does not go through `DistanceShade`. | 3 No duplication | Left as its own expression with a named falloff and minimum. | Kept: routing it through DistanceShade changes pixels |
| REND-17 | Raycaster.cpp:58 `CastRay` | The per-axis step/sideDist set-up is written out twice (x, y). | 3 No duplication | Only the two epsilons were named. | Kept: CastRay also drives line of sight and shots (bot baseline); a helper for 2x4 lines would only move them |
| REND-18 | SpriteRenderer.h:23 `kSpriteHitFlashStrength` | Public constant used only in SpriteRenderer.cpp. | 10 Module boundaries | None. | Kept: documented next to the sprite types it describes; moving it is a taste rename |

### `engine/ui`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| UI-1 | engine/ui/UiRenderer.cpp:597 `DrawMinimap` | Cognitive complexity 40: layout, cells, exits, locked doors, sprites, player and legend in one function with lambdas. | 2 Small functions | One helper per layer (`DrawMinimapCells`, `DrawMinimapExits`, `DrawMinimapLockedDoors`, `DrawMinimapSprites`, `DrawMinimapPlayer`, `DrawMinimapLegend`, `DrawExitMarker`, `DrawMinimapKey`) drawing through a `MinimapCanvas` clip square (replaces `FillClipped`, :193). Draw order unchanged. | Fixed |
| UI-2 | UiRenderer.cpp:616-617, 632-634, 661-667 | Exits and locked doors went into two per-frame `std::vector`s only to be drawn later. | 2 / 3 | Exits are drawn in the same row-major scan that found them. Locked doors get a second pass over the same visible range, so order and set are identical and there are no per-frame allocations. | Fixed |
| UI-3 | UiRenderer.cpp:652 and :668 | The exit-marker arm `max(3, cell/2+1)` was computed twice. | 3 No duplication | `ExitMarkerArm`, `kExitMarkerMinArm`. | Fixed |
| UI-4 | UiRenderer.cpp:641-644 | Identical `Exit` / `Empty` switch branches (bugprone-branch-clone). | 4 / tidy | Merged in `MinimapCellColor`. | Fixed |
| UI-5 | UiRenderer.cpp:456, :692-694 | Nested conditional operators. | 1 / tidy | `LevelEntryColor`, `KeyColorOfSprite`, `MinimapSpriteColor`. | Fixed |
| UI-6 | UiRenderer.cpp:254, 287, 363, 414, 505, 555 (+ loops :378, 416, 512, 561) | C arrays, with loop bounds hard-coded as 3, 4 and 6. | tidy / 5 | `std::array` (or `std::string(1, c)` for the key letter); loops use the container size. | Fixed |
| UI-7 | UiRenderer.cpp:142 `DrawGlyph` | Signed shift `1 << (4 - col)` with an unexplained 4. | 5 / tidy | Unsigned mask from `kGlyphWidth - 1 - col`, with a comment on the bit order. | Fixed |
| UI-8 | UiRenderer.cpp:327 `KeyCardRect` | Copied DrawHud's ammo x as `kMargin + 6 + 80 // as in DrawHud`. | 3 No duplication | `UiLayout::kTextInset`, `kHudHealthX`, `kHudAmmoX` (UiRenderer.h), used by both. | Fixed |
| UI-9 | UiRenderer.cpp:292, 312, 499 | `"LEVEL " + std::to_string(n)` written three times. | 3 | `LevelNumberText`. | Fixed |
| UI-10 | UiRenderer.cpp:412, 435, 566 | Footer y `height - kMargin - kLineHeight - 6` written three times. | 3 | `FooterY`. | Fixed |
| UI-11 | UiRenderer.cpp:293, 462, 467, 470 | Right-aligned text arithmetic written four times. | 3 | `DrawTextRightAligned`. | Fixed |
| UI-12 | UiRenderer.cpp:175 `DrawCenteredPanel` | The comment said "returns its top-left corner", but the function used out-parameters. | 1 / 9 | Returns a `Point`. | Fixed |
| UI-13 | UiRenderer.cpp:286, 289, 340-348, 354, 379, 387, 428, 497, 528, 543, 602, 623-639, 653-656, 681, 703-715 | About 30 `Rgb(...)` literals inside function bodies. Some repeat (12,14,22 twice; 16,16,20 twice; 255,255,255 twice), and the minimap tile colours are a legend. | 5 No magic numbers | All colours are named in the palette block (screens, key cards, minimap). | Fixed |
| UI-14 | UiRenderer.cpp, whole file (about 400 literals) | Principle 5 judgement for the 320x240 layout. **Named** the values that repeat or carry meaning: text inset 6 (10 uses), overlay title row 9 (3), overlay text inset 8 (`boxW - 16`, 5), dim percentages 40/35, low-health threshold 25, minimap frame 2 (:575, :604-606), minimum panel 16, grid minimum cell 5, exit-marker arm 3 and legend 7/4/11, seconds per minute and hour. **Kept as literals:** one-off offsets inside a single layout function (pause rows y+30/48/60/72, game-over and level-complete rows, banner and campaign-complete spacing, menu and level-select spacing, crosshair arms, the minimap key pixel art, debug-overlay padding). | 5 No magic numbers | Named as listed. A comment above the overlay functions states the literal rule. | Kept (one-off offsets): each is used once next to the text it places, a name would only restate the position, and CODING_STANDARDS 5 allows it |
| UI-15 | UiRenderer.cpp:20 `GetGlyph` | 60-case switch of hex rows. | 2 / 5 | None. | Kept: it is font data; a table would only move it |
| UI-16 | UiRenderer.cpp:325 `KeyCardRect` | `framebufferWidth` parameter is unused (`(void)`). | 4 | None. | Kept: public signature used by Engine and tests; documented why it is unused |
| UI-17 | UiRenderer.cpp:483, 498, 529 (pause, level complete, game over) | `cx` uses the requested `boxW`, not the width after `DrawCenteredPanel` shrinks it. The two differ only below about 218 / 248 px framebuffer width, never at 320x240. | 8 | None. | Kept: fixing changes output at small sizes (behaviour change); possible latent quirk for a later phase |
| UI-18 | UiRenderer.cpp:728-729 minimap legend | Laid out for the minimum 7-px marker, while the marker grows with the cell size. No overlap with the text (max arm 5 -> right edge +8, text at +11). | 9 | Comment now says so. | Kept: changing it changes pixels |

## Game world and content

### `engine/world`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| WORLD-1 | engine/world/EntityPlacementLoader.cpp:110 `ReadPlacement` | Cognitive complexity 74: type, coordinates, the per-type fields, two lambdas and the unknown-field scan all in one function | 2 Small functions | Split into ReadPlacementType, ReadCellFields, ReadPlayerFields, ReadPickupFields (+ ParsePickupKind), ReadDoorFields, ReadEnemyFields (+ ParseEnemyVariant). Lambdas became ReadKeyColorField and WarnInapplicable. Message order and wording unchanged | Fixed |
| WORLD-2 | engine/world/EntityPlacementLoader.cpp:371 `ParseEntityPlacementsJson` | Cognitive complexity 32 | 2 Small functions | Split into WarnTopLevelFields, ReadLightZones and ReadEntities | Fixed |
| WORLD-3 | engine/world/EntityPlacementLoader.cpp:453 `ParseEntityPlacementsCsv` | Cognitive complexity 34 | 2 Small functions | Split into SplitCsvCells and ReadCsvRow; the line loop stays | Fixed |
| WORLD-4 | engine/world/EntityPlacementLoader.cpp:243, :292 `ReadPlacement`, `ReadLightZone` | The same unknown-field warning loop appears twice | 3 No duplication | One WarnUnknownFields(object, where, {known...}) | Fixed |
| WORLD-5 | engine/world/EntityPlacementLoader.cpp:20 `Diagnostics` | Struct with reference members (cppcoreguidelines-avoid-const-or-ref-data-members) | 7 Ownership | Class with borrowed pointers and Error()/Warning() methods | Fixed |
| WORLD-6 | engine/world/EntityPlacementLoader.cpp:472, :496 `ParseEntityPlacementsCsv` | Literal BOM bytes, length 3 and the "3 columns" minimum | 5 Magic numbers | kUtf8ByteOrderMark, kUtf8ByteOrderMarkLength, kCsvRequiredColumns | Fixed |
| WORLD-7 | engine/world/EntityPlacementLoader.cpp:27, :31 `Trim`; :45, :243, :292, :313 | Implicit int-to-bool from isspace; C arrays (kTypes, known-field lists, kCsvColumns) | tidy | `!= 0`; std::array / initializer_list | Fixed |
| WORLD-8 | engine/world/EntityPlacementLoader.cpp:147 `ReadPlacement` | "mapX and mapY are required" is only reported when no earlier field (x, y) failed. This is an existing quirk | 8 Error handling | Kept as is and made explicit (`reportMissing` parameter + comment) | Kept: changing it would change the error output (tests/DevKit parity) |
| WORLD-9 | engine/world/EntityPlacementLoader.h:22, :26, :39; Entity.h:72, :96; LevelBuilder.h:16; Map.h:44-46 | Redundant `{}` member initialisers | tidy | Removed | Fixed |
| WORLD-10 | engine/world/InteractionRules.cpp:51 `UpdateEntityInteractions` | Cognitive complexity 44: exit switch, pickups, triggers and doors in one loop | 2 Small functions | Split into UseExitSwitch, CollectPickup and UseDoor. Loop and distance tests unchanged | Fixed |
| WORLD-11 | engine/world/InteractionRules.cpp:54-56 `UpdateEntityInteractions` | Reach radii as function locals with no explanation | 5 Magic numbers / 9 Comments | Namespace constants with a comment (squared, centre to centre, map cells) | Fixed |
| WORLD-12 | engine/world/InteractionRules.cpp:12, :43, :157 | C array kOffsets; hand-written any_of; if-clamp instead of std::min | tidy | std::array, std::any_of, std::min (same comparison, same result) | Fixed |
| WORLD-13 | engine/world/EnemyAi.cpp:120 `UpdateEnemy` | Cognitive complexity 27 | 2 Small functions | Idle and Chase handling moved to UpdateIdle and UpdateChase. State switch unchanged | Fixed |
| WORLD-14 | engine/world/EnemyAi.cpp:74, :82, :194, :253, :280 | Unnamed 1e-4f (x3), 0.2f (stuck progress), 0.9f (chase stop share) | 5 Magic numbers | kMinStep, kCoincidentDistance, kStuckProgressFraction, kChaseStopFraction (same float values) | Fixed |
| WORLD-15 | engine/world/MovementRules.cpp:56 `MoveAxisSeparated` | The x and y step loops are copies of each other | 3 No duplication | One SlideAlongAxis(…, alongX, …). Arithmetic per step unchanged (`c + sign * step`) | Fixed |
| WORLD-16 | engine/world/MovementRules.cpp:18-27 `IsBlockedByWall`, :111 `IsCircleOverlappingCell` | Same closest-point-to-cell computation twice (only the comparison differs) | 3 No duplication | DistanceSquaredToCell helper. `<` and `<=` kept where they were | Fixed |
| WORLD-17 | engine/world/MovementRules.cpp:18, :20, :46, :112, :114; :60 | use-auto casts; hand-written any_of; step size local without comment | tidy / 9 | auto, std::any_of; kStepSize at namespace scope with comment | Fixed |
| WORLD-18 | engine/world/Navigation.cpp:72-73, :115-116 `FindPath`, `ComputeSoundDistances` | Two copies of the direction tables, as C arrays; `d >= 4` and parent sentinels -1/-2 unnamed | 3 / 5 / tidy | One kNeighbourDx/Dy std::array pair, kOrthogonalNeighbours, kUnvisited, kStartCell | Fixed |
| WORLD-19 | engine/world/Navigation.cpp:33-34, :47-48 `HasLineOfSight`, `HasClearCorridor` | Length hand-written via sqrt; 1e-4f unnamed | 3 / 5 | Length(delta) (identical expression), kSameSpotDistance | Fixed |
| WORLD-20 | engine/world/Map.cpp:45-95 | Row-major index expression written 6 times; cell count 3 times in Resize | 3 No duplication | Private Map::IndexOf; one cellCount local | Fixed |
| WORLD-21 | engine/world/LevelBuilder.cpp:42 `SpriteForEntity` | Nested conditional for the key texture | tidy | KeySpriteTexture(KeyColor) switch | Fixed |
| WORLD-22 | engine/world/LevelBuilder.cpp:102, :236, :272 | push_back of a literal; repeated cast type; C array kKinds | tidy | emplace_back, auto, std::array | Fixed |
| WORLD-23 | engine/world/GameplayDefinitions.h:40 `kEnemyVariants`, KeyColor.h:19 `kKeyColors` | C arrays in public headers | tidy | std::array (all users index/iterate the same way; no caller edits needed) | Fixed |
| WORLD-24 | engine/world/Sprite.h:18 `SpriteTextureId` | Only the first three enumerators initialised (inconsistent) | tidy | Only `Guard = 0` initialised; values unchanged | Fixed |
| WORLD-25 | Entity.h, InteractionRules.h, Map.h, MapLoader.h, MovementRules.h, Sprite.h, EntityPlacementLoader.h, LevelBuilder.h, Player.h | Header does not start with a comment saying what it is for (MapLoader.h had no doc at all) | 9 Comments | Short purpose comment added to each | Fixed |
| WORLD-26 | engine/world/EntityPlacementLoader.cpp:25 `Trim` | Same Trim as in assets/ContentConfig.cpp | 3 No duplication | none | Kept: sharing it needs a string helper in `core` (another module); left for the core owner |
| WORLD-27 | engine/world/EntityPlacementLoader.cpp:80 `ReadInteger` | Range check like GameplayConfig's GameplayReader::Int | 3 No duplication | none | Kept: different message formats (both pinned by tests/DevKit) and different modules; a shared helper would only move the differences into parameters |
| WORLD-28 | engine/world/LevelBuilder.cpp:271 `DescribeLevelContents` | Type names repeat PlacementTypeName's strings | 3 No duplication | none | Kept: the names are keyed by EntityType here, by PlacementType there (two enums, the C API owns EntityType) |

### `engine/assets`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| ASSET-1 | engine/assets/AudioConfig.cpp:13-77, ContentConfig.cpp:15-78 | Trim, LoadKeyValueConfig and RequireKey copied byte for byte into both files | 3 No duplication | One copy in ContentConfig.cpp, declared in ContentConfig.h as LoadKeyValueConfig and RequireConfigKey; AudioConfig uses it | Fixed |
| ASSET-2 | engine/assets/AudioConfig.cpp:15, :20, ContentConfig.cpp:17, :22 `Trim` | Implicit int-to-bool from isspace | tidy | `!= 0` (in the one remaining copy) | Fixed |
| ASSET-3 | engine/assets/ContentConfig.cpp:66 `LoadKeyValueConfig` | Returns `errors.empty()`, so a caller that passes a non-empty error list gets false even for a good file. The other loaders compare against `errorsBefore` | 8 Error handling | Behaviour documented in the header | Kept: a contract change; every current caller passes an empty list (Engine::Init clears it between loads). Suggest `errors.size() == errorsBefore` as a separate, deliberate change |
| ASSET-4 | engine/assets/CampaignConfig.cpp:66 `ParseCampaign` | Cognitive complexity 40 | 2 Small functions | Split into ReadLevels, ReadLevel, ReadLevelId and ReadLevelTheme. Messages and order unchanged | Fixed |
| ASSET-5 | engine/assets/CampaignConfig.cpp:57 `IsValidContentId`, :139 | Hand-written all_of; copy where a move works | tidy | std::all_of; std::move | Fixed |
| ASSET-6 | engine/assets/GameplayConfig.cpp:156-158 `GameplayReader` | Reference data members | 7 Ownership | Owns a copy of the source name; borrowed pointers to the message lists | Fixed |
| ASSET-7 | engine/assets/GameplayConfig.cpp:146 `GameplayReader::Hint` | Returns std::string built from literals | tidy | Returns const char* (std::string_view cannot be `+`-ed onto a std::string in C++17) | Fixed |
| ASSET-8 | engine/assets/GameplayConfig.cpp:215, :298 | C arrays (archetype targets, snprintf buffer); unnamed 512 | tidy / 5 | std::array; kDescribeBufferSize | Fixed |
| ASSET-9 | engine/assets/ContentConfig.h:29 `kSpriteTextureKeys` | C array in a public header | tidy | std::array (callers unchanged) | Fixed |
| ASSET-10 | engine/assets/AssetManager.h:19, Texture.h:25 | Redundant `{}` member initialisers | tidy | Removed | Fixed |
| ASSET-11 | engine/assets/TextureLoader.cpp:70 `LoadPPM` | Normalising lambda rebuilt for every pixel inside the loop | 2 Small functions | NormalizeChannel helper (same arithmetic) | Fixed |
| ASSET-12 | engine/assets/TextureLoader.cpp:91-93 `BuildFallbackChecker` | `tileSize` breaks the kPascalCase constant rule; colours named `a`/`b` | 1 Names | kTileSize, `magenta`/`black`, comment why | Fixed |
| ASSET-13 | AssetManager.h, AudioConfig.h, Texture.h, TextureLoader.h; AudioConfig.h:11 | No header purpose comment; masterVolume default 0.85 without a note | 9 Comments | Purpose comments; default documented (matches docs/CONTENT_DEFINITIONS.md) | Fixed |
| ASSET-14 | engine/assets/AudioConfig.cpp:99 `LoadAudioEffectDefinitions` | `catch (...)` around std::stof, and stof accepts a trailing garbage suffix ("0.5x") | 8 Error handling | none | Kept: stricter parsing would reject files that load today (behaviour change) |
| ASSET-15 | engine/assets/Texture.cpp:48, :56 `GetPixel`, `SetPixel` | Bounds check and index written twice | 3 No duplication | none | Kept: two uses only, in the renderer's sampling path; a helper adds nothing |

## C API and autoplay bot

### `engine/interop`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| INTEROP-1 | engine/interop/CommandBridge.cpp:50 `ProcessCommands` | Cognitive complexity 44: one switch that also parses LoadLevel paths, applies damage and kills enemies | 2 Small functions | `RunCommand` (the switch) + `SplitLevelPaths`, `LoadLevelCommand`, `DamagePlayerCommand`, `DamageEnemyCommand`, `SetDoorCellCommand`, `ShowMessageCommand`; `ProcessCommands` only swaps the queue and loops | Fixed |
| INTEROP-2 | engine/interop/CommandBridge.cpp:150 `ProcessCommands` (ShowMessage) | `2.0f` repeats `Engine::ShowMessage`'s default duration | 3 No duplication / 5 Magic numbers | call `ShowMessage(text)` without a duration and let the default apply | Fixed |
| INTEROP-3 | engine/interop/CommandBridge.cpp:139-148 `ProcessCommands` (OpenDoor/CloseDoor) | Same bounds check and `SetCell` in two cases | 3 No duplication | `SetDoorCellCommand(engine, cmd, tile)` | Fixed |
| INTEROP-4 | engine/interop/CommandBridge.cpp:89-91 `ProcessCommands` (LoadLevel) | `"game/defs/"` and `".entities.json"` (twice) unexplained literals | 5 Magic numbers | `kDerivedEntitiesDir`, `kEntitiesSuffix`, with a comment pointing to C_API.md | Fixed |
| INTEROP-5 | engine/interop/CommandBridge.cpp:10-53 (7 places) | `std::lock_guard` non-const; tidy wants `const std::scoped_lock` | 6 Const-correctness | `const std::scoped_lock lock(mutex_)` | Fixed |
| INTEROP-6 | engine/interop/CommandBridge.cpp:70-88 `ProcessCommands` | Non-const locals (`str`, `dot`, `slash`, `levelName`), redundant `= ""` init | 6 Const-correctness | const locals, gone with INTEROP-1 | Fixed |
| INTEROP-7 | engine/interop/CommandBridge.cpp:6 | `Log.h` included but unused | 4 Dead code | removed | Fixed |
| INTEROP-8 | engine/interop/CommandBridge.cpp:51 `ProcessCommands` | The reason the commands run outside the lock (a command pushes events, non-recursive mutex) was not written down | 9 Comments explain why | comment added | Fixed |
| INTEROP-9 | engine/interop/CommandBridge.h:55,91 `InteropCommand::strArg`, `InteropEvent::strArg` | C arrays (`avoid-c-arrays`); size 128 written twice | 5 Magic numbers / tidy | `kInteropStringCapacity = 128` names the size; `NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays)`, since the field is part of the C API struct layout | Kept: C API ABI (docs/C_API.md), the layout must stay `char[128]` |
| INTEROP-10 | engine/interop/CommandBridge.h:57-63, 93-99 `SetString` | Null check repeats what `CopyBoundedString` already does for a null source | 3 No duplication | one-line `SetString` calling `CopyBoundedString` (same result: an empty string) | Fixed |
| INTEROP-11 | engine/interop/CommandBridge.h:57-99 | `InteropCommand` and `InteropEvent` are identical structs with the same `SetString` | 3 No duplication | none | Kept: both are separate C API types; a shared base or alias would change the types the API exposes |
| INTEROP-12 | engine/interop/CommandBridge.h:117-119 | Redundant `{}` member initialisers | tidy readability-redundant-member-init | removed | Fixed |
| INTEROP-13 | engine/interop/CommandBridge.h:8 | `<string>` included but unused in the header | 4 Dead code | moved to CommandBridge.cpp, which uses it | Fixed |
| INTEROP-14 | engine/interop/CommandBridge.h:1, EngineInterop.h:1 | No header comment saying what the module is for | 9 Comments | header comments added (and the ABI rule: layouts and enum values must not change) | Fixed |
| INTEROP-15 | engine/interop/EngineInterop.cpp:19,23 `nw_engine_create` / `nw_engine_destroy` | Raw `new`/`delete` (`cppcoreguidelines-owning-memory`) | 7 RAII | `NOLINT(cppcoreguidelines-owning-memory)` + comment | Kept: the C caller owns the opaque `void*` handle and releases it with `nw_engine_destroy`; a smart pointer cannot cross the C API |
| INTEROP-16 | engine/interop/EngineInterop.cpp:28-225 (20 places) | `if` without braces; out-parameters tested as implicit `ptr -> bool` | tidy readability-braces / implicit-bool-conversion | braces, `!= nullptr` | Fixed |
| INTEROP-17 | engine/interop/EngineInterop.cpp:192-195 `nw_engine_spawn_particle` | `0.5f`, `0xFFFFC266u`, `0.06f` unexplained | 5 Magic numbers | `kDefaultParticleSeconds`, `kDefaultParticleColor`, `kParticleSize` | Fixed |
| INTEROP-18 | engine/interop/EngineInterop.cpp:168, 180 | `Entity& ent` and `Engine* engine` (play_sound) not const | 6 Const-correctness | const | Fixed |
| INTEROP-19 | engine/interop/EngineInterop.cpp:3 | `<cmath>` included but unused | 4 Dead code | removed; `<cstddef>`/`<cstdint>` added for what is used | Fixed |
| INTEROP-20 | engine/interop/EngineInterop.h:3 | `<stdint.h>`, not `<cstdint>` | tidy (did not fire) | comment saying why (C programs include the header) | Kept: the header must stay valid C |
| INTEROP-21 | engine/interop/EngineInterop.h:14 `nw_engine_create()` | Empty parameter list is "no prototype" in C (C_API.md notes it) | ABI | none | Kept: exported signature; changing it is an API change, out of scope |
| INTEROP-22 | engine/interop/CommandBridge.cpp:98-147 | Commands write `Engine` members directly (`playerHealth_`, `map_`, `entityManager_`) | 10 Module boundaries | none | Kept: interop sits on top of the engine by design (ARCHITECTURE.md); adding Engine setters would be a cross-module change with no second user |

### `engine/autoplay`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| AUTO-1 | engine/autoplay/AutoplayBot.cpp:808 `TraceRecorder::AfterFrame` | Cognitive complexity 52; six kinds of event in one function | 2 Small functions | one private method per event kind (`TraceHitsDealt`, `TracePickups`, `TraceTriggers`, `TraceDoors`, `TraceExit`, `TraceEnemyStates`, `TraceHitsTaken`), called in the same order | Fixed |
| AUTO-2 | engine/autoplay/AutoplayBot.cpp:1014 `RunAutoplaySession` | Cognitive complexity 50: frame loop, level start, end-of-level classification, carry-over and progress checks | 2 Small functions | `NewRunReport`, `SessionStartProblem`, `FrameLimit`, `BeginLevelReport`, `EndLevelIfOver`, `FinishCampaignReport` (`AddNotPlayedLevels`, `CheckCarryOver`, `CheckSavedProgress`) | Fixed |
| AUTO-3 | engine/autoplay/AutoplayBot.cpp:570 `Plan` | Cognitive complexity 37 | 2 Small functions | `FindPickupGoals`, `FindExitGoal`, `KeyOpensADoor`; the `consider` lambda became `Candidate::Consider` (Candidate is a private nested type) | Fixed |
| AUTO-4 | engine/autoplay/AutoplayBot.cpp:251 `Observe` | Cognitive complexity 37 | 2 Small functions | `ObserveKeys`, `ObserveEntities`, `ObserveDoor`, `ObserveSupplies`; same order of side effects | Fixed |
| AUTO-5 | engine/autoplay/AutoplayBot.cpp:657 `FollowPath` | Cognitive complexity 29 | 2 Small functions | `OpenDoorAhead`, `LookaheadTarget`, `ActAtGoal` | Fixed |
| AUTO-6 | engine/autoplay/AutoplayBot.cpp:514 `DescribeBlockage` | Cognitive complexity 28; two identical "title + items" loops; door lookup repeated from `IsPassable` | 2 Small functions / 3 No duplication | `DoorEntityAt` (shared with `IsPassable`), `AppendList` | Fixed |
| AUTO-7 | engine/autoplay/AutoplayReport.cpp:177 `FormatAutoplayTable` | Cognitive complexity 50 | 2 Small functions | `TableHeading`, `LevelRow`, `FormatColumns`, `TableSummary`; output byte-identical | Fixed |
| AUTO-8 | engine/autoplay/AutoplayReport.cpp:85 `ParseAutoplayCommandLine` | Cognitive complexity 41 | 2 Small functions | `TakesValue`, `ApplyOptionValue` (same messages, same check order) | Fixed |
| AUTO-9 | engine/autoplay/AutoplayBot.cpp (1173 lines) | One file held three topics: the bot, the `--trace` recorder, the session runner | 1 One topic per file | `TraceRecorder` moved to new engine/autoplay/AutoplayTrace.h/.cpp (CMakeLists.txt `neowolf_autoplay` sources, ARCHITECTURE.md module row updated); AutoplayBot.cpp is now 1027 lines | Fixed |
| AUTO-10 | engine/autoplay/AutoplayBot.cpp:469-470, 613-614 `SearchFromPlayer`, `Plan` | `kDx`/`kDy` C arrays declared twice | 3 No duplication / tidy avoid-c-arrays | one `kNeighbourSteps` `std::array<Vec2i, 4>` in the same order (the order decides between equally short paths) | Fixed |
| AUTO-11 | engine/autoplay/AutoplayBot.cpp:56, 63, 755, 963; AutoplayReport.cpp:56 | C-array `snprintf` buffers | tidy avoid-c-arrays | `std::array<char, N>` | Fixed |
| AUTO-12 | engine/autoplay/AutoplayBot.cpp:68-74, 257 `KeyMask`, `Observe` | Signed bit operations on the key mask (`bugprone-signed-bitwise`) | tidy | `unsigned` mask (`keyMask_` too) and `KeyBit(i)` | Fixed |
| AUTO-13 | engine/autoplay/AutoplayBot.cpp:192, 269, 593 | "available pickup" test (`Pickup && active && !collected`) written three times | 3 No duplication | `IsAvailablePickup` | Fixed |
| AUTO-14 | engine/autoplay/AutoplayBot.cpp:682-685, 708-711 `FollowPath` | Interact press + cooldown written twice | 3 No duplication | `PressInteract` | Fixed |
| AUTO-15 | engine/autoplay/AutoplayBot.cpp:1163-1166 `RunAutoplay` | Error-path report set the same three fields as `RunAutoplaySession` | 3 No duplication | both use `NewRunReport` | Fixed |
| AUTO-16 | engine/autoplay/AutoplayBot.cpp:133, 143, 1040 | `0.6f` (deadband share), `0.02f` (arrival distance), `10.0` (frame-limit margin) unexplained | 5 Magic numbers | `kDeadbandShareOfAimTolerance`, `kArrivedDistance`, `kFrameLimitMarginSeconds` (same float/double operations, same order) | Fixed |
| AUTO-17 | engine/autoplay/AutoplayReport.cpp:19 `ParseLevel` | `3` digits unexplained | 5 Magic numbers | `kMaxLevelDigits` with comment ("007" is level 7) | Fixed |
| AUTO-18 | engine/autoplay/AutoplayReport.cpp:127 `ParseAutoplayCommandLine` | "(at most 86400)" repeats `kMaxTimeLimitSeconds` | 3 No duplication | built from the constant (`Fixed(kMaxTimeLimitSeconds, 0)`, same text) | Fixed |
| AUTO-19 | engine/autoplay/AutoplayBot.cpp:832-834 `AfterFrame` (pickups) | The door/trigger branch of the cell expression can never apply to a pickup | 4 Dead code | pickup test first, then `CellOf(position)` | Fixed |
| AUTO-20 | engine/autoplay/AutoplayBot.cpp:1070 `RunAutoplaySession` | `where` string formatted every frame and used only on two end paths | 2 / clarity | formatted in those branches only (same text) | Fixed |
| AUTO-21 | engine/autoplay/AutoplayBot.cpp:121, 258, 767 | `use-auto`, `use-emplace`, `loop-convert` | tidy | applied | Fixed |
| AUTO-22 | engine/autoplay/AutoplayBot.cpp:7, 19 | `<map>` and `InteractionRules.h` no longer used after the trace move | 4 Dead code | removed | Fixed |
| AUTO-23 | engine/autoplay/AutoplayBot.h:172 `levelIndex_ = -2` | Sentinel unexplained | 9 Comments | comment (-2 none yet, -1 loaded by path) | Fixed |
| AUTO-24 | engine/autoplay/AutoplayBot.h:164 `Engine& engine_` | Reference member (`cppcoreguidelines-avoid-const-or-ref-data-members`) | tidy | `NOLINTNEXTLINE` with reason | Kept: the bot borrows its engine for its whole life and is never copied or assigned; a reference says "never null", a pointer would only weaken that |
| AUTO-25 | engine/autoplay/AutoplayBot.cpp:127, 394 `TurnTowards`, `FightVisibleEnemy` | `1e-8f`, `1e-3f` epsilon literals | 5 Magic numbers | none | Kept: obvious "zero length" guards, each used once next to the comparison |
| AUTO-26 | engine/autoplay/AutoplayBot.cpp:182-218, 251-315 `StartLevel` / `Observe` | Both count pickups and enemy health over the entities | 3 No duplication | none (beyond `IsAvailablePickup`) | Kept: StartLevel also records the doors and Observe compares and records progress; a shared census would only move the loop |

## Tests

### `tests/`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| TEST-1 | tests/LogicTests.cpp (whole file) | 83 clang-tidy diagnostics: 38 misc-const-correctness, 20 C arrays, 9 signed-bitwise, 4 isolate-declaration, 3 misplaced-widening-cast, 2 use-anyofallof, and one each of no-automatic-move, return-braced-init-list, pro-type-member-init, special-member-functions, use-emplace, inefficient-vector-operation, redundant-lambda-parameter-list, integer-division, implicit-widening, float-loop-counter, unnecessary-copy-initialization, std-exception-baseclass, exception-escape | 6 Const / 7 RAII / tooling | const locals and ScratchDirs, std::array/std::vector for the arrays, ScratchDir move members deleted, View fields default-initialised, unsigned pixel maths (TEST-9), size_t indices, isolated declarations, reserve, emplace_back; rest below | Fixed (3 Kept: TEST-2..4) |
| TEST-2 | tests/LogicTests.cpp:78 `FailTest` | bugprone-std-exception-baseclass: TestFailure is not a std::exception | 8 Error handling | `// NOLINT` with reason | Kept: deliberate, so no `catch (const std::exception&)` in code under test can swallow a failed assertion |
| TEST-3 | tests/LogicTests.cpp:2982 `TestKeyBindings` | C array `InputAction actions[2]` | 6 / tooling | Merge: platform's PLAT-5 made `MapKeyToActions` take `std::array<InputAction, kMaxActionsPerKey>`; the test uses it and the `NOLINT` is gone | Fixed (at merge) |
| TEST-4 | tests/LogicTests.cpp:4466 `main` | bugprone-exception-escape | 8 Error handling | `// NOLINTNEXTLINE` with reason | Kept: the test cases run inside RunTestCase's catch blocks; only stream output/allocation in the runner can throw, and terminating then is the right result |
| TEST-5 | tests/LogicTests.cpp:576 `RunAi` | float loop counter (`t += dt`); step count depended on float accumulation and a 1e-4 fudge | 2 / tooling | integer step count `lround(seconds / dt)`; checked it gives the same step count as the old loop for all 10 durations used (0.05 to 4.1 s) | Fixed |
| TEST-6 | :435-437, 800-801, 891, 1310, 1329, 1619-1628, 1767-1778, 1809, 1826, 1991-2035, 2454, 2968, 3516-3530, 4059-4068 | Magic numbers that are facts about the frozen reference level 1 (16x12, door (8,7), trigger (13,8), exit (15,8), 7 placements, 4 sprites, spawn (2,2)/2.5, guard x 6.5) repeated in many tests | 5 Magic numbers | `namespace ReferenceLevel1` constants with a comment pointing at tests/fixtures/reference | Fixed |
| TEST-7 | :3438-3469 `TestLockedDoorsAndKeys` | reference level 2's blue door (9,3) repeated 6 times | 5 Magic numbers | local kBlueDoorX/Y; the comment now lists level 2's doors and key | Fixed |
| TEST-8 | 22 calls such as :379; :705-706 `TestPickupCollection`; :2062 `MakeFlow` | player maxima `100, 99` repeated in every UpdateEntityInteractions call | 5 Magic numbers / 3 DRY | kPlayerMaxHealth / kPlayerMaxAmmo | Fixed |
| TEST-9 | :2130-2133, 3570-3575, 3714, 3737, 4103-4122, 4359 | Raw framebuffer indexing (`Data()[24 * 64 + 16]`) and channel extraction (`(p >> 16) & 0xFFu`) copied in 5 tests (also the signed-bitwise diagnostics) | 3 DRY | PixelAt, RedOf, GreenOf, BlueOf helpers | Fixed |
| TEST-10 | 19 calls (e.g. :1944), :2897 `kDt`; :3203, 3219, 3666 | 60 Hz frame step `1.0 / 60.0` / `1.0f / 60.0f` repeated | 5 / 3 | kFrameSeconds, kAiFrameSeconds | Fixed |
| TEST-11 | :3569, :4112 | field of view `1.04719755f` repeated | 5 Magic numbers | kFieldOfViewRadians (commented as the engine's 60-degree FOV) | Fixed |
| TEST-12 | :1022 `ReferenceEngineOptions`; :2592-2610, 2692-2695, 2733-2751, 2836-2840, 3023-3027, 3726-3730, 3769-3773 | The headless EngineOptions set-up (dataDir, headless, errorDialogs) copied 9 times | 3 DRY | HeadlessEngineOptions(dataDir, startLevel), ShippedEngineOptions(startLevel); ReferenceEngineOptions uses it | Fixed |
| TEST-13 | :2899-2903, :2932-2936 | Same "hold only this action" input injector in two tests | 3 DRY | HoldOnlyAction(engine, pressed) | Fixed |
| TEST-14 | :2904-2909 vs :2937-2942 | The two `tap` lambdas look duplicated | 3 DRY | none | Kept: they differ on purpose (release-then-press vs press-then-release). Merging them would change the frames the tests run |
| TEST-15 | :2947-2950, 3517-3520, 4053-4056 | Hand-written "find the enemy" loops; FindFirstEntity only worked on a const manager | 3 DRY | FindFirstEntity made a template (const follows the manager) and used here. Each case is reference level 1, which has one enemy, so it returns the same entity | Fixed |
| TEST-16 | :1794-1799 `TestDecorationsAndSpriteSync` | same kind of loop for the pickup and the enemy | 3 DRY | none | Kept: it picks the *last* of level 1's two pickups; FindFirstEntity would collect a different pickup |
| TEST-17 | :882-922 `TestMapPerimeterAndReachabilityValidation` | Inline flood fill with a local GridCoord struct, int/size_t mixing | 2 Small functions | CountUnreachableWalkableCells(map, start) using Vec2i and std::array | Fixed |
| TEST-18 | :2069-2085 `HasRequest`/`FindRequest`; :1047 `CandidateListContains` | the same search loop written twice | 3 DRY | HasRequest calls FindRequest; std::any_of | Fixed |
| TEST-19 | :3612-3629 `TestCorpseAndHitFlash` | dead code: `Entity brute` set up and never used, silenced with `(void)brute` | 4 Dead code | removed (the brute corpse case is still covered through `single`) | Fixed |
| TEST-20 | :12 | unused `#include <functional>` | 4 Dead code | removed | Fixed |
| TEST-21 | :2087 `Completed`; :942 `TestEngineInteropC_API` | unclear helper name; test name breaks PascalCase | 1 Names | LevelCompletedEvents; TestEngineInteropCApi (the CTest id is unchanged) | Fixed |
| TEST-22 | :1042 `MakeContentRoot`, :2058 `MakeFlow`, file top | helpers with no comment; no file header comment | 9 Comments | added comments | Fixed |
| TEST-23 | :531-565 `TestPlayerCombatAndShooting` | damage 35 and range 12.0f repeated in 4 calls | 5 Magic numbers | local kDamage, kRange | Fixed |
| TEST-24 | :3874-3894 `TestAutoplayFixtureLevel` | 30, 24, 100, 25: the fixture's start health and engine defaults | 5 Magic numbers | kStartHealth (also used to build the fixture's JSON), kDefaultWeaponDefinition.initialAmmo, kDefaultPlayerDefinition.maxHealth, kDefaultPickupDefinitions.health.value (same values) | Fixed |
| TEST-25 | :2666, 2679-2680 `TestThemeLoadingAndSwap`; :4101-4122 `TestSpriteVerticalCrop`; :3758-3761 `TestAiStress20Enemies` | 32 (theme texture size), 320/240/160/120±58 (screen), `(i / 5) * 9` integer division | 5 Magic numbers | kThemeTextureSize; kWidth/kHeight/centreX with a comment on the 120-pixel sprite; named column/row | Fixed |
| TEST-26 | :3682-3683 `TestHudKeyIconsLayout` | `6 + 80` mirror private offsets in UiRenderer::DrawHud | 5 Magic numbers | Merge: the ui module now exports `UiLayout::kHudAmmoX` and `kTextInset` (UI-14); the test uses them | Fixed (at merge) |
| TEST-27 | :4374 `kTests`, :4466-4550 `main` | C array test list; main parsed arguments, selected, ran and summarised in one function | 2 Small functions | AllTests() (function-local static vector; a namespace-scope vector trips bugprone-throwing-static-initialization); main split into SelectByFilter, RunTestCase, RunTestCases. Same output and exit codes | Fixed |
| TEST-28 | :762 `TestLogFile` | writes test_run.log in the working directory (the repo root under CTest) | 7 RAII | none | Kept: moving it to a ScratchDir would change the relative-path case it covers |
| TEST-29 | :4135-4188 `TestContentParityFixtures` | three similar "expected.json says otherwise" FailTest calls | 3 DRY | none | Kept: wording differs per section, and a helper would report its own line instead of the caller's |
| TEST-30 | :1331-1338 `TestCrlfContentLoading` | per-field placement comparison resembles expectEquivalent in TestEntityPlacementsJsonAndCsv | 3 DRY | none | Kept: it checks fewer fields with a 1e-4 tolerance; reusing expectEquivalent would change what is asserted |
| TEST-31 | :946-967 `TestEngineInteropC_API` | raw ints (`set_state(handle, 4)`, entity type 1) | 5 Magic numbers | none | Kept: the test pins the C ABI's integer values; each one is commented |

## Python tools

### `tools/devkit`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| DEVKIT-1 | tools/devkit/** (all modules and tests) | ruff: 121 UP006, 11 UP045, 1 UP007 (in files with `from __future__ import annotations`), 9 F401, 3 I001, 2 UP015, 2 UP012, 2 F541, 1 C416 | 4 Dead code / tooling | ruff fixes (PEP 585/604 only in annotations of future-import files; runtime aliases such as `Color = Tuple[...]`, `Table = List[...]` kept as typing forms for 3.8) | Fixed |
| DEVKIT-2 | validator.py:177 `validate_ppm` / asset_tools.py:9 `inspect_ppm` | the same PPM comment-stripping tokenizer written twice (and both reassigned the loop variable, PLW2901) | 3 No duplication | `asset_tools.ppm_tokens`, used by both | Fixed |
| DEVKIT-3 | validator.py:270 `validate_wav` / asset_tools.py:74 `inspect_wav` | the same RIFF chunk walk written twice (unused `byte_rate`, `block_align`: RUF059) | 3 No duplication | `asset_tools.read_wav_format` returning a `WavFormat` | Fixed |
| DEVKIT-4 | validator.py:69 `VALID_TILE_CHARS`, :109 `VALID_ENTITY_TYPES`, :110 `MAX_PICKUP_VALUE` | never used (MAX_PICKUP_VALUE also duplicated engine_formats.MAX_GAMEPLAY_STAT) | 4 Dead code | deleted | Fixed |
| DEVKIT-5 | validator.py:518 `json_dump` | public one-line wrapper with a function-local `import json`, used once | 4 Dead code | `json.dumps` with a module import | Fixed |
| DEVKIT-6 | validator.py:454 `GAMEPLAY_SCHEMA`, :634 pickup schema | limit 10000 repeated 10 times; the pickup schema was copied inline in `validate_gameplay` | 5 Magic numbers, 3 DRY | `MAX_GAMEPLAY_STAT`; pickups read `GAMEPLAY_SCHEMA["pickup"]` with the per-kind default | Fixed |
| DEVKIT-7 | validator.py:547 `_read_section` | int/number type checks re-implemented (same as engine_formats `_is_int`/`_is_number`) | 3 No duplication | engine_formats `is_int`/`is_number` made public and reused | Fixed |
| DEVKIT-8 | validator.py:568 `validate_gameplay` | 24 branches: defaults, section access, enemies, pickups, cross-field rules in one function; schema-defaults comprehension repeated 4x | 2 Small functions | `_gameplay_defaults`, `_schema_defaults`, `_section`, `_read_enemies`, `_read_pickups`, `_check_starting_values` | Fixed |
| DEVKIT-9 | validator.py:116 `validate_map`, :177 `validate_ppm` | 16 / 17 branches | 2 Small functions | `_check_map_perimeter`, `_check_ppm_pixel_sample` | Fixed |
| DEVKIT-10 | validator.py:197, :289 | `except Exception` around a file read | 8 Error handling | `except OSError` (the only error those reads raise) | Fixed |
| DEVKIT-11 | validator.py:256, :293, :342 | magic 300 (pixel sample), 44 (WAV header), 255, 0xFFFE | 5 Magic numbers | `PPM_SAMPLED_TOKENS`, `WAV_HEADER_BYTES`, `MAX_PPM_COLOR`, `WAV_FORMAT_EXTENSIBLE`, `WAV_FORMAT_PCM` | Fixed |
| DEVKIT-12 | validator.py:677 `check_level_layout` | 19 branches | 2 Small functions | `_count_map_cells`, `_check_door_and_trigger_cells` | Fixed |
| DEVKIT-13 | validator.py:787 `analyse_level_progress` | 21 branches (entity scan, BFS, exit count in one) | 2 Small functions | `_progress_entities`, `_flood_fill`, `_count_exits`, `_NEIGHBOURS` | Fixed |
| DEVKIT-14 | validator.py:884 `read_campaign` | 22 branches | 2 Small functions | `_check_campaign_title`, `_read_campaign_level` | Fixed |
| DEVKIT-15 | validator.py:1074 `validate_project` | 33 branches / 84 statements | 2 Small functions | `_validate_maps`, `_validate_assets`, `_ReferenceChecker`, `_validate_themes`, `_validate_sprites_cfg`, `_validate_audio_cfg`, `_validate_entity_files` | Fixed |
| DEVKIT-16 | validator.py:660 `_cell_of`, :871 `_is_content_id` | private names imported by other modules (cli imported `_is_content_id`; map_tools re-implemented `_cell_of` 3x with `int(math.floor())`, RUF046 x14) | 1 Names, 3 DRY | public `cell_of`, `is_content_id`; `math.floor` without `int()` | Fixed |
| DEVKIT-17 | validator.py:763 `find_reachable_exits`, map_tools.py:134 `analyze_reachability` | "a door on every 'D'" list built twice | 3 No duplication | `validator.door_on_every_d` | Fixed |
| DEVKIT-18 | map_tools.py:111, :349 | function-local imports "avoid an import cycle", but validator never imports map_tools: the comment was wrong | 9 Comments | top-level import | Fixed |
| DEVKIT-19 | map_tools.py:186 `render_ascii_preview` | 25 branches, 70 statements, 12 UPPER_CASE locals (N806), colour codes repeated in an if chain and in the legend | 2 Small functions, 3 DRY | `PREVIEW_COLORS`, `ENTITY_SYMBOLS`, `LEGEND` tables, `_paint`, `_entity_overlay` | Fixed |
| DEVKIT-20 | map_tools.py:5, :8 | unused `deque`, `Set`, `Tuple` | 4 Dead code | removed | Fixed |
| DEVKIT-21 | cli.py:122 `cmd_inspect_map` | 13 returns, 36 branches, 82 statements | 2 Small functions | `_default_entity_file`, `_inspected_files`, `_load_inspected_entities`, `_print_engine_errors`, `_print_light_zones`, `_reachability_spawn`, `_print_reachability` | Fixed |
| DEVKIT-22 | cli.py:282 `cmd_create_map` | 10 returns, 19 branches; magic default theme "bunker" | 2 Small functions, 5 Magic numbers | `_created_map_path`, `_campaign_to_register`, `_register_level`, `DEFAULT_THEME` | Fixed |
| DEVKIT-23 | cli.py (all commands) | exit codes as bare 0/1/2 | 8 Error handling (documented codes) | `EXIT_OK`, `EXIT_FAILURE`, `EXIT_USAGE` (values unchanged, docs/DEVKIT_GUIDE.md "Exit codes") | Fixed |
| DEVKIT-24 | cli.py:635 `main`, :382 `cmd_create_asset` | fallbacks for an unknown command / asset type that argparse `choices` make unreachable | 4 Dead code | dict dispatch (`handlers[...]`, `ASSET_TYPES`; `--type` choices come from the table) | Fixed |
| DEVKIT-25 | cli.py:117, :220, :240, :55 | `l` name (E741), `%` formatting (UP031), magic 10 unreachable cells, list concatenation (RUF005) | 1 Names, 5 Magic numbers | `level`, f-string, `UNREACHABLE_CELLS_SHOWN`, unpacking | Fixed |
| DEVKIT-26 | doctor.py:227 `check_toolchain` | 21 branches | 2 Small functions | `_check_cmake`, `_check_ninja`, `_check_windows_compiler`, `_check_unix_compiler`, `_first_line` | Fixed |
| DEVKIT-27 | doctor.py:267 / packager.py:112 | vswhere.exe path built twice | 3 No duplication | `doctor.vswhere_path`, used by `packager.find_cmake` | Fixed |
| DEVKIT-28 | doctor.py:332 `run_doctor` | `color` parameter unused (`del color`, "kept for compatibility" with no caller passing it); function also did layout and content checks inline | 4 Dead code, 2 Small functions | parameter removed; `_check_project_layout`, `_check_content` | Fixed |
| DEVKIT-29 | doctor.py:97, :436, :444 | timeout 5, 5 errors / 3 warnings shown; subprocess.run without check (PLW1510) | 5 Magic numbers | `COMMAND_TIMEOUT_SECONDS`, `CONTENT_ERRORS_SHOWN`, `CONTENT_WARNINGS_SHOWN`; `check=False` | Fixed |
| DEVKIT-30 | packager.py:317 `create_release_package`, :174 `check_package_tree` | 13 returns / 20 branches; 16 branches | 2 Small functions | `_build`, `_install_and_check`, `_check_app_plist`, `_app_bundle` | Fixed |
| DEVKIT-31 | packager.py:81, :92, :126, :235, :258, :260 | 65536, 127, 30, 315532800+86400, 3, 0x10; subprocess.run without check | 5 Magic numbers | `HASH_CHUNK_BYTES`, `EXIT_COMMAND_NOT_FOUND`, `VSWHERE_TIMEOUT_SECONDS`, `ZIP_EPOCH_SECONDS`, `ZIP_UNIX_SYSTEM`, `MSDOS_DIRECTORY_ATTRIBUTE`; `check=False` | Fixed |
| DEVKIT-32 | engine_formats.py:241 `check_placement` | 21 branches | 2 Small functions | `_check_pickup` | Fixed |
| DEVKIT-33 | engine_formats.py:328, :331 | "0.2 to 1.0" hard-coded next to MIN/MAX_LIGHT_LEVEL; loop variable `raw` reassigned (PLW2901 x2) | 3 DRY | messages built from the constants (same text); new local names | Fixed |
| DEVKIT-34 | def_tools.py:22 `CSV_COLUMNS` | the engine's CSV column table copied from engine_formats.CSV_COLUMNS; magic 3 (type,x,y) | 3 No duplication | derived from engine_formats; `MIN_CSV_COLUMNS` | Fixed |
| DEVKIT-35 | level_docs.py:57 `LEGEND` | never used by code (the pages carry the text) | 4 Dead code | deleted | Fixed |
| DEVKIT-36 | asset_tools.py:213 `generate_beep_wav` | 32767, -32768, 36, 16, 1 in the WAV writer; comment "slight attack and fade out" was wrong (no attack) | 5 Magic numbers, 9 Comments | `INT16_MAX`, `WAV_HEADER_BYTES`, `PCM_FMT_CHUNK_BYTES`, `WAV_FORMAT_PCM`; comment corrected | Fixed |
| DEVKIT-37 | gen_textures.py:172 `bunker`, :222 `stone`, :262 `tech`, :329 `classic` | 17/17/17/25 branches: one if/elif chain per theme | 2 Small functions | one painter function per slot, a slot table per theme, `paint_slot`; theme-wide colours as constants | Fixed |
| DEVKIT-38 | gen_sprites.py:51-67 | `SEED`, `SIZE`, `Color`, `clamp`, `shade` copied from gen_textures (which it already imports) | 3 No duplication | imported from gen_textures | Fixed |
| DEVKIT-39 | gen_icon.py:183 `render` | nested `fill`/`fill_pattern` captured loop variables (B023 x6) | 2 Small functions | module-level `_fill_span`, `_copy_span` taking the row and body range | Fixed |
| DEVKIT-40 | gen_sounds.py:73, :289 | `int(round())` (RUF046), 32767 literal | 5 Magic numbers | `round()`, `INT16_MAX` | Fixed |
| DEVKIT-41 | release_readme.py:119 `_format_table` | magic 26 | 5 Magic numbers | `KEY_COLUMN_MAX` | Fixed |
| DEVKIT-42 | devkit.py:12 | import after the sys.path set-up (E402) | tooling | `# noqa: E402  needs the sys.path entry above (run as a script)` | Kept: the import must follow the path set-up for script runs |
| DEVKIT-43 | tests/*.py | RUF013 x3, RUF005 x4, PLW1510 x6, RUF059 x4, PLW2901, unused imports | tooling | `Optional[...]`, unpacking, `check=False` (return codes are what the tests assert), `_` | Fixed |
| DEVKIT-44 | gen_textures.py:449, gen_sprites.py:362, gen_sounds.py:310 `main` | the same --check / write loop in three generators | 3 No duplication | - | Kept: they differ on purpose (text compared with universal newlines vs bytes, one subfolder per theme), the scripts run standalone, and gen_sounds imports nothing from the others; a shared helper would need a new module |
| DEVKIT-45 | gen_sounds.py:53 `SEED` | same seed value as gen_textures | 3 No duplication | - | Kept: gen_sounds is independent of the image generators; importing gen_textures only for a number would couple them |
| DEVKIT-46 | def_tools.py:38 `load_entity_csv` vs engine_formats.parse_entities_csv | two CSV readers | 3 No duplication | - | Kept: sync-entities reads leniently (Python float/int, a short row is skipped) while the engine reader is strict; switching would change sync-entities' behaviour |
| DEVKIT-47 | asset_tools.py:50 `inspect_ppm` | 240/15 near-magenta thresholds | 5 Magic numbers | - | Kept: used once, next to the comment that explains them (colour-key detection tolerance) |
| DEVKIT-48 | cli.py:521-522 `--stats`, `--reachability` | hidden flags that do nothing | 4 Dead code | - | Kept: accepted for old command lines (CLI flags must not change) |

### `tools/ci`

| ID | Where | Finding | Principle | Fix | Status |
|---|---|---|---|---|---|
| CI-1 | tools/ci/package_smoke.py:331 `smoke_archive` | 22 branches, 74 statements | 2 Small functions | `_check_checksum`, `_locate_game`, `_check_layout`, `_run_package` | Fixed |
| CI-2 | package_smoke.py:157-211 `pe_summary`, `check_windows_exe` | PE values 0x20B, 0x8664, 2, 3/14/16, 0x80000000 and header offsets unexplained | 5 Magic numbers, 9 Comments | `PE32_PLUS_MAGIC`, `IMAGE_FILE_MACHINE_AMD64`, `IMAGE_SUBSYSTEM_WINDOWS_GUI`, `RT_ICON`/`RT_GROUP_ICON`/`RT_VERSION`, `RESOURCE_NAME_IS_STRING`; comments on e_lfanew, the optional header and data directories | Fixed |
| CI-3 | package_smoke.py:227, :300 | subprocess.run without check (PLW1510); 120 s, 127, 65536 literals | 8 Error handling, 5 Magic numbers | `check=False` (return codes are inspected); `TOOL_TIMEOUT_SECONDS`, `EXIT_COMMAND_NOT_FOUND`, `HASH_CHUNK_BYTES` | Fixed |
| CI-4 | package_smoke.py:260-274 `check_macos_app` | `l` names (E741 x5); output-indent expression repeated | 1 Names, 3 DRY | `line`/`lib`; `_indented` | Fixed |
| CI-5 | tools/ci/smoke_run.py:59, :64, :72 `run` | 0.1 s, 10 s, 60 lines | 5 Magic numbers | `POLL_SECONDS`, `TERMINATE_GRACE_SECONDS`, `LOG_LINES_SHOWN` | Fixed |
| CI-6 | package_smoke.py:53-64 `FORBIDDEN_EXECUTABLES`, `REQUIRED_FILES` | same lists as tools/devkit/packager.py | 3 No duplication | - | Kept: the smoke test is an independent second check of a downloaded package and runs as a standalone script in CI; sharing the list with the packager would let one mistake pass both |
