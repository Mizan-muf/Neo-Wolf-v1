#include "engine/core/Engine.h"

#include <cmath>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/core/Paths.h"
#include "engine/world/EntityPlacementLoader.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/MapLoader.h"
#include "engine/world/MovementRules.h"

Engine::~Engine() {
    Shutdown();
}

std::string Engine::ResolveContentPath(const std::string& relativePath) const {
    return JoinContentPath(contentRoot_, relativePath);
}

bool Engine::IsRunning() const {
    return running_;
}

GameState Engine::GetState() const {
    return state_;
}

void Engine::SetState(GameState state) {
    if (state_ != state) {
        state_ = state;
        LogInfo(std::string("GameState changed to: ") + GameStateToString(state));
        InteropEvent evt{};
        evt.type = InteropEventType::StateChanged;
        evt.intArg1 = static_cast<int>(state);
        bridge_.PushEvent(evt);
    }
}

void Engine::ShowMessage(const std::string& message, float duration) {
    hudMessage_ = message;
    hudMessageTimer_ = duration;
}

void Engine::ResetLevel() {
    LogInfo("Resetting level...");
    LoadLevel(currentMapPath_, currentEntitiesPath_);
    state_ = GameState::Gameplay;
}

bool Engine::LoadLevel(const std::string& mapPath, const std::string& entitiesPath) {
    LogInfo("Loading level map: " + mapPath + ", entities: " + entitiesPath);

    std::vector<std::string> errors;
    Map newMap{};
    const std::string resolvedMapPath = ResolveContentPath(mapPath);
    if (!LoadMapFromFile(resolvedMapPath, newMap, errors)) {
        LogError("Failed to load map: " + resolvedMapPath);
        for (const std::string& error : errors) {
            LogError("  " + error);
        }
        return false;
    }

    map_ = std::move(newMap);

    entityManager_.Clear();
    sprites_.clear();
    particleManager_.Clear();
    latestRayHits_.clear();
    latestWallDepthBuffer_.clear();

    player_ = Player{};
    playerHealth_ = 100;
    playerAmmo_ = 24;
    ambientDustSpawnTimer_ = 0.0f;
    enemyAlertCooldown_ = 0.0f;
    playerAttackCooldown_ = 0.0f;
    hudMessage_.clear();
    hudMessageTimer_ = 0.0f;

    std::vector<EntityPlacement> placements;
    errors.clear();
    const std::string resolvedEntitiesPath = ResolveContentPath(entitiesPath);
    bool loadedEntities = false;
    std::error_code existsError;
    if (std::filesystem::exists(PathFromUtf8(resolvedEntitiesPath), existsError)) {
        loadedEntities = LoadEntityPlacementsFromFile(resolvedEntitiesPath, placements, errors);
        if (!loadedEntities) {
            // Known issue C1 (Roadmap Phase 4): the placement loader only reads CSV, so the
            // JSON entity files always end up here.
            LogWarning("Could not read entity placements from " + resolvedEntitiesPath + " (" +
                       std::to_string(errors.size()) + " error(s)" +
                       (errors.empty() ? std::string() : "; first: " + errors.front()) +
                       "); using the built-in layout.");
        }
    } else {
        LogWarning("Entity placement file not found: " + resolvedEntitiesPath + "; using the built-in layout.");
    }

    if (loadedEntities) {
        for (const EntityPlacement& placement : placements) {
            if (placement.type == PlacementType::Player) {
                player_.position = placement.position;
                Entity& playerEntity = entityManager_.CreateEntity(EntityType::Player, placement.position);
                playerEntity.visible = false;
            } else if (placement.type == PlacementType::Enemy) {
                Entity& entity = entityManager_.CreateEntity(EntityType::Enemy, placement.position);
                entity.health = 50;
                entity.maxHealth = 50;
                entity.damage = 10;
                sprites_.push_back(SpriteEntity{entity.position, 0, entity.id, SpriteType::Enemy, true, true});
            } else if (placement.type == PlacementType::Pickup) {
                Entity& entity = entityManager_.CreateEntity(EntityType::Pickup, placement.position);
                entity.pickupType = (entityManager_.Entities().size() % 2 == 0) ? PickupType::Ammo : PickupType::Health;
                sprites_.push_back(SpriteEntity{entity.position, 1, entity.id, SpriteType::Pickup, true, true});
            } else if (placement.type == PlacementType::Door) {
                Entity& entity = entityManager_.CreateEntity(EntityType::Door, placement.position);
                entity.mapX = placement.mapX;
                entity.mapY = placement.mapY;
                entity.doorOpen = false;
                if (entity.mapX >= 0 && entity.mapY >= 0) {
                    map_.SetCell(entity.mapX, entity.mapY, TileType::Door);
                }
            } else if (placement.type == PlacementType::Trigger) {
                Entity& entity = entityManager_.CreateEntity(EntityType::Trigger, placement.position);
                entity.mapX = placement.mapX;
                entity.mapY = placement.mapY;
                entity.triggerActivated = false;
                if (entity.mapX >= 0 && entity.mapY >= 0) {
                    map_.SetCell(entity.mapX, entity.mapY, TileType::Trigger);
                }
            } else if (placement.type == PlacementType::Decoration) {
                Entity& entity = entityManager_.CreateEntity(EntityType::Trigger, placement.position);
                sprites_.push_back(SpriteEntity{entity.position, 2, entity.id, SpriteType::Decoration, true, true});
            }
        }
    } else {
        // Fallback default setup
        player_.position = {2.5f, 2.5f};
        Entity& playerEntity = entityManager_.CreateEntity(EntityType::Player, player_.position);
        playerEntity.visible = false;
        Entity& enemy = entityManager_.CreateEntity(EntityType::Enemy, {6.5f, 2.8f});
        sprites_.push_back(SpriteEntity{enemy.position, 0, enemy.id, SpriteType::Enemy, true, true});
        Entity& pickup = entityManager_.CreateEntity(EntityType::Pickup, {11.8f, 6.6f});
        sprites_.push_back(SpriteEntity{pickup.position, 1, pickup.id, SpriteType::Pickup, true, true});
        Entity& door = entityManager_.CreateEntity(EntityType::Door, {8.5f, 7.5f});
        door.mapX = 8;
        door.mapY = 7;
        map_.SetCell(door.mapX, door.mapY, TileType::Door);
        Entity& trigger = entityManager_.CreateEntity(EntityType::Trigger, {13.5f, 8.5f});
        trigger.mapX = 13;
        trigger.mapY = 8;
        map_.SetCell(trigger.mapX, trigger.mapY, TileType::Trigger);
    }

    currentMapPath_ = mapPath;
    currentEntitiesPath_ = entitiesPath;

    InteropEvent evt{};
    evt.type = InteropEventType::LevelLoaded;
    evt.SetString(mapPath.c_str());
    bridge_.PushEvent(evt);

    LogInfo("Level loaded successfully.");
    return true;
}

