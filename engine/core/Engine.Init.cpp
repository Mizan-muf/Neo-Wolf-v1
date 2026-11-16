#include "engine/core/Engine.h"

#include <cstddef>
#include <string>
#include <vector>

#include "engine/assets/AudioConfig.h"
#include "engine/assets/ContentConfig.h"
#include "engine/assets/TextureLoader.h"
#include "engine/core/Log.h"
#include "engine/core/Paths.h"
#include "engine/platform/PlatformSDL.h"

namespace {
constexpr int kWindowWidth = 1024;
constexpr int kWindowHeight = 768;
constexpr int kRenderWidth = 320;
constexpr int kRenderHeight = 240;
constexpr const char* kErrorTitle = "Neo-Wolf cannot start";

// Logs `message` as an error and, if allowed, shows it in a message box.
void ReportFatal(const std::string& message, bool showDialog) {
    LogError(message);
    const std::string logPath = GetLogFilePath();
    if (showDialog) {
        std::string text = message;
        if (!logPath.empty()) {
            text += "\n\nLog file: " + logPath;
        }
        PlatformSDL::ShowErrorMessageBox(kErrorTitle, text);
    }
}

// Logs every message collected by a loader (they are otherwise silently dropped).
void LogLoadErrors(std::vector<std::string>& errors) {
    for (const std::string& error : errors) {
        LogWarning(error);
    }
    errors.clear();
}
} // namespace

