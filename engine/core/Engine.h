#pragma once

// The engine: owns every subsystem and runs start-up, the frame update, rendering and
// shutdown. The members are public because the C API, the command bridge, the autoplay bot
// and the tests work on them directly (docs/ARCHITECTURE.md).

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/assets/AssetManager.h"
#include "engine/assets/CampaignConfig.h"
#include "engine/audio/AudioSystem.h"
#include "engine/core/GameFlow.h"
#include "engine/core/GameState.h"
#include "engine/core/InputState.h"
#include "engine/core/PreviewScreen.h"
#include "engine/core/RenderSize.h"
#include "engine/core/TimeState.h"
#include "engine/interop/CommandBridge.h"
#include "engine/particles/Particle.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/Raycaster.h"
#include "engine/world/EnemyAi.h"
#include "engine/world/Entity.h"
#include "engine/world/GameplayDefinitions.h"
#include "engine/world/KeyColor.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"
#include "engine/world/Sprite.h"

class PlatformSDL;
struct AudioEffectDefinitions;
struct GameplayEvents;
struct SpriteTextureDefinitions;

// Startup settings (filled from the command line by App; the defaults suit the C API and tests).
struct EngineOptions {
    std::string dataDir;                                  // --data (UTF-8); empty = automatic content-root search
    std::string campaignPath{"game/defs/campaign.json"};  // relative to the content root
    std::string gameplayPath{"game/defs/gameplay.json"};
    // --level N: campaign level to start in (1-based), skipping the menu and the unlock
    // state. 0 = start on the main menu (level 1 is loaded behind it).
    int startLevel = 0;
    // Folder for progress.json (App resolves --pref-dir / NEOWOLF_PREF_DIR / SDL_GetPrefPath).
    // Empty = progress is neither loaded nor saved (C API, tests).
    std::filesystem::path prefDir;
    PreviewScreen preview = PreviewScreen::None;  // --preview: a still screen with sample data
    // --render-size: the framebuffer size (IsValidRenderSize, else Init fails). Only drawing
    // depends on it, never the simulation.
    RenderSize renderSize = kDefaultRenderSize;
    bool fullscreen = false;
    // Show a message box for fatal startup errors. Off for automated runs (--frames).
    bool errorDialogs = true;
    // No window, renderer or audio: Update and Render work on the framebuffer only (tests and
    // the Phase 7 bot). Content, campaign, progress and the first level load as usual.
    bool headless = false;
};

// Input injection seam (tests, Phase 7 bot): called once per Update, after the platform's
// events were read into the InputState and before anything uses it. Set actions with
// InputState::SetActionState (edges such as WasPressed follow from the previous frame).
using InputInjector = std::function<void(InputState& input)>;

