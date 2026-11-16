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
| `native.<id>` (58 tests) | `native` | `neo_wolf_tests <id>` | C++ engine logic: math, input, map queries, raycasting, texture/asset loading, entities, collision, interaction rules, game state, combat, AI, pickups, command bridge, logging and log-file rotation, level reset, a scripted vertical-slice run, map reachability, the C interop API, content-root resolution (with temporary directories, including a non-ASCII path), command-line parsing, CRLF content files, and the data pipeline: exact `gameplay.json` values and its validation errors, JSON/CSV entity placements and their error messages, decorations and sprite sync, pickup kind from data, failing level loads, and `edited_data_changes_game`. Phase 5 adds: `game_flow_menu`, `game_flow_progression`, `game_flow_death_pause` (every `GameFlow` transition, the death snapshot, carry-over clamping, Continue), `campaign_manifest_validation`, `exit_switch_activation` (in reach and facing activates; behind, sideways, too far or behind a wall does not), `progress_file_roundtrip` (round trip, atomic save, 11 corrupt files, the engine ignoring a corrupt save), `pref_dir_resolution`, `theme_loading_and_swap`, `ui_layout_bounds` (every screen stays inside 320×240 with a 4-pixel margin and nothing is clipped), `campaign_playthrough` (a waypoint bot drives `Engine::Update` through injected input from the main menu through level01 → level02 → level03 → CampaignComplete, then Continue), `new_game_resets_world` (C7), `key_bindings` (B13, minimap toggle) and `wall_texture_orientation` (wall textures are no longer mirrored). Phase 6 adds: `line_of_sight` (walls and closed doors block, open doors do not), `enemy_ai_behavior` (rewritten: Idle → Alert → Chase → Attack with the reaction time and attack rate, no waking through walls, Chase → Search → Idle when the player is lost), `enemy_hearing` (shots heard through open cells only, path length counts; in the engine a shot wakes level 3's brute but not the guard behind a closed door), `enemy_pathfinding` (round a wall, no corner cutting, closed doors stop enemies), `enemy_archetypes_from_data`, `entity_variant_validation`, `locked_doors_and_keys`, `door_close_blocked`, `corpse_and_hit_flash`, `enemy_separation`, `hud_key_icons_layout`, and `ai_stress_20_enemies` (20 enemies chasing in a 48×48 map for 600 frames of `Update` + `Render`: no enemy ends up in a wall; it prints the average and worst time per frame and fails if the average exceeds 8 ms in an optimised build, 100 ms in a Debug build). Source: `tests/LogicTests.cpp`. |
| `native_test_registry` | `native` | `cmake -P cmake/CheckTestRegistry.cmake` | Checks that every test case compiled into `neo_wolf_tests` has a CTest entry and the reverse. Catches a test that was added but never registered. |
| `game_smoke_frames` | `game` | `cmake -P cmake/RunGame.cmake` → `neo_wolf --frames 120 --pref-dir <build>/test-pref/<test>` | Starts the real game headless (`SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`, set with the `ENVIRONMENT_MODIFICATION` test property, which also **removes** `SDL_RENDER_DRIVER`, `NEOWOLF_DATA_DIR` and `NEOWOLF_PREF_DIR`). Runs from the build directory, so content must be found relative to the executable. Passes if the game exits with 0 and logs the `--pref-dir` folder, its content root, the campaign, its renderer and 120 frames. Proves the software-renderer fallback (B11) and the content-root search. Takes about 2 s (60 FPS cap). |
| `game_smoke_level_1` … `_3` | `game` | `neo_wolf --level N --frames 120 --pref-dir …` | Each campaign level starts directly, loads its theme and runs 120 frames; exit code 0. |
| `game_smoke_level_out_of_range` | `game` | `neo_wolf --level 4 --frames 10 --pref-dir …` | Exit code 1 and `the campaign has 3 level(s)`. |
| `game_preview_<screen>` (10 tests) | `game` | `neo_wolf --preview <screen> --frames 5 --screenshot <build>/test-shots/<screen>.bmp --pref-dir …` | Each preview screen renders and is saved as a BMP; exit code 0. |
| `game_smoke_missing_data` | `game` | `neo_wolf --data <build>/test-empty-data --frames 10 --pref-dir …` | Passes if the game exits with code 1 and reports `Game content not found` (fail-fast startup). |
| `game_smoke_invalid_gameplay` | `game` | `neo_wolf --data <build>/test-bad-gameplay --frames 10 --pref-dir …` | The content root's `gameplay.json` (written at configure time) has a string where an integer belongs. Passes if the game exits with code 1 and names the key (`weapon.damage must be an integer`). |
| `game_smoke_invalid_campaign` | `game` | `neo_wolf --data <build>/test-bad-campaign --frames 10 --pref-dir …` | The content root's `campaign.json` has no levels. Exit code 1 and `Invalid campaign manifest`. |
| `devkit_tests` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color test` | The DevKit's 39 Python unit tests (`tools/devkit/tests/test_*.py`): map tools (including the `X` exit tile in templates), asset tools, definitions (CSV/JSON export), validator (including campaign exit reachability, enemy variants, key and lock colours, and key-before-lock reachability), the theme texture generator (deterministic, committed files up to date, locked doors), the sprite generator (deterministic, magenta background, distinct enemy silhouettes, committed files up to date), packager. |
| `content_validation` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color validate --strict` | Validates the maps, PPM/WAV assets (including `game/assets/themes/`), the theme files and `sprites.cfg`/`audio.cfg`, the entity placement files (`*.entities.json` and `*.entities.csv`, with the engine's rules) and `campaign.json` (fields, unique ids, files, themes, and a reachable exit switch `X` in every campaign level, with locked doors passable only once their key can be collected first, and every lock colour matched by a key) in `game/`. Warnings count as failures. It does **not** check `gameplay.json` yet (the engine does, at startup; DevKit check: Roadmap Phase 7). |

Every game run gets its own `--pref-dir` folder under `<build>/test-pref/`, so the tests never
read or replace the player's real log or `progress.json` (known issue B15). The headless
native tests that start an engine (`progress_file_roundtrip`, `campaign_playthrough`,
`new_game_resets_world`) use temporary folders as well; the others set no per-user folder.

`<Python3_EXECUTABLE>` is the interpreter CMake found at configure time (`find_package(Python3)`).
The two `devkit` tests are only registered when CMake finds a Python 3 interpreter. If it does
not, configure prints a warning and the two tests are **not registered** at all: they do not
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
ctest --preset macos -L game                    # the 18 headless game runs
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
lets a test play the game: `campaign_playthrough` plays the whole campaign this way, fetching
level 2's blue key card before the blue locked door and fighting level 3's brute and runner. The
Phase 7 autoplay bot builds on the same seam.

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

On failure the job uploads `LastTest.log` and `CMakeConfigureLog.yaml` as an artifact. Later
phases add a scripted "bot" play-through and packaging checks.
