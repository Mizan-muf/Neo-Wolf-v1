# Testing Neo-Wolf

All tests are registered with CTest, so one command runs everything:

```bash
ctest --preset macos            # macOS   (after cmake --preset macos && cmake --build --preset macos)
ctest --preset linux            # Linux   (run in CI as ci-linux)
ctest --preset windows-msvc     # Windows (run in CI as ci-windows-msvc; windows-msvc-debug for Debug)
```

`./build.sh`, `build.cmd`, `build.ps1` and `make check` configure, build and then run exactly
this. Any failing test makes them exit non-zero.

## Test layers

| CTest name | Label | What it runs | What it covers |
|---|---|---|---|
| `native.<id>` (69 tests) | `native` | `neo_wolf_tests <id>` | C++ engine logic: math, input, map queries, raycasting, texture/asset loading, entities, collision, interaction rules, game state, combat, AI, pickups, command bridge, logging and log-file rotation, level reset, a scripted vertical-slice run, map reachability, the C interop API, content-root resolution (with temporary directories, including a non-ASCII path), command-line parsing, CRLF content files, and the data pipeline: exact `gameplay.json` values and its validation errors, JSON/CSV entity placements and their error messages, decorations and sprite sync, pickup kind from data, failing level loads, and `edited_data_changes_game`. Phase 5 adds: `game_flow_menu`, `game_flow_progression`, `game_flow_death_pause` (every `GameFlow` transition, the death snapshot, carry-over clamping, Continue), `campaign_manifest_validation`, `exit_switch_activation` (in reach and facing activates; behind, sideways, too far or behind a wall does not), `progress_file_roundtrip` (round trip, atomic save, 11 corrupt files, the engine ignoring a corrupt save), `pref_dir_resolution`, `theme_loading_and_swap`, `ui_layout_bounds` (every screen stays inside the framebuffer with a 4-UI-pixel margin and nothing is clipped; after 1.0.0 at 320×240, 640×360, 640×480, 960×540, 1280×720, 320×200 and 1920×1080 and at the edges of the shape limits, 400×320, 1350×1080 (5:4), 640×270 and 1920×810 (64:27), plus the UI scale per size, the UI at 640×480 and 960×720 being the 320×240 UI scaled up pixel for pixel, every shipped level's minimap having its 320×240 panel and cell size (on a canvas at least 270 UI pixels high and wide: a bigger panel, cells at least as big), the level banner clear of the debug overlay's text and at least 3 UI pixels above the crosshair, the pause, level-complete and game-over panels ending above the HUD bar, the HUD with a long test-level title and every key card, and the five-item main menu (saved progress and the test level) keeping its controls summary), `campaign_playthrough` (since Phase 7 a thin test on the autoplay bot: `RunAutoplaySession` plays from the main menu through every level of `campaign.json` → CampaignComplete with carry-over; since Phase 8 it is data-driven: the test checks the engine state, each level's id and enemy count against its entity file, that every locked door the bot opened has that lock and was opened with a key collected in the level, and that the saved progress unlocks the last level, then uses Continue), `new_game_resets_world` (C7), `key_bindings` (B13, minimap toggle) and `wall_texture_orientation` (wall textures are no longer mirrored). Phase 6 adds: `line_of_sight` (walls and closed doors block, open doors do not), `enemy_ai_behavior` (rewritten: Idle → Alert → Chase → Attack with the reaction time and attack rate, no waking through walls, Chase → Search → Idle when the player is lost), `enemy_hearing` (shots heard through open cells only, path length counts; in the engine a shot wakes the reference level 3's brute but not the guard behind a closed door), `enemy_pathfinding` (round a wall, no corner cutting, closed doors stop enemies), `enemy_archetypes_from_data`, `entity_variant_validation`, `locked_doors_and_keys`, `door_close_blocked`, `corpse_and_hit_flash`, `enemy_separation`, `hud_key_icons_layout`, and `ai_stress_20_enemies` (20 enemies chasing in a 48×48 map for 600 frames of `Update` + `Render`: no enemy ends up in a wall; it prints the average and worst time per frame and fails if the average exceeds 8 ms in an optimised build, 100 ms in a Debug build). Phase 7 adds: `autoplay_fixture_level` (the bot on a generated level with a health detour, an unlocked and a blue locked door, the blue key and a guard: complete, the report's numbers, the same JSON on a second run, `timeout` with a 1 s limit), `autoplay_unwinnable_levels` (`stuck` with a reason for the walled-off fixture and for a red door without a key, `--all` stops and marks later levels `not-played`, a content root that does not start), `autoplay_command_line` (options, usage errors, the JSON fields), `player_enemy_collision` (C15: living enemies block the player, an overlapping player can back away, corpses do not block, enemies are pushed out of the player; in the engine, walking at level 1's guard), `sprite_vertical_crop` (a sprite taller than the screen is cropped, not squeezed) and `content_parity_fixtures` (the engine's loaders against `tests/fixtures/parity/expected.json`, see below). Phase 8 adds `light_zones` (the `"lights"` rectangles of an entity file: order, ranges, field and out-of-map errors, wall faces lit from the front). After 1.0.0: `render_size_option` (`--render-size` valid and invalid values, including shapes outside 5:4 to 64:27, and their error message, the default 640×360, the engine rendering at a given size and refusing an invalid one, the initial window size and `PlatformSDL::FitWindow` on large, small and unknown desktops) and `projection_square_pixels` (at 320×240, 640×360 and 640×480 a wall at distance 2.5 is 1/2.5 of the screen height, a one-unit wall face as many pixels wide as high, every column's ray meets the wall at the sideways offset square pixels give, and a sprite and a particle at the same distance match the wall, centred or off to the side). For the Test Chamber (1.5): `game_flow_test_level` (the test level's menu item, its exit switch restarting it with no level-complete screen and no save, death and restart, pause and quit to the menu, the boot request, `CancelLevelStart` after a failed load) and `test_level` (a scratch content root with a small test level: without `EngineOptions::testLevelBoot` the engine starts on the menu, `--level` and `--preview` win over `bootIntoTestLevel`, with it the engine starts in the test level at the initial values; a sign trigger shows its message; the exit switch restarts it, reports level 0 and writes no `progress.json`; death, `ResetLevel`, pause → quit and the menu item; a campaign level that fails to load from the menu leaves the flow in the test level, whose exit still saves nothing; its HUD and banner inside the margins); `campaign_manifest_validation` and `entity_placements_json_csv` also cover bad `testLevel` / `bootIntoTestLevel` definitions and the trigger `message` field in JSON and CSV (including CSV bytes that are not UTF-8: an error, never an exception). Source: `tests/LogicTests.cpp`. |
| `native_test_registry` | `native` | `cmake -P cmake/CheckTestRegistry.cmake` | Checks that every test case compiled into `neo_wolf_tests` has a CTest entry and the reverse. Catches a test that was added but never registered. |
| `game_smoke_frames` | `game` | `cmake -P cmake/RunGame.cmake` → `neo_wolf --frames 120 --pref-dir <build>/test-pref/<test>` | Starts the real game headless (`SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`, set with the `ENVIRONMENT_MODIFICATION` test property, which also **removes** `SDL_RENDER_DRIVER`, `NEOWOLF_DATA_DIR` and `NEOWOLF_PREF_DIR`). Runs from the build directory, so content must be found relative to the executable. Passes if the game exits with 0 and logs the `--pref-dir` folder, its content root, the campaign with its level count, its renderer, the first level's entity count (level 1's, or the test level's when `campaign.json` sets `bootIntoTestLevel`; read from `campaign.json` and that entity file at configure time) and 120 frames. Proves the software-renderer fallback (B11) and the content-root search. Takes about 2 s (60 FPS cap). |
| `game_smoke_level_1` … `_N` | `game` | `neo_wolf --level N --frames 120 --pref-dir …` | Each campaign level starts directly, loads its theme and runs 120 frames; exit code 0. One test per level of `campaign.json` (read at configure time). |
| `game_smoke_level_out_of_range` | `game` | `neo_wolf --level N+1 --frames 10 --pref-dir …` | Exit code 1 and `the campaign has N level(s)`. |
| `game_preview_<screen>` (10 tests) | `game` | `neo_wolf --preview <screen> --frames 5 --screenshot <build>/test-shots/<screen>.bmp --pref-dir …` | Each preview screen renders and is saved as a BMP of the default render size (640×360, read from the BMP header by `RunGame.cmake`); exit code 0. |
| `game_test_level_boot` | `game` | `neo_wolf --frames 30 --pref-dir …` | Registered only while `campaign.json` sets `"bootIntoTestLevel": true`: the game starts in the test level (`Booting into the test level`, then `Starting the test level`), plays 30 frames and writes no `progress.json` in its per-user folder (`NO_PROGRESS`, `cmake/RunGame.cmake`'s `EXPECT_NO_FILE`). |
| `game_test_level_not_with_level` | `game` | `neo_wolf --level 1 --frames 30 --pref-dir …` | `--level 1` starts campaign level 1, and the log has neither `Booting into the test level` nor `Starting the test level` (`NO_REGEX`, `cmake/RunGame.cmake`'s `EXPECT_NO_REGEX`). |
| `game_test_level_not_with_preview` | `game` | `neo_wolf --preview hud --frames 5 --screenshot … --pref-dir …` | A preview shows its screen, not the test level (neither log line). |
| `game_render_size` | `game` | `neo_wolf --level 1 --frames 5 --render-size 320x240 --screenshot <build>/test-shots/render-size-320x240.bmp --pref-dir …` | The game logs `Render size: 320x240.` and saves a 320×240 BMP; exit code 0. |
| `game_render_size_invalid` | `game` | `neo_wolf --render-size 641x360 --frames 5 --pref-dir …` | A usage error: exit code 2 and the message naming the limits. |
| `game_smoke_missing_data` | `game` | `neo_wolf --data <build>/test-empty-data --frames 10 --pref-dir …` | Passes if the game exits with code 1 and reports `Game content not found` (fail-fast startup). |
| `game_smoke_invalid_gameplay` | `game` | `neo_wolf --data <build>/test-bad-gameplay --frames 10 --pref-dir …` | The content root's `gameplay.json` (written at configure time) has a string where an integer belongs. Passes if the game exits with code 1 and names the key (`weapon.damage must be an integer`). |
| `game_smoke_invalid_campaign` | `game` | `neo_wolf --data <build>/test-bad-campaign --frames 10 --pref-dir …` | The content root's `campaign.json` has no levels. Exit code 1 and `Invalid campaign manifest`. |
| `autoplay.level.1` … `.N` | `autoplay` | `neo_wolf_autoplay --level N --data <repo> --json <build>/autoplay/level-N.json --pref-dir …` | The [autoplay bot](#autoplay-bot) plays campaign level N from its start (initial health and ammo); passes when the level is `complete` (exit 0). One test per level of `campaign.json`, generated at configure time. |
| `autoplay.campaign` | `autoplay` | `neo_wolf_autoplay --all --data <repo> --json <build>/autoplay/campaign.json --pref-dir …` | The whole campaign from New Game with health and ammo carried over; passes when every level is complete, the carry-over check is OK and the progress file unlocks the last level. |
| `autoplay.fixture_unwinnable` | `autoplay` | `neo_wolf_autoplay --level 1 --data tests/fixtures/unwinnable …` | A level whose exit is walled off: passes when the bot exits with 1 and reports `stuck` with `no route to an exit switch`. The DevKit test `test_committed_unwinnable_fixture` checks that `validate` fails on the same level. |
| `autoplay.fixture_aim_blocked` | `autoplay` | `neo_wolf_autoplay --level 1 --data tests/fixtures/aim_blocked --time-limit 15 …` | Phase 8 regression: level 10's great hall, cropped, with an idle runner in an alcove whose centre the bot can see but whose shot grazes the corner. Passes when the bot completes it within 15 s of game time (it gives up on that enemy after 2 s; the old bot stood there for 33 s, and for good in a jittered level 10). |
| `autoplay.trace` | `autoplay` | `cmake/CheckAutoplayTrace.cmake`: `neo_wolf_autoplay --level 3 --data <repo> --trace <build>/autoplay/trace/trace-{1,2}.txt …` twice | The [event trace](#event-trace---trace): both runs complete, every line has the `L<level> t=… f=… <event>` prefix, the trace has a level start, an enemy noticing the player, a kill, a key, an unlocked door, the exit and the level end, and the two traces are byte-identical. |
| `autoplay.trace_unwritable`, `autoplay.trace_usage` | `autoplay` | `neo_wolf_autoplay --level 1 --trace <a path under a file>`; `… --trace` (no value) | A trace file that cannot be created: exit 1 with `cannot write the trace to`, before anything is played. `--trace` without a value: exit 2 with `--trace needs a value`. |
| `devkit_tests` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color test` | The DevKit's 155 Python unit tests (`tools/devkit/tests/test_*.py`): map tools, asset tools, definitions (CSV/JSON export), the validator (`test_validator.py`), **multi-level fixture campaigns** with one good campaign and one broken variant per rule (`test_campaign_rules.py`: campaign schema, `--require-levels`, orphans, players, doors/`D`, triggers/`T`, entities in walls, exits and key-before-lock, unreachable pickups and enemies, the `gameplay.json` schema, theme files, and the test level: `testLevel` / `bootIntoTestLevel` definitions, the level rules applied to it, `inspect-map --level testchamber`, trigger messages), **engine parity** (`test_parity.py`, below), the command line (`test_cli.py`: `create-map --level-id` for every template style, `--register` and the campaign position it reports (and its refusal of the test level's id), no overwrite without `--force`; `inspect-map` on every level of the shipped `campaign.json` (by number and by id) and the key order on a fixture level, the spawn in a wall, an unreachable exit and the exit codes; `doctor` for macOS, Linux and Windows with a simulated environment, the SDL2 cache/network cases and a real smoke run; root resolution from another directory; the launchers; colours), the texture and sprite generators (deterministic, committed files up to date, every theme's door unlike its walls), the level pages (`test_level_docs.py`: each `docs/levels/levelNN.md`, and the test level's `docs/levels/testchamber.md`, shows its shipped `.map` byte for byte and an annotated grid that matches its entity file) and, since Phase 9, the packager (`test_packager.py`: CMake simulated by a fake command runner; a package without the executable fails for every OS, also through the command line (known issue D1); test/autoplay executables, missing licences or docs, a wrong `Info.plist`, version mismatch, a release (`--expect-version`) while `bootIntoTestLevel` is true and failed build/install stop it; zip/tar layout and permissions, `SHA256SUMS` merging, `package_smoke.py --no-run` on the result and on a tampered archive; the package scripts), the icon generator (`test_gen_icon.py`), the package README (`test_release_readme.py`: every row of `CONTROLS.md`'s per-screen and window-key tables appears, per-OS Gatekeeper/SmartScreen and save paths, the licence summary) and, since Phase 10, the sound generator (`test_gen_sounds.py`), asset provenance (`test_asset_provenance.py`: every file under `game/assets` is reproduced byte for byte by a generator) and the Markdown link checker (`test_doc_links.py`). `fixture_builder.py` builds the fixture campaigns. |
| `content_validation` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color validate --strict` | Validates everything in `game/` with the engine's rules and the DevKit's level rules ([DEVKIT_GUIDE.md](DEVKIT_GUIDE.md#validate--content-and-campaign-validation)): maps, PPM/WAV assets, theme files, `sprites.cfg`, `audio.cfg`, `gameplay.json`, the entity files, `campaign.json`, and per level one player on floor, door/`D` and trigger/`T` pairing, nothing in walls, an exit reachable with keys before locks, every pickup and enemy reachable, no orphan files. Warnings count as failures. |
| `content_validation_campaign_size` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color validate --strict --require-levels 10` | The same validation, plus: `campaign.json` has exactly 10 levels (Roadmap Phase 8). The number is the CMake cache variable `NEOWOLF_REQUIRED_CAMPAIGN_LEVELS` (default 10). |

Opt-in **package tests** (Roadmap Phase 9; not registered by default because they build the
separate Release tree `build/package-<os>/`): configure with `-DNEOWOLF_PACKAGE_TESTS=ON`, then
`ctest --preset macos -L package`. `package.build` runs `devkit.py package --out-dir
<build>/package-test --clean`; `package.smoke` (needs it, CTest fixture) runs
`tools/ci/package_smoke.py <build>/package-test` ([RELEASE.md](RELEASE.md#smoke-test)). Label `package`.

Every game run gets its own `--pref-dir` folder under `<build>/test-pref/`, so the tests never
read or replace the player's real log or `progress.json` (known issue B15). The headless
native tests that start an engine (`progress_file_roundtrip`, `campaign_playthrough`,
`new_game_resets_world`) use temporary folders as well; the others set no per-user folder.

**Shipped levels versus fixtures (Phase 8).** The shipped levels were rebuilt in Phase 8 and
may change again, so the native tests do not depend on their details. Tests about the campaign as a whole read
`game/defs/campaign.json` and hold for any number of levels (`campaign_manifest_validation`,
`campaign_playthrough`, `theme_loading_and_swap`, `exit_switch_activation`,
`entity_placements_json_csv`, the CRLF test, `ui_layout_bounds`, `progress_file_roundtrip`).
Tests that need exact positions (level 1's door at (8, 7), level 2's blue key, level 3's brute)
use the Phase 7 levels frozen in `tests/fixtures/reference/game/` (a `campaign.json` and
`level01`–`level03` maps, entity JSON and CSV): file-level tests read them directly
(`ReferencePath`), engine tests copy the shipped `game/` into a temporary folder and overlay the
fixture on it (`MakeReferenceContentRoot`). Do not edit the fixture to follow the shipped levels.
The `enemies` and `items` previews no longer use level 1 either: they build a fixed stage (the
same hall) in code (`Engine::SetupShowcase`).

`<Python3_EXECUTABLE>` is the interpreter CMake found at configure time (`find_package(Python3)`).
The three `devkit` tests are only registered when CMake finds a Python 3 interpreter. If it does
not, configure prints a warning and the three tests are **not registered** at all: they do not
appear in `ctest -N` and are not reported as skipped, so CTest runs only the native, game and autoplay tests (115 instead of 118).

The native and DevKit tests run with the repository root as their working directory; the
`game_smoke_*` tests run in the build directory. The native tests also find
repository files through the `NW_SOURCE_DIR` compile definition (the absolute source path), so
`neo_wolf_tests` works from any directory. If that path no longer exists, they fall back to the
current directory and then its parent.

## Running a subset

With CTest (`-R` takes a regular expression, `-L` a label):

```bash
ctest --preset macos -R native.map_queries      # one test
ctest --preset macos -R 'native\.(map|raycast)' # several
ctest --preset macos -L native                  # all C++ tests
ctest --preset macos -L devkit                  # DevKit tests + content validation
ctest --preset macos -L game                    # the headless game runs
ctest --preset macos -L autoplay                # the autoplay bot: every level, the campaign, the unwinnable and aim-blocked fixtures, --trace
ctest --preset macos --rerun-failed             # only the tests that failed last time
ctest --preset macos -N                         # list tests without running them
```

The test presets already enable `--output-on-failure`.

With the test executable directly (useful in a debugger):

```bash
build/macos/neo_wolf_tests                    # run every test case
build/macos/neo_wolf_tests --list             # print the id of every test case
build/macos/neo_wolf_tests map_queries        # an exact id runs just that test
build/macos/neo_wolf_tests map raycast        # otherwise: every test whose id or description contains the text
build/macos/neo_wolf_tests --help
```

### Windows notes (verified in CI, not yet on a Windows PC)

- Use the `windows-msvc` preset for Release and `windows-msvc-debug` for Debug, e.g.
  `ctest --preset windows-msvc-debug -L native`. Both use the one `build\windows-msvc\` directory.
- In `cmd.exe`, quote regular expressions with double quotes, not single quotes:
  `ctest --preset windows-msvc -R "native\.(map|raycast)"`.
- The test executable is `build\windows-msvc\Release\neo_wolf_tests.exe`, or
  `build\windows-msvc\Debug\neo_wolf_tests.exe` after a Debug build.

Exit codes: `0` all selected tests passed; `1` at least one failed, or a filter matched nothing;
`2` bad command-line usage.

## How failures are reported

`TEST_ASSERT(expr)` and `TEST_ASSERT_NEAR(a, b, eps)` record the file, line and expression
(and, for `_NEAR`, both values), then stop **only the current test case**. The runner carries
on with the next test case and prints a summary at the end:

```text
[FAIL] map_queries
       /path/to/Neo-Wolf-v1/tests/LogicTests.cpp:169: TEST_ASSERT_NEAR(1.0f, 2.0f, 0.1f) failed: 1 vs 2

21 of 23 test case(s) passed; 2 failed:
  vec2_math_helpers: /path/to/Neo-Wolf-v1/tests/LogicTests.cpp:107: TEST_ASSERT(1 + 1 == 3) failed
  map_queries: /path/to/Neo-Wolf-v1/tests/LogicTests.cpp:169: TEST_ASSERT_NEAR(1.0f, 2.0f, 0.1f) failed: 1 vs 2
```

(Output from a deliberately broken copy of the tests; the line numbers are illustrative.) The
file name is the absolute path of the source file as the compiler saw it (`__FILE__`), so it
shows your checkout's location. Any other exception that escapes a test
case is also reported as a failure of that test case.

## Adding a native test

1. In `tests/LogicTests.cpp`, write a `void TestSomething()` function inside the anonymous
   namespace. Use `TEST_ASSERT` / `TEST_ASSERT_NEAR`, and `ResolveRepoPath("game/...")` for any
   repository file.
2. Add it to the `kTests` table at the bottom of the file with a short snake_case id and a
   description: `{"something", "Something works", &TestSomething},`.
3. Add the same id to `NEOWOLF_NATIVE_TESTS` in `CMakeLists.txt`.
4. Re-run `cmake --build --preset macos && ctest --preset macos`.

If you forget step 3, `native_test_registry` fails and names the missing id.

## Adding a DevKit test

Add a `test_*.py` file (standard `unittest`) under `tools/devkit/tests/`. It is discovered
automatically. Run the DevKit tests alone with:

```bash
python3 tools/devkit/devkit.py test
```

## Screen previews

Developer tools for looking at every screen without playing (`neo_wolf --help`, "Developer
tools"):

- `--preview <screen>` sets up one screen with fixed sample data and freezes the simulation:
  `menu`, `levelselect`, `hud`, `minimap`, `pause`, `levelcomplete`, `gameover`,
  `campaigncomplete`, and (Phase 6) `enemies` (a guard, a runner, a brute and a guard's corpse
  in a fixed showcase hall built in code (`Engine::SetupShowcase`), in front of its door, locked
  blue for the preview) and `items` (health,
  ammo and the three key cards in a row, all three keys held in the HUD, minimap on, the door
  locked red). The sample progress has level 2 unlocked and level 3 locked; nothing is saved.
  `Esc` (or closing the window) quits.
- `--screenshot <file.bmp>` saves the last rendered framebuffer (at the render size, 640×360
  unless `--render-size` says otherwise) when the game exits; combine it with `--frames N`.

```bash
mkdir -p /tmp/shots
for s in menu levelselect hud minimap pause levelcomplete gameover campaigncomplete enemies items; do
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/macos/neo_wolf --pref-dir /tmp/shots/prefs \
    --preview "$s" --frames 5 --screenshot "/tmp/shots/$s.bmp"
  sips -s format png "/tmp/shots/$s.bmp" --out "/tmp/shots/$s.png"   # macOS; any BMP viewer works
done
```

The reference images in `docs/images/screens/` were made this way, at the default 640×360
(shown in [GAMEPLAY.md](GAMEPLAY.md#screens); up to 1.0.0 they were 320×240). Test
`ui_layout_bounds` checks the same screens automatically at several render sizes: it records every rectangle the UI draws (`Framebuffer::GetDrawBounds`) and fails if
anything lies within 4 UI pixels (4 × the UI scale in framebuffer pixels) of an edge or was
clipped.

## Driving the engine from code (input injection)

`Engine::SetInputInjector` installs a callback that runs once per `Engine::Update`, after the
platform's events were read and before anything uses them. It sets actions with
`InputState::SetActionState`; edges (`WasPressed`) follow from the previous frame, exactly as
for keyboard input. Together with `EngineOptions::headless` (no window, renderer or audio) this
lets code play the game: the autoplay bot below does exactly that, and `campaign_playthrough`
uses it to play the whole campaign, fetching key cards before their locked doors and fighting
every enemy type the levels contain.

## Autoplay bot

`neo_wolf_autoplay` (Roadmap Phase 7) proves that levels can be finished: a bot plays the **real
engine** headless (`EngineOptions::headless`), `Engine::Update` at a fixed step of 1/60 s, using
**only simulated input** through `Engine::SetInputInjector`, the same actions a keyboard sets. It
never moves the player, opens a door or changes game state itself; it only reads the state to
decide which keys to hold. The bot is `engine/autoplay/AutoplayBot.{h,cpp}` (static library
`neowolf_autoplay`); the command-line front end is `src/autoplay_main.cpp`.

```bash
build/macos/neo_wolf_autoplay --level 2                       # one level, initial health/ammo
build/macos/neo_wolf_autoplay --all --json /tmp/report.json   # the campaign with carry-over
build/macos/neo_wolf_autoplay --all --data tests/fixtures/unwinnable   # a level that must fail
```

| Option | Meaning |
|---|---|
| `--level N` | Play campaign level N (1-based) once, from its start with the `gameplay.json` initial health and ammo. |
| `--all` | Play the whole campaign from the main menu (New Game) in order, health and ammo carried over like a player; stop at the first level that is not completed. |
| `--data DIR` | Content root (default: the game's automatic search from the executable's folder). |
| `--pref-dir DIR` | Folder for `progress.json` (default: a temporary folder, removed afterwards). |
| `--time-limit SECONDS` | Game time allowed per level (default 300). |
| `--json FILE` | Also write the machine-readable report. |
| `--trace FILE` | Also write an [event trace](#event-trace---trace): one line per event (hits, kills, enemy states, pickups, keys, doors, triggers, the exit), for balancing. |
| `--verbose` | Print the engine's log (default: only warnings and errors). |
| `--help`, `--version` | |

Exactly one of `--level` and `--all` is required. Exit codes: **0** every requested level was
completed (and, with `--all`, the carry-over check passed); **1** otherwise, or the game could not
start, or the JSON or the trace could not be written (a trace file that cannot be created stops
the tool before it plays); **2** usage error.

**What it does, every frame** (re-planning whenever a key, door or pickup changes, health or ammo
crosses a threshold, or no progress is made):

1. A living enemy in line of sight (the engine's `HasLineOfSight`, the same test the enemies use)
   and within weapon range: stop, turn to it and fire when the shot will hit (aim within 0.32
   cells of its centre, no wall in front), until it is dead. Out of ammo, it does not fight.
2. Health at or below 40 % of the maximum: walk to the nearest reachable health pickup, if any;
   ammo at or below 6: the same for ammo.
3. Otherwise walk to the nearest reachable floor cell beside an exit switch, face the switch and
   press Interact.
4. If no exit switch can be reached yet: walk to the nearest reachable key card that opens a
   locked door of the level and is not held yet, then plan again. This collects keys in
   dependency order.
5. Nothing reachable: give up at once with the reason (result `stuck`, e.g. `no route to an exit
   switch: ...; locked without a reachable key: red door at (3, 1)`).

Paths are breadth-first searches over cells (4-connected); closed doors are passable when they
can be opened. The bot walks a path by turning and moving forward, cutting corners where a body
fits (`HasClearCorridor`), walks up to a closed door and presses Interact. Without progress for
8 s (getting closer to the goal, a hit, a pickup, a key, a door) it backs off, sidesteps and
plans again; after 3 such attempts it gives up (`stuck`, with where it stopped). Each level has a
time limit of game time (`timeout`). Health 0 is `died`.

**Report**, per level: result (`complete`, `died`, `timeout`, `stuck`, or `not-played` after an
earlier failure with `--all`) and reason, game time and frames, damage taken, health and ammo at
the start and at the exit, kills/total, pickups/total, keys collected and keys used (locked doors
opened), shots fired, doors opened and re-plans. Damage and shots come from the engine
(`LevelResults::damageTaken`, `shotsFired`). With `--all` the tool also checks, independently of
the game flow, that level 1 started with the initial values and every later level with the
previous level's exit health and ammo (clamped to the maxima, health at least 1), and that the
saved progress unlocks the last level. stdout gets a table, `--json` the same as JSON.

**Deterministic.** A run depends only on the content and the build: a fixed time step, no wall
clock, no randomness in the simulation (the engine's `std::rand` only places dust particles). Two
runs give byte-identical JSON (checked by `autoplay_fixture_level` and in the Phase 7 report).
Different compilers or CPUs may round differently, so frame counts can differ between platforms.

**Limits** (it is a straightforward player, not a skilled one):

- It does not dodge, strafe in fights or retreat; it fights where it stands. Several enemies at
  once, or too little health or ammo for the enemies on its way, can kill it where a person
  would back off. Failing such a level is deliberate signal, not a bot bug.
- It ignores enemies it cannot see; it does not plan around living enemies (they block the
  player since C15), it shoots them when they are in the way.
- It fires only when the shot can reach the enemy (walls stop shots). If an enemy's centre is
  in sight but a ready shot stays blocked by a corner for 2 s, the bot ignores that enemy for
  3 s and walks on (Phase 8; before, an idle enemy out of its own notice range could hold the
  bot there until the time limit: CTest `autoplay.fixture_aim_blocked`).
- It only fetches pickups when it is low on health or ammo, and keys only when they open a door
  of the level; the report's pickup count is not a "100 %" run.
- Pressing Interact toggles **every** door within reach (a game rule), so two doors next to the
  same cell can close each other; the no-progress check catches a resulting loop.
- It does not use triggers, the minimap, pausing or restarting; with `--level` it skips the menu.
- Exits are used from the four orthogonal neighbours of an `X` (as the validator assumes).

### Event trace (`--trace`)

`--trace FILE` writes what happened during the run, one plain-text line per event, for balancing
and for debugging a level the bot fails. It works with `--level N` and `--all` (every level's
lines, in play order). The events are found by comparing the engine's state before and after
each frame (enemy states and health, the player's health and the engine's damage counter, keys,
doors, pickups, triggers); nothing is logged per frame, so a whole `--all` run is about 700
lines (~45 KB).

```text
# neo_wolf_autoplay trace 1: one event per line, "L<level> t=<game seconds> f=<frame> <event> key=value..." (docs/TESTING.md, Autoplay bot)
L3 t=0.000 f=0 level-start id=level03 hp=100 ammo=36 pos=1.50,13.50 enemies=6 pickups=5
L3 t=7.933 f=476 key colour=red cell=11,5 held=red
L3 t=18.967 f=1138 enemy guard#6 cell=21,5 alert->chase
L3 t=20.083 f=1205 hit-dealt target=guard#6 cell=20,4 damage=35 hp=15/50
L3 t=20.083 f=1205 enemy guard#6 cell=20,4 chase->attack
L3 t=20.083 f=1205 hit-taken by=guard#6 from=20,4 damage=12 hp=88 pos=19.32,4.62
L3 t=20.367 f=1222 hit-dealt target=guard#6 cell=20,4 damage=15 hp=0/50 killed
L3 t=20.367 f=1222 enemy guard#6 cell=20,4 attack->dead
L3 t=22.117 f=1327 door-open cell=20,2 lock=red unlocked
L3 t=22.483 f=1349 trigger id=4 cell=21,2 pos=20.74,2.56
L3 t=29.233 f=1754 pickup kind=health value=25 cell=23,12 hp=100 ammo=36
L3 t=30.317 f=1819 exit cell=22,16 pos=22.52,15.29 hp=100 ammo=36
L3 t=30.317 f=1819 level-end result=complete hp=100 ammo=36 damage=12 kills=6/6 pickups=3/5 shots=12
```

(level 3 of `--all`, shortened). Every line starts with `L<campaign level number>`, `t=` the
level's game time in seconds (3 decimals, `Engine::levelTimeSeconds_`) and `f=` the frame
number within the level (the report's `frames`), then the event name and `key=value` fields
without spaces (except a `level-end` `reason="…"`); `#` lines are comments. Cells are `x,y` map
cells, `pos=` a position in cells (2 decimals); an enemy is `<variant>#<entity id>`.

| Event | Fields |
|---|---|
| `level-start` | `id` (campaign id), `hp`, `ammo`, `pos` (spawn), `enemies`, `pickups` (totals) |
| `enemy` | an enemy's AI state changed: `cell`, `<old>-><new>` (`idle`, `alert`, `chase`, `attack`, `search`, `dead`) |
| `hit-dealt` | the player's shot hit: `target`, `cell`, `damage` (health removed), `hp=<left>/<max>`, `killed` |
| `hit-taken` | an enemy hit the player: `by`, `from` (its cell), `damage`, `hp` (the player's, after the hit), `pos` (the player's) |
| `pickup` | `kind` (`health`, `ammo`), `value`, `cell`, the player's `hp` and `ammo` after it |
| `key` | a key card was picked up: `colour`, `cell`, `held` (all keys held) |
| `door-open`, `door-close` | `cell`, `lock` (`none`, `red`, `blue`, `gold`); `unlocked` the first time a locked door opens |
| `trigger` | a trigger was walked over: `id`, `cell`, `pos` |
| `exit` | the exit switch used: `cell` (the `X`), `pos`, `hp`, `ammo` |
| `level-end` | `result` (as in the report), `hp`, `ammo`, `damage`, `kills`, `pickups`, `shots`, and `reason` when not complete |

Within one frame the lines follow the engine's order: the player's shot, pickups and keys,
triggers, doors, the exit, then the enemy AI (state changes, then hits on the player), entities
in entity order. An enemy's state is compared once per frame, so a change and its reverse within
the same frame (rare) are not listed, and `idle->chase` means it skipped `alert` (no reaction
time). A hit on the player is attributed to the enemy that attacked in that frame (in `attack`,
its attack cooldown just reset); the `hit-taken` damage of a level adds up to the report's
damage (an unattributable remainder would be listed as `by=unknown`).

**Deterministic:** no wall-clock time, no addresses, a fixed order: two runs of the same build on
the same content give byte-identical traces (checked by the CTest test `autoplay.trace`). The
trace only observes; the bot plays exactly as without `--trace`.

## Engine/DevKit parity fixtures

`tests/fixtures/parity/` holds small maps and entity files with tricky bytes: CRLF and mixed
line endings, blank CRLF lines, whitespace-only lines, indented `;` comments, trailing spaces,
tabs, byte order marks, a lone CR, a form feed, a no-break space, `1_5` and exponent numbers,
`NaN`, a repeated key, a JSON comment, a float where an integer belongs, trigger messages with
a comma, a tab, a space at an end, too many bytes or a byte that is not UTF-8. `expected.json` records
whether the engine accepts each file (and the grid size or entity types). The native test
`content_parity_fixtures` loads them with the engine's loaders, the DevKit test `test_parity.py`
with the DevKit's readers; both must match `expected.json`. `.gitattributes` keeps the files
byte for byte (`-text`).

## Warnings as errors

The `ci-macos`, `ci-linux` and `ci-windows-msvc` presets set `NEOWOLF_WERROR=ON`, which turns
compiler warnings into errors for Neo-Wolf's own code (`-Werror` or `/WX`). SDL is never affected.
Run the same check locally before pushing:

```bash
cmake --preset ci-macos && cmake --build --preset ci-macos && ctest --preset ci-macos
```

## Continuous integration

**Status: green.** Run 43866272111 passed all six jobs below: macOS,
Windows MSVC x64, Linux, `windows-scripts`, Package (macOS universal) and Package (Windows x64).
On Windows 110/110 CTest tests pass, and the autoplay bot completes all 10 levels with results
identical to macOS. Run 43864756039 was the first all-green run with the package
jobs. Earlier: run 43861234139, run 43820751721. CI runs everything headless with SDL's dummy drivers:
it proves that the game builds, passes its tests and starts on each OS, not how it plays on a
desktop. (On the first run, 43817862544, the Windows preset job failed on MSVC C4459 and the
macOS job did not start on the retired `macos-14` image; both were fixed in Phase 3.)

`.github/workflows/ci.yml` runs on pushes to `main` and `completion/**`, on `v*` tags, on pull
requests and on demand. It has one build-and-test job per OS:

| Job | Runner | Preset | Smoke-run executable |
|---|---|---|---|
| macOS (universal, clang) | `macos-latest` | `ci-macos` | `build/ci-macos/neo_wolf` |
| Windows (MSVC x64) | `windows-latest` | `ci-windows-msvc` | `build/ci-windows-msvc/Release/neo_wolf.exe` |
| Linux (gcc) | `ubuntu-latest` | `ci-linux` | `build/ci-linux/neo_wolf` |

Each job runs:

```bash
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset>                              # every test layer above, warnings as errors
python tools/ci/smoke_run.py <executable> --frames 300  # headless smoke run, must exit 0
```

`tools/ci/smoke_run.py` starts the game from a fresh temporary directory (so content must be
found relative to the executable) with SDL's dummy video and audio drivers, and with
`--pref-dir` pointing at another temporary folder (unless you pass one), so the runner's real log
and saved progress are never touched. It removes
`SDL_RENDER_DRIVER` from the environment: the game has to fall back to the software renderer by
itself (known issue B11, fixed). With `--frames N` the game renders N frames and exits; the run
passes if the exit code is 0 within `--timeout` seconds (default 60). 300 frames take about 5
seconds at the 60 FPS cap. Without `--frames`, the older mode applies: the game must still be
running after `--seconds` (default 5) and is then stopped. Run it locally the same way, e.g.
`python3 tools/ci/smoke_run.py build/macos/neo_wolf --frames 300`. Unrecognised options are
passed to the game; `--cwd DIR` overrides the working directory.

A separate job, `windows-scripts`, runs the user-facing Windows entry points that the preset
job bypasses: `build.cmd` from Command Prompt (Release) and `.\build.ps1 -Config Debug` from
Windows PowerShell 5.1. Each script configures, builds and runs the full test suite. Since
Phase 9 it also checks that `package.cmd` and `package.ps1` pass the DevKit's exit codes on
(usage errors, exit code 2), without a second full build.

Since Phase 9 the job `package` (macOS and Windows; green since run 43864756039) builds the release package through `./package.sh` / `package.cmd` (on a `v*` tag with
`--expect-version` set from the tag), runs `python tools/ci/package_smoke.py dist` and uploads
the archive and `SHA256SUMS` as the artifact `neo-wolf-package-<OS>` ([RELEASE.md](RELEASE.md)).

Since Phase 7 each job also runs the autoplay bot on the whole campaign and uploads its report:

```bash
<build dir>/neo_wolf_autoplay --all --json autoplay-report-<preset>.json   # shell: bash on every OS
```

The step runs even when the tests failed (as long as the build succeeded), and the report is
uploaded as the artifact `autoplay-report-<preset>` (macOS, Windows MSVC, Linux); a level the bot
cannot finish fails the job. On failure the job uploads `LastTest.log`, `CMakeConfigureLog.yaml`
and the CTest autoplay reports (`build/<preset>/autoplay/*.json`) as `logs-<preset>`.
