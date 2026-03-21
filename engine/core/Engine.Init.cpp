#include "engine/core/Engine.h"

#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/platform/PlatformSDL.h"

bool Engine::Init() {
    constexpr int kWindowWidth = 1280;
    constexpr int kWindowHeight = 720;
    constexpr int kMapWidth = 16;
    constexpr int kMapHeight = 12;

    if (running_) {
        LogWarning("Engine::Init called while already running.");
        return true;
    }

    if (platform_ == nullptr) {
        platform_ = new PlatformSDL();
    }

    if (!platform_->Init("Raycast Engine - Phase 7", kWindowWidth, kWindowHeight)) {
        LogError("PlatformSDL initialization failed.");
        return false;
    }

    if (!framebuffer_.Resize(kWindowWidth, kWindowHeight)) {
        LogError("Framebuffer allocation failed.");
        return false;
    }

    if (!map_.Resize(kMapWidth, kMapHeight)) {
        LogError("Map allocation failed.");
        return false;
    }

    for (int y = 0; y < map_.Height(); ++y) {
        for (int x = 0; x < map_.Width(); ++x) {
            const bool isBorder = (x == 0 || y == 0 || x == map_.Width() - 1 || y == map_.Height() - 1);
            map_.SetCell(x, y, isBorder ? TileType::Wall : TileType::Empty);
        }
    }

    for (int x = 3; x <= 11; ++x) {
        map_.SetCell(x, 3, TileType::Wall);
    }
    for (int y = 5; y <= 9; ++y) {
        map_.SetCell(8, y, TileType::Wall);
    }

    map_.SetCell(8, 7, TileType::Door);
    map_.SetCell(13, 8, TileType::Trigger);

    player_ = {};
    player_.position = {2.5f, 2.5f};
    player_.directionAngleRadians = DegreesToRadians(15.0f);
    player_.moveSpeed = 3.0f;
    player_.turnSpeedRadians = DegreesToRadians(120.0f);
    player_.collisionRadius = 0.22f;

    input_ = {};
    time_ = {};
    input_.quitRequested = false;
    running_ = true;
    LogInfo("Engine initialized successfully.");
    return true;
}
