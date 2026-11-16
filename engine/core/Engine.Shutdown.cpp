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
        platform_.reset();
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
    gameplay_ = GameplayDefinitions{};
    ResetLevelState();
    campaign_ = CampaignDefinition{};
    flow_ = GameFlow{};
    prefDir_.clear();
    currentThemeId_.clear();
    currentLevelIndex_ = -1;
    simulationFrozen_ = false;
    inputInjector_ = nullptr;
    reportedState_ = GameState::Menu;
    running_ = false;

    LogInfo("Engine shutdown finished.");
}
