#include "engine/core/Engine.h"

#include <filesystem>
#include <string>
#include <vector>

#include "engine/assets/AudioConfig.h"
#include "engine/assets/ContentConfig.h"
#include "engine/assets/TextureLoader.h"
#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/platform/PlatformSDL.h"
#include "engine/world/EntityPlacementLoader.h"
#include "engine/world/MapLoader.h"
#include "engine/world/Sprite.h"

namespace {
std::string ResolveContentPath(const std::string& pathFromRepoRoot) {
    if (std::filesystem::exists(pathFromRepoRoot)) {
        return pathFromRepoRoot;
    }

    const std::string fromBuild = "../" + pathFromRepoRoot;
    if (std::filesystem::exists(fromBuild)) {
        return fromBuild;
    }

    return pathFromRepoRoot;
}
} // namespace

bool Engine::Init() {
    constexpr int kWindowWidth = 1280;
    constexpr int kWindowHeight = 720;

    if (running_) {
        LogWarning("Engine::Init called while already running.");
        return true;
    }

    if (platform_ == nullptr) {
        platform_ = new PlatformSDL();
    }

    if (!platform_->Init("Raycast Engine - Phase 17", kWindowWidth, kWindowHeight)) {
        LogError("PlatformSDL initialization failed.");
        return false;
    }

    if (!framebuffer_.Resize(kWindowWidth, kWindowHeight)) {
        LogError("Framebuffer allocation failed.");
        return false;
    }

    std::vector<std::string> errors;

    if (!LoadMapFromFile(ResolveContentPath("game/maps/level01.map"), map_, errors)) {
        for (const std::string& error : errors) {
            LogError(error.c_str());
        }
        return false;
    }

    WallTextureAssignments wallAssignments{};
    if (!LoadWallTextureAssignments(ResolveContentPath("game/defs/wall_textures.cfg"), wallAssignments, errors)) {
        for (const std::string& error : errors) {
            LogError(error.c_str());
        }
        return false;
    }

    SpriteTextureDefinitions spriteDefinitions{};
    if (!LoadSpriteTextureDefinitions(ResolveContentPath("game/defs/sprites.cfg"), spriteDefinitions, errors)) {
        for (const std::string& error : errors) {
            LogError(error.c_str());
        }
        return false;
    }

    AudioEffectDefinitions audioDefinitions{};
    if (!LoadAudioEffectDefinitions(ResolveContentPath("game/defs/audio.cfg"), audioDefinitions, errors)) {
        for (const std::string& error : errors) {
            LogError(error.c_str());
        }
        return false;
    }

    std::vector<EntityPlacement> placements;
    if (!LoadEntityPlacementsFromFile(ResolveContentPath("game/defs/level01.entities.csv"), placements, errors)) {
        for (const std::string& error : errors) {
            LogError(error.c_str());
        }
        return false;
    }

    assets_.Clear();
    TextureLoader::BuildFallbackChecker(missingTexture_, 64, 64);
    assets_.SetTexture("__missing", missingTexture_);

    const auto loadTextureWithFallback = [&](const std::string& id, const std::string& relativePath) {
        if (!assets_.LoadTextureFromPPM(id, ResolveContentPath(relativePath), errors)) {
            assets_.SetTexture(id, missingTexture_);
        }
    };

    loadTextureWithFallback("wall", wallAssignments.wall);
    loadTextureWithFallback("wall_alt_a", wallAssignments.wallAltA);
    loadTextureWithFallback("wall_alt_b", wallAssignments.wallAltB);
    loadTextureWithFallback("door", wallAssignments.door);
    loadTextureWithFallback("floor", wallAssignments.floor);
    loadTextureWithFallback("ceiling", wallAssignments.ceiling);
    loadTextureWithFallback("sprite_enemy", spriteDefinitions.enemy);
    loadTextureWithFallback("sprite_pickup", spriteDefinitions.pickup);
    loadTextureWithFallback("sprite_decor", spriteDefinitions.decoration);

    for (const std::string& error : errors) {
        LogWarning(error.c_str());
    }

    wallTexture_ = assets_.GetTexture("wall");
    wallAltATexture_ = assets_.GetTexture("wall_alt_a");
    wallAltBTexture_ = assets_.GetTexture("wall_alt_b");
    doorTexture_ = assets_.GetTexture("door");
    floorTexture_ = assets_.GetTexture("floor");
    ceilingTexture_ = assets_.GetTexture("ceiling");
    enemySpriteTexture_ = assets_.GetTexture("sprite_enemy");
    pickupSpriteTexture_ = assets_.GetTexture("sprite_pickup");
    decorationSpriteTexture_ = assets_.GetTexture("sprite_decor");

    if (wallTexture_ == nullptr) wallTexture_ = &missingTexture_;
    if (wallAltATexture_ == nullptr) wallAltATexture_ = &missingTexture_;
    if (wallAltBTexture_ == nullptr) wallAltBTexture_ = &missingTexture_;
    if (doorTexture_ == nullptr) doorTexture_ = &missingTexture_;
    if (floorTexture_ == nullptr) floorTexture_ = &missingTexture_;
    if (ceilingTexture_ == nullptr) ceilingTexture_ = &missingTexture_;
    if (enemySpriteTexture_ == nullptr) enemySpriteTexture_ = &missingTexture_;
    if (pickupSpriteTexture_ == nullptr) pickupSpriteTexture_ = &missingTexture_;
    if (decorationSpriteTexture_ == nullptr) decorationSpriteTexture_ = &missingTexture_;

    audio_.Shutdown();
    if (!audio_.Init()) {
        LogWarning("Audio device failed to initialize. Continuing without sound.");
    } else {
        audio_.SetMasterVolume(audioDefinitions.masterVolume);
        std::vector<std::string> audioErrors;
        const auto loadEffect = [&](const char* id, const std::string& relativePath) {
            const std::string resolvedPath = ResolveContentPath(relativePath);
            if (!audio_.LoadEffectFromWav(id, resolvedPath, audioErrors)) {
                LogWarning(std::string("Audio effect unavailable: ") + id + " -> " + resolvedPath);
            }
        };

        loadEffect("door_open", audioDefinitions.doorOpen);
        loadEffect("pickup", audioDefinitions.pickup);
        loadEffect("enemy_alert", audioDefinitions.enemyAlert);

        if (!audioDefinitions.weaponFire.empty()) {
            loadEffect("weapon_fire", audioDefinitions.weaponFire);
        }

        for (const std::string& error : audioErrors) {
            LogWarning(error.c_str());
        }
    }

    player_ = {};
    player_.position = {2.5f, 2.5f};
    player_.directionAngleRadians = DegreesToRadians(15.0f);
    player_.moveSpeed = 3.0f;
    player_.turnSpeedRadians = DegreesToRadians(120.0f);
    player_.collisionRadius = 0.22f;

    input_ = {};
    time_ = {};
    playerHealth_ = 100;
    playerAmmo_ = 24;
    paused_ = false;
    debugTopDownEnabled_ = true;
    latestRayHits_.clear();
    latestWallDepthBuffer_.clear();
    entityManager_.Clear();
    sprites_.clear();
    particleManager_.Clear();
    ambientDustSpawnTimer_ = 0.0f;
    enemyAlertCooldown_ = 0.0f;

    bool playerSpawnFound = false;
    for (const EntityPlacement& placement : placements) {
        switch (placement.type) {
        case PlacementType::Player: {
            player_.position = placement.position;
            Entity& playerEntity = entityManager_.CreateEntity(EntityType::Player, player_.position);
            playerEntity.visible = false;
            playerEntity.active = true;
            playerSpawnFound = true;
            break;
        }
        case PlacementType::Enemy: {
            Entity& enemy = entityManager_.CreateEntity(EntityType::Enemy, placement.position);
            sprites_.push_back(SpriteEntity{enemy.position, 0, enemy.id, SpriteType::Enemy, true, true});
            break;
        }
        case PlacementType::Pickup: {
            Entity& pickup = entityManager_.CreateEntity(EntityType::Pickup, placement.position);
            sprites_.push_back(SpriteEntity{pickup.position, 1, pickup.id, SpriteType::Pickup, true, true});
            break;
        }
        case PlacementType::Door: {
            Entity& door = entityManager_.CreateEntity(EntityType::Door, placement.position);
            door.mapX = placement.mapX;
            door.mapY = placement.mapY;
            door.doorOpen = false;
            if (door.mapX >= 0 && door.mapY >= 0) {
                map_.SetCell(door.mapX, door.mapY, TileType::Door);
            }
            break;
        }
        case PlacementType::Trigger: {
            Entity& trigger = entityManager_.CreateEntity(EntityType::Trigger, placement.position);
            trigger.mapX = placement.mapX;
            trigger.mapY = placement.mapY;
            trigger.triggerActivated = false;
            if (trigger.mapX >= 0 && trigger.mapY >= 0) {
                map_.SetCell(trigger.mapX, trigger.mapY, TileType::Trigger);
            }
            break;
        }
        case PlacementType::Decoration:
            sprites_.push_back(SpriteEntity{placement.position, 2, -1, SpriteType::Decoration, true, true});
            break;
        }
    }

    if (!playerSpawnFound) {
        Entity& playerEntity = entityManager_.CreateEntity(EntityType::Player, player_.position);
        playerEntity.visible = false;
        playerEntity.active = true;
        LogWarning("No player entry found in entity placement file. Using fallback spawn.");
    }

    const float centerX = static_cast<float>(map_.Width()) * 0.5f;
    const float centerY = static_cast<float>(map_.Height()) * 0.5f;
    const float maxDistance = std::sqrt(centerX * centerX + centerY * centerY);
    for (int y = 0; y < map_.Height(); ++y) {
        for (int x = 0; x < map_.Width(); ++x) {
            const float dx = static_cast<float>(x) - centerX;
            const float dy = static_cast<float>(y) - centerY;
            const float normalized = (maxDistance > 0.0001f) ? std::sqrt(dx * dx + dy * dy) / maxDistance : 0.0f;
            const float baseLight = Clamp(1.0f - normalized * 0.55f, 0.45f, 1.0f);
            map_.SetLightLevel(x, y, baseLight);
        }
    }

    for (const Entity& entity : entityManager_.Entities()) {
        if (entity.type == EntityType::Trigger && entity.mapX >= 0 && entity.mapY >= 0) {
            map_.SetLightLevel(entity.mapX, entity.mapY, 1.0f);
        }
        if (entity.type == EntityType::Door && entity.mapX >= 0 && entity.mapY >= 0) {
            map_.SetLightLevel(entity.mapX, entity.mapY, 0.85f);
        }
    }

    input_.quitRequested = false;
    running_ = true;
    LogInfo("Engine initialized successfully.");
    return true;
}





