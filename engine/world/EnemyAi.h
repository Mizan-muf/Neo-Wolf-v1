#pragma once

// Enemy AI (Roadmap Phase 6, known issue C10). State diagram and rules: docs/GAMEPLAY.md
// "Enemy AI".
//
//   Idle --(sees the player within alertRadius, or hears a shot)--> Alert
//   Alert --(after reactionTime; the alert sound plays)--> Chase
//   Chase --(in attackRadius with line of sight)--> Attack --(out of range or no sight)--> Chase
//   Chase --(no sight of the player for kEnemyLoseTargetSeconds)--> Search
//   Search --(sees or hears the player)--> Chase;  Search --(kEnemySearchSeconds)--> Idle
//   any --(health 0)--> Dead (a corpse)
//
// Sight uses the raycaster's DDA: walls and closed doors block it. Sound spreads through open
// cells only (closed doors block it). Chasing enemies walk a grid path (Navigation.h) to the
// player's last known position, steering for cell centres and sliding along walls; they never
// open doors. Living enemies keep apart from each other.

#include <vector>

#include "engine/core/Vec2.h"
#include "engine/world/Entity.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/Map.h"
#include "engine/world/MovementRules.h"
#include "engine/world/Player.h"

inline constexpr float kEnemyRadius = 0.25f;               // map cells (collision with walls and other enemies)
inline constexpr float kEnemyRepathInterval = 0.3f;        // seconds: at most ~3 path searches per second each
inline constexpr float kEnemyLoseTargetSeconds = 3.0f;     // Chase without sight this long -> Search
inline constexpr float kEnemySearchSeconds = 4.0f;         // Search this long without contact -> Idle
inline constexpr float kEnemyHitFlashSeconds = 0.15f;      // the sprite flashes this long after a hit
inline constexpr float kEnemyAttackHysteresis = 0.15f;     // Attack ends beyond attackRadius + this
inline constexpr float kEnemyChaseSightFactor = 2.0f;      // once alerted, sees up to alertRadius x this
inline constexpr float kEnemyStuckSeconds = 0.5f;          // no progress this long -> new path
inline constexpr float kEnemyAlertSoundCooldown = 2.0f;    // the alert sound plays at most this often

// A shot the enemies may hear (one per frame at most: the weapon's cooldown is longer).
struct NoiseEvent {
    bool active = false;
    Vec2 position{};
    float radius = 0.0f;  // path length in cells (weapon.noiseRadius)
};

bool IsEnemyAlive(const Entity& entity);
// A dead enemy: drawn as a corpse, blocks nothing (not doors, not other enemies).
bool IsEnemyCorpse(const Entity& entity);

// One AI step for every living enemy: perception (sight, the optional noise), the state
// machine, movement, attacks (they reduce playerHealth and fill outEvents), then separation.
void UpdateEnemyAi(
    EntityManager& entityManager,
    const Map& map,
    const Player& player,
    int& playerHealth,
    float deltaSeconds,
    GameplayEvents& outEvents,
    const NoiseEvent* noise = nullptr);

// Called when the player's shot hits a living enemy: it flashes and, unless it is already
// fighting, starts chasing the player at once (no reaction delay). Returns true if the enemy
// was not alerted before (the caller plays the alert sound).
bool AlertEnemyByDamage(Entity& enemy, const Vec2& playerPosition);

// Pushes overlapping living enemies apart (each by half the overlap, sliding along walls).
// Part of UpdateEnemyAi; public for tests.
void SeparateEnemies(EntityManager& entityManager, const Map& map);

// Player-vs-enemy bodies (known issue C15), the counterpart of SeparateEnemies: a living enemy
// closer to the player than player.collisionRadius + kEnemyRadius is pushed back out by the
// whole overlap (sliding along walls). The player is never pushed; the player's own movement
// is stopped by living enemies instead (MoveWithSlidingAndBodies, LivingEnemyBodies). Part of
// UpdateEnemyAi; public for tests.
void SeparateEnemiesFromPlayer(EntityManager& entityManager, const Map& map, const Player& player);

// The bodies of every living enemy (radius kEnemyRadius), for the player's movement.
std::vector<BodyObstacle> LivingEnemyBodies(const EntityManager& entityManager);
