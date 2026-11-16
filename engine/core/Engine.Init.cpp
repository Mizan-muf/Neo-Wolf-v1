#include "engine/core/Engine.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "engine/assets/AudioConfig.h"
#include "engine/assets/ContentConfig.h"
#include "engine/assets/TextureLoader.h"
#include "engine/core/Log.h"
#include "engine/core/Paths.h"
#include "engine/platform/PlatformSDL.h"

namespace {
constexpr const char* kErrorTitle = "Neo-Wolf cannot start";
constexpr std::size_t kShownErrors = 3;  // errors listed in the start-up dialog; the log has all of them

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

// "<heading>" followed by the first kShownErrors errors and how many more the log has.
std::string DescribeErrors(const std::string& heading, const std::vector<std::string>& errors) {
    std::string message = heading;
    for (std::size_t i = 0; i < errors.size() && i < kShownErrors; ++i) {
        message += "\n- " + errors[i];
    }
    if (errors.size() > kShownErrors) {
        message += "\n(" + std::to_string(errors.size() - kShownErrors) + " more in the log)";
    }
    return message;
}

// The "content not found" message, with a hint that depends on where the search looked.
std::string DescribeMissingContent(const std::vector<ContentRootCandidate>& candidates, const ContentRootResult& root) {
    std::string message = "Game content not found.\n" + DescribeContentRootSearch(root);
    const std::string explicitSource = candidates.size() == 1 ? candidates.front().source : std::string();
    if (explicitSource == "--data") {
        message += "\nThe --data folder must contain game/maps and game/defs.";
    } else if (explicitSource == "NEOWOLF_DATA_DIR") {
        message += "\nThe NEOWOLF_DATA_DIR folder must contain game/maps and game/defs (or unset it).";
    } else {
        message += "\nKeep the game/ folder next to the game, or pass --data <folder that contains game/>.";
    }
    return message;
}

std::string DescribeRenderSize(const RenderSize& size) {
    return std::to_string(size.width) + "x" + std::to_string(size.height);
}

// Logs every message collected by a loader (they are otherwise silently dropped).
void LogLoadErrors(std::vector<std::string>& errors) {
    for (const std::string& error : errors) {
        LogWarning(error);
    }
    errors.clear();
}
}  // namespace

bool Engine::Init(const EngineOptions& options) {
    LogInfo("Initializing Neo-Wolf Engine...");

    // The command line validates --render-size; this also covers the C API and tests.
    if (!IsValidRenderSize(options.renderSize)) {
        ReportFatal("Invalid render size " + DescribeRenderSize(options.renderSize) + ": it needs " +
                        DescribeRenderSizeLimits() + ".",
                    options.errorDialogs);
        return false;
    }
    if (!InitContentRoot(options) || !InitDefinitions(options)) {
        return false;
    }

    // Saved progress (a missing or corrupt file only means "no progress").
    prefDir_ = options.prefDir;
    LoadSavedProgress();

    if (!InitWindow(options)) {
        return false;
    }
    if (!framebuffer_.Resize(options.renderSize.width, options.renderSize.height)) {
        ReportFatal("Failed to allocate the framebuffer.", options.errorDialogs);
        return false;
    }
    LogInfo("Render size: " + DescribeRenderSize(options.renderSize) + ".");

    // Audio is optional.
    if (options.headless) {
        LogInfo("Headless: no window, renderer or audio.");
    } else if (!audio_.Init()) {
        LogWarning("No audio device: the game runs without sound.");
    }

    InitAssets();
    if (!InitFirstLevel(options)) {
        return false;
    }

    reportedState_ = GameState::Menu;
    running_ = true;
    if (options.preview != PreviewScreen::None && !SetupPreview(options.preview)) {
        ReportFatal(std::string("Could not set up the preview ") + PreviewScreenName(options.preview) + ".",
                    options.errorDialogs);
        return false;
    }
    PublishStateChange();
    LogInfo("Engine initialized successfully.");
    return true;
}

// Resolved once, before any window is opened.
bool Engine::InitContentRoot(const EngineOptions& options) {
    const std::vector<ContentRootCandidate> candidates =
        BuildContentRootCandidates(GatherContentRootInputs(options.dataDir));
    const ContentRootResult root = FindContentRoot(candidates);
    if (!root.found) {
        ReportFatal(DescribeMissingContent(candidates, root), options.errorDialogs);
        return false;
    }
    contentRoot_ = root.root;
    LogInfo("Content root: " + PathToUtf8(contentRoot_) + " (" + root.source + ")");
    return true;
}

// An invalid gameplay.json or campaign.json is fatal (checked before any window opens):
// playing with half-applied or default values would hide the mistake.
bool Engine::InitDefinitions(const EngineOptions& options) {
    std::vector<std::string> errors;
    if (!LoadGameplay(options.gameplayPath, &errors)) {
        ReportFatal(
            DescribeErrors("Invalid gameplay definitions in " + ResolveContentPath(options.gameplayPath) + ":", errors),
            options.errorDialogs);
        return false;
    }
    if (!LoadCampaignDefinition(options.campaignPath, &errors)) {
        ReportFatal(
            DescribeErrors("Invalid campaign manifest " + ResolveContentPath(options.campaignPath) + ":", errors),
            options.errorDialogs);
        return false;
    }
    const int levelCount = static_cast<int>(campaign_.levels.size());
    if (options.startLevel < 0 || options.startLevel > levelCount) {
        ReportFatal("--level " + std::to_string(options.startLevel) + ": the campaign has " +
                        std::to_string(levelCount) + " level(s).",
                    options.errorDialogs);
        return false;
    }
    return true;
}

