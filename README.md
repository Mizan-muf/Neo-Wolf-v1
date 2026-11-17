# Neo-Wolf

[![CI](https://github.com/Mizan-muf/Neo-Wolf-v1/actions/workflows/ci.yml/badge.svg?branch=completion%2Fv1)](https://github.com/Mizan-muf/Neo-Wolf-v1/actions/workflows/ci.yml)

Neo-Wolf is a **game engine** for first-person games in the style of early-1990s raycasters,
written in C++17 on SDL2. It draws every frame in software (640×360 by default; `--render-size`
sets another size) and reads everything a game is made of from plain files: maps, entity
placements, enemy types, gameplay values and the level list, so content changes need no
recompiling. A Python DevKit checks content with the engine's own rules. It builds on macOS and
Windows (Linux best effort).

The engine ships with a **demo**: a 10-level campaign that shows what it can do. The engine's
tests run on it, and a headless bot proves in CI that every level can be finished.

| Level 1, "Outpost Gate" | Level 5, "Catacombs" |
|---|---|
| ![Level 1 with the HUD and the level banner](docs/images/screens/hud.png) | ![Level 5: a dim crypt with a guard](docs/images/levels/level05-crypt.png) |

## Features

### The engine

- **Software renderer:** DDA raycasting with textured walls, floor and ceiling, distance
  shading, authorable dark and dim areas, billboard sprites and simple particles; the image is
  scaled to any window size with sharp pixels, at a render size you choose.
- **Data-driven content:** text maps, JSON entity placements, `gameplay.json` for every stat
  and for the game's own enemy types and their sprites, `campaign.json` for the level list;
  invalid files are rejected with a message naming the file and field
  ([CONTENT_DEFINITIONS.md](docs/CONTENT_DEFINITIONS.md)).
- **Enemy AI:** enemies see you only with a clear line of sight, hear your shots through open
  doors, path-find around walls and search for you when you break contact.
- **Game systems:** a hitscan weapon with limited ammo, pickups, red, blue and gold key cards and
  locked doors, exit switches, a campaign with saved progress, menus, a HUD and a minimap.
- **Tooling:** a Python DevKit that validates levels with the engine's rules (including "the
  exit is reachable and keys come before their doors"), inspects maps, creates level stubs and
  builds packages; a deterministic autoplay bot (`neo_wolf_autoplay`) that plays every level
  through simulated input; a development level (the Test Chamber) with an area per feature
  ([DEVKIT_GUIDE.md](docs/DEVKIT_GUIDE.md), [TESTING.md](docs/TESTING.md#autoplay-bot)).
- **Embedding:** a C API and a command bridge to drive the engine from other code
  ([C_API.md](docs/C_API.md)).
- **Sound effects** for doors, pickups, shots and enemy alerts (no music yet). The engine also
  runs without an audio device.

### The demo campaign

- 10 levels in four texture themes, a main menu (New Game, Continue, Level Select, Test
  Chamber, Quit), health and ammo carried between levels, restart from the level start on death,
  saved progress and unlocked levels ([GAMEPLAY.md](docs/GAMEPLAY.md)).
- Three enemy types defined in its `gameplay.json` (guard, brute, runner), each with its own
  generated sprite and corpse.

## Download & play

There are no published releases yet: the first GitHub release comes with the `v1.0.0` tag
(Roadmap Phase 12). Until then, packages are built by the CI job `package` as the artifacts
`neo-wolf-package-macOS` and `neo-wolf-package-Windows` of a workflow run (open a run under
the repository's Actions tab). The job was added in Phase 9 and is green (since run 43864756039); the packages
have not yet been opened by hand from Finder or Explorer ([known issue D5](docs/KNOWN_ISSUES.md)). You can also build a package
yourself ([RELEASE.md](docs/RELEASE.md)).

Each zip holds one folder with the game, `README.txt`, `LICENSE.md`, `docs/` and `LICENSES/`.
Check a download against `SHA256SUMS` first ([RELEASE.md](docs/RELEASE.md#checksums)).

- **macOS 10.15+**, Apple silicon and Intel (Intel tested under Rosetta only;
  `neo-wolf-<version>-macos-universal.zip`): unzip
  and open `NeoWolf.app`. The app is signed ad hoc but not notarized, so Gatekeeper blocks the
  first start of a downloaded copy. On macOS 14 and older: Control-click the app → Open → Open.
  On macOS 15 and newer: try to open it once, then System Settings → Privacy & Security →
  Open Anyway. Or run `xattr -dr com.apple.quarantine NeoWolf.app`.
- **Windows 10/11 x64** (`neo-wolf-<version>-windows-x64.zip`): extract the whole zip and run
  `neo_wolf.exe`; no installer and no runtime to install (the exe imports no SDL or C/C++
  runtime DLL, checked in CI; not yet tried on a clean PC). The exe is not code-signed, so
  SmartScreen may say "Windows protected your PC": More info → Run anyway (or Properties →
  Unblock on the zip before extracting).

Problems starting it: [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md#packaged-builds).

## Build from source

You need CMake 3.25+, Python 3 and a C++17 compiler. SDL2 is downloaded (a pinned, checksummed
source release) and linked statically by CMake, so there is nothing else to install; the first
configure needs internet access.

**macOS** (Xcode Command Line Tools, `brew install cmake ninja`;
[BUILD_MACOS.md](docs/BUILD_MACOS.md)):

```bash
./build.sh                  # configure, build and run all tests (Release)
./build/macos/neo_wolf      # play; works from any directory
```

The main menu also has a **Test Chamber**: a development level with one signposted area per
thing to test (enemies, pickups, keys and locks, props, a weapon range, a dark room); it never
touches your saved progress. With `"bootIntoTestLevel": true` in `campaign.json` the game starts
straight in it; `--level N` starts campaign level N directly
([GAMEPLAY.md](docs/GAMEPLAY.md#the-test-chamber), [the Test Chamber's map](docs/levels/testchamber.md)).

**Windows** (Visual Studio 2022 or newer with *Desktop development with C++*, Python 3;
[BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md); built and tested in CI, not yet on a Windows PC):

```cmd
build.cmd
build\windows-msvc\Release\neo_wolf.exe
```

**Linux** (best effort: built and tested headless in CI, never run on a desktop): install the
X11/Wayland and audio development packages SDL builds against
([BUILD_AND_RELEASE.md](docs/BUILD_AND_RELEASE.md)), then `./build.sh` and
`./build/linux/neo_wolf`.

`neo_wolf --help` lists the options (`--level N`, `--data`, `--fullscreen`, ...). How to run
single tests, the bot and CI checks: [CONTRIBUTING.md](CONTRIBUTING.md).

## Controls

| Action | Keys |
|---|---|
| Move, strafe | `W` `S` or Up/Down arrows; `A` `D` |
| Turn | Left/Right arrows (`Q` also turns left) |
| Fire | `Space`, `F`, `Ctrl` |
| Use a door or the exit switch | `E` |
| Pause | `P` or `Esc` |
| Restart the level | `R` |
| Minimap | `M` or `Tab` |
| Menus | `W`/`S` or arrows, `Enter`/`Space` to select, `Esc` back |
| Fullscreen | `F11`, `Alt+Enter`; macOS also `Cmd+Ctrl+F` |

Every key on every screen: [CONTROLS.md](docs/CONTROLS.md).

## The campaign

Ten levels, each adding one idea: Outpost Gate (tutorial), Old Cellars (key cards), Server
Annex (enemies hear gunfire), Motor Pool (the brute), Catacombs (runners, a dim crypt), Relay
Station (ambushes), Blackout Barracks (dark wings), Reactor Spine (a three-key chain), Sunken
Archive (a non-linear hub) and Wolf's Den (the finale). The design, difficulty curve and each
level's map and walkthrough: [docs/levels/README.md](docs/levels/README.md).

## Documentation

| Document | What it is for |
|---|---|
| [GAMEPLAY.md](docs/GAMEPLAY.md) | How the game plays: states, rules, enemies, keys, saves |
| [CONTROLS.md](docs/CONTROLS.md) | Every key, per screen |
| [levels/README.md](docs/levels/README.md) | The campaign design, with one page per level |
| [levels/testchamber.md](docs/levels/testchamber.md) | The Test Chamber: the engine's development level (main menu, or `"bootIntoTestLevel"`) |
| [levels/BUILD_NOTES.md](docs/levels/BUILD_NOTES.md) | History: the level builders' briefs |
| [BUILD_MACOS.md](docs/BUILD_MACOS.md) | Building and running on macOS |
| [BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md) | Building and running on Windows |
| [BUILD_AND_RELEASE.md](docs/BUILD_AND_RELEASE.md) | Build overview for all platforms, Linux notes, validation |
| [TESTING.md](docs/TESTING.md) | Test layers, running tests, the autoplay bot, CI |
| [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Problems when running the game: logs, saves, content, sound, packages |
| [RELEASE.md](docs/RELEASE.md) | Versioning, packages, checksums, the release checklist |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Modules, dependencies, the frame loop, rendering, test seams |
| [C_API.md](docs/C_API.md) | The in-process C API and its command and event queues |
| [CONTENT_DEFINITIONS.md](docs/CONTENT_DEFINITIONS.md) | Every content file: `gameplay.json`, entity files, campaign, themes |
| [MAP_FORMAT.md](docs/MAP_FORMAT.md) | The `.map` grid format |
| [LEVEL_AUTHORING.md](docs/LEVEL_AUTHORING.md) | Step-by-step checklist for adding a level |
| [ASSET_GUIDE.md](docs/ASSET_GUIDE.md) | Texture, sprite and sound formats and their generators |
| [ASSET_CREDITS.md](docs/ASSET_CREDITS.md) | Where every asset comes from, and its licence |
| [DEVKIT_GUIDE.md](docs/DEVKIT_GUIDE.md) | Every DevKit command and option |
| [THIRD_PARTY_LICENSES.md](docs/THIRD_PARTY_LICENSES.md) | Licences of SDL2 and nlohmann/json |
| [ROADMAP.md](docs/ROADMAP.md) | The plan to v1.0 and what is verified (source of truth) |
| [KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md) | Open and resolved defects and gaps |
| [Checklist.md](docs/Checklist.md) | History: the original 26-phase checklist, reconciled with the code |
| [history/](docs/history/README.md) | Archived, superseded documents |
| [CHANGELOG.md](CHANGELOG.md) | What changed, version by version |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Building, testing, conventions and commit style for contributors |
| [third_party/README.md](third_party/README.md) | Vendored code: versions and checksums |

## Project status

Work toward v1.0 follows [docs/ROADMAP.md](docs/ROADMAP.md). The build, CI, runtime fixes, data
pipeline, campaign flow, AI, bot, all ten levels, packaging and the documentation pass with its
accuracy audit (Phases 1–10) are done;
they build and pass all tests on macOS and headless in Windows and Linux CI (nothing has been
played by hand yet, known issue C13), with the remaining gaps listed
per phase in the roadmap. Still open: a play-through by hand
and on a real Windows PC (Phase 11), and the clean-code review and `v1.0.0` tag (Phase 12). Known defects and
unverified areas are listed in [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md); changes in
[CHANGELOG.md](CHANGELOG.md).

## License

Copyright (c) 2026 Mizan-muf. Neo-Wolf is licensed under the
[PolyForm Noncommercial License 1.0.0](LICENSE.md): free for any noncommercial use, including
personal play, study, hobby projects and modding, and use by schools, charities and public
bodies. Commercial use needs the copyright holder's permission. Because it restricts commercial
use, Neo-Wolf is **source-available**, not "open source" in the OSI sense.

Third-party components keep their own licences, which allow distributing the game this way:
SDL2 (zlib) and nlohmann/json (MIT); see [docs/THIRD_PARTY_LICENSES.md](docs/THIRD_PARTY_LICENSES.md).
Asset origins: [docs/ASSET_CREDITS.md](docs/ASSET_CREDITS.md). Contributing:
[CONTRIBUTING.md](CONTRIBUTING.md).
