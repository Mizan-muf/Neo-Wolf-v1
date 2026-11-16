// Engine: campaign levels, texture themes, saved progress, game-flow requests and previews.

#include "engine/core/Engine.h"

#include <string>
#include <utility>
#include <vector>

#include "engine/assets/ContentConfig.h"
#include "engine/assets/TextureLoader.h"
#include "engine/core/Log.h"
#include "engine/core/Paths.h"
#include "engine/core/Progress.h"
#include "engine/world/LevelBuilder.h"

namespace {
constexpr float kLevelBannerSeconds = 3.0f;

struct ThemeSlot {
    const char* textureId;
    const std::string ThemeTextureAssignments::* path;
};

constexpr ThemeSlot kThemeSlots[] = {
    {"wall", &ThemeTextureAssignments::wall},           {"wall_alt_a", &ThemeTextureAssignments::wallAltA},
    {"wall_alt_b", &ThemeTextureAssignments::wallAltB}, {"door", &ThemeTextureAssignments::door},
    {"door_red", &ThemeTextureAssignments::doorRed},    {"door_blue", &ThemeTextureAssignments::doorBlue},
    {"door_gold", &ThemeTextureAssignments::doorGold},  {"floor", &ThemeTextureAssignments::floor},
    {"ceiling", &ThemeTextureAssignments::ceiling},     {"exit", &ThemeTextureAssignments::exit},
};

// The fixed stage of the "enemies" and "items" previews: the hall of the Phase 7 level 1
// (a door at (8, 7), a trigger at (13, 8), the exit switch at (15, 8)), built in code so the
// reference screenshots do not change when the campaign's first level is redrawn (Phase 8).
constexpr int kShowcaseWidth = 16;
constexpr int kShowcaseHeight = 12;
constexpr const char* kShowcaseRows[kShowcaseHeight] = {
    "################", "#..............#", "#..............#", "#..##A####B##..#",
    "#..............#", "#.......A......#", "#..............#", "#.......D......#",
    "#............T.X", "#.......B......#", "#..............#", "################",
};

TileType ShowcaseTile(char c) {
    switch (c) {
        case '#':
            return TileType::Wall;
        case 'A':
            return TileType::WallAltA;
        case 'B':
            return TileType::WallAltB;
        case 'D':
            return TileType::Door;
        case 'T':
            return TileType::Trigger;
        case 'X':
            return TileType::Exit;
        default:
            return TileType::Empty;
    }
}

EntityPlacement ShowcasePlacement(PlacementType type, Vec2 position, int mapX, int mapY) {
    EntityPlacement placement{};
    placement.type = type;
    placement.position = position;
    placement.mapX = mapX;
    placement.mapY = mapY;
    placement.source = "preview stage";
    return placement;
}
}  // namespace

bool Engine::LoadCampaignDefinition(const std::string& relativePath, std::vector<std::string>* outErrors) {
    const std::string resolvedPath = ResolveContentPath(relativePath);
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    CampaignDefinition campaign{};
    const bool loaded = LoadCampaign(resolvedPath, contentRoot_, campaign, errors, warnings);
    for (const std::string& warning : warnings) {
        LogWarning("  " + warning);
    }
    if (!loaded) {
        LogError("Invalid campaign manifest " + resolvedPath + ":");
        for (const std::string& error : errors) {
            LogError("  " + error);
        }
        if (outErrors != nullptr) {
            *outErrors = errors;
        }
        return false;
    }
    campaign_ = std::move(campaign);
    std::string names;
    for (std::size_t i = 0; i < campaign_.levels.size(); ++i) {
        names += (i == 0 ? "" : ", ") + campaign_.levels[i].id + " (" + campaign_.levels[i].theme + ")";
    }
    LogInfo("Campaign \"" + campaign_.title + "\" loaded from " + resolvedPath + ": " +
            std::to_string(campaign_.levels.size()) + " level(s): " + names);
    ConfigureFlow();
    return true;
}

void Engine::ConfigureFlow() {
    GameFlowConfig config{};
    config.levelCount = static_cast<int>(campaign_.levels.size());
    config.initial = PlayerStats{gameplay_.player.initialHealth, gameplay_.weapon.initialAmmo};
    config.maximum = PlayerStats{gameplay_.player.maxHealth, gameplay_.weapon.maxAmmo};
    const bool hadProgress = flow_.HasProgress();
    const int highest = flow_.HighestUnlocked();
    flow_.Configure(config);
    flow_.SetProgress(hadProgress, highest);
}