// The window and renderer (none in headless mode).
bool Engine::InitWindow(const EngineOptions& options) {
    if (options.headless) {
        return true;
    }
    auto platform = std::make_unique<PlatformSDL>();
    PlatformConfig platformConfig{};
    platformConfig.title = "Neo-Wolf";
    const RenderSize& size = options.renderSize;
    const RenderSize window = InitialWindowSize(size);
    platformConfig.windowWidth = window.width;
    platformConfig.windowHeight = window.height;
    platformConfig.logicalWidth = size.width;
    platformConfig.logicalHeight = size.height;
    platformConfig.fullscreen = options.fullscreen;
    if (!platform->Init(platformConfig)) {
        ReportFatal("Could not open the game window (see the log for the SDL error).", options.errorDialogs);
        return false;
    }
    platform_ = std::move(platform);
    return true;
}

// Sprite and audio definitions, then the sprite textures and sounds they name. Missing or
// broken files fall back to defaults with a warning. (Wall, door, floor, ceiling and exit
// textures come from each level's theme.)
void Engine::InitAssets() {
    std::vector<std::string> errors;
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

    LoadSpriteTextures(spriteDefinitions);
    if (audio_.IsAvailable()) {
        LoadSoundEffects(audioDefinitions);
    }
}

// A checkerboard stands in for any sprite texture that fails to load.
void Engine::LoadSpriteTextures(const SpriteTextureDefinitions& definitions) {
    assets_.Clear();
    currentThemeId_.clear();
    TextureLoader::BuildFallbackChecker(missingTexture_, kMissingTextureSize, kMissingTextureSize);
    assets_.SetTexture("__missing", missingTexture_);

    std::vector<std::string> errors;
    for (int i = 0; i < kSpriteTextureCount; ++i) {
        const std::string id = kSpriteTextureKeys[i];
        const std::string& relativePath = definitions.paths[static_cast<std::size_t>(i)];
        if (!assets_.LoadTextureFromPPM(id, ResolveContentPath(relativePath), errors)) {
            LogLoadErrors(errors);
            LogWarning("Using fallback texture for: " + id);
            assets_.SetTexture(id, missingTexture_);
        }
    }
    RefreshTexturePointers();
}

void Engine::LoadSoundEffects(const AudioEffectDefinitions& definitions) {
    std::vector<std::string> errors;
    std::size_t soundsLoaded = 0;
    const auto loadSound = [&](const std::string& soundId, const std::string& relativePath) {
        if (relativePath.empty()) {
            return;
        }
        if (audio_.LoadEffectFromWav(soundId, ResolveContentPath(relativePath), errors)) {
            ++soundsLoaded;
        }
        LogLoadErrors(errors);
    };
    loadSound("door_open", definitions.doorOpen);
    loadSound("pickup", definitions.pickup);
    loadSound("enemy_alert", definitions.enemyAlert);
    loadSound("weapon_fire", definitions.weaponFire);
    LogInfo("Sound effects loaded: " + std::to_string(soundsLoaded));
}

bool Engine::BootsIntoTestLevel(const EngineOptions& options) const {
    return options.testLevelBoot && campaign_.bootIntoTestLevel && campaign_.testLevel.has_value() &&
           options.startLevel == 0 && options.preview == PreviewScreen::None;
}

// --level N starts its level directly; campaign.json's "bootIntoTestLevel" starts the test level
// (the game only, see EngineOptions::testLevelBoot); otherwise level 1 is loaded behind the main
// menu (New Game reloads it fresh). Failing here is fatal: there is nothing to play.
bool Engine::InitFirstLevel(const EngineOptions& options) {
    int firstIndex = options.startLevel > 0 ? options.startLevel - 1 : 0;
    PlayerStats firstStats{gameplay_.player.initialHealth, gameplay_.weapon.initialAmmo};
    if (BootsIntoTestLevel(options)) {
        LogInfo("Booting into the test level (campaign.json \"bootIntoTestLevel\")");
        firstIndex = kTestLevelIndex;
        firstStats = flow_.StartTestLevel().stats;  // Gameplay in the test level
    } else if (options.startLevel > 0) {
        firstStats = flow_.StartLevelDirect(firstIndex).stats;  // --level: Gameplay, unlocks ignored
    }
    if (!LoadCampaignLevel(firstIndex, firstStats)) {
        const bool testLevel = firstIndex == kTestLevelIndex;
        const CampaignLevel* level =
            testLevel ? TestLevelDefinition() : &campaign_.levels[static_cast<std::size_t>(firstIndex)];
        const std::string what = testLevel ? "the test level" : "level " + std::to_string(firstIndex + 1);
        ReportFatal("Could not load " + what + " (" + (level != nullptr ? level->map : std::string()) + ") from " +
                        PathToUtf8(contentRoot_) + " (see the log for details).",
                    options.errorDialogs);
        return false;
    }
    return true;
}
