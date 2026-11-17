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
SpriteRole KeySpriteRole(KeyColor color) {
    switch (color) {
        case KeyColor::Blue:
            return SpriteRole::KeyBlue;
        case KeyColor::Gold:
            return SpriteRole::KeyGold;
        case KeyColor::Red:
        case KeyColor::None:
            break;
    }
    return SpriteRole::KeyRed;
}

// The sprite slot an entity is drawn with; false for types without a sprite. Enemies change to
// their type's corpse sprite when they die; an enemy without a type gets slot -1 (the missing
// texture).
bool SpriteForEntity(const Entity& entity, const EnemyDefinitions& enemies, SpriteType& outType, int& outSlot) {
    switch (entity.type) {
        case EntityType::Enemy: {
            const bool dead = IsEnemyCorpse(entity);
            outType = dead ? SpriteType::Corpse : SpriteType::Enemy;
            const EnemyType* type = enemies.At(entity.enemyType);
            if (type == nullptr) {
                outSlot = -1;
            } else {
                outSlot = dead ? type->corpseSpriteSlot : type->spriteSlot;
            }
            return true;
        }
        case EntityType::Pickup:
            outType = entity.pickupType == PickupType::Key ? SpriteType::Key : SpriteType::Pickup;
            switch (entity.pickupType) {
                case PickupType::Ammo:
                    outSlot = SpriteRoleSlot(SpriteRole::Ammo);
                    break;
                case PickupType::Key:
                    outSlot = SpriteRoleSlot(KeySpriteRole(entity.keyColor));
                    break;
                case PickupType::Health:
                    outSlot = SpriteRoleSlot(SpriteRole::Health);
                    break;
            }
            return true;
        case EntityType::Decoration:
            outType = SpriteType::Decoration;
            outSlot = SpriteRoleSlot(SpriteRole::Decoration);
            return true;
        default:
            return false;
    }
}

// Visible: alive/uncollected entities, and corpses (inactive but still drawn).
bool IsSpriteShown(const Entity& entity) {
    return entity.visible && (entity.active || IsEnemyCorpse(entity));
}

void AddSprite(const Entity& entity, const EnemyDefinitions& enemies, std::vector<SpriteEntity>& sprites) {
    SpriteType type = SpriteType::Decoration;
    int slot = SpriteRoleSlot(SpriteRole::Decoration);
    if (SpriteForEntity(entity, enemies, type, slot)) {
        sprites.push_back(SpriteEntity{entity.position, slot, entity.id, type, entity.active || IsEnemyCorpse(entity),
                                       IsSpriteShown(entity), entity.hitFlashTimer > 0.0f});
    }
}

std::string DescribeSource(const EntityPlacement& placement) {
    return (placement.source.empty() ? std::string("entity") : placement.source) + " (" +
           PlacementTypeName(placement.type) + ")";
}

bool IsInside(const Map& map, float x, float y) {
    return x >= 0.0f && y >= 0.0f && x < static_cast<float>(map.Width()) && y < static_cast<float>(map.Height());
}

// "guard, brute" style list of the type ids, or "no types".
std::string DescribeEnemyTypes(const EnemyDefinitions& enemies) {
    if (enemies.types.empty()) {
        return "no types";
    }
    std::string text;
    for (const EnemyType& type : enemies.types) {
        text += (text.empty() ? "" : ", ") + type.id;
    }
    return text;
}

// The enemy type a placement selects: its "variant", else the default type; -1 if neither exists.
int EnemyTypeFor(const EntityPlacement& placement, const EnemyDefinitions& enemies) {
    return placement.variant.empty() ? enemies.defaultType : enemies.Find(placement.variant);
}

void ValidatePlacements(const Map& map, const std::vector<EntityPlacement>& placements, const EnemyDefinitions& enemies,
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
        if (placement.type == PlacementType::Enemy && EnemyTypeFor(placement, enemies) < 0) {
            if (!placement.variant.empty()) {
                errors.push_back(DescribeSource(placement) + ": unknown enemy type \"" + placement.variant +
                                 "\" (gameplay.json \"enemies\" defines " + DescribeEnemyTypes(enemies) + ")");
            } else {
                errors.push_back(DescribeSource(placement) +
                                 ": no \"variant\", and gameplay.json has no default enemy type (set \"defaultEnemy\", "
                                 "or give the placement a variant: " +
                                 DescribeEnemyTypes(enemies) + ")");
            }
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
    ValidatePlacements(map, placements, gameplay.enemies, errors);
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
                ApplyEnemyArchetype(entity, EnemyTypeFor(placement, gameplay.enemies), gameplay.enemies);
                AddSprite(entity, gameplay.enemies, level.sprites);
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
                AddSprite(entity, gameplay.enemies, level.sprites);
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
                AddSprite(entity, gameplay.enemies, level.sprites);
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

void SyncSpritesWithEntities(const EntityManager& entities, const EnemyDefinitions& enemies,
                             std::vector<SpriteEntity>& sprites) {
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
        int slot = sprite.textureId;
        if (SpriteForEntity(entity, enemies, type, slot)) {
            sprite.type = type;  // e.g. an enemy that died becomes a corpse
            sprite.textureId = slot;
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
            AddSprite(entity, enemies, sprites);
        }
    }
}

void ApplyEnemyArchetype(Entity& enemy, int typeIndex, const EnemyDefinitions& enemies) {
    const EnemyType* type = enemies.At(typeIndex);
    const EnemyDefinition& stats = type != nullptr ? type->stats : kDefaultEnemyDefinition;
    enemy.enemyType = type != nullptr ? typeIndex : -1;
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