void Engine::LoadSavedProgress() {
    if (prefDir_.empty()) {
        LogInfo("No per-user folder: progress is not loaded or saved.");
        return;
    }
    const std::string path = PathToUtf8(prefDir_ / kProgressFileName);
    ProgressData data{};
    std::string message;
    switch (LoadProgressFile(path, data, message)) {
        case ProgressLoadResult::Loaded:
            flow_.SetProgress(true, data.highestUnlocked);
            LogInfo("Saved " + message + " (campaign has " + std::to_string(flow_.LevelCount()) + ")");
            break;
        case ProgressLoadResult::Missing:
            flow_.SetProgress(false, 1);
            LogInfo("Progress: " + message);
            break;
        case ProgressLoadResult::Invalid:
            flow_.SetProgress(false, 1);
            LogWarning("Ignoring the saved progress (starting without it): " + message);
            break;
    }
}

void Engine::SaveProgress(int highestUnlocked) {
    if (prefDir_.empty()) {
        LogInfo("Progress not saved (no per-user folder).");
        return;
    }
    const std::string path = PathToUtf8(prefDir_ / kProgressFileName);
    ProgressData data{};
    data.highestUnlocked = highestUnlocked;
    std::string error;
    if (SaveProgressFile(path, data, error)) {
        LogInfo("Progress saved to " + path + ": highest unlocked level " + std::to_string(highestUnlocked));
    } else {
        LogWarning("Could not save progress: " + error);
    }
}

void Engine::RefreshTexturePointers() {
    // AssetManager keeps each texture in a stable map node, so these pointers stay valid when
    // a texture is replaced; they are refreshed anyway after every swap.
    wallTexture_ = assets_.GetTexture("wall");
    wallAltATexture_ = assets_.GetTexture("wall_alt_a");
    wallAltBTexture_ = assets_.GetTexture("wall_alt_b");
    doorTexture_ = assets_.GetTexture("door");
    lockedDoorTextures_ = {assets_.GetTexture("door_red"), assets_.GetTexture("door_blue"),
                           assets_.GetTexture("door_gold")};
    floorTexture_ = assets_.GetTexture("floor");
    ceilingTexture_ = assets_.GetTexture("ceiling");
    exitTexture_ = assets_.GetTexture("exit");
    for (int i = 0; i < kSpriteTextureCount; ++i) {
        spriteTextures_[static_cast<std::size_t>(i)] = assets_.GetTexture(kSpriteTextureKeys[i]);
    }
}

bool Engine::ApplyTheme(const std::string& themeId) {
    if (themeId == currentThemeId_) {
        return true;
    }
    if (missingTexture_.Width() <= 0) {
        TextureLoader::BuildFallbackChecker(missingTexture_, 64, 64);
    }

    const std::string configPath = ResolveContentPath(ThemeConfigPath(themeId));
    std::vector<std::string> errors;
    ThemeTextureAssignments assignments{};
    bool complete = LoadThemeTextureAssignments(configPath, assignments, errors);
    for (const std::string& error : errors) {
        LogWarning(error);
    }

    // Load the whole set first; only then replace the textures in use.
    std::vector<std::pair<std::string, Texture>> loaded;
    for (const ThemeSlot& slot : kThemeSlots) {
        const std::string& relativePath = assignments.*(slot.path);
        Texture texture{};
        if (relativePath.empty() || !TextureLoader::LoadPPM(ResolveContentPath(relativePath), texture)) {
            LogWarning("Theme " + themeId + ": using the fallback texture for '" + slot.textureId + "'" +
                       (relativePath.empty() ? std::string() : " (cannot load " + relativePath + ")"));
            texture = missingTexture_;
            complete = false;
        }
        loaded.emplace_back(slot.textureId, std::move(texture));
    }
    for (const auto& entry : loaded) {
        assets_.SetTexture(entry.first, entry.second);
    }
    RefreshTexturePointers();
    currentThemeId_ = themeId;
    LogInfo("Texture theme \"" + themeId + "\" applied" + (complete ? "." : " (with fallbacks)."));
    return complete;
}

