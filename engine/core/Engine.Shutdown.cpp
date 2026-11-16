#include "engine/core/Engine.h"

#include "engine/core/Log.h"
#include "engine/platform/PlatformSDL.h"

void Engine::Shutdown() {
    if (!running_ && platform_ == nullptr) {
        return;
    }

    LogInfo("Engine shutdown started.");

    // Audio first: its SDL subsystem must be closed before the platform calls SDL_Quit.
    audio_.Shutdown();

    if (platform_ != nullptr) {
        platform_->Shutdown();
        delete platform_;
        platform_ = nullptr;
    }

    input_ = {};
    time_ = {};
    framebuffer_ = {};
    map_ = {};
    player_ = {};
    assets_.Clear();
    contentRoot_.clear();
    wallTexture_ = nullptr;
    wallAltATexture_ = nullptr;
    wallAltBTexture_ = nullptr;
    doorTexture_ = nullptr;
    lockedDoorTextures_ = {};
    exitTexture_ = nullptr;
    floorTexture_ = nullptr;
    ceilingTexture_ = nullptr;
    spriteTextures_ = {};
    missingTexture_ = {};
    minimapVisible_ = false;
    debugOverlayVisible_ = false;
    latestRayHits_.clear();
    latestWallDepthBuffer_.clear();
    sprites_.clear();
    entityManager_.Clear();
    particleManager_.Clear();
    ambientDustSpawnTimer_ = 0.0f;
    enemyAlertCooldown_ = 0.0f;
    playerAttackCooldown_ = 0.0f;
    gameplay_ = GameplayDefinitions{};
    playerHealth_ = gameplay_.player.initialHealth;
    playerAmmo_ = gameplay_.weapon.initialAmmo;
    playerMaxHealth_ = gameplay_.player.maxHealth;
    playerMaxAmmo_ = gameplay_.weapon.maxAmmo;
    keys_.Clear();
    pendingNoise_ = NoiseEvent{};
    hudMessage_.clear();
    hudMessageTimer_ = 0.0f;
    campaign_ = CampaignDefinition{};
    flow_ = GameFlow{};
    prefDir_.clear();
    currentThemeId_.clear();
    currentLevelIndex_ = -1;
    levelBannerTimer_ = 0.0f;
    levelTimeSeconds_ = 0.0;
    levelEnemyTotal_ = 0;
    levelPickupTotal_ = 0;
    levelShotsFired_ = 0;
    levelDamageTaken_ = 0;
    simulationFrozen_ = false;
    inputInjector_ = nullptr;
    reportedState_ = GameState::Menu;
    running_ = false;

    LogInfo("Engine shutdown finished.");
}
