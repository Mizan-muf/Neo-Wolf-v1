# C API reference (in-process)

Neo-Wolf has a small C-linkage API for driving the engine from code in the same process:
`engine/interop/EngineInterop.h` (27 functions, implemented in `EngineInterop.cpp`) and the
command and event queue behind it, `engine/interop/CommandBridge.h` (`CommandBridge.cpp`).
This page describes what the code does today, including its gaps.

**Scope for v1** ([ROADMAP.md](ROADMAP.md#decisions), "Out of scope for v1"):

- **No shared library is built.** The functions are compiled into the static library
  `neowolf_engine` (`CMakeLists.txt`) and are only reachable by code linked into the same
  executable. They carry no `dllexport` or visibility attributes, and no `.dll`, `.dylib` or
  `.so` is produced or shipped. Nothing in the shipped game calls them; the only caller is the
  native test `engine_interop_c_api` (`tests/LogicTests.cpp`).
- **C# (or any other managed layer) is out of scope for v1.** There are no bindings, no
  P/Invoke declarations and no managed code in the repository. The original checklist's C#
  phases are annotated in [Checklist.md](Checklist.md#phase-20--c-interop-layer--command-bridge).
- **No API version.** There is no version number, version function or version macro for this
  API. The only versioning rule is in the code comments: the enum values below are "append
  only, never renumber". The game's own version (`project(NeoWolf VERSION ...)` in
  `CMakeLists.txt`, [RELEASE.md](RELEASE.md#versioning)) is not exposed through the API.

## Using the header from C and from C++

- **From C++** the header includes `CommandBridge.h`, so `InteropCommand`, `InteropEvent` and
  their enums are complete types. This is how the test uses it.
- **From C** the header declares `InteropCommand` and `InteropEvent` only as incomplete
  (opaque) struct types. A C program can call every function, but it **cannot create an
  `InteropCommand` or read an `InteropEvent`**, so `nw_engine_send_command` and
  `nw_engine_poll_event` are unusable from C as the header stands. (Checked by compiling a C99
  client: declaring an `InteropEvent` variable fails with "incomplete type".)
- `nw_engine_create()` is declared with an empty parameter list, which C treats as "no
  prototype" (clang warns with `-Wstrict-prototypes`); it takes no arguments.
- Every function returns or takes plain `int`, `float`, `double`, `uint32_t`, `const char*`
  and `void*` values. Booleans are `int`: 1 for success or true, 0 for failure or false.

## Handles, ownership and lifetime

- The handle is an opaque `void*` to a C++ `Engine` object. `nw_engine_create` allocates it;
  the caller owns it and must release it with `nw_engine_destroy` (which also shuts the engine
  down). Do not use a handle after destroying it.
- Every function accepts a null handle and then does nothing and returns its failure value
  (0, or -1 for `nw_engine_spawn_entity`).
- Strings passed in (`soundId`, `message`, `mapPath`, `entitiesPath`) are copied or used
  during the call only; the caller keeps ownership. Commands are copied when queued; events
  are copied into the caller's `InteropEvent`. The API never returns pointers into the
  engine.
- Out-parameters may be null; a null out-parameter is skipped.

## Threading

- The command queue and the event queue share one `std::mutex` (`CommandBridge`), so `nw_engine_send_command`, `nw_engine_poll_event`,
  `nw_engine_has_pending_events` and `nw_engine_clear_bridge` may be called from another
  thread while the engine runs.
- **Every other function reads or writes engine state without locking.** Call them on the
  thread that calls `nw_engine_update` and `nw_engine_render`. Because the engine uses SDL for
  its window and events, that should be the thread that called `nw_engine_init` (on macOS,
  the main thread). Thread use beyond the test above has not been exercised.
- Queued commands are executed at the **start of the next `nw_engine_update`**, on the
  updating thread, in the order they were sent.

## Lifecycle

```c
void* engine = nw_engine_create();
if (nw_engine_init(engine)) {               /* content root, gameplay.json, campaign, window */
    while (nw_engine_is_running(engine)) {
        nw_engine_update(engine, 1.0 / 60.0);  /* commands, input, simulation */
        nw_engine_render(engine);              /* draw and present */
        /* poll events here (C++ only, see above) */
    }
}
nw_engine_destroy(engine);                  /* shuts down if needed, frees the handle */
```

Before a successful `nw_engine_init`, `nw_engine_update` and `nw_engine_render` do nothing (the
engine is not running), so queued commands wait until the first update after `init`. The
getters and setters work on the default-constructed state before `init` (the test
`engine_interop_c_api` uses them that way).

## Functions

### Lifecycle

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `void* nw_engine_create()` | none | a new engine handle (never null in practice: `new Engine()`) | Nothing is loaded and no window is opened. |
| `void nw_engine_destroy(void* handle)` | handle (may be null) | none | Deletes the engine; its destructor calls `Shutdown`. |
| `int nw_engine_init(void* handle)` | handle | 1 on success, 0 on failure | `Engine::Init` with default `EngineOptions`: automatic content-root search (`NEOWOLF_DATA_DIR` if set, and then it is the only place searched; otherwise the executable's folder and up to 3 parents, `<exe>/../Resources`, the working directory and its parent; [ARCHITECTURE.md](ARCHITECTURE.md#content-loading-and-the-content-root)), `game/defs/gameplay.json` and `campaign.json` (invalid files fail), **opens a window and the audio device** (there is no headless option in the C API; use `SDL_VIDEODRIVER=dummy` for an invisible run), main menu with level 1 loaded behind it, **error message boxes enabled**, and no per-user folder, so `progress.json` is neither read nor written. Afterwards the state is `Menu` (0). On failure the reason is logged. Calling it a second time (or again after `shutdown`) has not been tested. |
| `void nw_engine_update(void* handle, double deltaSeconds)` | handle; frame time in seconds | none | One frame: runs the queued commands, reads window events, then menus or gameplay. `deltaSeconds` is used as given (the game's own loop clamps it to 0.1 s; this function does not). No-op unless running. |
| `void nw_engine_render(void* handle)` | handle | none | Draws the 320×240 frame and presents it in the window. No-op unless running. |
| `void nw_engine_shutdown(void* handle)` | handle | none | Closes audio and the window and resets the engine. Safe to call more than once. Does not clear the command or event queues. |
| `int nw_engine_is_running(const void* handle)` | handle | 1 while running, else 0 | Running starts with a successful `init` and ends with `shutdown`, closing the window, or Quit / Esc on the main menu. |

### Game state

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `int nw_engine_get_state(const void* handle)` | handle | a [`GameState`](#gamestate) value; 0 for a null handle | |
| `void nw_engine_set_state(void* handle, int state)` | handle; a `GameState` value | none | Values outside 0–6 are ignored. 4 (`Victory`, deprecated) selects 5 (`LevelComplete`). 0 (`Menu`) returns to the main page of the menu. Forces the state (`GameFlow::ForceState`) without loading anything, and pushes a `StateChanged` event if the state changed. |
| `void nw_engine_reset_level(void* handle)` | handle | none | Restarts the current level from its files: a campaign level with its level-entry health and ammo, a level loaded by path with the `gameplay.json` initial values. The state becomes `Gameplay`. If the reload fails, the current level state is kept (logged). |
| `int nw_engine_load_level(void* handle, const char* mapPath, const char* entitiesPath)` | handle; map and entity file paths, **relative to the content root** (before `init`: relative to the working directory) | 1 if the level was loaded, 0 on any error or a null argument | Loads and builds the level with the current gameplay values; on failure the current level stays (errors logged). Health and ammo are set to the `gameplay.json` initial values and keys are cleared. The engine treats it as "loaded by path" (`LevelCompleted` reports level 0; `nw_engine_reset_level` reloads these files), but the game flow keeps its own campaign position: after a level was started through the menus, Enter on the level-complete screen loads the campaign level after *that* one; if no campaign level was started yet (straight after `nw_engine_init`), Enter goes to `CampaignComplete` (credits). The state is **not** changed. Pushes `LevelLoaded`. |

### Player

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `void nw_engine_get_player_pos(const void* handle, float* outX, float* outY, float* outAngle)` | handle; out: position in map cells and view angle in radians | none | Angle 0 looks along +x (east), π/2 along +y (south): [ARCHITECTURE.md](ARCHITECTURE.md#coordinates). |
| `void nw_engine_set_player_pos(void* handle, float x, float y, float angle)` | handle; position in cells; angle in radians | none | Sets the camera directly: **no collision check** (a position inside a wall is accepted). |
| `void nw_engine_get_player_stats(const void* handle, int* outHealth, int* outAmmo)` | handle; out: health, ammo | none | |
| `void nw_engine_set_player_stats(void* handle, int health, int ammo)` | handle; health, ammo | none | **Not clamped or validated**: negative values and values above the maxima are stored as given. Health 0 does not end the level by itself. |
| `void nw_engine_player_attack(void* handle)` | handle | none | Fires the weapon as if the fire key was pressed: respects the fire-rate cooldown, uses one ammo, may hit the nearest enemy in line, plays the sound and pushes `PlayerAttacked` (and `EnemyDamaged` / `EnemyDied`). Works in any state. With no ammo it only shows the message "Out of Ammo!" (the HUD draws messages in capitals). |

### Entities

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `int nw_engine_get_entity_count(const void* handle)` | handle | number of entities in the current level (including the player entity and corpses); 0 for a null handle | |
| `int nw_engine_get_entity(const void* handle, int index, int* outId, int* outType, float* outX, float* outY, int* outActive)` | handle; **index** 0 … count−1 (not an id); out: id, [`EntityType`](#entitytype), position, active flag | 1 if `index` is valid, else 0 | `outActive` is 0 for collected pickups and dead enemies (a corpse stays in the list and stays visible). Ids are unique within a loaded level; a level load or restart builds a new entity list. |
| `int nw_engine_spawn_entity(void* handle, int type, float x, float y)` | handle; `EntityType` value; position in cells | the new entity's id; -1 for a null handle | **`type` is not validated** (any value creates an entity). A spawned entity has the built-in defaults of `Entity`: an enemy is a guard with the compiled-in default stats (not the `gameplay.json` values), a pickup is a health pickup; it gets a sprite on the next gameplay update if its type has one. Door and trigger entities created this way have no map cell. |
| `int nw_engine_destroy_entity(void* handle, int id)` | handle; entity **id** | 1 if an entity with that id was removed, else 0 | Nothing stops removing the player entity. Its sprite is dropped on the next gameplay update. |

### Audio, effects and messages

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `void nw_engine_play_sound(void* handle, const char* soundId)` | handle; one of `door_open`, `pickup`, `enemy_alert`, `weapon_fire` (`game/defs/audio.cfg`) | none | Unknown ids, a null string, or no audio device: nothing happens. Sounds are queued on the device one after another (`SDL_QueueAudio`), not mixed. |
| `void nw_engine_spawn_particle(void* handle, float x, float y, float vx, float vy, float life, uint32_t color)` | handle; position and velocity in cells and cells/s; lifetime in seconds; colour `0xAARRGGBB` | none | `life` ≤ 0 becomes 0.5 s; colour 0 becomes `0xFFFFC266`; size 0.06 cells. At most 512 particles exist at once. Particles only move during gameplay updates. |
| `void nw_engine_show_message(void* handle, const char* message, float duration)` | handle; text; seconds | none | Replaces the HUD message (shown during gameplay only). A null message is ignored. A `duration` ≤ 0 keeps the message until another message replaces it or a level loads (unlike the `ShowMessage` command, which turns ≤ 0 into 2 s). |

### Command bridge

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `int nw_engine_send_command(void* handle, const InteropCommand* cmd)` | handle; a [command](#commands) | 1 if queued, 0 for a null handle or command | Thread-safe. Executed at the start of the next `nw_engine_update` (only while running). |
| `int nw_engine_poll_event(void* handle, InteropEvent* outEvt)` | handle; out: the oldest [event](#events) | 1 if an event was copied out, 0 if the queue is empty (or a null argument) | Thread-safe. First in, first out. |
| `int nw_engine_has_pending_events(const void* handle)` | handle | 1 if at least one event is queued, else 0 | Thread-safe. |
| `void nw_engine_clear_bridge(void* handle)` | handle | none | Thread-safe. Drops every queued command **and** event. |

The event queue has **no size limit**: a caller that never polls lets it grow for as long as
the engine runs.

## Enums

The values are part of the API. The code comments require them to be append-only.

### GameState

`engine/core/GameState.h`. Used by `nw_engine_get_state`, `nw_engine_set_state`, the
`SetGameState` command and the `StateChanged` event.

| Value | Name | Meaning |
|---|---|---|
| 0 | `Menu` | Main menu or level select |
| 1 | `Gameplay` | Playing a level |
| 2 | `Paused` | Pause screen |
| 3 | `GameOver` | The player died |
| 4 | `Victory` | **Deprecated.** Never entered; setting 4 selects 5 instead. `GameStateToString` still returns `"Victory"`. |
| 5 | `LevelComplete` | The exit switch was used |
| 6 | `CampaignComplete` | Credits after the last level |

What each state does, and the transitions the game makes by itself, is in
[GAMEPLAY.md](GAMEPLAY.md#states-and-transitions).

### EntityType

`engine/world/Entity.h`. Reported by `nw_engine_get_entity`, taken by
`nw_engine_spawn_entity` and the `SpawnEntity` command.

| Value | Name |
|---|---|
| 0 | `Player` |
| 1 | `Enemy` |
| 2 | `Pickup` |
| 3 | `Door` |
| 4 | `Trigger` |
| 5 | `Decoration` |

### PickupType

`engine/world/Entity.h`. The `PickupCollected` event's `intArg1`.

| Value | Name |
|---|---|
| 0 | `Health` |
| 1 | `Ammo` |
| 2 | `Key` (a key card; the colour is not in the event) |

`EnemyState` (`Idle` 0, `Alert` 1, `Chase` 2, `Attack` 3, `Dead` 4, `Search` 5) is declared
append-only "for the C API" in `Entity.h`, but no function or event exposes it.

### Commands

`InteropCommandType` (`int32_t`) in `CommandBridge.h`. An `InteropCommand` is a plain struct
(152 bytes on the tested compilers, standard layout): `type`, `intArg1`, `intArg2`,
`floatArg1`, `floatArg2`, `floatArg3`, and `char strArg[128]` (set it with `SetString`, which
truncates to 127 bytes and always NUL-terminates). Fields a command does not use are ignored.
Commands do **not** push events of their own, except where noted.

| Value | Command | Arguments | Effect |
|---|---|---|---|
| 0 | `None` | | Ignored. |
| 1 | `SetGameState` | `intArg1` = `GameState` | As `nw_engine_set_state` (0–6; 4 means 5; others ignored). |
| 2 | `ResetLevel` | | As `nw_engine_reset_level`. |
| 3 | `LoadLevel` | `strArg` = `"<map>;<entities>"` or `"<map>,<entities>"`, or only `"<map>"` | As `nw_engine_load_level` (content-relative paths; the result is not reported, only logged). With only a map path the entity file is derived: for `dir/name.map` it is `game/defs/name.entities.json` (the directory is not used); for `name.map` without a slash it is `name.entities.json`. Both paths together must fit in 127 bytes. |
| 4 | `SpawnEntity` | `intArg1` = `EntityType`, `floatArg1`/`floatArg2` = x, y | As `nw_engine_spawn_entity`, but the new id is not reported. |
| 5 | `DestroyEntity` | `intArg1` = entity id | As `nw_engine_destroy_entity`. |
| 6 | `DamagePlayer` | `intArg1` = amount | Health −amount, not below 0. The sign is not checked: a negative amount adds health with no cap. At 0 the state is forced to `GameOver` (pushes `StateChanged`, no `PlayerDied`), in any state, including `Menu`. |
| 7 | `HealPlayer` | `intArg1` = amount | Health +amount, capped at the maximum health. The sign is not checked: a negative amount lowers health, below 0 if large enough. |
| 8 | `AddAmmo` | `intArg1` = amount | Ammo +amount, capped at the maximum ammo. The sign is not checked. |
| 9 | `PlayerAttack` | | As `nw_engine_player_attack` (pushes the attack events). |
| 10 | `DamageEnemy` | `intArg1` = enemy id, `intArg2` = damage | For a living enemy: health −damage and the hit flash; at 0 or below it dies and leaves a corpse. Does not alert the enemy. Ignored for other ids. |
| 11 | `PlaySound` | `strArg` = sound id | As `nw_engine_play_sound`. |
| 12 | `SpawnParticle` | `floatArg1`/`floatArg2` = x, y | Spawns the built-in **hit spark** burst at that point (unlike `nw_engine_spawn_particle`, no velocity, lifetime or colour). |
| 13 | `OpenDoor` | `intArg1`/`intArg2` = cell x, y | Sets that map cell to empty. **The cell type is not checked** (it removes a wall just as well), and the door entity's open/closed state is not updated, so the next `E` press on that door may disagree with the map. |
| 14 | `CloseDoor` | `intArg1`/`intArg2` = cell x, y | Sets that map cell to a door tile, with the same caveats (any cell; no check that the doorway is empty; the door entity is not updated). |
| 15 | `ShowMessage` | `strArg` = text, `floatArg1` = seconds (≤ 0 means 2 s) | As `nw_engine_show_message`. |

Unknown command values are ignored.

### Events

`InteropEventType` (`int32_t`) in `CommandBridge.h`. An `InteropEvent` has the same layout as
`InteropCommand`. Unused fields are 0 or empty. Events are pushed by the engine itself
(`Engine.Core.cpp`, `Engine.Update.cpp`); gameplay events are pushed during `Gameplay`
updates, except the attack events from `nw_engine_player_attack` / `PlayerAttack`, which work in
any state.

| Value | Event | Pushed when | Arguments |
|---|---|---|---|
| 0 | `None` | never | |
| 1 | `StateChanged` | the state differs from the last reported one: checked after every update (not one that quits, or a frozen preview), on `nw_engine_set_state`/`SetGameState`, `nw_engine_reset_level`/`ResetLevel`, and when the `DamagePlayer` command reaches 0 health. `nw_engine_init` pushes none (the state starts as `Menu`). | `intArg1` = new `GameState` |
| 2 | `PlayerDamaged` | an enemy hit the player | `intArg1` = damage |
| 3 | `PlayerDied` | an enemy hit brought health to 0 | |
| 4 | `PlayerAttacked` | a shot was fired (keyboard, `nw_engine_player_attack` or the command) | |
| 5 | `EnemyAlerted` | the enemy AI raised the alert and the alert sound played (at most once per 2 s, `kEnemyAlertSoundCooldown`; alerts caused by the player's shot *hitting* an enemy do not push it and start the 2 s cooldown; alerts from hearing a shot do push it) | |
| 6 | `EnemyDamaged` | the player's shot hit an enemy | `intArg1` = enemy id, `intArg2` = damage, `floatArg1`/`floatArg2` = hit point |
| 7 | `EnemyDied` | that shot killed it | `intArg1` = enemy id |
| 8 | `PickupCollected` | the player walked over a pickup | `intArg1` = `PickupType`, `intArg2` = the pickup's value (not reduced by the cap; 0 for a key card) |
| 9 | `DoorToggled` | `E` opened or closed a door | `intArg1`/`intArg2` = door cell x, y (whether it opened or closed is not reported; if one press toggles two doors, only one event is pushed) |
| 10 | `TriggerActivated` | the player first came within 0.8 cells of a trigger | `intArg1` = trigger entity id |
| 11 | `LevelLoaded` | any level load succeeded (`nw_engine_init` with level 1 behind the menu, start, next level, restart, `nw_engine_load_level`) | `strArg` = the map path as passed (content-relative) |
| 12 | `LevelCompleted` | the exit switch was used | `intArg1` = campaign level number (1-based; 0 for a level loaded by path), `intArg2` = health, `floatArg1` = seconds played on the level |

## Tests

- `engine_interop_c_api`: create, state get/set (including 4 → 5 and an ignored 99), player
  position and stats, spawn and destroy an entity, read an entity, queue a command, show a
  message, destroy. It does not call `nw_engine_init`, so no queued command is executed.
- `command_bridge_events`: the `CommandBridge` queues on their own (push and poll).

No test runs a command through the engine or polls a gameplay event via the C API. A C program
that calls the API (`nw_engine_init` with `SDL_VIDEODRIVER=dummy` and `NEOWOLF_DATA_DIR` set,
then update, render, shutdown and destroy) was compiled against the header and linked with
`libneowolf_engine.a` on macOS while this page was written; it is not part of the build.
