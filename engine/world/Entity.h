#pragma once

// The game objects of a level (player, enemies, pickups, doors, triggers, decorations) and the
// manager that owns them and hands out their ids.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/core/Vec2.h"
#include "engine/core/Vec2i.h"
#include "engine/world/GameplayDefinitions.h"
#include "engine/world/KeyColor.h"

enum class EntityType : std::uint8_t {
    Player = 0,
    Enemy,
    Pickup,
    Door,
    Trigger,
    Decoration  // static billboard, no interaction (appended: the C API exposes these values)
};

// Enemy AI states (engine/world/EnemyAi.h, docs/GAMEPLAY.md "Enemy AI"). Values are part of
// the C API: append only.
enum class EnemyState : std::uint8_t {
    Idle = 0,
    Alert,   // noticed the player: waits its reaction time, then chases
    Chase,   // follows a path to the player's last known position
    Attack,  // in attack range with line of sight
    Dead,    // a corpse: drawn on the floor, blocks nothing
    Search   // lost the player: goes to the last known position, then gives up (Idle)
};

enum class PickupType : std::uint8_t {
    Health = 0,
    Ammo,
    Key  // a key card (Entity::keyColor)
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
    int enemyType = -1;  // index into GameplayDefinitions::enemies.types; -1 = no type
    int health = kDefaultEnemyDefinition.health;
    int maxHealth = kDefaultEnemyDefinition.health;
    int damage = kDefaultEnemyDefinition.damage;
    float alertRadius = kDefaultEnemyDefinition.alertRadius;
    float attackRadius = kDefaultEnemyDefinition.attackRadius;
    float attackCooldown = 0.0f;
    float attackRate = kDefaultEnemyDefinition.attackRate;
    float moveSpeed = kDefaultEnemyDefinition.moveSpeed;
    float reactionTime = kDefaultEnemyDefinition.reactionTime;
    int score = kDefaultEnemyDefinition.score;
    EnemyState enemyState = EnemyState::Idle;

    // Enemy AI working state (engine/world/EnemyAi.cpp).
    float stateTimer = 0.0f;     // Alert: reaction time left; Search: time left before giving up
    float lostTimer = 0.0f;      // Chase: seconds without line of sight to the player
    float repathTimer = 0.0f;    // seconds until the path may be recomputed
    float hitFlashTimer = 0.0f;  // > 0: the sprite flashes (just hit)
    float stuckTimer = 0.0f;     // seconds of moving with (almost) no progress
    Vec2 lastKnownPlayerPosition{};
    std::vector<Vec2i> path;  // cells still to walk, nearest first
    Vec2i pathGoal{-1, -1};   // the cell the path leads to

    // Pickup kind and amount (from the entity file and gameplay.json for levels).
    PickupType pickupType = PickupType::Health;
    int pickupValue = kDefaultPickupDefinitions.health.value;
    KeyColor keyColor = KeyColor::None;  // pickup of kind Key: which key
    KeyColor doorLock = KeyColor::None;  // door: the key needed to open it (None = unlocked)
    std::string triggerMessage;          // trigger: the HUD message it shows (empty = "CHECKPOINT REACHED")
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
    std::vector<Entity> entities_;
};
