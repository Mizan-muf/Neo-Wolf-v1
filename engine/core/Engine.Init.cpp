#include "engine/core/Engine.h"

#include "engine/platform/PlatformSDL.h"

bool Engine::Init() {
    if (running_) {
        return true;
    }

    if (platform_ == nullptr) {
        platform_ = new PlatformSDL();
    }

    if (!platform_->Init("Raycast Engine - Phase 2", 1280, 720)) {
        return false;
    }

    input_ = {};
    time_ = {};
    input_.quitRequested = false;
    running_ = true;
    return true;
}
