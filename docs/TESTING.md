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
| `native.<id>` (64 tests) | `native` | `neo_wolf_tests <id>` | C++ engine logic: math, input, map queries, raycasting, texture/asset loading, entities, collision, interaction rules, game state, combat, AI, pickups, command bridge, logging and log-file rotation, level reset, a scripted vertical-slice run, map reachability, the C interop API, content-root resolution (with temporary directories, including a non-ASCII path), command-line parsing, CRLF content files, and the data pipeline: exact `gameplay.json` values and its validation errors, JSON/CSV entity placements and their error messages, decorations and sprite sync, pickup kind from data, failing level loads, and `edited_data_changes_game`. Phase 5 adds: `game_flow_menu`, `game_flow_progression`, `game_flow_death_pause` (every `GameFlow` transition, the death snapshot, carry-over clamping, Continue), `campaign_manifest_validation`, `exit_switch_activation` (in reach and facing activates; behind, sideways, too far or behind a wall does not), `progress_file_roundtrip` (round trip, atomic save, 11 corrupt files, the engine ignoring a corrupt save), `pref_dir_resolution`, `theme_loading_and_swap`, `ui_layout_bounds` (every screen stays inside 320×240 with a 4-pixel margin and nothing is clipped), `campaign_playthrough` (since Phase 7 a thin test on the autoplay bot: `RunAutoplaySession` plays from the main menu through every level of `campaign.json` → CampaignComplete with carry-over; since Phase 8 it is data-driven: the test checks the engine state, each level's id and enemy count against its entity file, that every locked door the bot opened has that lock and was opened with a key collected in the level, and that the saved progress unlocks the last level, then uses Continue), `new_game_resets_world` (C7), `key_bindings` (B13, minimap toggle) and `wall_texture_orientation` (wall textures are no longer mirrored). Phase 6 adds: `line_of_sight` (walls and closed doors block, open doors do not), `enemy_ai_behavior` (rewritten: Idle → Alert → Chase → Attack with the reaction time and attack rate, no waking through walls, Chase → Search → Idle when the player is lost), `enemy_hearing` (shots heard through open cells only, path length counts; in the engine a shot wakes the reference level 3's brute but not the guard behind a closed door), `enemy_pathfinding` (round a wall, no corner cutting, closed doors stop enemies), `enemy_archetypes_from_data`, `entity_variant_validation`, `locked_doors_and_keys`, `door_close_blocked`, `corpse_and_hit_flash`, `enemy_separation`, `hud_key_icons_layout`, and `ai_stress_20_enemies` (20 enemies chasing in a 48×48 map for 600 frames of `Update` + `Render`: no enemy ends up in a wall; it prints the average and worst time per frame and fails if the average exceeds 8 ms in an optimised build, 100 ms in a Debug build). Phase 7 adds: `autoplay_fixture_level` (the bot on a generated level with a health detour, an unlocked and a blue locked door, the blue key and a guard: complete, the report's numbers, the same JSON on a second run, `timeout` with a 1 s limit), `autoplay_unwinnable_levels` (`stuck` with a reason for the walled-off fixture and for a red door without a key, `--all` stops and marks later levels `not-played`, a content root that does not start), `autoplay_command_line` (options, usage errors, the JSON fields), `player_enemy_collision` (C15: living enemies block the player, an overlapping player can back away, corpses do not block, enemies are pushed out of the player; in the engine, walking at level 1's guard), `sprite_vertical_crop` (a sprite taller than the screen is cropped, not squeezed) and `content_parity_fixtures` (the engine's loaders against `tests/fixtures/parity/expected.json`, see below). Source: `tests/LogicTests.cpp`. |
| `native_test_registry` | `native` | `cmake -P cmake/CheckTestRegistry.cmake` | Checks that every test case compiled into `neo_wolf_tests` has a CTest entry and the reverse. Catches a test that was added but never registered. |
| `game_smoke_frames` | `game` | `cmake -P cmake/RunGame.cmake` → `neo_wolf --frames 120 --pref-dir <build>/test-pref/<test>` | Starts the real game headless (`SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`, set with the `ENVIRONMENT_MODIFICATION` test property, which also **removes** `SDL_RENDER_DRIVER`, `NEOWOLF_DATA_DIR` and `NEOWOLF_PREF_DIR`). Runs from the build directory, so content must be found relative to the executable. Passes if the game exits with 0 and logs the `--pref-dir` folder, its content root, the campaign with its level count, its renderer, level 1's entity count (both read from `campaign.json` and level 1's entity file at configure time) and 120 frames. Proves the software-renderer fallback (B11) and the content-root search. Takes about 2 s (60 FPS cap). |
| `game_smoke_level_1` … `_N` | `game` | `neo_wolf --level N --frames 120 --pref-dir …` | Each campaign level starts directly, loads its theme and runs 120 frames; exit code 0. One test per level of `campaign.json` (read at configure time). |
| `game_smoke_level_out_of_range` | `game` | `neo_wolf --level N+1 --frames 10 --pref-dir …` | Exit code 1 and `the campaign has N level(s)`. |
| `game_preview_<screen>` (10 tests) | `game` | `neo_wolf --preview <screen> --frames 5 --screenshot <build>/test-shots/<screen>.bmp --pref-dir …` | Each preview screen renders and is saved as a BMP; exit code 0. |
| `game_smoke_missing_data` | `game` | `neo_wolf --data <build>/test-empty-data --frames 10 --pref-dir …` | Passes if the game exits with code 1 and reports `Game content not found` (fail-fast startup). |
| `game_smoke_invalid_gameplay` | `game` | `neo_wolf --data <build>/test-bad-gameplay --frames 10 --pref-dir …` | The content root's `gameplay.json` (written at configure time) has a string where an integer belongs. Passes if the game exits with code 1 and names the key (`weapon.damage must be an integer`). |
| `game_smoke_invalid_campaign` | `game` | `neo_wolf --data <build>/test-bad-campaign --frames 10 --pref-dir …` | The content root's `campaign.json` has no levels. Exit code 1 and `Invalid campaign manifest`. |
| `autoplay.level.1` … `.N` | `autoplay` | `neo_wolf_autoplay --level N --data <repo> --json <build>/autoplay/level-N.json --pref-dir …` | The [autoplay bot](#autoplay-bot) plays campaign level N from its start (initial health and ammo); passes when the level is `complete` (exit 0). One test per level of `campaign.json`, generated at configure time. |
| `autoplay.campaign` | `autoplay` | `neo_wolf_autoplay --all --data <repo> --json <build>/autoplay/campaign.json --pref-dir …` | The whole campaign from New Game with health and ammo carried over; passes when every level is complete, the carry-over check is OK and the progress file unlocks the last level. |
| `autoplay.fixture_unwinnable` | `autoplay` | `neo_wolf_autoplay --level 1 --data tests/fixtures/unwinnable …` | A level whose exit is walled off: passes when the bot exits with 1 and reports `stuck` with `no route to an exit switch`. The DevKit test `test_committed_unwinnable_fixture` checks that `validate` fails on the same level. |
| `devkit_tests` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color test` | The DevKit's 89 Python unit tests (`tools/devkit/tests/test_*.py`): map tools, asset tools, definitions (CSV/JSON export), the validator (`test_validator.py`), **multi-level fixture campaigns** with one good campaign and one broken variant per rule (`test_campaign_rules.py`: campaign schema, `--require-levels`, orphans, players, doors/`D`, triggers/`T`, entities in walls, exits and key-before-lock, unreachable pickups and enemies, the `gameplay.json` schema, theme files), **engine parity** (`test_parity.py`, below), the command line (`test_cli.py`: `create-map --level-id` for every template style, `--register`, no overwrite without `--force`; `inspect-map` on every level of the shipped `campaign.json` (by number and by id) and the key order on a fixture level, the spawn in a wall, an unreachable exit and the exit codes; `doctor` for macOS, Linux and Windows with a simulated environment, the SDL2 cache/network cases and a real smoke run; root resolution from another directory; the launchers; colours), the texture and sprite generators (deterministic, committed files up to date) and the packager. `fixture_builder.py` builds the fixture campaigns. |
| `content_validation` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color validate --strict` | Validates everything in `game/` with the engine's rules and the DevKit's level rules ([DEVKIT_GUIDE.md](DEVKIT_GUIDE.md#validate--content-and-campaign-validation)): maps, PPM/WAV assets, theme files, `sprites.cfg`, `audio.cfg`, `gameplay.json`, the entity files, `campaign.json`, and per level one player on floor, door/`D` and trigger/`T` pairing, nothing in walls, an exit reachable with keys before locks, every pickup and enemy reachable, no orphan files. Warnings count as failures. |
| `content_validation_campaign_size` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color validate --strict --require-levels 10` | The same validation, plus: `campaign.json` has exactly 10 levels (Roadmap Phase 8). The number is the CMake cache variable `NEOWOLF_REQUIRED_CAMPAIGN_LEVELS` (default 10). |

Every game run gets its own `--pref-dir` folder under `<build>/test-pref/`, so the tests never
read or replace the player's real log or `progress.json` (known issue B15). The headless
native tests that start an engine (`progress_file_roundtrip`, `campaign_playthrough`,
`new_game_resets_world`) use temporary folders as well; the others set no per-user folder.

**Shipped levels versus fixtures (Phase 8).** The shipped levels change during Phase 8, so the
native tests do not depend on their details. Tests about the campaign as a whole read
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
appear in `ctest -N` and are not reported as skipped, so CTest runs only the native tests.

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
ctest --preset macos -L autoplay                # the autoplay bot: every level, the campaign, the unwinnable fixture
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
  in level 1's hall in front of its door, locked blue for the preview) and `items` (health,
  ammo and the three key cards in a row, all three keys held in the HUD, minimap on, the door
  locked red). The sample progress has level 2 unlocked and level 3 locked; nothing is saved.
  `Esc` (or closing the window) quits.
- `--screenshot <file.bmp>` saves the last rendered 320×240 framebuffer when the game exits;
  combine it with `--frames N`.

```bash
mkdir -p /tmp/shots
for s in menu levelselect hud minimap pause levelcomplete gameover campaigncomplete enemies items; do
  SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy build/macos/neo_wolf --pref-dir /tmp/shots/prefs \
    --preview "$s" --frames 5 --screenshot "/tmp/shots/$s.bmp"
  sips -s format png "/tmp/shots/$s.bmp" --out "/tmp/shots/$s.png"   # macOS; any BMP viewer works
done
```

The reference images in `docs/images/screens/` were made this way (shown in
[GAMEPLAY.md](GAMEPLAY.md#screens)). Test `ui_layout_bounds` checks the same screens
automatically: it records every rectangle the UI draws (`Framebuffer::GetDrawBounds`) and fails
if anything lies within 4 pixels of an edge or was clipped.

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
| `--verbose` | Print the engine's log (default: only warnings and errors). |
| `--help`, `--version` | |

Exactly one of `--level` and `--all` is required. Exit codes: **0** every requested level was
completed (and, with `--all`, the carry-over check passed); **1** otherwise, or the game could not
start, or the JSON could not be written; **2** usage error.

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
- It only fetches pickups when it is low on health or ammo, and keys only when they open a door
  of the level; the report's pickup count is not a "100 %" run.
- Pressing Interact toggles **every** door within reach (a game rule), so two doors next to the
  same cell can close each other; the no-progress check catches a resulting loop.
- It does not use triggers, the minimap, pausing or restarting; with `--level` it skips the menu.
- Exits are used from the four orthogonal neighbours of an `X` (as the validator assumes).

## Engine/DevKit parity fixtures

`tests/fixtures/parity/` holds small maps and entity files with tricky bytes: CRLF and mixed
line endings, blank CRLF lines, whitespace-only lines, indented `;` comments, trailing spaces,
tabs, byte order marks, a lone CR, a form feed, a no-break space, `1_5` and exponent numbers,
`NaN`, a repeated key, a JSON comment, a float where an integer belongs. `expected.json` records
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

**Status: green.** Run 43820751721 on commit `65c26b0` passed all four jobs below (macOS,
Windows MSVC, Linux, `windows-scripts`). CI runs everything headless with SDL's dummy drivers:
it proves that the game builds, passes its tests and starts on each OS, not how it plays on a
desktop. (On the first run, at `1b10a57`, the Windows preset job failed on MSVC C4459 and the
macOS job did not start on the retired `macos-14` image; both were fixed in Phase 3.)

`.github/workflows/ci.yml` runs on pushes to `main` and `completion/**`, on pull requests and on
demand. It has one job per OS:

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
Windows PowerShell 5.1. Each script configures, builds and runs the full test suite.

Since Phase 7 each job also runs the autoplay bot on the whole campaign and uploads its report:

```bash
<build dir>/neo_wolf_autoplay --all --json autoplay-report-<preset>.json   # shell: bash on every OS
```

The step runs even when the tests failed (as long as the build succeeded), and the report is
uploaded as the artifact `autoplay-report-<preset>` (macOS, Windows MSVC, Linux); a level the bot
cannot finish fails the job. On failure the job uploads `LastTest.log`, `CMakeConfigureLog.yaml`
and the CTest autoplay reports (`build/<preset>/autoplay/*.json`) as `logs-<preset>`.
