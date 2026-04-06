#include "engine/core/Engine.h"

#include "engine/core/Log.h"
#include "engine/platform/PlatformSDL.h"

void Engine::Shutdown() {
    if (!running_ && platform_ == nullptr) {
        return;
    }

    LogInfo("Engine shutdown started.");

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
    audio_.Shutdown();
    wallTexture_ = nullptr;
    wallAltATexture_ = nullptr;
    wallAltBTexture_ = nullptr;
    doorTexture_ = nullptr;
    floorTexture_ = nullptr;
    ceilingTexture_ = nullptr;
    enemySpriteTexture_ = nullptr;
    pickupSpriteTexture_ = nullptr;
    decorationSpriteTexture_ = nullptr;
    missingTexture_ = {};
    debugTopDownEnabled_ = true;
    latestRayHits_.clear();
    latestWallDepthBuffer_.clear();
    sprites_.clear();
    entityManager_.Clear();
    particleManager_.Clear();
    ambientDustSpawnTimer_ = 0.0f;
    enemyAlertCooldown_ = 0.0f;
    playerHealth_ = 100;
    playerAmmo_ = 24;
    paused_ = false;
    running_ = false;

    LogInfo("Engine shutdown finished.");
}


