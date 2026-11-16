# Testing Neo-Wolf

All tests are registered with CTest, so one command runs everything:

```bash
ctest --preset macos            # macOS   (after cmake --preset macos && cmake --build --preset macos)
ctest --preset linux            # Linux   (not yet run on Linux)
ctest --preset windows-msvc     # Windows (not yet run on Windows; windows-msvc-debug for Debug)
```

`./build.sh`, `build.cmd`, `build.ps1` and `make check` configure, build and then run exactly
this. Any failing test makes them exit non-zero.

## Test layers

| CTest name | Label | What it runs | What it covers |
|---|---|---|---|
| `native.<id>` (27 tests) | `native` | `neo_wolf_tests <id>` | C++ engine logic: math, input, map queries, raycasting, texture/asset loading, entities, collision, interaction rules, game state, combat, AI, pickups, command bridge, logging and log-file rotation, level reset, a scripted vertical-slice run, map reachability, the C interop API, content-root resolution (with temporary directories, including a non-ASCII path), command-line parsing, and CRLF content files. Source: `tests/LogicTests.cpp`. |
| `native_test_registry` | `native` | `cmake -P cmake/CheckTestRegistry.cmake` | Checks that every test case compiled into `neo_wolf_tests` has a CTest entry and the reverse. Catches a test that was added but never registered. |
| `game_smoke_frames` | `game` | `cmake -P cmake/RunGame.cmake` → `neo_wolf --frames 120` | Starts the real game headless (`SDL_VIDEODRIVER=dummy`, `SDL_AUDIODRIVER=dummy`, set with the `ENVIRONMENT_MODIFICATION` test property, which also **removes** `SDL_RENDER_DRIVER` and `NEOWOLF_DATA_DIR`). Runs from the build directory, so content must be found relative to the executable. Passes if the game exits with 0 and logs its content root, its renderer and 120 frames. Proves the software-renderer fallback (B11) and the content-root search. Takes about 2 s (60 FPS cap). |
| `game_smoke_missing_data` | `game` | `neo_wolf --data <build>/test-empty-data --frames 10` | Passes if the game exits with code 1 and reports `Game content not found` (fail-fast startup). |
| `devkit_tests` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color test` | The DevKit's 19 Python unit tests (`tools/devkit/tests/test_*.py`): map tools, asset tools, definitions, validator, packager. |
| `content_validation` | `devkit` | `<Python3_EXECUTABLE> tools/devkit/devkit.py --no-color validate --strict` | Validates the maps, PPM/WAV assets, `.cfg` definition files (`wall_textures.cfg`, `sprites.cfg`, `audio.cfg`), entity placement files and `entities.json` in `game/`. Warnings count as failures. It does **not** check `gameplay.json`, `sprites.json` or `textures.json` yet (Roadmap Phases 4 and 7). |

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
ctest --preset macos -L game                    # the two headless game runs
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

### Windows notes (not yet run on Windows)

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

## Warnings as errors

The `ci-macos`, `ci-linux` and `ci-windows-msvc` presets set `NEOWOLF_WERROR=ON`, which turns
compiler warnings into errors for Neo-Wolf's own code (`-Werror` or `/WX`). SDL is never affected.
Run the same check locally before pushing:

```bash
cmake --preset ci-macos && cmake --build --preset ci-macos && ctest --preset ci-macos
```

## Continuous integration

**Status: not all jobs have passed yet** (Roadmap Phase 2 is done when they do). On the first
run (commit `1b10a57`) the Linux job and the `windows-scripts` job passed; the Windows preset job
failed on an MSVC `/WX` warning in `tests/LogicTests.cpp` (C4459, fixed since); the macOS job
never started because the `macos-14` runner image is retired (the workflow now uses
`macos-latest`).

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
found relative to the executable) with SDL's dummy video and audio drivers. It removes
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
