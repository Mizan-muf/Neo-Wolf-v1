#include "engine/core/Engine.h"

Engine::~Engine() {
    Shutdown();
}

bool Engine::IsRunning() const {
    return running_;
}
