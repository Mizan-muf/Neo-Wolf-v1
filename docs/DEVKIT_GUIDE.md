# Neo-Wolf DevKit Guide

The DevKit (`tools/devkit/`, Python 3.8 or newer, standard library only) checks and builds game
content: it validates maps, entity files, assets and the campaign with the engine's own rules
(plus stricter level rules), shows maps with their reachability, creates level stubs, checks the
build environment and packages releases. Every command and flag is listed below; `neo_devkit
<command> --help` prints the same.

To prove a level can actually be *played* to the end, use the autoplay bot as well:
`neo_wolf_autoplay` ([TESTING.md](TESTING.md#autoplay-bot)); its `--trace FILE` writes a
per-event log (hits taken and dealt, kills, pickups, doors, the exit) for balancing
([TESTING.md](TESTING.md#event-trace---trace)). Adding a level step by step:
[LEVEL_AUTHORING.md](LEVEL_AUTHORING.md).

## Running it

```bash
./tools/devkit/neo_devkit <command> [options]        # macOS / Linux (bash)
tools\devkit\neo_devkit.cmd <command> [options]      # Windows (Command Prompt or PowerShell)
python3 tools/devkit/devkit.py <command> [options]   # any OS, from any directory
python3 -m tools.devkit <command> [options]          # from the repository root
```

- **Any working directory.** The DevKit works on the repository it belongs to (found from its own
  location), not on the current directory. `--root DIR` points it at another content root (a
  folder with `game/maps` and `game/defs`, e.g. a test fixture). If the DevKit's own folder is not
  inside such a root, it falls back to the working directory or its nearest parent that is.
  Paths you pass as arguments (`--map`, `inspect-map FILE`, ...) are relative to the working
  directory as usual.
- **`neo_devkit` (bash)** sets `PYTHONPATH` to the repository root (without a stray `:`), uses
  `python3` (or `python`) and passes the DevKit's exit code on.
- **`neo_devkit.cmd`** uses `python` when it is a real Python 3.8+, otherwise the Python launcher
  `py -3` (the Microsoft Store `python` stub fails the check and is skipped). If neither works it
  prints how to install Python and exits with 9009. Its variables stay local (`setlocal`) and the
  DevKit's exit code is passed on (`exit /b`).
- **Colours.** ANSI colours are used only on a terminal that shows them: never with `--no-color`
  or the `NO_COLOR` environment variable, never when the output is redirected (a file, a pipe,
  CTest), not with `TERM=dumb`, and on Windows only when the console's virtual-terminal mode is on
  or can be switched on (Windows 10 1511 and later); older consoles get plain text.

### Global options

| Option | Meaning |
|---|---|
| `--no-color` | No ANSI colours (see above for when they are off anyway). |
| `--root DIR` | The content/repository root to work on (default: the DevKit's repository). |
| `-h`, `--help` | Help (also after a command: `neo_devkit validate --help`). |

Global options come **before** the command: `neo_devkit --root tests/fixtures/unwinnable validate`.

### Exit codes

`0` success; `1` the check failed (validation errors, an unreachable exit, a failing doctor
check, a file that would be overwritten, ...); `2` usage error (unknown option, missing
argument, contradicting options).

---

## Commands

### `doctor` — can this machine build and run Neo-Wolf?

```bash
./tools/devkit/neo_devkit doctor
./tools/devkit/neo_devkit doctor --no-network
```

| Option | Meaning |
|---|---|
| `--no-network` | Skip the connection test to github.com (the SDL2 check then only reports `INFO`). |

Checks, each with a platform-specific hint when something is missing:

| Section | Check | Fails (`FAIL`, exit 1) when |
|---|---|---|
| Host | OS; Python version | Python older than 3.8 |
| Toolchain | CMake (`cmake --version`) | CMake missing or older than **3.25** |
| Toolchain | Ninja | missing on macOS or Linux (the presets use it); on Windows only `INFO` (the Visual Studio generator is used) |
| Toolchain | C++ compiler | macOS: no `clang++`; Linux: no `g++`/`clang++`; Windows: no Visual Studio with the C++ workload (found with **vswhere**, no Developer prompt needed) and no `cl` on `PATH` (MinGW only: `WARN`) |
| Toolchain | `make` | never (optional: `INFO`) |
| SDL2 | the pinned SDL2 source | never: `PASS` when the sources are already extracted under `build/*/_deps/sdl2-src`, the pinned tarball is cached (its SHA-256 is checked), `FETCHCONTENT_SOURCE_DIR_SDL2` points at a source folder, or a build uses `NEOWOLF_SYSTEM_SDL2=ON`; otherwise a TCP connection to github.com:443 is tried (**nothing is downloaded**): reachable `PASS`, unreachable `WARN` with the offline options |
| Project | directories, core files | a required directory or file is missing |
| Project | builds | never (`INFO`: which `neo_wolf_autoplay` builds exist) |
| Content | `validate` on the whole project | validation errors (`WARN` for warnings) |

Statuses: `PASS`, `INFO` (optional or informative), `WARN` (works, look at it), `FAIL`. Only
`FAIL` makes the exit code 1; optional tools never fail.

---

### `validate` — content and campaign validation

```bash
./tools/devkit/neo_devkit validate                     # everything under game/
./tools/devkit/neo_devkit validate --strict            # warnings fail too (CTest content_validation)
./tools/devkit/neo_devkit validate --strict --require-levels 10
./tools/devkit/neo_devkit validate --map game/maps/level01.map
./tools/devkit/neo_devkit validate --asset game/assets/sfx_pickup.wav
./tools/devkit/neo_devkit --root tests/fixtures/unwinnable validate
```

| Option | Meaning |
|---|---|
| `--strict` | Warnings fail the validation too (exit 1). |
| `--require-levels N` | The campaign must have exactly `N` levels. Without it, any number is accepted: `campaign.json` is the source of truth. |
| `--map FILE` | Validate one `.map` file only (engine rules plus the DevKit map rules below). |
| `--asset FILE` | Validate one `.ppm` or `.wav` file only. |

Output: `[INFO]`, `[WARN]` and `[FAIL]` lines, then `Validation PASSED` or `FAILED`.

**Engine rules** (a file that fails these fails to load in the game). The DevKit reads every file
exactly like the engine (`tools/devkit/engine_formats.py`):

- **Maps** (`MapLoader`): lines are split on LF and one trailing CR is removed (LF and CRLF
  files are the same); an empty line, or a line whose *first* character is `;`, is skipped; every
  other line is a grid row as is, so whitespace-only lines, indented `;` comments, tabs, trailing
  spaces and a byte order mark make the map invalid. Rows must have equal width and only the
  tiles `. # A B D T X` ([MAP_FORMAT.md](MAP_FORMAT.md)).
- **Entity files** (`EntityPlacementLoader`): JSON with nlohmann/json's rules (a byte order mark
  is skipped; no comments, no `NaN`/`Infinity`; a key repeated in one object is an error); CSV
  split on LF, a byte order mark removed from line 1, ASCII-whitespace trimming, `#` comments,
  numbers in C `strtod`/`strtoll` syntax (so `1_5`, a no-break space or a lone CR are errors, as in
  the engine). Then the placement rules: valid `type`; `x`/`y` finite numbers; `mapX`/`mapY`
  integers 0–100000 on doors and triggers; `kind` on pickups, `key` on key pickups, `value`
  1–10000, `variant`, `lock`; fields that do not apply and unknown fields are warnings
  ([CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md#2-entity-placements-levelentitiesjson)).
  Light zones (the optional top-level `lights` array, JSON only): integers `x`, `y` ≥ 0 and
  `w`, `h` ≥ 1, `level` a number from 0.2 to 1.0, each zone fully inside the map
  ([CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md#light-zones-lights)).
  Positions and door/trigger cells must lie inside the map; exactly one player.
- **Key/value files** (themes, `sprites.cfg`, `audio.cfg`): `key=value` lines, `#` comments; a
  line without `=` or with an empty key or value is an **error** (the engine then ignores the
  whole file). Required keys: the ten theme keys, the twelve sprite keys (the pre-Phase 6
  `enemy`/`pickup` keys are errors), `door_open`, `pickup` and `enemy_alert` in `audio.cfg`.
  Every referenced file must exist; unknown keys are warnings.
- **`gameplay.json`** (`GameplayConfig`): the same types, ranges and defaults as the engine
  (integers must be JSON integers: `35`, not `35.0`), duplicate keys, sections that are not
  objects, the cross-field rules (`initialHealth` ≤ `maxHealth`, `initialAmmo` ≤ `maxAmmo`), the
  `enemies` section (and the deprecated `enemy` section: a warning; both: an error); unknown keys
  are warnings, with a hint for the keys that moved in Phase 4. A missing file is an error.
- **`campaign.json`** (`CampaignConfig`): a non-empty `title`, a non-empty `levels` array, every
  level an object with non-empty `id`, `name`, `map`, `entities`, `theme`; ids and themes use
  only `a-z 0-9 _ -`; ids are unique; the map, entity and theme files exist. Unknown keys and
  names longer than 32 bytes are warnings.
- **Assets**: every `.ppm` under `game/assets/` (P3, header, pixel count, value range; a size that
  is not a power of two is a warning) and every `.wav` in `game/assets/` (RIFF/WAVE, PCM, 1 or 2
  channels).

**DevKit rules** (stricter than the engine, so a level that validates can be finished):

| Rule | Level |
|---|---|
| A map is at least 3×3, its outer cells are solid (`# A B X`), it has a walkable cell | error |
| Exactly one player per level, on a floor cell (`.` or `T`), its body (`player.collisionRadius` from `gameplay.json`) clear of solid cells | error |
| Every door entity sits on a `D` cell and its `x`/`y` lie in that cell; every `D` has exactly one door entity (a `D` without one is a wall for good) | error |
| Every trigger entity sits on a `T` cell and its `x`/`y` lie in it | error |
| A `T` cell without a trigger entity | warning |
| Enemies, pickups and decorations stand on floor cells, not in walls, the exit or a door cell | error |
| An enemy whose body (radius 0.25) overlaps a wall (it starts wedged) | warning |
| At least one exit switch `X` per level, with a reachable floor cell beside it, when every key is collected before the door it opens (breadth-first search from the spawn; a locked door opens once its key's cell was reached) | error |
| Every lock colour has a key in the level, and that key can be collected before the door it opens | error |
| Every pickup and enemy can be reached (with all collectable keys) | warning |
| A map or entity file in `game/` that no campaign level uses (an orphan; a `.csv` export of a used `.json` is fine) | warning |
| `--require-levels N` and the campaign does not have `N` levels | error |

Warnings fail with `--strict`; CTest's `content_validation` runs `validate --strict`. The `info`
lines list, per level, the reachable exits and the order in which the keys become reachable.

---

### `inspect-map` — a map, its entities and its reachability

```bash
./tools/devkit/neo_devkit inspect-map --level level02          # a campaign level by id ...
./tools/devkit/neo_devkit inspect-map --level 2                # ... or by number
./tools/devkit/neo_devkit inspect-map game/maps/level01.map    # finds the level's entity file
./tools/devkit/neo_devkit inspect-map my.map --entities my.entities.csv
./tools/devkit/neo_devkit inspect-map my.map --no-entities --spawn-x 1.5 --spawn-y 1.5
```

| Option | Meaning |
|---|---|
| `MAP` | The `.map` file (optional with `--level`). |
| `--level ID_OR_N` | A campaign level, by id or 1-based number: its map and entity file. |
| `--entities FILE` | The entity file (`.json` or `.csv`). Default: the campaign level that uses `MAP`, else `game/defs/<map name>.entities.json` if it exists. |
| `--no-entities` | Do not look for an entity file (every `D` then counts as an openable door). |
| `--spawn-x X`, `--spawn-y Y` | The spawn for the analysis (both or neither). Default: the player entity. |
| `--no-stats` | Skip the tile statistics. |
| `--no-reachability` | Skip the reachability analysis (exit code 0 once the map and entities load). |

`--stats` and `--reachability` are still accepted and do nothing (both are on by default).

It prints the grid with the entities on top (`P` player, `E` enemy, `$` pickup, `*` decoration,
`D` door, `T` trigger, `X` exit), the tile statistics, and the reachability from the spawn
**with the validator's rules**: door entities open their cells, a locked door only once its key
was reached, a `D` without a door entity is a wall. It lists the reachable and unreachable
cells, the usable exit switches, the key order, key/lock problems and unreachable pickups and
enemies. A level with light zones also gets a `Light Zones` list (each zone's cells and level,
in the order they apply); a zone outside the map is an error (exit code 1), as in the engine.

Exit code `1` when the map or entity file does not load, there is no spawn (no or several player
entities and no `--spawn-x/--spawn-y`), the spawn is outside the map or inside a solid or door
cell (reported, no crash: known issue D3), no exit switch can be reached, a key/lock problem
exists, or a pickup or enemy cannot be reached. Unreachable empty floor cells are listed but do
not fail. Exit code `2` for usage errors.

---

### `inspect-asset` — texture and sound metadata

```bash
./tools/devkit/neo_devkit inspect-asset game/assets/sprites/guard.ppm
./tools/devkit/neo_devkit inspect-asset game/assets/sfx_door_open.wav
```

`ASSET` is a `.ppm` or `.wav` file. Exit code `1` for a missing file or another extension.

---

### `create-map` — a template map, or a whole level stub

```bash
# A level stub: game/maps/level04.map + game/defs/level04.entities.json, added to campaign.json
./tools/devkit/neo_devkit create-map --level-id level04 --style dungeon --width 20 --height 14 \
    --register --name "Supply Depot" --theme stone

# Only a map file
./tools/devkit/neo_devkit create-map /tmp/test.map --style arena
```

| Option | Meaning |
|---|---|
| `OUTPUT` | The `.map` to write. Optional with `--level-id` (default `game/maps/<id>.map`). |
| `--level-id ID` | Also write `game/defs/<id>.entities.json`: the player (angle 0) on the first floor cell, row by row, from which an exit switch can be reached, a door entity on every `D` and a trigger entity on every `T` of the template. `ID` uses only `a-z 0-9 _ -`. |
| `--register` | Append the level to `campaign.json` (needs `--level-id`; the map must be inside the content root). Refused if the campaign already has that id, unless `--force` (then that entry is replaced in place). |
| `--name NAME` | The level name for `--register` (default: the id). |
| `--theme THEME` | The theme for `--register` (default: the last campaign level's theme); `game/defs/themes/<THEME>.cfg` must exist. |
| `--force` | Overwrite an existing map or entity file; replace an existing campaign entry. Without it nothing is overwritten (exit 1). |
| `--width W`, `--height H` | Size in cells (default 16×12, at least 4×4). |
| `--style S` | `arena` (default: open hall, four pillars, a free-standing door and a trigger), `dungeon` (two halves joined by a door, a trigger), `crossroads` (cross-shaped corridors, a trigger in the middle), `box` (an empty room). |

Every template has a closed outer wall and an exit switch `X` in the middle of the east wall with
a floor cell in front of it. A stub made with `--level-id --register` passes `validate --strict`
as it is (tested for every style); it has no enemies, pickups or keys yet. Without `--register`
the new files are orphans (a warning) until you add the level to `campaign.json`.

---

### `create-asset` — procedural test assets

```bash
./tools/devkit/neo_devkit create-asset game/assets/test_checker.ppm --type checker --width 64 --height 64
./tools/devkit/neo_devkit create-asset game/assets/test_brick.ppm --type brick
./tools/devkit/neo_devkit create-asset game/assets/sfx_test.wav --type beep --duration 0.2
```

| Option | Meaning |
|---|---|
| `OUTPUT` | The file to write (overwritten). |
| `--type` | `checker` (default), `brick`, `solid` (PPM textures) or `beep` (a 440 Hz WAV tone). |
| `--width`, `--height` | Texture size (default 64×64). |
| `--duration` | Tone length in seconds (default 0.25). |

The shipped sprites and theme textures come from `gen_sprites.py` and `gen_textures.py` instead
([ASSET_GUIDE.md](ASSET_GUIDE.md)).

---

### `sync-entities` — JSON to CSV export

```bash
./tools/devkit/neo_devkit sync-entities              # every game/defs/<level>.entities.json -> .csv
./tools/devkit/neo_devkit sync-entities --from-csv   # the reverse: overwrites the JSON files
./tools/devkit/neo_devkit sync-entities --level level03   # only campaign level level03
./tools/devkit/neo_devkit sync-entities --level 3 --from-csv   # only level 3, CSV -> JSON
```

| Option | Meaning |
|---|---|
| `--from-csv` | Convert CSV to JSON instead (overwrites every `<level>.entities.json` that has a CSV). |
| `--level ID\|N` | Only one campaign level: its id from `campaign.json` (e.g. `level03`) or its 1-based number, as for `inspect-map --level`. The pair is the level's `entities` file and the `.entities.csv` next to it; no other file is written. An unknown id, a number outside 1…N, or a missing source file (the JSON, or the CSV with `--from-csv`) is an `Error:` with exit code 1 and nothing written. |

The JSON files are canonical (the game loads them); the CSV files are an optional, **lossy**
export: they cannot hold light zones. JSON → CSV prints a `Warning:` line for every level with
light zones and writes a `# lights: ...` comment line into its CSV; CSV → JSON keeps the light
zones of each JSON it overwrites, and refuses (exit code 1, no file written) if an existing JSON
cannot be read, since its light zones would be lost. The old `--prefer-json` flag is accepted
and does nothing.

---

### `package` — release bundle

```bash
./tools/devkit/neo_devkit package --version 1.0.0 --out-dir release
```

| Option | Meaning |
|---|---|
| `--version V` | Version string for the bundle name (default `1.0.0`). |
| `--out-dir DIR` | Output folder (default `release/` in the root). |
| `--no-build` | Do not try to compile a missing executable. |
| `--no-zip`, `--no-tar` | Skip the `.zip` / `.tar.gz` archive. |

Known issue D1: the fallback compile is broken and the bundle can lack an executable; Roadmap
Phase 9 replaces this command.

---

### `test` — the DevKit's unit tests

```bash
./tools/devkit/neo_devkit test
```

Runs every `tools/devkit/tests/test_*.py` (`unittest`); CTest runs it as `devkit_tests`.

---

## Where things live

| File | Contents |
|---|---|
| `cli.py` | The commands above, root resolution |
| `engine_formats.py` | Readers that behave like the engine's loaders (maps, entity JSON/CSV, strict JSON, key/value files) |
| `validator.py` | `validate`: engine rules, DevKit level rules, campaign, `gameplay.json`, orphans |
| `map_tools.py` | Map loading, statistics, ASCII preview, reachability, templates and level stubs |
| `doctor.py` | `doctor` |
| `term.py` | When to use colours (Windows VT mode, `NO_COLOR`, terminals) |
| `def_tools.py` | Entity JSON/CSV conversion (`sync-entities`) |
| `asset_tools.py`, `gen_sprites.py`, `gen_textures.py` | Asset inspection and generation |
| `level_docs.py` | Level pages: checks that `docs/levels/levelNN.md` shows the shipped map byte for byte and regenerates its annotated entity grid (`python3 -m tools.devkit.level_docs [--write]`; [LEVEL_AUTHORING.md](LEVEL_AUTHORING.md#8-document-it)) |
| `packager.py` | `package` |
| `tests/` | Unit tests; `fixture_builder.py` builds the multi-level fixture campaigns they use |

The engine and DevKit readers are kept in step by the parity fixtures in
`tests/fixtures/parity/` (native test `content_parity_fixtures`, DevKit `test_parity.py`).
