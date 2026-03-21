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
    running_ = false;

    LogInfo("Engine shutdown finished.");
}
