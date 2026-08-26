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

| Action | Primary Key | Alternate Key | Behavior |
| :--- | :--- | :--- | :--- |
| `MoveForward` | `W` | `Up Arrow` | Moves player forward along camera direction vector |
| `MoveBackward` | `S` | `Down Arrow` | Moves player backward |
| `StrafeLeft` | `A` | — | Strafes player perpendicular to camera direction |
| `StrafeRight` | `D` | — | Strafes player right |
| `TurnLeft` | `Left Arrow` | `Q` | Rotates camera yaw left |
| `TurnRight` | `Right Arrow` | `E` | Rotates camera yaw right |
| `Interact` | `Space` | `F` | Interacts with forward door or trigger |
| `Pause` | `P` | `Escape` | Toggles gameplay simulation pause |
| `ToggleDebugView` | `M` | `F1` | Toggles overhead top-down 2D minimap |

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

### 3. Trigger / Terminal Activation
- When player activates a `Trigger` within reach:
  - Trigger is marked `triggerActivated = true`.
  - Map cell light level is pulsed to maximum brightness `1.0f`.

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
