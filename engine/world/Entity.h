#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/Vec2.h"

enum class EntityType : std::uint8_t {
    Player = 0,
    Enemy,
    Pickup,
    Door,
    Trigger
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

    // Enemy AI & Combat stats
    int health = 50;
    int maxHealth = 50;
    int damage = 10;
    float alertRadius = 6.0f;
    float attackRadius = 1.1f;
    float attackCooldown = 0.0f;
    float attackRate = 1.0f;
    float moveSpeed = 1.6f;
    EnemyState enemyState = EnemyState::Idle;

    // Pickup stats
    PickupType pickupType = PickupType::Health;
    int pickupValue = 25;
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
