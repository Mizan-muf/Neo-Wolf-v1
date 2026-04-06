#pragma once

#include <string>
#include <vector>

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/core/InputState.h"
#include "engine/core/TimeState.h"
#include "engine/particles/Particle.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/Raycaster.h"
#include "engine/world/Entity.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"
#include "engine/world/Sprite.h"

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
    AssetManager assets_{};
    AudioSystem audio_{};
    Texture missingTexture_{};
    const Texture* wallTexture_ = nullptr;
    const Texture* wallAltATexture_ = nullptr;
    const Texture* wallAltBTexture_ = nullptr;
    const Texture* doorTexture_ = nullptr;
    const Texture* floorTexture_ = nullptr;
    const Texture* ceilingTexture_ = nullptr;
    const Texture* enemySpriteTexture_ = nullptr;
    const Texture* pickupSpriteTexture_ = nullptr;
    const Texture* decorationSpriteTexture_ = nullptr;
    bool debugTopDownEnabled_ = true;
    std::vector<RaycastHit> latestRayHits_{};
    std::vector<float> latestWallDepthBuffer_{};
    std::vector<SpriteEntity> sprites_{};
    EntityManager entityManager_{};
    ParticleManager particleManager_{};
    float ambientDustSpawnTimer_ = 0.0f;
    float enemyAlertCooldown_ = 0.0f;
    int playerHealth_ = 100;
    int playerAmmo_ = 24;
    bool paused_ = false;
    bool running_ = false;
};
