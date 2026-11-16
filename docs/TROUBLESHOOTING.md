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
- Automated runs do **not** touch these files: every `ctest` game run and the CI smoke run
  (`tools/ci/smoke_run.py`) pass `--pref-dir` with a temporary folder (known issue B15, fixed in
  Phase 5).

### Using another folder: `--pref-dir` and `NEOWOLF_PREF_DIR`

The per-user folder holds the log files and the saved progress. To use a different folder
(a second profile, a portable install, tests):

```bash
neo_wolf --pref-dir /path/to/folder           # this run only
NEOWOLF_PREF_DIR=/path/to/folder neo_wolf     # environment variable (set NEOWOLF_PREF_DIR=... on Windows)
```

`--pref-dir` wins over `NEOWOLF_PREF_DIR`, which wins over the default folder above. A relative
path is taken relative to the current directory, and the folder is created if needed. The log
names the folder and where the choice came from, e.g.
`Per-user folder: /tmp/nw-prefs (--pref-dir)`. With either override the default folder is not
created or used at all.

## Saved progress

The game saves which levels are unlocked to `progress.json` in the per-user folder above
(macOS `~/Library/Application Support/NeoWolf/NeoWolf/progress.json`, Windows
`%APPDATA%\NeoWolf\NeoWolf\progress.json`), each time a level is completed. Format and rules:
[GAMEPLAY.md](GAMEPLAY.md#saves).

- **Continue is missing / levels are locked again:** the file is missing, or it was unreadable
  and ignored. The log says which, e.g. `Ignoring the saved progress (starting without it): ...`.
  A broken file is not repaired or deleted; it is replaced the next time a level is completed.
- **Reset progress:** quit the game and delete `progress.json`.
- **Unlock everything for testing:** start a level directly with `--level N` (no unlock check),
  or write `{ "version": 1, "highestUnlocked": 10 }` into the file.
- A leftover `progress.json.tmp` means the game stopped while saving; it is safe to delete
  (the previous `progress.json` is intact).

## "Game content not found"

The game needs its content root: the folder that contains `game/maps` and `game/defs`. It looks,
in order, in:

1. the folder given with `--data <dir>`, otherwise the folder in the `NEOWOLF_DATA_DIR`
   environment variable. Either of these is the *only* place searched when given;
2. the executable's folder and up to 3 folders above it (inside a macOS `.app` SDL reports
   `Contents/Resources` as this folder; the log then says `(app bundle Resources)`);
3. `<executable folder>/../Resources` (inside a macOS `.app`);
4. the current working directory and its parent.

The error dialog and the log list every folder that was searched. Fixes:

- Keep the `game` folder next to the executable (or in the repository, for a developer build).
- Or start the game with `--data "<folder that contains game>"`, e.g.
  `neo_wolf --data "/Users/me/Neo-Wolf"`, not `.../Neo-Wolf/game`.
- If `NEOWOLF_DATA_DIR` is set to an old location, fix or unset it.

The game exits with code 1 in this case. The same happens if the first level's map cannot be
loaded, or with `--level N` when the campaign (`game/defs/campaign.json`) has fewer than N levels.

## "Invalid gameplay definitions", "Invalid campaign manifest" or "Failed to load entity placements"

The content was found, but `game/defs/gameplay.json`, `game/defs/campaign.json` or the level's
entity file (`game/defs/levelNN.entities.json`) is missing or invalid. The game exits with code 1 and the
log lists every problem, with the file, the entry and the field, for example:

