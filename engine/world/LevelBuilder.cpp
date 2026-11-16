#include "engine/world/LevelBuilder.h"

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "engine/core/MathUtils.h"
#include "engine/world/EnemyAi.h"

namespace {
SpriteTextureId KeySpriteTexture(KeyColor color) {
    switch (color) {
        case KeyColor::Blue:
            return SpriteTextureId::KeyBlue;
        case KeyColor::Gold:
            return SpriteTextureId::KeyGold;
        case KeyColor::Red:
        case KeyColor::None:
            break;
    }
    return SpriteTextureId::KeyRed;
}

// The sprite an entity is drawn with; false for types without a sprite. Enemies change to
// their corpse texture when they die.
bool SpriteForEntity(const Entity& entity, SpriteType& outType, SpriteTextureId& outTexture) {
    switch (entity.type) {
        case EntityType::Enemy: {
            const bool dead = IsEnemyCorpse(entity);
            outType = dead ? SpriteType::Corpse : SpriteType::Enemy;
            switch (entity.variant) {
                case EnemyVariant::Brute:
                    outTexture = dead ? SpriteTextureId::BruteDead : SpriteTextureId::Brute;
                    break;
                case EnemyVariant::Runner:
                    outTexture = dead ? SpriteTextureId::RunnerDead : SpriteTextureId::Runner;
                    break;
                case EnemyVariant::Guard:
                    outTexture = dead ? SpriteTextureId::GuardDead : SpriteTextureId::Guard;
                    break;
            }
            return true;
        }
        case EntityType::Pickup:
            outType = entity.pickupType == PickupType::Key ? SpriteType::Key : SpriteType::Pickup;
            switch (entity.pickupType) {
                case PickupType::Ammo:
                    outTexture = SpriteTextureId::Ammo;
                    break;
                case PickupType::Key:
                    outTexture = KeySpriteTexture(entity.keyColor);
                    break;
                case PickupType::Health:
                    outTexture = SpriteTextureId::Health;
                    break;
            }
            return true;
        case EntityType::Decoration:
            outType = SpriteType::Decoration;
            outTexture = SpriteTextureId::Decoration;
            return true;
        default:
            return false;
    }
}

// Visible: alive/uncollected entities, and corpses (inactive but still drawn).
bool IsSpriteShown(const Entity& entity) {
    return entity.visible && (entity.active || IsEnemyCorpse(entity));
}

void AddSprite(const Entity& entity, std::vector<SpriteEntity>& sprites) {
    SpriteType type = SpriteType::Decoration;
    SpriteTextureId texture = SpriteTextureId::Decoration;
    if (SpriteForEntity(entity, type, texture)) {
        sprites.push_back(SpriteEntity{entity.position, static_cast<int>(texture), entity.id, type,
                                       entity.active || IsEnemyCorpse(entity), IsSpriteShown(entity),
                                       entity.hitFlashTimer > 0.0f});
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
        errors.emplace_back("the level has no player placement (exactly one \"player\" entity is required)");
    } else if (players.size() > 1) {
        errors.push_back("the level has " + std::to_string(players.size()) +
                         " player placements; exactly one is required (second: " + DescribeSource(*players[1]) + ")");
    }
}

// Every zone must lie fully inside the map. The loader has checked x, y >= 0 and w, h >= 1.
void ValidateLightZones(const Map& map, const std::vector<LightZone>& lights, std::vector<std::string>& errors) {
    for (const LightZone& zone : lights) {
        const long long right = static_cast<long long>(zone.x) + zone.width;
        const long long bottom = static_cast<long long>(zone.y) + zone.height;
        if (zone.x < 0 || zone.y < 0 || zone.width < 1 || zone.height < 1 || right > map.Width() ||
            bottom > map.Height()) {
            errors.push_back((zone.source.empty() ? std::string("light zone") : zone.source) + ": cells (" +
                             std::to_string(zone.x) + ", " + std::to_string(zone.y) + ") to (" +
                             std::to_string(right - 1) + ", " + std::to_string(bottom - 1) +
                             ") are not all inside the " + std::to_string(map.Width()) + "x" +
                             std::to_string(map.Height()) + " map");
        }
    }
}
}  // namespace

