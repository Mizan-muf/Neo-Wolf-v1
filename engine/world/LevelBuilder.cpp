#include "engine/world/LevelBuilder.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "engine/core/MathUtils.h"

namespace {
// Sprite texture slots, as resolved by SpriteRenderer (0 enemy, 1 pickup, 2 decoration).
constexpr int kEnemySpriteTexture = 0;
constexpr int kPickupSpriteTexture = 1;
constexpr int kDecorationSpriteTexture = 2;

// The sprite an entity type is drawn with; false for types without a sprite.
bool SpriteForEntityType(EntityType type, SpriteType& outType, int& outTexture) {
    switch (type) {
    case EntityType::Enemy:
        outType = SpriteType::Enemy;
        outTexture = kEnemySpriteTexture;
        return true;
    case EntityType::Pickup:
        outType = SpriteType::Pickup;
        outTexture = kPickupSpriteTexture;
        return true;
    case EntityType::Decoration:
        outType = SpriteType::Decoration;
        outTexture = kDecorationSpriteTexture;
        return true;
    default:
        return false;
    }
}

void AddSprite(const Entity& entity, std::vector<SpriteEntity>& sprites) {
    SpriteType type = SpriteType::Decoration;
    int texture = 0;
    if (SpriteForEntityType(entity.type, type, texture)) {
        const bool shown = entity.active && entity.visible;
        sprites.push_back(SpriteEntity{entity.position, texture, entity.id, type, entity.active, shown});
    }
}

std::string DescribeSource(const EntityPlacement& placement) {
    return (placement.source.empty() ? std::string("entity") : placement.source) + " (" +
           PlacementTypeName(placement.type) + ")";
}

bool IsInside(const Map& map, float x, float y) {
    return x >= 0.0f && y >= 0.0f && x < static_cast<float>(map.Width()) && y < static_cast<float>(map.Height());
}

void ValidatePlacements(const Map& map, const std::vector<EntityPlacement>& placements,
                        std::vector<std::string>& errors) {
    const std::string mapSize = std::to_string(map.Width()) + "x" + std::to_string(map.Height());
    std::vector<const EntityPlacement*> players;
    for (const EntityPlacement& placement : placements) {
        if (placement.type == PlacementType::Player) {
            players.push_back(&placement);
        }
        if (!IsInside(map, placement.position.x, placement.position.y)) {
            errors.push_back(DescribeSource(placement) + ": position (" + std::to_string(placement.position.x) + ", " +
                             std::to_string(placement.position.y) + ") is outside the " + mapSize + " map");
        }
        const bool isCellEntity = placement.type == PlacementType::Door || placement.type == PlacementType::Trigger;
        if (isCellEntity && !map.IsInsideMap(placement.mapX, placement.mapY)) {
            errors.push_back(DescribeSource(placement) + ": map cell (" + std::to_string(placement.mapX) + ", " +
                             std::to_string(placement.mapY) + ") is outside the " + mapSize + " map");
        }
    }
    if (players.empty()) {
        errors.push_back("the level has no player placement (exactly one \"player\" entity is required)");
    } else if (players.size() > 1) {
        errors.push_back("the level has " + std::to_string(players.size()) +
                         " player placements; exactly one is required (second: " + DescribeSource(*players[1]) + ")");
    }
}
} // namespace