bool Engine::LoadCampaignLevel(int levelIndex, const PlayerStats& stats) {
    if (levelIndex < 0) {
        if (!LoadLevel(currentMapPath_, currentEntitiesPath_)) {
            return false;
        }
    } else {
        if (levelIndex >= static_cast<int>(campaign_.levels.size())) {
            LogError("Campaign level " + std::to_string(levelIndex + 1) + " does not exist (the campaign has " +
                     std::to_string(campaign_.levels.size()) + ").");
            return false;
        }
        const CampaignLevel& level = campaign_.levels[static_cast<std::size_t>(levelIndex)];
        LogInfo("Starting level " + std::to_string(levelIndex + 1) + " \"" + level.name + "\" (" + level.id +
                ") with health " + std::to_string(stats.health) + ", ammo " + std::to_string(stats.ammo));
        if (!LoadLevel(level.map, level.entities)) {
            return false;
        }
        ApplyTheme(level.theme);
        currentLevelIndex_ = levelIndex;
        levelBannerTimer_ = kLevelBannerSeconds;
    }
    playerHealth_ = stats.health;
    playerAmmo_ = stats.ammo;
    return true;
}

const CampaignLevel* Engine::CurrentCampaignLevel() const {
    if (currentLevelIndex_ < 0 || currentLevelIndex_ >= static_cast<int>(campaign_.levels.size())) {
        return nullptr;
    }
    return &campaign_.levels[static_cast<std::size_t>(currentLevelIndex_)];
}

LevelResults Engine::CurrentLevelResults() const {
    LevelResults results{};
    results.totalEnemies = levelEnemyTotal_;
    results.totalPickups = levelPickupTotal_;
    results.seconds = levelTimeSeconds_;
    results.shotsFired = levelShotsFired_;
    results.damageTaken = levelDamageTaken_;
    for (const Entity& entity : entityManager_.Entities()) {
        if (entity.type == EntityType::Enemy && entity.enemyState == EnemyState::Dead) {
            ++results.kills;
            results.score += entity.score;
        } else if (entity.type == EntityType::Pickup && entity.collected) {
            ++results.pickups;
        }
    }
    return results;
}

void Engine::ExecuteFlowRequests(const std::vector<FlowRequest>& requests) {
    for (const FlowRequest& request : requests) {
        switch (request.type) {
            case FlowRequestType::LoadLevel:
            case FlowRequestType::RestartLevel:
                if (!LoadCampaignLevel(request.levelIndex, request.stats)) {
                    LogError("Could not start the level; back to the main menu.");
                    flow_.ForceState(GameState::Menu);
                }
                break;
            case FlowRequestType::SaveProgress:
                SaveProgress(request.highestUnlocked);
                break;
            case FlowRequestType::Quit:
                LogInfo("Quit from the main menu.");
                running_ = false;
                break;
        }
    }
}

bool Engine::SetupPreview(PreviewScreen screen) {
    if (screen == PreviewScreen::None) {
        return true;
    }
    LogInfo(std::string("Preview: ") + PreviewScreenName(screen) + " (sample data; the simulation is frozen).");
    // Sample progress: level 2 unlocked, later levels locked. Nothing is saved.
    flow_.SetProgress(true, 2);
    simulationFrozen_ = true;

    const auto startLevel = [this](int index) {
        ExecuteFlowRequests({flow_.StartLevelDirect(index)});
        return flow_.State() == GameState::Gameplay;
    };
    const auto startLevelOne = [&startLevel]() { return startLevel(0); };

    switch (screen) {
        case PreviewScreen::None:
            break;
        case PreviewScreen::Menu:
            flow_.SetMenuPage(MenuPage::Main, 0);
            break;
        case PreviewScreen::LevelSelect:
            flow_.SetMenuPage(MenuPage::LevelSelect, 1);
            break;
        case PreviewScreen::Hud:
        case PreviewScreen::Minimap:
            if (!startLevelOne()) {
                return false;
            }
            playerHealth_ = 75;
            playerAmmo_ = 18;
            if (screen == PreviewScreen::Hud) {
                ShowMessage("HEALTH RESTORED (+25 HP)", 60.0f);
            } else {
                levelBannerTimer_ = 0.0f;
                minimapVisible_ = true;
            }
            break;
        case PreviewScreen::Pause:
            if (!startLevelOne()) {
                return false;
            }
            flow_.ForceState(GameState::Paused);
            break;
        case PreviewScreen::LevelComplete: {
            if (!startLevelOne()) {
                return false;
            }
            LevelResults results{};
            results.kills = 1;
            results.totalEnemies = 1;
            results.pickups = 1;
            results.totalPickups = 1;
            results.seconds = 42.0;
            flow_.SetLastResults(results);
            flow_.ForceState(GameState::LevelComplete);
            break;
        }
        case PreviewScreen::GameOver:
            if (!startLevelOne()) {
                return false;
            }
            playerHealth_ = 0;
            flow_.ForceState(GameState::GameOver);
            break;
        case PreviewScreen::CampaignComplete:
            flow_.ForceState(GameState::CampaignComplete);
            break;
        case PreviewScreen::Enemies:
        case PreviewScreen::Items:
            // A fixed hall (not the campaign's level 1, which Phase 8 redraws): the preview locks
            // its door and puts a fixed line-up in front of the player.
            if (!startLevelOne() || !SetupShowcase(screen == PreviewScreen::Enemies)) {
                return false;
            }
            break;
    }
    PublishStateChange();
    return true;
}

