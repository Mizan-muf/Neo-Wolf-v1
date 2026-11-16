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

void SyncPlayerEntity(EntityManager& entityManager, const Player& player);

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
