#include "engine/core/Engine.h"

#include <cmath>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/assets/GameplayConfig.h"
#include "engine/core/Log.h"
#include "engine/core/Paths.h"
#include "engine/world/EntityPlacementLoader.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/LevelBuilder.h"
#include "engine/world/MapLoader.h"

namespace {
void LogMessages(const std::vector<std::string>& messages, void (*log)(std::string_view)) {
    for (const std::string& message : messages) {
        log("  " + message);
    }
}
} // namespace

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
    if (!LoadLevel(currentMapPath_, currentEntitiesPath_)) {
        LogError("Level reset failed (see above); continuing with the current level state.");
    }
    state_ = GameState::Gameplay;
}

bool Engine::LoadGameplay(const std::string& relativePath, std::vector<std::string>* outErrors) {
    const std::string resolvedPath = ResolveContentPath(relativePath);
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    GameplayDefinitions definitions = gameplay_;
    const bool loaded = LoadGameplayDefinitions(resolvedPath, definitions, errors, warnings);
    if (!warnings.empty()) {
        LogWarning("Gameplay definitions " + resolvedPath + ": " + std::to_string(warnings.size()) + " warning(s):");
        LogMessages(warnings, &LogWarning);
    }
    if (!loaded) {
        LogError("Invalid gameplay definitions " + resolvedPath + ":");
        LogMessages(errors, &LogError);
        if (outErrors != nullptr) {
            *outErrors = errors;
        }
        return false;
    }
    gameplay_ = definitions;
    LogInfo("Gameplay definitions loaded from " + resolvedPath + ": " + DescribeGameplayDefinitions(gameplay_));
    return true;
}

bool Engine::LoadLevel(const std::string& mapPath, const std::string& entitiesPath) {
    LogInfo("Loading level map: " + mapPath + ", entities: " + entitiesPath);

    // Everything is loaded and built into locals first: a level that fails to load leaves
    // the current one untouched.
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    Map newMap{};
    const std::string resolvedMapPath = ResolveContentPath(mapPath);
    if (!LoadMapFromFile(resolvedMapPath, newMap, errors)) {
        LogError("Failed to load map: " + resolvedMapPath);
        LogMessages(errors, &LogError);
        return false;
    }

    std::vector<EntityPlacement> placements;
    const std::string resolvedEntitiesPath = ResolveContentPath(entitiesPath);
    const bool placementsLoaded = LoadEntityPlacementsFromFile(resolvedEntitiesPath, placements, errors, warnings);
    if (!warnings.empty()) {
        LogWarning("Entity placements " + resolvedEntitiesPath + ": " + std::to_string(warnings.size()) +
                   " warning(s):");
        LogMessages(warnings, &LogWarning);
    }
    if (!placementsLoaded) {
        LogError("Failed to load entity placements: " + resolvedEntitiesPath);
        LogMessages(errors, &LogError);
        return false;
    }

    LevelContents contents{};
    if (!BuildLevelFromPlacements(newMap, placements, gameplay_, contents, errors)) {
        LogError("Invalid entity placements in " + resolvedEntitiesPath + ":");
        LogMessages(errors, &LogError);
        return false;
    }
    LogInfo("Level entities: " + DescribeLevelContents(contents));

    map_ = std::move(newMap);
    entityManager_ = std::move(contents.entities);
    sprites_ = std::move(contents.sprites);
    player_ = contents.player;
    particleManager_.Clear();
    latestRayHits_.clear();
    latestWallDepthBuffer_.clear();

    playerHealth_ = gameplay_.player.initialHealth;
    playerAmmo_ = gameplay_.weapon.initialAmmo;
    playerMaxHealth_ = gameplay_.player.maxHealth;
    playerMaxAmmo_ = gameplay_.weapon.maxAmmo;
    ambientDustSpawnTimer_ = 0.0f;
    enemyAlertCooldown_ = 0.0f;
    playerAttackCooldown_ = 0.0f;
    hudMessage_.clear();
    hudMessageTimer_ = 0.0f;

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

    playerAttackCooldown_ = gameplay_.weapon.fireRate;

    GameplayEvents events{};
    const bool fired = PerformPlayerAttack(
        entityManager_, map_, player_, playerAmmo_, gameplay_.weapon.damage, gameplay_.weapon.range, events);
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
        ShowMessage("Hit enemy! (-" + std::to_string(events.enemyDamageAmount) + " HP)", 1.0f);
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
