#include "engine/core/Engine.h"

#include "engine/platform/PlatformSDL.h"

void Engine::Render() {
    if (!running_ || platform_ == nullptr) {
        return;
    }

    platform_->Clear(20, 20, 24, 255);
    platform_->Present();
}
