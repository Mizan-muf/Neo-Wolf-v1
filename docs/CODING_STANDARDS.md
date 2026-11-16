# Coding standards

The rules every change to Neo-Wolf follows, and how they are checked. Phase 12 of the
[roadmap](ROADMAP.md) reviewed the whole code base against them; the findings and what was
done about each are in [CLEAN_CODE_REVIEW.md](CLEAN_CODE_REVIEW.md).

Some rules are checked by tools in CI (see [Automated checks](#automated-checks)); the rest are
checked in review.

## Principles

### 1. Meaningful names
- Names say what a thing is or does, in the game's own words: `playerHealth_`, `FindContentRoot`,
  `exitMapX`. Avoid abbreviations except the ones used everywhere (`dt`, `x`/`y`, `id`, `ui`).
- Naming conventions (C++):

  | Kind | Style | Example |
  |---|---|---|
  | Types, functions, methods | `PascalCase` | `ContentRootResult`, `PumpEvents()` |
  | Local variables, parameters, struct fields | `camelCase` | `moveDelta`, `dataOption` |
  | Private data members | `camelCase_` | `hasFocus_`, `map_` |
  | Named constants (`constexpr`, or `const` at namespace or class scope) | `kPascalCase` | `kPi`, `kDoorPressDistance` |
  | Macros | `UPPER_SNAKE_CASE`, only where a macro is unavoidable | `TEST_ASSERT` |
  | Files | `PascalCase.cpp` / `.h`, one main type or topic per file | `EnemyAi.cpp` |

- Python follows PEP 8: `snake_case` functions and variables, `PascalCase` classes,
  `UPPER_SNAKE_CASE` module constants.

### 2. Small functions with one responsibility
- A function does one thing at one level of abstraction. If describing it needs "and", split it.
- C++ functions stay under a cognitive complexity of 25 (clang-tidy enforces this outside the
  tests). Python functions stay under 15 branches, 8 returns and 60 statements (ruff enforces
  this outside the tests).
- Long parsers are split into one helper per field or per section, not one long `if` chain.

### 3. No duplication
- The same rule or calculation lives in one place. Data shared between the engine and the
  DevKit (tile codes, entity types, campaign rules) is defined in the content files and read by
  both, not copied into both.

### 4. No dead or commented-out code
- Code that is not called is deleted, not commented out or kept "for later"; git history keeps
  it. `#if 0` blocks are not allowed.

### 5. No magic numbers
- A number that means something (a tuning value, a size, a limit) gets a named constant, or
  comes from the content files (`game/defs/gameplay.json` and friends). Literals are fine where
  the meaning is obvious: `0`, `1`, `2` in arithmetic, `0.5f` for "half", and pixel offsets
  inside one UI layout function where each value is used once next to a comment.

### 6. Const-correctness
- Anything not modified is `const`: locals, reference parameters, member functions that do not
  change the object. Pass large objects by `const&`, small ones by value.

### 7. RAII and clear ownership
- No raw `new`/`delete`. Owned objects are values, `std::unique_ptr` or standard containers.
- SDL resources (window, renderer, texture, audio device) are owned by one object whose
  destructor releases them. Raw pointers mean "borrowed, not owned".

### 8. Consistent error handling
- Content and file errors are reported, logged with the file and the reason, and handled: a
  bad level fails to load with a message, it does not crash or silently continue.
- Library code returns a result (`bool` + message, or a result struct); only `main` and the
  top-level loop decide to exit. Exceptions do not cross module boundaries; the JSON parser's
  exceptions are caught where the file is read.
- Python tools exit with the codes they document (0 success, 1 failure, 2 usage error).

### 9. Comments explain *why*
- Comments say why the code is the way it is, what a value means, or what is not obvious:
  units, coordinate systems, platform quirks, links to known issues. They do not restate the
  code. Every header starts with a short comment saying what the module is for.

### 10. Clear module boundaries
- `engine/` is split into modules (`core`, `platform`, `render`, `world`, `assets`, `audio`,
  `ui`, `interop`, `autoplay`, `particles`). [ARCHITECTURE.md](ARCHITECTURE.md) lists what each
  may include.
- SDL is included only in `platform`, `audio`, `core` (start-up, paths and logging) and the
  test runner. Game rules (`world`, `GameFlow`) do not include SDL, so they can be tested
  without a window.
- Modules depend downwards only: `core/Engine.h` is included by `core`, and by `interop` and
  `autoplay`, which sit on top of the engine; never by `world`, `render`, `assets` or `ui`.

## Formatting

- C++: [.clang-format](../.clang-format) (Google base, 4-space indent, 120 columns, `Type* p`).
  Run `clang-format -i <files>` before committing.
- Python: `ruff format` with [ruff.toml](../ruff.toml) (120 columns, double quotes).
- Every text file ends with one newline and uses LF, except `*.cmd` / `*.bat`
  ([.gitattributes](../.gitattributes)).

## Automated checks

| Check | Tool and config | What it covers |
|---|---|---|
| C++ formatting | clang-format, [.clang-format](../.clang-format) | every tracked `.cpp` / `.h` outside `third_party/` |
| C++ static analysis | clang-tidy, [.clang-tidy](../.clang-tidy) and [tests/.clang-tidy](../tests/.clang-tidy) | `engine/`, `src/`, `tests/`: bugprone, cppcoreguidelines, misc, modernize, performance and readability checks; the excluded checks and the reason for each are listed in the config |
| Python lint and format | ruff, [ruff.toml](../ruff.toml) | `tools/devkit`, `tools/ci` |

One script runs them all, locally and in CI:

```sh
python3 tools/ci/lint.py                     # formatting + ruff (no build needed)
cmake --preset macos-debug -B build/tidy -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/tidy                     # once: SDL's headers are generated by the build
python3 tools/ci/lint.py --tidy -p build/tidy
```

CI installs pinned versions with pip (`clang-format`, `clang-tidy` and `ruff` wheels) so local
and CI results match; the versions are in [.github/workflows/ci.yml](../.github/workflows/ci.yml).
A tool that is not on `PATH` can be pointed to with `--tool-dir`.

A check is suppressed in code (`// NOLINT(check-name)` or `# noqa: CODE`) only with a comment
on the same line saying why.

## Changing behaviour

Clean-up commits do not change behaviour. Before and after each one: `ctest` passes, and the
autoplay bot's per-level results (`neo_wolf_autoplay --all`) are identical, damage taken
included. The bot is deterministic, so any difference means behaviour changed.
