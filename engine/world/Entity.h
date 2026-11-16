#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/Vec2.h"
#include "engine/world/GameplayDefinitions.h"

enum class EntityType : std::uint8_t {
    Player = 0,
    Enemy,
    Pickup,
    Door,
    Trigger,
    Decoration  // static billboard, no interaction (appended: the C API exposes these values)
};

enum class EnemyState : std::uint8_t {
    Idle = 0,
    Alert,
    Chase,
    Attack,
    Dead
};

enum class PickupType : std::uint8_t {
    Health = 0,
    Ammo
};

struct Entity {
    int id = -1;
    EntityType type = EntityType::Enemy;
    Vec2 position{};
    bool active = true;
    bool visible = true;

    int mapX = -1;
    int mapY = -1;
    bool doorOpen = false;
    bool triggerActivated = false;
    bool collected = false;

    // Enemy AI & combat stats. Levels set them from gameplay.json (BuildLevelFromPlacements);
    // these fallback defaults only reach entities created another way (C API, tests).
    int health = kDefaultEnemyDefinition.health;
    int maxHealth = kDefaultEnemyDefinition.health;
    int damage = kDefaultEnemyDefinition.damage;
    float alertRadius = kDefaultEnemyDefinition.alertRadius;
    float attackRadius = kDefaultEnemyDefinition.attackRadius;
    float attackCooldown = 0.0f;
    float attackRate = kDefaultEnemyDefinition.attackRate;
    float moveSpeed = kDefaultEnemyDefinition.moveSpeed;
    EnemyState enemyState = EnemyState::Idle;

    // Pickup kind and amount (from the entity file and gameplay.json for levels).
    PickupType pickupType = PickupType::Health;
    int pickupValue = kDefaultPickupDefinitions.health.value;
};

class EntityManager {
public:
    void Clear();
    Entity& CreateEntity(EntityType type, const Vec2& position);
    bool DestroyEntity(int id);

    Entity* GetEntityById(int id);
    const Entity* GetEntityById(int id) const;

    std::vector<Entity>& Entities();
    const std::vector<Entity>& Entities() const;

private:
    int nextId_ = 1;
    std::vector<Entity> entities_{};
};
