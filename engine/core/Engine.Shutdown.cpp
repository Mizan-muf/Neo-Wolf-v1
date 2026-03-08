#include "engine/core/Engine.h"

#include "engine/platform/PlatformSDL.h"

void Engine::Shutdown() {
    if (!running_ && platform_ == nullptr) {
        return;
    }

    if (platform_ != nullptr) {
        platform_->Shutdown();
        delete platform_;
        platform_ = nullptr;
    }

    input_ = {};
    time_ = {};
    running_ = false;
}
