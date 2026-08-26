#include "engine/core/Engine.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "engine/assets/AudioConfig.h"
#include "engine/assets/ContentConfig.h"
#include "engine/assets/TextureLoader.h"
#include "engine/core/Log.h"
#include "engine/platform/PlatformSDL.h"

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
constexpr int kRenderWidth = 320;
constexpr int kRenderHeight = 240;

std::string ResolveContentPath(const std::string& relativePath) {
    if (std::filesystem::exists(relativePath)) {
        return relativePath;
    }
    const std::filesystem::path fromParent = std::filesystem::path("..") / relativePath;
    if (std::filesystem::exists(fromParent)) {
        return fromParent.string();
    }
    return relativePath;
}
} // namespace

bool Engine::Init() {
    LogInfo("Initializing Neo-Wolf Engine...");

    platform_ = new PlatformSDL();
    if (!platform_->Init("Neo-Wolf - Modern Wolf3D Engine", kWindowWidth, kWindowHeight)) {
        LogError("Failed to initialize SDL platform.");
        delete platform_;
        platform_ = nullptr;
        return false;
    }

    if (!framebuffer_.Resize(kRenderWidth, kRenderHeight)) {
        LogError("Failed to resize framebuffer.");
        return false;
    }

    if (!audio_.Init()) {
        LogWarning("Audio initialization failed or not supported in this environment.");
    }

    // Load configs
    std::vector<std::string> errors;
    WallTextureAssignments wallAssignments{};
    if (LoadWallTextureAssignments(ResolveContentPath("game/defs/wall_textures.cfg"), wallAssignments, errors)) {
        LogInfo("Wall texture assignments loaded.");
    }

    SpriteTextureDefinitions spriteDefinitions{};
    if (LoadSpriteTextureDefinitions(ResolveContentPath("game/defs/sprites.cfg"), spriteDefinitions, errors)) {
        LogInfo("Sprite definitions loaded.");
    }

    AudioEffectDefinitions audioDefinitions{};
    if (LoadAudioEffectDefinitions(ResolveContentPath("game/defs/audio.cfg"), audioDefinitions, errors)) {
        LogInfo("Audio definitions loaded.");
        audio_.SetMasterVolume(audioDefinitions.masterVolume);
    }

    // Load assets
    assets_.Clear();
    TextureLoader::BuildFallbackChecker(missingTexture_, 64, 64);
    assets_.SetTexture("__missing", missingTexture_);

    const auto loadTextureWithFallback = [&](const std::string& id, const std::string& relativePath) {
        if (!assets_.LoadTextureFromPPM(id, ResolveContentPath(relativePath), errors)) {
            LogWarning("Using fallback texture for: " + id);
            assets_.SetTexture(id, missingTexture_);
        }
    };

    loadTextureWithFallback("wall", wallAssignments.wall);
    loadTextureWithFallback("wall_alt_a", wallAssignments.wallAltA);
    loadTextureWithFallback("wall_alt_b", wallAssignments.wallAltB);
    loadTextureWithFallback("door", wallAssignments.door);
    loadTextureWithFallback("floor", wallAssignments.floor);
    loadTextureWithFallback("ceiling", wallAssignments.ceiling);

    loadTextureWithFallback("enemy", spriteDefinitions.enemy);
    loadTextureWithFallback("pickup", spriteDefinitions.pickup);
    loadTextureWithFallback("decoration", spriteDefinitions.decoration);

    wallTexture_ = assets_.GetTexture("wall");
    wallAltATexture_ = assets_.GetTexture("wall_alt_a");
    wallAltBTexture_ = assets_.GetTexture("wall_alt_b");
    doorTexture_ = assets_.GetTexture("door");
    floorTexture_ = assets_.GetTexture("floor");
    ceilingTexture_ = assets_.GetTexture("ceiling");

    enemySpriteTexture_ = assets_.GetTexture("enemy");
    pickupSpriteTexture_ = assets_.GetTexture("pickup");
    decorationSpriteTexture_ = assets_.GetTexture("decoration");

    const auto loadSoundSafe = [&](const std::string& soundId, const std::string& relativePath) {
        if (!relativePath.empty()) {
            audio_.LoadEffectFromWav(soundId, ResolveContentPath(relativePath), errors);
        }
    };

    loadSoundSafe("door_open", audioDefinitions.doorOpen);
    loadSoundSafe("pickup", audioDefinitions.pickup);
    loadSoundSafe("enemy_alert", audioDefinitions.enemyAlert);
    loadSoundSafe("weapon_fire", audioDefinitions.weaponFire);

    // Initial level load
    if (!LoadLevel(currentMapPath_, currentEntitiesPath_)) {
        LogWarning("LoadLevel failed on initial run.");
    }

    state_ = GameState::Menu;
    running_ = true;
    LogInfo("Engine initialized successfully.");
    return true;
}
