#include "engine/core/Engine.h"

#include "engine/core/Log.h"
#include "engine/platform/PlatformSDL.h"

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

    if (!platform_->Init("Raycast Engine - Phase 5", kWindowWidth, kWindowHeight)) {
        LogError("PlatformSDL initialization failed.");
        return false;
    }

    if (!framebuffer_.Resize(kWindowWidth, kWindowHeight)) {
        LogError("Framebuffer allocation failed.");
        return false;
    }

    input_ = {};
    time_ = {};
    input_.quitRequested = false;
    running_ = true;
    LogInfo("Engine initialized successfully.");
    return true;
}