void Engine::PlayerAttack() {
    if (playerAttackCooldown_ > 0.0f) {
        return;
    }

    if (playerAmmo_ <= 0) {
        ShowMessage("Out of Ammo!", 1.0f);
        return;
    }

    playerAttackCooldown_ = 0.25f;

    GameplayEvents events{};
    const bool fired = PerformPlayerAttack(entityManager_, map_, player_, playerAmmo_, 35, 12.0f, events);
    if (!fired) {
        return;
    }

    audio_.PlayEffect("weapon_fire");

    // Spawn muzzle flash / spark
    const Vec2 playerDir{std::cos(player_.directionAngleRadians), std::sin(player_.directionAngleRadians)};
    const Vec2 muzzlePos = player_.position + playerDir * 0.4f;
    particleManager_.SpawnHitSpark(muzzlePos);

    InteropEvent atkEvt{};
    atkEvt.type = InteropEventType::PlayerAttacked;
    bridge_.PushEvent(atkEvt);

    if (events.enemyDamaged) {
        ShowMessage("Hit enemy! (-35 HP)", 1.0f);
        particleManager_.SpawnHitSpark(events.hitPosition);

        InteropEvent dmgEvt{};
        dmgEvt.type = InteropEventType::EnemyDamaged;
        dmgEvt.intArg1 = events.damagedEnemyId;
        dmgEvt.intArg2 = events.enemyDamageAmount;
        dmgEvt.floatArg1 = events.hitPosition.x;
        dmgEvt.floatArg2 = events.hitPosition.y;
        bridge_.PushEvent(dmgEvt);

        if (events.enemyDied) {
            ShowMessage("Enemy eliminated!", 2.0f);
            particleManager_.SpawnPickupSparkle(events.hitPosition);

            InteropEvent dieEvt{};
            dieEvt.type = InteropEventType::EnemyDied;
            dieEvt.intArg1 = events.deadEnemyId;
            bridge_.PushEvent(dieEvt);
        }
    } else {
        particleManager_.SpawnHitSpark(events.hitPosition);
    }
}