bool BuildLevelFromPlacements(
    Map& map,
    const std::vector<EntityPlacement>& placements,
    const GameplayDefinitions& gameplay,
    LevelContents& out,
    std::vector<std::string>& errors) {
    const std::size_t errorsBefore = errors.size();
    ValidatePlacements(map, placements, errors);
    if (errors.size() != errorsBefore) {
        return false;
    }

    LevelContents level{};
    level.player.moveSpeed = gameplay.player.moveSpeed;
    level.player.turnSpeedRadians = gameplay.player.turnSpeed;
    level.player.collisionRadius = gameplay.player.collisionRadius;

    for (const EntityPlacement& placement : placements) {
        switch (placement.type) {
        case PlacementType::Player: {
            level.player.position = placement.position;
            level.player.directionAngleRadians = NormalizeAngle(DegreesToRadians(placement.angleDegrees));
            Entity& entity = level.entities.CreateEntity(EntityType::Player, placement.position);
            entity.visible = false;  // the camera
            break;
        }
        case PlacementType::Enemy: {
            // placement.variant selects an archetype from Phase 6 on; today every enemy uses
            // the default "enemy" definition.
            const EnemyDefinition& stats = gameplay.enemy;
            Entity& entity = level.entities.CreateEntity(EntityType::Enemy, placement.position);
            entity.health = stats.health;
            entity.maxHealth = stats.health;
            entity.damage = stats.damage;
            entity.moveSpeed = stats.moveSpeed;
            entity.alertRadius = stats.alertRadius;
            entity.attackRadius = stats.attackRadius;
            entity.attackRate = stats.attackRate;
            AddSprite(entity, level.sprites);
            break;
        }
        case PlacementType::Pickup: {
            Entity& entity = level.entities.CreateEntity(EntityType::Pickup, placement.position);
            entity.pickupType = placement.pickupKind;
            const PickupDefinition& definition = placement.pickupKind == PickupType::Ammo ? gameplay.pickups.ammo
                                                                                         : gameplay.pickups.health;
            entity.pickupValue = placement.pickupValue.value_or(definition.value);
            AddSprite(entity, level.sprites);
            break;
        }
        case PlacementType::Door: {
            Entity& entity = level.entities.CreateEntity(EntityType::Door, placement.position);
            entity.mapX = placement.mapX;
            entity.mapY = placement.mapY;
            entity.doorOpen = false;
            break;
        }
        case PlacementType::Trigger: {
            Entity& entity = level.entities.CreateEntity(EntityType::Trigger, placement.position);
            entity.mapX = placement.mapX;
            entity.mapY = placement.mapY;
            entity.triggerActivated = false;
            break;
        }
        case PlacementType::Decoration: {
            const Entity& entity = level.entities.CreateEntity(EntityType::Decoration, placement.position);
            AddSprite(entity, level.sprites);
            break;
        }
        }
    }

    // Validation passed: now the map cells can change.
    for (const Entity& entity : level.entities.Entities()) {
        if (entity.type == EntityType::Door) {
            map.SetCell(entity.mapX, entity.mapY, TileType::Door);
        } else if (entity.type == EntityType::Trigger) {
            map.SetCell(entity.mapX, entity.mapY, TileType::Trigger);
        }
    }
    out = std::move(level);
    return true;
}

void SyncSpritesWithEntities(const EntityManager& entities, std::vector<SpriteEntity>& sprites) {
    std::unordered_map<int, const Entity*> byId;
    byId.reserve(entities.Entities().size());
    for (const Entity& entity : entities.Entities()) {
        byId.emplace(entity.id, &entity);
    }

    std::unordered_set<int> withSprite;
    std::size_t kept = 0;
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        SpriteEntity sprite = sprites[i];
        const auto it = byId.find(sprite.entityId);
        if (it == byId.end()) {
            continue;  // entity destroyed: drop its sprite
        }
        const Entity& entity = *it->second;
        sprite.position = entity.position;
        sprite.active = entity.active;
        sprite.visible = entity.active && entity.visible;
        withSprite.insert(sprite.entityId);
        sprites[kept++] = sprite;
    }
    sprites.resize(kept);

    for (const Entity& entity : entities.Entities()) {
        if (withSprite.count(entity.id) == 0) {
            AddSprite(entity, sprites);
        }
    }
}

std::string DescribeLevelContents(const LevelContents& contents) {
    static constexpr struct {
        EntityType type;
        const char* name;
    } kKinds[] = {
        {EntityType::Player, "player"}, {EntityType::Enemy, "enemy"},     {EntityType::Pickup, "pickup"},
        {EntityType::Door, "door"},     {EntityType::Trigger, "trigger"}, {EntityType::Decoration, "decoration"},
    };
    std::string text = std::to_string(contents.entities.Entities().size()) + " entities:";
    bool first = true;
    for (const auto& kind : kKinds) {
        int count = 0;
        for (const Entity& entity : contents.entities.Entities()) {
            count += entity.type == kind.type ? 1 : 0;
        }
        text += (first ? " " : ", ") + std::to_string(count) + " " + kind.name;
        first = false;
    }
    return text;
}
