# Neo-Wolf Gameplay API & Mechanics Reference

## Overview

Neo-Wolf models its gameplay rules and entity interactions through clean, modular C++ components with a strict separation between core engine systems and gameplay logic.

---

## 1. Player Mechanics (`engine/world/Player.h`)

The `Player` structure maintains the camera and physical avatar state:

```cpp
struct Player {
    Vec2 position{2.5f, 2.5f};            // Continuous world position
    float directionAngleRadians = 0.0f;     // Yaw angle (0 = East, PI/2 = South)
    float moveSpeed = 3.0f;                // Linear movement speed in units/sec
    float turnSpeedRadians = 2.4f;         // Rotational speed in rad/sec
    float fovRadians = 1.04719755f;        // Field of View (~60 degrees)
    float radius = 0.28f;                  // Collision bounding circle radius
};
```

### Controls & Input Actions (`engine/core/InputState.h`)

The key bindings, per screen, are in [CONTROLS.md](CONTROLS.md) (source of truth:
`PlatformSDL::MapKeyToActions` in `engine/platform/PlatformSDL.cpp`). Phase 5 appended the
actions `Select` (Enter, Space), `ToggleMinimap` (M, Tab) and `QuitToMenu` (Q); `ToggleDebugView`
moved from Tab to F3.

---

## 2. Collision & Movement Resolution (`engine/world/MovementRules.h`)

Collision physics uses a circular bounding volume vs grid-aligned axis planes with continuous sliding:

```cpp
void ResolveCollisionAgainstWalls(const Map& map, Player& player, const Vec2& displacement);
```

- When the player moves toward an obstacle, the collision solver separates movement into independent X and Y components.
- If the X displacement collides with a solid cell, X motion is halted while Y sliding motion continues, preventing the player from sticking on corners.
- Collision checks evaluate candidate grid cells surrounding the player's bounding radius `player.radius`.

---

## 3. Entity & World Interactions (`engine/world/InteractionRules.h`)

```cpp
void UpdateEntityInteractions(
    EntityManager& entityManager,
    Map& map,
    const Player& player,
    bool interactTriggered);
```

### 1. Pickup Collection
- When player distance to a `Pickup` entity is `<= (player.radius + pickup.radius)`:
  - Pickup is marked `collected = true`, `active = false`, `visible = false`.
  - Audio event `"pickup"` is triggered.
  - Golden sparkle particles are spawned at the pickup's world position.

### 2. Door Interactions
- When player presses `Interact` while facing a `Door` within reach (`distance <= 1.5f`):
  - Door entity state toggles (`doorOpen = !doorOpen`).
  - The map grid cell is updated to `TileType::Empty` when open or `TileType::Door` when closed.
  - Audio event `"door_open"` is triggered.
  - Hit spark particles are spawned at the door frame.

### 3. Triggers and the exit switch
- Walking within 0.8 cells of a `Trigger` marks it `triggerActivated = true` and sends the
  `TriggerActivated` event once; the HUD shows `CHECKPOINT REACHED`. It does not end the level.
- Pressing Interact while facing an exit switch (`X` tile) within 1.1 cells completes the level
  (`FindExitSwitchInReach`; [GAMEPLAY.md](GAMEPLAY.md#winning-and-losing-a-level)).

### 4. Enemy Detection & Alert
- Proximity detection monitors distance between player and active `Enemy` entities.
- When an enemy is within detection range (`distance <= 3.75f`), the audio system plays `"enemy_alert"` (with a cooldown timer to prevent sound flooding).

---

## 4. Particle Effects (`engine/particles/`)

The particle system simulates dynamic particles with drag and gravity:
- `SpawnHitSpark(position)`: High-velocity orange/yellow sparks when striking solid walls or doors.
- `SpawnPickupSparkle(position)`: Upward drifting golden particles upon collecting items.
- `SpawnAmbientDust(position)`: Gentle floating dust motes adding atmospheric depth.

---

## 5. C API & Managed Boundary Design

Neo-Wolf's architecture is structured to support native or managed gameplay scripting (e.g. C# via C interop):
- Native engine exposes C-compatible functions for state observation (`GetPlayerPosition`, `GetEntityCount`, `GetMapTile`).
- Native engine receives command callbacks (`SetPlayerVelocity`, `TriggerDoor`, `SpawnParticle`).
- Data definition tables (`.json` / `.csv`) allow gameplay scripts to drive level progression and entity stats.

---

## 6. C API: game states and events (Phase 5)

`nw_engine_get_state` / `nw_engine_set_state`, the `StateChanged` event (`intArg1`) and the
`SetGameState` command use these values:

| Value | State | Notes |
|---|---|---|
| 0 | `Menu` | Main menu or level select |
| 1 | `Gameplay` | |
| 2 | `Paused` | |
| 3 | `GameOver` | |
| 4 | `Victory` | **Deprecated.** Kept so existing callers still compile and run: the engine never enters it, and setting 4 (`nw_engine_set_state` or `SetGameState`) selects `LevelComplete` (5) instead. `GameStateToString` still returns `"Victory"` for it. |
| 5 | `LevelComplete` | The exit switch was used; Enter continues |
| 6 | `CampaignComplete` | Credits after the last level |

Values outside 0–6 are ignored by `nw_engine_set_state` and `SetGameState` (range check
`IsValidGameStateValue`, `engine/core/GameState.h`). Before Phase 5, `nw_engine_set_state`
accepted any value.

The state machine itself is `GameFlow` (`engine/core/GameFlow.h`); setting a state from outside
calls `GameFlow::ForceState`. `nw_engine_reset_level` restarts a campaign level with its
level-entry health and ammo, or reloads a level loaded by path with the initial values.

New event (appended, so earlier values are unchanged): `LevelCompleted` (12): `intArg1` = the
campaign level number (1-based; 0 for a level loaded by path), `intArg2` = health,
`floatArg1` = seconds played on the level.

`nw_engine_init` uses the default `EngineOptions`: the main menu, level 1 loaded behind it, and
no per-user folder (progress is neither loaded nor saved).
