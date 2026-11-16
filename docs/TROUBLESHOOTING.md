# Troubleshooting

Problems when **running** the game. For build problems see the Troubleshooting sections of
[BUILD_MACOS.md](BUILD_MACOS.md#troubleshooting) and [BUILD_WINDOWS.md](BUILD_WINDOWS.md#troubleshooting).

## The log file

Every run writes a log. Start here for any problem.

| OS | Log file |
|---|---|
| macOS | `~/Library/Application Support/NeoWolf/NeoWolf/neowolf.log` |
| Windows | `%APPDATA%\NeoWolf\NeoWolf\neowolf.log` (usually `C:\Users\<you>\AppData\Roaming\NeoWolf\NeoWolf\neowolf.log`) |
| Linux | `$XDG_DATA_HOME/NeoWolf/NeoWolf/neowolf.log`, or `~/.local/share/NeoWolf/NeoWolf/neowolf.log` if `XDG_DATA_HOME` is not set |

- The file is started fresh at every launch. The previous run's log is kept next to it as
  `neowolf.prev.log`, so after a crash, look at `neowolf.prev.log` if you have started the game again since.
- The first lines show the version, the SDL version, the log path, the **content root** in use
  and how it was found, the video driver, the renderer and the audio device.
- The same lines are printed to the terminal when the game is started from one (macOS, Linux,
  Windows Debug builds). Windows Release builds have no console; use the file.
- If the per-user folder cannot be created, the game logs a warning to the console and runs
  without a log file.
- Quick ways to open the folder: macOS Finder → Go → Go to Folder… →
  `~/Library/Application Support/NeoWolf/NeoWolf`; Windows Explorer address bar → `%APPDATA%\NeoWolf\NeoWolf`.
- Running the automated tests (`ctest`) also starts the game, so it replaces these files too.

## "Game content not found"

The game needs its content root: the folder that contains `game/maps` and `game/defs`. It looks,
in order, in:

1. the folder given with `--data <dir>`, otherwise the folder in the `NEOWOLF_DATA_DIR`
   environment variable. Either of these is the *only* place searched when given;
2. the executable's folder and up to 3 folders above it;
3. `<executable folder>/../Resources` (inside a macOS `.app`);
4. the current working directory and its parent.

The error dialog and the log list every folder that was searched. Fixes:

- Keep the `game` folder next to the executable (or in the repository, for a developer build).
- Or start the game with `--data "<folder that contains game>"`, e.g.
  `neo_wolf --data "/Users/me/Neo-Wolf"`, not `.../Neo-Wolf/game`.
- If `NEOWOLF_DATA_DIR` is set to an old location, fix or unset it.

The game exits with code 1 in this case. The same happens if the first level's map cannot be
loaded (for example `--level 5` when there is no `game/maps/level05.map`).

## "Invalid gameplay definitions" or "Failed to load entity placements"

The content was found, but `game/defs/gameplay.json` or the level's entity file
(`game/defs/levelNN.entities.json`) is missing or invalid. The game exits with code 1 and the
log lists every problem, with the file, the entry and the field, for example:

```text
[ERROR]   .../game/defs/gameplay.json: weapon.damage must be an integer from 1 to 10000, got "35"
[ERROR]   .../game/defs/level01.entities.json: entities[2] (pickup): field "kind" is required ("health" or "ammo")
```

Fix the file (formats, ranges and defaults: [CONTENT_DEFINITIONS.md](CONTENT_DEFINITIONS.md))
or restore it from the release. `python3 tools/devkit/devkit.py validate --strict` checks the
entity files without starting the game. Warnings such as `unknown key ... ignored` do not stop
the game, but mean that a value you edited is not used (usually a typo).

## No sound

If there is no usable audio device (none installed, a remote session, a broken driver, or an
invalid `SDL_AUDIODRIVER`), the log shows

```text
[WARN] SDL audio initialisation failed: ...      (or: SDL_OpenAudioDevice failed: ...)
[WARN] No audio device: the game runs without sound.
```

and the game runs silently. Check the system's output device and volume, unset `SDL_AUDIODRIVER`
if you set it, then restart the game (devices are only looked for at startup). Individual sound
files that fail to load are logged as warnings.

## No GPU, virtual machines, remote desktop, headless

The game first asks SDL for a GPU-accelerated renderer with vsync. If there is none (virtual
machine without 3D acceleration, Remote Desktop, missing GPU driver, `SDL_VIDEODRIVER=dummy`) it
logs

```text
[WARN] No accelerated renderer (...); falling back to the software renderer.
[INFO] Renderer: software (not accelerated), vsync off, ...
[INFO] No vsync: capping the frame rate at 60 FPS.
```

and continues with SDL's software renderer. The game draws its 320×240 image on the CPU anyway,
so it stays playable. The old workaround `SDL_RENDER_DRIVER=software` is no longer needed (it
still works and forces the software renderer).

If the window cannot be opened at all, the log contains the SDL error (`SDL video
initialisation failed` or `SDL_CreateWindow failed`) and the game exits with code 1.

For automated runs without a display use SDL's dummy drivers and a fixed number of frames:

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./neo_wolf --frames 300   # exits 0 after 300 frames
```

`--frames` also suppresses error dialogs, so a failing automated run never waits for a click.

## Window, fullscreen, pausing

- Toggle desktop fullscreen with `F11` or `Alt+Enter` (macOS: also `Cmd+Ctrl+F`). `--fullscreen`
  starts in fullscreen, `--windowed` in a window.
- The window can be resized; the image keeps its 4:3 shape with black bars and sharp pixels.
- The game pauses when its window loses focus (switching apps). Press `Esc`, `P` or `Enter` to
  resume.
- To quit: `Esc` on the main menu, close the window, `Cmd+Q` (macOS) or `Alt+F4` (Windows).

## macOS Gatekeeper (packaged builds, Roadmap Phase 9)

*Placeholder until the signed and notarized `.app`/`.dmg` exists.* A build you compiled yourself
runs without warnings. A downloaded, unsigned or un-notarized app is blocked with "cannot be
opened because Apple cannot check it for malicious software". Phase 9 signs and notarizes the
release; until then, right-click the app → Open → Open (or System Settings → Privacy & Security →
Open Anyway). Do not disable Gatekeeper globally.

## Windows SmartScreen (packaged builds, Roadmap Phase 9)

*Placeholder until the Windows package exists.* A downloaded, unsigned `neo_wolf.exe` may show
"Windows protected your PC". Choose More info → Run anyway if you trust the source. Phase 9
decides on code signing.

## Sending a log with a bug report

1. Reproduce the problem, then quit the game (or note that it crashed).
2. Take `neowolf.log` from the folder above. If you have restarted the game since the problem,
   take `neowolf.prev.log` instead (or both).
3. Attach it to the bug report with: what you did, what you expected, what happened, your OS
   version, and how you started the game (Finder/Explorer, terminal command and options).

The log contains file paths from your computer (for example your user name inside
`/Users/<you>/...` or `C:\Users\<you>\...`); remove anything you do not want to share.
