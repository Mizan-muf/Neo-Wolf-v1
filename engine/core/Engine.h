#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "engine/assets/AssetManager.h"
#include "engine/audio/AudioSystem.h"
#include "engine/core/GameState.h"
#include "engine/core/InputState.h"
#include "engine/core/TimeState.h"
#include "engine/interop/CommandBridge.h"
#include "engine/particles/Particle.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/Raycaster.h"
#include "engine/world/Entity.h"
#include "engine/world/GameplayDefinitions.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"
#include "engine/world/Sprite.h"

class PlatformSDL;

// Startup settings (filled from the command line by App; the defaults suit the C API and tests).
struct EngineOptions {
    std::string dataDir{};  // --data (UTF-8); empty = automatic content-root search
    std::string mapPath{"game/maps/level01.map"};             // relative to the content root
    std::string entitiesPath{"game/defs/level01.entities.json"};
    std::string gameplayPath{"game/defs/gameplay.json"};
    bool fullscreen = false;
    // Show a message box for fatal startup errors. Off for automated runs (--frames).
    bool errorDialogs = true;
};

class Engine {
public:
    Engine() = default;
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Returns false (after logging and, if enabled, showing an error box) when the game
    // cannot start: no content root, an invalid gameplay.json, no window/renderer, or the
    // first level fails to load.
    bool Init(const EngineOptions& options = EngineOptions{});
    void Update(double deltaSeconds);
    void Render();
    void Shutdown();

    bool IsRunning() const;

    GameState GetState() const;
    void SetState(GameState state);
    void ResetLevel();
    // Loads gameplay.json (content-relative path) into gameplay_; it applies from the next
    // LoadLevel on. On any error the previous values stay and false is returned; the errors are
    // logged and, if outErrors is given, also stored there.
    bool LoadGameplay(const std::string& relativePath, std::vector<std::string>* outErrors = nullptr);
    // Loads a map and its entity file (content-relative paths) and builds the level with the
    // current gameplay_ values. On any error (missing/invalid map or entity file, invalid
    // placement) the current level stays as it was and false is returned (errors logged).
    bool LoadLevel(const std::string& mapPath, const std::string& entitiesPath);
    void ShowMessage(const std::string& message, float duration = 2.0f);
    void PlayerAttack();
    // Content-relative path (e.g. "game/maps/level01.map") -> path under the content root
    // (UTF-8). Without a content root (tests, C API before Init) the path is returned as is,
    // i.e. relative to the working directory.
    std::string ResolveContentPath(const std::string& relativePath) const;
    const std::filesystem::path& ContentRoot() const { return contentRoot_; }

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
    float playerAttackCooldown_ = 0.0f;
    GameplayDefinitions gameplay_{};  // from gameplay.json (Init); defaults until then
    int playerHealth_ = kDefaultPlayerDefinition.initialHealth;
    int playerAmmo_ = kDefaultWeaponDefinition.initialAmmo;
    int playerMaxHealth_ = kDefaultPlayerDefinition.maxHealth;
    int playerMaxAmmo_ = kDefaultWeaponDefinition.maxAmmo;
    std::string hudMessage_{};
    float hudMessageTimer_ = 0.0f;
    std::filesystem::path contentRoot_{};
    std::string currentMapPath_{"game/maps/level01.map"};
    std::string currentEntitiesPath_{"game/defs/level01.entities.json"};
    CommandBridge bridge_{};
    GameState state_ = GameState::Menu;
    bool paused_ = false;
    bool running_ = false;
};