```text
[ERROR]   .../game/defs/gameplay.json: weapon.damage must be an integer from 1 to 10000, got "35"
[ERROR]   .../game/defs/level01.entities.json: entities[4] (pickup): field "kind" is required ("health", "ammo" or "key")
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

Sound effects play one after another, not mixed (the engine queues them on one SDL audio
device), so a burst of shots can delay the next sound. This is a known limitation, not a fault
(known issue C23).

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
- To quit: **Quit** on the main menu (or `Esc` there), close the window, `Cmd+Q` (macOS) or
  `Alt+F4` (Windows). From play: `P`, then `Q` for the menu. All keys: [CONTROLS.md](CONTROLS.md).

## Packaged builds

Problems with the downloaded packages (`neo-wolf-<version>-macos-universal.zip`,
`neo-wolf-<version>-windows-x64.zip`; [RELEASE.md](RELEASE.md)). A build you compiled yourself
does not have them.

### macOS: "cannot be opened" / "Apple could not verify" (Gatekeeper)

`NeoWolf.app` is signed ad hoc but **not notarized** (the project has no Apple Developer ID), so
macOS blocks the first start of a downloaded copy. Allow it once:

- **macOS 14 (Sonoma) and older:** Control-click (right-click) `NeoWolf.app` → **Open** → **Open**.
- **macOS 15 (Sequoia) and newer:** the Control-click route no longer offers Open. Double-click
  the app once and close the message, then System Settings → **Privacy & Security** → scroll down
  to the message about `NeoWolf.app` → **Open Anyway** → confirm.
- **Any version, Terminal:** `xattr -dr com.apple.quarantine /path/to/NeoWolf.app` removes the
  download mark.

Do not turn Gatekeeper off globally. Only do this for a copy from the project's release page;
compare the zip with `SHA256SUMS` first (`shasum -a 256 -c SHA256SUMS --ignore-missing`).

Other macOS cases:
- **"NeoWolf.app is damaged and can't be opened"**: the app was changed after signing (for
  example a file added inside `Contents/`), or extracted by a tool that dropped the execute
  permission. Extract the zip again with Finder (double-click) and do not edit the bundle. To
  check: `codesign --verify --deep --strict NeoWolf.app` must print nothing and exit 0.
- **The app starts from a random read-only path** (`/private/var/folders/.../AppTranslocation/...`
  in the log): macOS runs quarantined apps from a temporary copy until they are allowed once or
  moved (for example into Applications). The game is unaffected: its content is inside the bundle.
- **"Game content not found" from the app**: `Contents/Resources/game` is missing; the bundle is
  incomplete. Download and extract it again. The log lists the searched folders; the first one
  must be `.../NeoWolf.app/Contents/Resources`.
- The log and progress are in `~/Library/Application Support/NeoWolf/NeoWolf/` (above), not in
  the app.

### Windows: "Windows protected your PC" (SmartScreen)

`neo_wolf.exe` is not code-signed, so SmartScreen may stop the first start of a downloaded copy
(standard Windows behaviour for unsigned downloads; not yet observed with this game, known issue D5).
Click **More info** → **Run anyway**. Alternatively, before extracting, right-click the downloaded
zip → Properties → tick **Unblock** → OK; the extracted files then carry no download mark. Only do
this for a copy from the project's release page (`Get-FileHash <zip> -Algorithm SHA256`, compare
with `SHA256SUMS`).

Other Windows cases:
- **"Game content not found" right after double-clicking in the zip window**: Explorer runs the
  exe from a temporary folder without `game\` (standard Explorer behaviour; not yet tried with this
  game on a Windows PC). Extract the whole zip first (right-click → Extract
  All...) and start `neo_wolf.exe` from the extracted folder; keep `game\` next to it.
- **A missing DLL message** (`VCRUNTIME140.dll`, `SDL2.dll`, ...): the official package needs
  none (static C runtime and SDL). You are running another build; use the release package.
- The Release exe has no console: `--version` and `--help` print only when started from a
  Command Prompt or PowerShell window (not yet verified on a Windows PC); problems are in the log
  file (above).

## Sending a log with a bug report

1. Reproduce the problem, then quit the game (or note that it crashed).
2. Take `neowolf.log` from the folder above. If you have restarted the game since the problem,
   take `neowolf.prev.log` instead (or both).
3. Attach it to the bug report with: what you did, what you expected, what happened, your OS
   version, and how you started the game (Finder/Explorer, terminal command and options).

The log contains file paths from your computer (for example your user name inside
`/Users/<you>/...` or `C:\Users\<you>\...`); remove anything you do not want to share.