class Engine {
public:
    // Defined where PlatformSDL is a complete type (platform_ is a unique_ptr to it).
    Engine();
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) = delete;
    Engine& operator=(Engine&&) = delete;

    // Returns false (after logging and, if enabled, showing an error box) when the game
    // cannot start: an invalid render size, no content root, an invalid gameplay.json or
    // campaign.json, a --level outside the campaign, no window/renderer, or the first level
    // fails to load.
    bool Init(const EngineOptions& options = EngineOptions{});
    void Update(double deltaSeconds);
    void Render();
    void Shutdown();

    bool IsRunning() const;

    GameState GetState() const;
    // External state change (C API, command bridge). Victory (deprecated) means LevelComplete.
    void SetState(GameState state);
    // Restarts the current level: a campaign level with its level-entry health and ammo, a
    // level loaded by path with the gameplay.json initial values. State becomes Gameplay.
    void ResetLevel();
    void SetInputInjector(InputInjector injector) { inputInjector_ = std::move(injector); }
    // Loads game/defs/campaign.json (content-relative) into campaign_ and configures the game
    // flow. On error the previous campaign stays and false is returned (errors logged).
    bool LoadCampaignDefinition(const std::string& relativePath, std::vector<std::string>* outErrors = nullptr);
    // Loads campaign level `levelIndex` (0-based) with its texture theme and starts it with
    // `stats` (health/ammo). A negative index reloads the level loaded by path.
    bool LoadCampaignLevel(int levelIndex, const PlayerStats& stats);
    // Loads game/defs/themes/<themeId>.cfg and swaps the wall, door, floor, ceiling and exit
    // textures (only when the theme differs from the current one). A missing or broken
    // texture becomes the checkerboard; returns false if anything had to fall back.
    bool ApplyTheme(const std::string& themeId);
    // Sets up a still preview screen with sample data (--preview); the simulation is frozen.
    bool SetupPreview(PreviewScreen screen);
    // Loads gameplay.json (content-relative path) into gameplay_; it applies from the next
    // LoadLevel on. On any error the previous values stay and false is returned; the errors are
    // logged and, if outErrors is given, also stored there.
    bool LoadGameplay(const std::string& relativePath, std::vector<std::string>* outErrors = nullptr);
    // Loads a map and its entity file (content-relative paths) and builds the level with the
    // current gameplay_ values. On any error (missing/invalid map or entity file, invalid
    // placement) the current level stays as it was and false is returned (errors logged).
    bool LoadLevel(const std::string& mapPath, const std::string& entitiesPath);
    void ShowMessage(const std::string& message, float duration = 2.0f);
    PlayerStats CurrentStats() const { return PlayerStats{playerHealth_, playerAmmo_}; }
    // Kills, pickups and time of the current level so far.
    LevelResults CurrentLevelResults() const;
    // The loaded campaign level, or nullptr for a level loaded by path.
    const CampaignLevel* CurrentCampaignLevel() const;
    void PlayerAttack();
    // Content-relative path (e.g. "game/maps/level01.map") -> path under the content root
    // (UTF-8). Without a content root (tests, C API before Init) the path is returned as is,
    // i.e. relative to the working directory.
    std::string ResolveContentPath(const std::string& relativePath) const;
    const std::filesystem::path& ContentRoot() const { return contentRoot_; }

    std::unique_ptr<PlatformSDL> platform_;  // null when headless
    InputState input_{};
    TimeState time_{};
    Framebuffer framebuffer_;
    Map map_;
    Player player_{};
    AssetManager assets_{};
    AudioSystem audio_;
    Texture missingTexture_;
    const Texture* wallTexture_ = nullptr;
    const Texture* wallAltATexture_ = nullptr;
    const Texture* wallAltBTexture_ = nullptr;
    const Texture* doorTexture_ = nullptr;
    std::array<const Texture*, kKeyColorCount> lockedDoorTextures_{};  // red, blue, gold (theme)
    const Texture* exitTexture_ = nullptr;
    const Texture* floorTexture_ = nullptr;
    const Texture* ceilingTexture_ = nullptr;
    std::array<const Texture*, kSpriteTextureCount> spriteTextures_{};  // by SpriteTextureId (sprites.cfg)
    bool minimapVisible_ = false;                                       // M / Tab (off by default, C9)
    bool debugOverlayVisible_ = false;                                  // F3
    std::vector<RaycastHit> latestRayHits_;
    std::vector<float> latestWallDepthBuffer_;
    std::vector<SpriteEntity> sprites_;
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
    KeyRing keys_{};             // key cards held; cleared on every level load and restart
    NoiseEvent pendingNoise_{};  // the last shot, heard by enemies in the next AI update
    std::string hudMessage_;
    float hudMessageTimer_ = 0.0f;
    std::filesystem::path contentRoot_;
    std::string currentMapPath_{"game/maps/level01.map"};
    std::string currentEntitiesPath_{"game/defs/level01.entities.json"};
    CampaignDefinition campaign_{};
    GameFlow flow_{};
    std::filesystem::path prefDir_;  // where progress.json lives; empty = not saved
    std::string currentThemeId_;
    int currentLevelIndex_ = -1;     // campaign index of the loaded level; -1 = loaded by path
    float levelBannerTimer_ = 0.0f;  // "LEVEL N / name" banner at level start
    double levelTimeSeconds_ = 0.0;  // time spent in Gameplay on this level
    int levelEnemyTotal_ = 0;
    int levelPickupTotal_ = 0;
    int levelShotsFired_ = 0;        // shots fired on this level (LevelResults)
    int levelDamageTaken_ = 0;       // health lost to enemy attacks on this level
    bool simulationFrozen_ = false;  // previews: Update only reads input
    InputInjector inputInjector_;
    GameState reportedState_ = GameState::Menu;  // last state announced with a StateChanged event
    CommandBridge bridge_;
    bool running_ = false;

private:
    static constexpr int kMissingTextureSize = 64;  // the checkerboard that stands in for missing textures

    // Init steps, in order. Each returns false after reporting a fatal error.
    bool InitContentRoot(const EngineOptions& options);
    bool InitDefinitions(const EngineOptions& options);  // gameplay.json, campaign.json, --level
    bool InitWindow(const EngineOptions& options);
    void InitAssets();  // sprite and audio definitions, sprite textures, sounds
    bool InitFirstLevel(const EngineOptions& options);
    void LoadSpriteTextures(const SpriteTextureDefinitions& definitions);
    void LoadSoundEffects(const AudioEffectDefinitions& definitions);

    // One frame of play (movement, interactions, AI); fills `events` for the game flow.
    void UpdateGameplay(float dt, FlowEvents& events);
    void MovePlayer(float dt);  // turning and walking from the input
    // HUD messages, sounds, particles and C API events for pickups, doors and triggers.
    void ReportInteractionEvents(const GameplayEvents& events);
    void CompleteLevel(const GameplayEvents& events, FlowEvents& flowEvents);
    // Reports this frame's enemy attacks; returns true if the player died.
    bool ReportPlayerDamage(const GameplayEvents& events, FlowEvents& flowEvents);
    // Plays the alert sound unless it played less than kEnemyAlertSoundCooldown ago; returns
    // true if it played.
    bool PlayEnemyAlertSound();
    void UpdateAmbientDust(float dt);
    void PushInteropEvent(InteropEventType type, int intArg1 = 0, int intArg2 = 0);
    // Player stats from gameplay_, keys, timers, HUD message and level counters back to the
    // start of a level (LoadLevel, Shutdown).
    void ResetLevelState();
    void ExecuteFlowRequests(const std::vector<FlowRequest>& requests);
    void SaveProgress(int highestUnlocked) const;
    void LoadSavedProgress();
    void ConfigureFlow();
    // Pushes a StateChanged event (and logs) if the flow's state differs from the last one.
    void PublishStateChange();
    void RefreshTexturePointers();
    // Previews "enemies" / "items": replaces the loaded level with a fixed stage (a hall with a
    // locked door, independent of the campaign) and a line-up of enemies or pickups.
    bool SetupShowcase(bool enemies);
    void RenderWorld();
    void RenderUi();
};
