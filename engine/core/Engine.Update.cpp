#include "engine/core/Engine.h"

#include "engine/platform/PlatformSDL.h"

void Engine::Update(double deltaSeconds) {
    if (!running_ || platform_ == nullptr) {
        return;
    }

    if (deltaSeconds < 0.0) {
        deltaSeconds = 0.0;
    }

    time_.deltaSeconds = deltaSeconds;
    time_.totalSeconds += deltaSeconds;
    ++time_.frameIndex;

    platform_->PumpEvents(input_);

    if (input_.quitRequested) {
        running_ = false;
    }
}