bool Engine::SetupShowcase(bool enemies) {
    // Replace level 1 with the stage: its map, the player looking east along the hall at the
    // door, the door and the trigger; nothing else. The bunker theme, whatever level 1 uses.
    Map stage(kShowcaseWidth, kShowcaseHeight);
    for (int y = 0; y < kShowcaseHeight; ++y) {
        for (int x = 0; x < kShowcaseWidth; ++x) {
            stage.SetCell(x, y, ShowcaseTile(kShowcaseRows[y][x]));
        }
    }
    std::vector<EntityPlacement> placements = {
        ShowcasePlacement(PlacementType::Player, {2.5f, 7.5f}, -1, -1),
        ShowcasePlacement(PlacementType::Door, {8.5f, 7.5f}, 8, 7),
        ShowcasePlacement(PlacementType::Trigger, {13.5f, 8.5f}, 13, 8),
    };
    LevelContents contents{};
    std::vector<std::string> errors;
    if (!BuildLevelFromPlacements(stage, placements, gameplay_, contents, errors)) {
        LogError("Preview stage could not be built:");
        for (const std::string& error : errors) {
            LogError("  " + error);
        }
        return false;
    }
    map_ = std::move(stage);
    entityManager_ = std::move(contents.entities);
    sprites_ = std::move(contents.sprites);
    player_ = contents.player;
    ApplyTheme("bunker");

    Entity* door = nullptr;
    for (Entity& entity : entityManager_.Entities()) {
        if (entity.type == EntityType::Door && door == nullptr) {
            door = &entity;
        }
    }
    const KeyColor lock = enemies ? KeyColor::Blue : KeyColor::Red;
    if (door != nullptr) {
        door->doorLock = lock;
        map_.SetDoorLock(door->mapX, door->mapY, lock);
    }
    levelBannerTimer_ = 0.0f;
    playerHealth_ = 82;
    playerAmmo_ = 31;

    if (enemies) {
        // One enemy of each type and a guard's corpse on the floor.
        const struct {
            EnemyVariant variant;
            Vec2 position;
        } lineUp[] = {{EnemyVariant::Guard, {6.0f, 6.0f}},
                      {EnemyVariant::Runner, {5.5f, 6.95f}},
                      {EnemyVariant::Brute, {5.8f, 8.9f}},
                      {EnemyVariant::Guard, {4.6f, 8.0f}}};
        for (const auto& placement : lineUp) {
            Entity& enemy = entityManager_.CreateEntity(EntityType::Enemy, placement.position);
            ApplyEnemyArchetype(enemy, placement.variant, gameplay_.enemies);
        }
        Entity& corpse = entityManager_.Entities().back();
        corpse.enemyState = EnemyState::Dead;
        corpse.active = false;
        ShowMessage("NEED BLUE KEY", 60.0f);
    } else {
        // Health, ammo and the three key cards; every key held; minimap on.
        const struct {
            PickupType kind;
            KeyColor key;
            Vec2 position;
        } items[] = {{PickupType::Health, KeyColor::None, {7.0f, 5.6f}},
                     {PickupType::Key, KeyColor::Red, {7.0f, 6.4f}},
                     {PickupType::Key, KeyColor::Blue, {7.0f, 7.5f}},
                     {PickupType::Key, KeyColor::Gold, {7.0f, 8.6f}},
                     {PickupType::Ammo, KeyColor::None, {7.0f, 9.4f}}};
        for (const auto& item : items) {
            Entity& pickup = entityManager_.CreateEntity(EntityType::Pickup, item.position);
            pickup.pickupType = item.kind;
            pickup.keyColor = item.key;
            pickup.pickupValue =
                item.kind == PickupType::Ammo ? gameplay_.pickups.ammo.value : gameplay_.pickups.health.value;
        }
        for (const KeyColor color : kKeyColors) {
            keys_.Add(color);
        }
        minimapVisible_ = true;
        ShowMessage("GOLD KEY CARD", 60.0f);
    }
    SyncSpritesWithEntities(entityManager_, sprites_);
    return true;
}