bool Engine::Init(const EngineOptions& options) {
    LogInfo("Initializing Neo-Wolf Engine...");

    // 1. Content root: resolved once, before any window is opened.
    const std::vector<ContentRootCandidate> candidates =
        BuildContentRootCandidates(GatherContentRootInputs(options.dataDir));
    const ContentRootResult root = FindContentRoot(candidates);
    if (!root.found) {
        std::string message = "Game content not found.\n" + DescribeContentRootSearch(root);
        const std::string explicitSource = candidates.size() == 1 ? candidates.front().source : std::string();
        if (explicitSource == "--data") {
            message += "\nThe --data folder must contain game/maps and game/defs.";
        } else if (explicitSource == "NEOWOLF_DATA_DIR") {
            message += "\nThe NEOWOLF_DATA_DIR folder must contain game/maps and game/defs (or unset it).";
        } else {
            message += "\nKeep the game/ folder next to the game, or pass --data <folder that contains game/>.";
        }
        ReportFatal(message, options.errorDialogs);
        return false;
    }
    contentRoot_ = root.root;
    LogInfo("Content root: " + PathToUtf8(contentRoot_) + " (" + root.source + ")");

    // 2. Gameplay values. An invalid gameplay.json is fatal (checked before any window opens):
    //    playing with half-applied or default values would hide the mistake.
    std::vector<std::string> gameplayErrors;
    if (!LoadGameplay(options.gameplayPath, &gameplayErrors)) {
        // The dialog shows the first problems; the log has all of them.
        constexpr std::size_t kShownErrors = 3;
        std::string message = "Invalid gameplay definitions in " + ResolveContentPath(options.gameplayPath) + ":";
        for (std::size_t i = 0; i < gameplayErrors.size() && i < kShownErrors; ++i) {
            message += "\n- " + gameplayErrors[i];
        }
        if (gameplayErrors.size() > kShownErrors) {
            message += "\n(" + std::to_string(gameplayErrors.size() - kShownErrors) + " more in the log)";
        }
        ReportFatal(message, options.errorDialogs);
        return false;
    }

    // 3. Window and renderer.
    platform_ = new PlatformSDL();
    PlatformConfig platformConfig{};
    platformConfig.title = "Neo-Wolf";
    platformConfig.windowWidth = kWindowWidth;
    platformConfig.windowHeight = kWindowHeight;
    platformConfig.logicalWidth = kRenderWidth;
    platformConfig.logicalHeight = kRenderHeight;
    platformConfig.fullscreen = options.fullscreen;
    if (!platform_->Init(platformConfig)) {
        delete platform_;
        platform_ = nullptr;
        ReportFatal("Could not open the game window (see the log for the SDL error).", options.errorDialogs);
        return false;
    }

    if (!framebuffer_.Resize(kRenderWidth, kRenderHeight)) {
        ReportFatal("Failed to allocate the framebuffer.", options.errorDialogs);
        return false;
    }

    // 4. Audio is optional.
    if (!audio_.Init()) {
        LogWarning("No audio device: the game runs without sound.");
    }

    // 5. Texture, sprite and audio definitions. Missing or broken files fall back to
    //    defaults with a warning.
    std::vector<std::string> errors;
    WallTextureAssignments wallAssignments{};
    if (LoadWallTextureAssignments(ResolveContentPath("game/defs/wall_textures.cfg"), wallAssignments, errors)) {
        LogInfo("Wall texture assignments loaded.");
    }
    LogLoadErrors(errors);

    SpriteTextureDefinitions spriteDefinitions{};
    if (LoadSpriteTextureDefinitions(ResolveContentPath("game/defs/sprites.cfg"), spriteDefinitions, errors)) {
        LogInfo("Sprite definitions loaded.");
    }
    LogLoadErrors(errors);

    AudioEffectDefinitions audioDefinitions{};
    if (LoadAudioEffectDefinitions(ResolveContentPath("game/defs/audio.cfg"), audioDefinitions, errors)) {
        LogInfo("Audio definitions loaded.");
        audio_.SetMasterVolume(audioDefinitions.masterVolume);
    }
    LogLoadErrors(errors);

    // 6. Textures (a checkerboard stands in for any that fail).
    assets_.Clear();
    TextureLoader::BuildFallbackChecker(missingTexture_, 64, 64);
    assets_.SetTexture("__missing", missingTexture_);

    const auto loadTextureWithFallback = [&](const std::string& id, const std::string& relativePath) {
        if (!assets_.LoadTextureFromPPM(id, ResolveContentPath(relativePath), errors)) {
            LogLoadErrors(errors);
            LogWarning("Using fallback texture for: " + id);
            assets_.SetTexture(id, missingTexture_);
        }
    };

    loadTextureWithFallback("wall", wallAssignments.wall);
    loadTextureWithFallback("wall_alt_a", wallAssignments.wallAltA);
    loadTextureWithFallback("wall_alt_b", wallAssignments.wallAltB);
    loadTextureWithFallback("door", wallAssignments.door);
    loadTextureWithFallback("floor", wallAssignments.floor);
    loadTextureWithFallback("ceiling", wallAssignments.ceiling);

    loadTextureWithFallback("enemy", spriteDefinitions.enemy);
    loadTextureWithFallback("pickup", spriteDefinitions.pickup);
    loadTextureWithFallback("decoration", spriteDefinitions.decoration);

    wallTexture_ = assets_.GetTexture("wall");
    wallAltATexture_ = assets_.GetTexture("wall_alt_a");
    wallAltBTexture_ = assets_.GetTexture("wall_alt_b");
    doorTexture_ = assets_.GetTexture("door");
    floorTexture_ = assets_.GetTexture("floor");
    ceilingTexture_ = assets_.GetTexture("ceiling");

    enemySpriteTexture_ = assets_.GetTexture("enemy");
    pickupSpriteTexture_ = assets_.GetTexture("pickup");
    decorationSpriteTexture_ = assets_.GetTexture("decoration");

    // 7. Sounds (skipped when there is no audio device).
    if (audio_.IsAvailable()) {
        std::size_t soundsLoaded = 0;
        const auto loadSoundSafe = [&](const std::string& soundId, const std::string& relativePath) {
            if (relativePath.empty()) {
                return;
            }
            if (audio_.LoadEffectFromWav(soundId, ResolveContentPath(relativePath), errors)) {
                ++soundsLoaded;
            }
            LogLoadErrors(errors);
        };

        loadSoundSafe("door_open", audioDefinitions.doorOpen);
        loadSoundSafe("pickup", audioDefinitions.pickup);
        loadSoundSafe("enemy_alert", audioDefinitions.enemyAlert);
        loadSoundSafe("weapon_fire", audioDefinitions.weaponFire);
        LogInfo("Sound effects loaded: " + std::to_string(soundsLoaded));
    }

    // 8. The first level. Failing here is fatal: there would be nothing to play.
    if (!LoadLevel(options.mapPath, options.entitiesPath)) {
        ReportFatal("Could not load the level " + options.mapPath + " from " + PathToUtf8(contentRoot_) +
                        " (see the log for details).",
                    options.errorDialogs);
        return false;
    }

    state_ = GameState::Menu;
    running_ = true;
    LogInfo("Engine initialized successfully.");
    return true;
}
