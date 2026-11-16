# Controls

The source of truth is `PlatformSDL::MapKeyToActions` and `PlatformSDL::IsFullscreenShortcut`
in `engine/platform/PlatformSDL.cpp`; this page matches them (test `key_bindings`). Keys map to
input actions (`engine/core/InputState.h`); what an action does depends on the screen
(`engine/core/GameFlow.cpp`, [GAMEPLAY.md](GAMEPLAY.md)).

## Key → action

| Key | Action(s) |
|---|---|
| `W`, Up arrow | `MoveForward` (also "menu up") |
| `S`, Down arrow | `MoveBackward` (also "menu down") |
| `A` / `D` | `StrafeLeft` / `StrafeRight` |
| Left arrow | `TurnLeft` |
| `Q` | `TurnLeft` and `QuitToMenu` |
| Right arrow | `TurnRight` |
| `E` | `Interact` |
| `Space` | `Attack` and `Select` |
| `F`, left `Ctrl`, right `Ctrl` | `Attack` (`Ctrl` only while `Cmd`/Windows key is **not** held, see B13 below) |
| `Esc` | `Pause` and `Back` |
| `P` | `Pause` |
| `R` | `Restart` |
| `Enter`, keypad `Enter` | `Confirm` and `Select` |
| `M`, `Tab` | `ToggleMinimap` |
| `F3` | `ToggleDebugView` |

Window keys, handled by the platform layer and never passed to the game:

| Key | Effect |
|---|---|
| `F11`, `Alt+Enter`; macOS also `Cmd+Ctrl+F` | Toggle desktop fullscreen (the 320×240 image keeps its 4:3 shape with black bars) |
| Close button, `Cmd+Q` (macOS), `Alt+F4` (Windows) | Quit at once, from any screen |

## Per screen

### Main menu

| Keys | Effect |
|---|---|
| `W`/`S` or Up/Down | Move the highlight (wraps around) |
| `Enter` or `Space` | Activate the highlighted item: New Game, Continue (only with saved progress), Level Select, Quit |
| `Esc` | Quit the game |

### Level select

| Keys | Effect |
|---|---|
| `W`/`S` or Up/Down | Move the highlight |
| `Enter` or `Space` | Start the highlighted level if it is unlocked (locked levels are dimmed and marked `LOCKED`) |
| `Esc` | Back to the main menu |

### Play

| Keys | Effect |
|---|---|
| `W`/`S`, Up/Down | Move forward / back |
| `A`/`D` | Strafe left / right |
| Left/Right, `Q` | Turn |
| `Space`, `F`, `Ctrl` | Fire |
| `E` | Open/close a door within reach; use an exit switch within 1.1 cells that you face |
| `P` or `Esc` | Pause |
| `R` | Restart the level (with the health and ammo you entered it with) |
| `M` or `Tab` | Minimap on/off (off by default) |
| `F3` | Debug overlay on/off (frame rate, position, angle, cell) |

Pickups and checkpoint triggers work by walking over them. The game also pauses by itself when
its window loses focus.

### Pause

| Keys | Effect |
|---|---|
| `Esc`, `P` or `Enter` | Resume |
| `R` | Restart the level (level-entry health and ammo) |
| `Q` | Quit to the main menu |

### Level complete

| Keys | Effect |
|---|---|
| `Enter` | Next level (health and ammo carry over), or the credits after the last level |
| `Esc` | Main menu (progress is already saved) |

`Space` does not continue here, so a shot fired just as the level ends cannot skip the screen.

### Game over

| Keys | Effect |
|---|---|
| `R` or `Enter` | Restart the level (level-entry health and ammo) |
| `Esc` | Main menu |

### Campaign complete (credits)

| Keys | Effect |
|---|---|
| `Enter` or `Esc` | Main menu |

## Known issue B13: `Cmd+Ctrl+F` and the fire key (fixed)

`Ctrl` is a fire key, and `Cmd+Ctrl+F` is the macOS fullscreen shortcut. Since Phase 5:

- `Ctrl` pressed while `Cmd` (or the Windows key) is already held does not fire;
- pressing `Cmd` while `Ctrl` is held, and the chord itself, cancel a fire press made in the
  same frame (`InputState::CancelAction`).

If you press `Ctrl` first and wait before adding `Cmd`, that `Ctrl` press is an ordinary shot:
the game cannot know a chord is coming. Press `Cmd` first, or use `F11`, to avoid it.
