#pragma once

#include "engine/core/InputState.h"
#include "engine/core/TimeState.h"
#include "engine/render/Framebuffer.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

class PlatformSDL;

class Engine {
public:
    Engine() = default;
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool Init();
    void Update(double deltaSeconds);
    void Render();
    void Shutdown();

    bool IsRunning() const;

    PlatformSDL* platform_ = nullptr;
    InputState input_{};
    TimeState time_{};
    Framebuffer framebuffer_{};
    Map map_{};
    Player player_{};
    bool running_ = false;
};