bool BuildLevelFromPlacements(Map& map, const std::vector<EntityPlacement>& placements,
                              const GameplayDefinitions& gameplay, LevelContents& out,
                              std::vector<std::string>& errors) {
    return BuildLevelFromPlacements(map, placements, {}, gameplay, out, errors);
}

bool BuildLevelFromPlacements(Map& map, const std::vector<EntityPlacement>& placements,
                              const std::vector<LightZone>& lights, const GameplayDefinitions& gameplay,
                              LevelContents& out, std::vector<std::string>& errors) {
    const std::size_t errorsBefore = errors.size();
    ValidatePlacements(map, placements, errors);
    ValidateLightZones(map, lights, errors);
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
                Entity& entity = level.entities.CreateEntity(EntityType::Enemy, placement.position);
                ApplyEnemyArchetype(entity, placement.variant, gameplay.enemies);
                AddSprite(entity, level.sprites);
                break;
            }
            case PlacementType::Pickup: {
                Entity& entity = level.entities.CreateEntity(EntityType::Pickup, placement.position);
                entity.pickupType = placement.pickupKind;
                if (placement.pickupKind == PickupType::Key) {
                    entity.keyColor = placement.key;
                    entity.pickupValue = 0;
                } else {
                    const PickupDefinition& definition =
                        placement.pickupKind == PickupType::Ammo ? gameplay.pickups.ammo : gameplay.pickups.health;
                    entity.pickupValue = placement.pickupValue.value_or(definition.value);
                }
                AddSprite(entity, level.sprites);
                break;
            }
            case PlacementType::Door: {
                Entity& entity = level.entities.CreateEntity(EntityType::Door, placement.position);
                entity.mapX = placement.mapX;
                entity.mapY = placement.mapY;
                entity.doorOpen = false;
                entity.doorLock = placement.lock;
                break;
            }
            case PlacementType::Trigger: {
                Entity& entity = level.entities.CreateEntity(EntityType::Trigger, placement.position);
                entity.mapX = placement.mapX;
                entity.mapY = placement.mapY;
                entity.triggerActivated = false;
                entity.triggerMessage = placement.message;
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
            map.SetDoorLock(entity.mapX, entity.mapY, entity.doorLock);
        } else if (entity.type == EntityType::Trigger) {
            map.SetCell(entity.mapX, entity.mapY, TileType::Trigger);
        }
    }
    for (const LightZone& zone : lights) {
        for (int cellY = zone.y; cellY < zone.y + zone.height; ++cellY) {
            for (int cellX = zone.x; cellX < zone.x + zone.width; ++cellX) {
                map.SetLightLevel(cellX, cellY, zone.level);
            }
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
        SpriteType type = sprite.type;
        auto texture = static_cast<SpriteTextureId>(sprite.textureId);
        if (SpriteForEntity(entity, type, texture)) {
            sprite.type = type;  // e.g. an enemy that died becomes a corpse
            sprite.textureId = static_cast<int>(texture);
        }
        sprite.position = entity.position;
        sprite.active = entity.active || IsEnemyCorpse(entity);
        sprite.visible = IsSpriteShown(entity);
        sprite.flash = entity.hitFlashTimer > 0.0f;
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

void ApplyEnemyArchetype(Entity& enemy, EnemyVariant variant, const EnemyDefinitions& enemies) {
    const EnemyDefinition& stats = enemies.For(variant);
    enemy.variant = variant;
    enemy.health = stats.health;
    enemy.maxHealth = stats.health;
    enemy.damage = stats.damage;
    enemy.moveSpeed = stats.moveSpeed;
    enemy.alertRadius = stats.alertRadius;
    enemy.attackRadius = stats.attackRadius;
    enemy.attackRate = stats.attackRate;
    enemy.reactionTime = stats.reactionTime;
    enemy.score = stats.score;
}

std::string DescribeLevelContents(const LevelContents& contents) {
    struct Kind {
        EntityType type;
        const char* name;
    };
    static constexpr std::array<Kind, 6> kKinds = {{
        {EntityType::Player, "player"},
        {EntityType::Enemy, "enemy"},
        {EntityType::Pickup, "pickup"},
        {EntityType::Door, "door"},
        {EntityType::Trigger, "trigger"},
        {EntityType::Decoration, "decoration"},
    }};
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
