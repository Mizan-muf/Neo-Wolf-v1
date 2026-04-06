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
