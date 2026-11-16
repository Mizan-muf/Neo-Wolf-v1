#pragma once

#include "engine/core/Vec2.h"
#include "engine/world/Entity.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

struct GameplayEvents {
    bool playerDamaged = false;
    int playerDamageAmount = 0;
    bool playerDied = false;

    bool pickupCollected = false;
    PickupType pickupType = PickupType::Health;
    int pickupValue = 0;

    bool doorToggled = false;
    bool doorOpened = false;
    int doorMapX = -1;
    int doorMapY = -1;

    bool triggerActivated = false;
    int triggerId = -1;

    bool exitActivated = false;  // the player used an exit switch: the level is complete
    int exitMapX = -1;
    int exitMapY = -1;

    bool enemyAlerted = false;
    int alertedEnemyId = -1;

    bool enemyDamaged = false;
    int damagedEnemyId = -1;
    int enemyDamageAmount = 0;

    bool enemyDied = false;
    int deadEnemyId = -1;

    bool weaponFired = false;
    bool attackHit = false;
    int hitEnemyId = -1;
    Vec2 hitPosition{};
};

// Exit switch rules ('X' tiles). Interact activates a switch when one of a few rays fanned
// around the view direction (centre and up to kExitFacingHalfAngle either side) hits an Exit
// cell no further than kExitReach from the player. Rays stop at the first solid cell, so a
// switch behind a wall, behind the player or out of reach is never activated.
inline constexpr float kExitReach = 1.1f;              // map cells, player centre to the switch face
inline constexpr float kExitFacingHalfAngle = 0.35f;   // radians (about 20 degrees)

struct ExitSwitchHit {
    bool found = false;
    int mapX = -1;
    int mapY = -1;
    float distance = 0.0f;
};

ExitSwitchHit FindExitSwitchInReach(const Map& map, const Player& player);

void SyncPlayerEntity(EntityManager& entityManager, const Player& player);

// Pickups (walk over), triggers (walk over: one interop event and a HUD message, nothing
// else), doors and the exit switch (both with Interact).
void UpdateEntityInteractions(
    EntityManager& entityManager,
    Map& map,
    const Player& player,
    int& playerHealth,
    int& playerAmmo,
    int maxHealth,
    int maxAmmo,
    bool interactPressed,
    GameplayEvents& outEvents);

void UpdateEnemyAi(
    EntityManager& entityManager,
    const Map& map,
    const Player& player,
    int& playerHealth,
    float deltaSeconds,
    GameplayEvents& outEvents);

bool PerformPlayerAttack(
    EntityManager& entityManager,
    const Map& map,
    const Player& player,
    int& playerAmmo,
    int damage,
    float maxRange,
    GameplayEvents& outEvents);
