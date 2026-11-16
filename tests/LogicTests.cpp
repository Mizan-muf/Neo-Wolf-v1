// The native test suite: unit and integration tests of the engine, run by CTest as
// native.<id> (one process per test case; the list is AllTests() at the end of this file).
#include <SDL.h>

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/assets/AssetManager.h"
#include "engine/assets/AudioConfig.h"
#include "engine/assets/CampaignConfig.h"
#include "engine/assets/ContentConfig.h"
#include "engine/assets/GameplayConfig.h"
#include "engine/assets/Texture.h"
#include "engine/assets/TextureLoader.h"
#include "engine/autoplay/AutoplayBot.h"
#include "engine/autoplay/AutoplayReport.h"
#include "engine/core/Color.h"
#include "engine/core/CommandLine.h"
#include "engine/core/Engine.h"
#include "engine/core/GameFlow.h"
#include "engine/core/GameState.h"
#include "engine/core/InputState.h"
#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/core/Paths.h"
#include "engine/core/PreviewScreen.h"
#include "engine/core/Progress.h"
#include "engine/core/RenderSize.h"
#include "engine/core/Vec2.h"
#include "engine/interop/CommandBridge.h"
#include "engine/interop/EngineInterop.h"
#include "engine/particles/Particle.h"
#include "engine/platform/PlatformSDL.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/ParticleRenderer.h"
#include "engine/render/Raycaster.h"
#include "engine/render/SpriteRenderer.h"
#include "engine/ui/UiRenderer.h"
#include "engine/world/EnemyAi.h"
#include "engine/world/Entity.h"
#include "engine/world/EntityPlacementLoader.h"
#include "engine/world/GameplayDefinitions.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/LevelBuilder.h"
#include "engine/world/Map.h"
#include "engine/world/MapLoader.h"
#include "engine/world/MovementRules.h"
#include "engine/world/Navigation.h"
#include "engine/world/Player.h"
#include "engine/world/Sprite.h"

// Test harness
// ------------
// A failed assertion records file:line and the expression, then throws TestFailure so the
// current test case stops and the runner moves on to the next one. The process exits
// non-zero if any test case failed.
namespace {
struct TestFailure {
    std::string message;
};

[[noreturn]] void FailTest(const char* file, int line, const std::string& what) {
    std::ostringstream out;
    out << file << ":" << line << ": " << what;
    // TestFailure is deliberately not a std::exception, so no catch (const std::exception&) in
    // the code under test can swallow a failed assertion.
    throw TestFailure{out.str()};  // NOLINT(bugprone-std-exception-baseclass): see above
}
}  // namespace

#define TEST_ASSERT(expr)                                                  \
    do {                                                                   \
        if (!(expr)) {                                                     \
            FailTest(__FILE__, __LINE__, "TEST_ASSERT(" #expr ") failed"); \
        }                                                                  \
    } while (false)

#define TEST_ASSERT_NEAR(a, b, eps)                                                                                \
    do {                                                                                                           \
        const auto nwActualA = (a);                                                                                \
        const auto nwActualB = (b);                                                                                \
        if (!(std::fabs(nwActualA - nwActualB) <= (eps))) {                                                        \
            std::ostringstream nwDetail;                                                                           \
            nwDetail << "TEST_ASSERT_NEAR(" #a ", " #b ", " #eps ") failed: " << nwActualA << " vs " << nwActualB; \
            FailTest(__FILE__, __LINE__, nwDetail.str());                                                          \
        }                                                                                                          \
    } while (false)

namespace {
// Resolves a repository-relative path. NW_SOURCE_DIR is set by CMake to the repo root, so
// the tests work from any working directory; the cwd and its parent are fallbacks.
std::string ResolveRepoPath(const std::string& relativePath) {
#ifdef NW_SOURCE_DIR
    const std::filesystem::path fromSourceDir = std::filesystem::path(NW_SOURCE_DIR) / relativePath;
    if (std::filesystem::exists(fromSourceDir)) {
        return fromSourceDir.string();
    }
#endif
    if (std::filesystem::exists(relativePath)) {
        return relativePath;
    }
    std::string candidate = "../" + relativePath;
    if (std::filesystem::exists(candidate)) {
        return candidate;
    }
    return relativePath;
}

// The repository root (the content root of the shipped game).
std::filesystem::path RepoRoot() {
    return std::filesystem::absolute(std::filesystem::path(ResolveRepoPath("game/defs/campaign.json")))
        .parent_path()
        .parent_path()
        .parent_path();
}

// The Phase 7 campaign (level01..level03 at commit 940f3bd), frozen as a test fixture in
// tests/fixtures/reference/game/. Tests that need exact level details (level 1's door at
// (8, 7), level 2's blue key, level 3's brute) read it instead of the shipped levels, so
// Phase 8 can redraw game/ without touching them. Tests about the shipped campaign as a whole
// read game/defs/campaign.json (ShippedCampaign) and hold for any number of levels.
std::string ReferencePath(const std::string& relativePath) {
    return ResolveRepoPath("tests/fixtures/reference/" + relativePath);
}

void BuildBorderWalls(Map& map) {
    for (int x = 0; x < map.Width(); ++x) {
        map.SetCell(x, 0, TileType::Wall);
        map.SetCell(x, map.Height() - 1, TileType::Wall);
    }
    for (int y = 0; y < map.Height(); ++y) {
        map.SetCell(0, y, TileType::Wall);
        map.SetCell(map.Width() - 1, y, TileType::Wall);
    }
}

// Facts about the frozen reference level 1 (tests/fixtures/reference/game/maps/level01.map
// and defs/level01.entities.json) that several tests rely on.
namespace ReferenceLevel1 {
constexpr int kWidth = 16;
constexpr int kHeight = 12;
constexpr Vec2i kSpawnCell{2, 2};  // the player starts at (2.5, 2.5)
constexpr float kSpawnX = 2.5f;
constexpr float kGuardX = 6.5f;  // its only enemy, a guard at (6.5, 2.7) in the hall
constexpr int kDoorX = 8;        // its only door, unlocked
constexpr int kDoorY = 7;
constexpr int kTriggerX = 13;  // the walk-over trigger in front of the exit switch
constexpr int kTriggerY = 8;
constexpr int kExitX = 15;  // the exit switch on the east wall
constexpr int kExitY = 8;
constexpr std::size_t kPlacementCount = 7;  // entities in level01.entities.json (and the .csv export)
constexpr std::size_t kSpriteCount = 4;     // the guard, two pickups and the decoration
}  // namespace ReferenceLevel1

// The player maxima the interaction tests pass to UpdateEntityInteractions and GameFlow (the
// shipped gameplay.json values).
constexpr int kPlayerMaxHealth = 100;
constexpr int kPlayerMaxAmmo = 99;

// One 60 Hz frame, the engine's fixed update step: Engine::Update takes it as a double, the
// AI functions as a float.
constexpr double kFrameSeconds = 1.0 / 60.0;
constexpr float kAiFrameSeconds = 1.0f / 60.0f;

// A framebuffer pixel, 0xAARRGGBB (Framebuffer::PackRGBA), and its colour channels.
std::uint32_t PixelAt(const Framebuffer& framebuffer, int x, int y) {
    return framebuffer.Data()[static_cast<std::size_t>(y) * static_cast<std::size_t>(framebuffer.Width()) +
                              static_cast<std::size_t>(x)];
}
int RedOf(std::uint32_t pixel) {
    return static_cast<int>((pixel >> 16u) & 0xFFu);
}
int GreenOf(std::uint32_t pixel) {
    return static_cast<int>((pixel >> 8u) & 0xFFu);
}
int BlueOf(std::uint32_t pixel) {
    return static_cast<int>(pixel & 0xFFu);
}

void TestVec2AndMathHelpers() {
    const Vec2 a{1.5f, 2.0f};
    const Vec2 b{0.5f, -1.0f};

    const Vec2 sum = a + b;
    TEST_ASSERT_NEAR(sum.x, 2.0f, 0.0001f);
    TEST_ASSERT_NEAR(sum.y, 1.0f, 0.0001f);

    const Vec2 diff = a - b;
    TEST_ASSERT_NEAR(diff.x, 1.0f, 0.0001f);
    TEST_ASSERT_NEAR(diff.y, 3.0f, 0.0001f);

    const Vec2 scaled = a * 2.0f;
    TEST_ASSERT_NEAR(scaled.x, 3.0f, 0.0001f);
    TEST_ASSERT_NEAR(scaled.y, 4.0f, 0.0001f);

    TEST_ASSERT_NEAR(LengthSquared(a), 6.25f, 0.0001f);
    TEST_ASSERT_NEAR(DistanceSquared(a, b), 10.0f, 0.0001f);

    TEST_ASSERT_NEAR(Clamp(5.0f, 0.0f, 3.0f), 3.0f, 0.0001f);
    TEST_ASSERT_NEAR(Clamp(-2.0f, -1.0f, 3.0f), -1.0f, 0.0001f);
    TEST_ASSERT_NEAR(Clamp(1.5f, 0.0f, 3.0f), 1.5f, 0.0001f);

    // kPi comes from engine/core/MathUtils.h (a local copy would shadow it: MSVC C4459).
    constexpr float kTwoPi = kPi * 2.0f;

    TEST_ASSERT_NEAR(NormalizeAngle(0.0f), 0.0f, 0.0001f);
    TEST_ASSERT_NEAR(NormalizeAngle(kTwoPi + 0.5f), 0.5f, 0.0001f);
    TEST_ASSERT_NEAR(NormalizeAngle(-0.5f), kTwoPi - 0.5f, 0.0001f);
}

void TestInputStateTransitions() {
    InputState state{};
    state.BeginFrame();

    TEST_ASSERT(!state.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!state.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!state.WasReleased(InputAction::MoveForward));

    state.SetActionState(InputAction::MoveForward, true);
    TEST_ASSERT(state.IsDown(InputAction::MoveForward));
    TEST_ASSERT(state.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!state.WasReleased(InputAction::MoveForward));

    state.BeginFrame();
    TEST_ASSERT(state.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!state.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!state.WasReleased(InputAction::MoveForward));

    state.SetActionState(InputAction::MoveForward, true);
    TEST_ASSERT(state.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!state.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!state.WasReleased(InputAction::MoveForward));

    state.SetActionState(InputAction::MoveForward, false);
    TEST_ASSERT(!state.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!state.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(state.WasReleased(InputAction::MoveForward));
}

void TestMapQueries() {
    Map map(16, 12);
    TEST_ASSERT(map.Width() == 16);
    TEST_ASSERT(map.Height() == 12);

    TEST_ASSERT(!map.IsWall(0, 0));
    TEST_ASSERT(!map.IsWall(15, 11));

    map.SetCell(0, 0, TileType::Wall);
    map.SetCell(3, 4, TileType::Door);
    map.SetCell(5, 5, TileType::WallAltA);
    map.SetCell(6, 5, TileType::WallAltB);

    TEST_ASSERT(map.IsWall(0, 0));
    TEST_ASSERT(map.IsWall(3, 4));
    TEST_ASSERT(map.IsWall(5, 5));
    TEST_ASSERT(map.IsWall(6, 5));
    TEST_ASSERT(!map.IsWall(1, 1));

    TEST_ASSERT(map.IsWall(-1, 0));
    TEST_ASSERT(map.IsWall(0, -1));
    TEST_ASSERT(map.IsWall(16, 0));
    TEST_ASSERT(map.IsWall(0, 12));

    TEST_ASSERT(map.GetCell(3, 4) == TileType::Door);
    TEST_ASSERT(map.GetCell(5, 5) == TileType::WallAltA);
    TEST_ASSERT(map.GetCell(6, 5) == TileType::WallAltB);
    TEST_ASSERT(map.GetCell(-2, -2) == TileType::Wall);

    // The exit switch is solid (movement, rays) like a wall.
    map.SetCell(7, 5, TileType::Exit);
    TEST_ASSERT(map.IsWall(7, 5));
    TEST_ASSERT(map.GetCell(7, 5) == TileType::Exit);
    TEST_ASSERT(static_cast<int>(TileType::Exit) == 6);  // appended
    map.SetCell(8, 5, TileType::Trigger);
    TEST_ASSERT(!map.IsWall(8, 5));  // a trigger stays walkable
}

void TestRaycastHitDistance() {
    Map map(16, 12);
    BuildBorderWalls(map);

    const Vec2 origin{2.5f, 2.5f};
    const Vec2 dirEast{1.0f, 0.0f};

    const RaycastHit hitEast = Raycaster::CastRay(map, origin, dirEast, 0.0f);
    TEST_ASSERT(hitEast.hit);
    TEST_ASSERT(hitEast.mapX == 15);
    TEST_ASSERT(hitEast.mapY == 2);
    TEST_ASSERT_NEAR(hitEast.distanceRaw, 12.5f, 0.001f);
    TEST_ASSERT_NEAR(hitEast.distanceCorrected, 12.5f, 0.001f);

    const Vec2 dirNorth{0.0f, -1.0f};
    constexpr float kAngleNorth = 4.71238898038469f;
    const RaycastHit hitNorth = Raycaster::CastRay(map, origin, dirNorth, kAngleNorth);
    TEST_ASSERT(hitNorth.hit);
    TEST_ASSERT(hitNorth.mapX == 2);
    TEST_ASSERT(hitNorth.mapY == 0);
    TEST_ASSERT_NEAR(hitNorth.distanceRaw, 1.5f, 0.001f);
    TEST_ASSERT_NEAR(hitNorth.distanceCorrected, 1.5f, 0.001f);
}

void TestTextureLoaderAndFallback() {
    Texture fallback{};
    TextureLoader::BuildFallbackChecker(fallback, 16, 16);
    TEST_ASSERT(fallback.Width() == 16);
    TEST_ASSERT(fallback.Height() == 16);
    TEST_ASSERT(fallback.GetPixel(0, 0) != fallback.GetPixel(8, 0));

    Texture fileTexture{};
    const std::string validPath = ResolveRepoPath("game/assets/themes/classic/wall.ppm");
    const bool loaded = TextureLoader::LoadPPM(validPath, fileTexture);
    TEST_ASSERT(loaded);
    TEST_ASSERT(fileTexture.Width() > 0);
    TEST_ASSERT(fileTexture.Height() > 0);
}

void TestEntityManagerBasics() {
    EntityManager manager{};
    TEST_ASSERT(manager.Entities().empty());

    const Entity& e1 = manager.CreateEntity(EntityType::Enemy, Vec2{3.0f, 4.0f});
    const int id1 = e1.id;
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(e1.type == EntityType::Enemy);
    TEST_ASSERT_NEAR(e1.position.x, 3.0f, 0.0001f);
    TEST_ASSERT_NEAR(e1.position.y, 4.0f, 0.0001f);

    const Entity& e2 = manager.CreateEntity(EntityType::Pickup, Vec2{7.0f, 8.0f});
    TEST_ASSERT(e2.id > id1);
    TEST_ASSERT(manager.Entities().size() == 2);

    const Entity* fetched = manager.GetEntityById(id1);
    TEST_ASSERT(fetched != nullptr);
    TEST_ASSERT(fetched->id == id1);

    TEST_ASSERT(manager.DestroyEntity(id1));
    TEST_ASSERT(manager.GetEntityById(id1) == nullptr);
    TEST_ASSERT(manager.Entities().size() == 1);

    manager.Clear();
    TEST_ASSERT(manager.Entities().empty());
}

void TestPlayerCollisionRules() {
    Map map(16, 12);
    BuildBorderWalls(map);

    Player player{};
    player.position = {2.5f, 2.5f};
    player.collisionRadius = 0.22f;

    player.position = MoveWithSliding(map, player.position, Vec2{0.5f, 0.0f}, player.collisionRadius);
    TEST_ASSERT_NEAR(player.position.x, 3.0f, 0.0001f);
    TEST_ASSERT_NEAR(player.position.y, 2.5f, 0.0001f);

    player.position = MoveWithSliding(map, player.position, Vec2{20.0f, 0.0f}, player.collisionRadius);
    TEST_ASSERT(player.position.x < 15.0f);
    TEST_ASSERT(player.position.x > 14.0f);
    TEST_ASSERT_NEAR(player.position.y, 2.5f, 0.0001f);
}

void TestSpriteDistanceSorting() {
    Player player{};
    player.position = {2.0f, 2.0f};

    const std::vector<SpriteEntity> sprites{
        SpriteEntity{{6.0f, 2.0f}, 0, -1, SpriteType::Enemy, true, true},       // dist^2=16
        SpriteEntity{{3.0f, 2.0f}, 1, -1, SpriteType::Pickup, true, true},      // dist^2=1
        SpriteEntity{{2.0f, 5.0f}, 2, -1, SpriteType::Decoration, true, true},  // dist^2=9
    };

    const std::vector<std::size_t> sorted = SpriteRenderer::BuildSortedSpriteIndices(player, sprites);
    TEST_ASSERT(sorted.size() == 3);
    TEST_ASSERT(sorted[0] == 0);
    TEST_ASSERT(sorted[1] == 2);
    TEST_ASSERT(sorted[2] == 1);
}

void TestEntityInteractionRules() {
    Map map(16, 12);
    BuildBorderWalls(map);

    EntityManager manager{};
    Player player{};
    player.position = {5.4f, 4.5f};
    player.collisionRadius = 0.22f;

    Entity& playerEntity = manager.CreateEntity(EntityType::Player, player.position);
    playerEntity.visible = false;

    const Entity& pickup = manager.CreateEntity(EntityType::Pickup, Vec2{5.6f, 4.5f});
    const int pickupId = pickup.id;
    Entity& trigger = manager.CreateEntity(EntityType::Trigger, Vec2{6.0f, 4.5f});
    const int triggerId = trigger.id;
    trigger.mapX = 6;
    trigger.mapY = 4;
    map.SetCell(trigger.mapX, trigger.mapY, TileType::Trigger);

    Entity& door = manager.CreateEntity(EntityType::Door, Vec2{7.5f, 4.5f});
    const int doorId = door.id;
    door.mapX = 7;
    door.mapY = 4;
    door.doorOpen = false;
    map.SetCell(door.mapX, door.mapY, TileType::Door);

    int health = 100;
    int ammo = 24;
    GameplayEvents events{};

    SyncPlayerEntity(manager, player);
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, false, events);
    const Entity* pickupAfterCollect = manager.GetEntityById(pickupId);
    const Entity* triggerAfterActivate = manager.GetEntityById(triggerId);
    const Entity* doorInitial = manager.GetEntityById(doorId);
    TEST_ASSERT(pickupAfterCollect != nullptr);
    TEST_ASSERT(triggerAfterActivate != nullptr);
    TEST_ASSERT(doorInitial != nullptr);
    TEST_ASSERT(pickupAfterCollect->collected);
    TEST_ASSERT(!pickupAfterCollect->active);
    TEST_ASSERT(triggerAfterActivate->triggerActivated);
    TEST_ASSERT(!doorInitial->doorOpen);
    TEST_ASSERT(map.GetCell(doorInitial->mapX, doorInitial->mapY) == TileType::Door);

    player.position = {6.6f, 4.5f};
    SyncPlayerEntity(manager, player);
    GameplayEvents doorOpenEvents{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true,
                             doorOpenEvents);
    const Entity* doorOpen = manager.GetEntityById(doorId);
    TEST_ASSERT(doorOpen != nullptr);
    TEST_ASSERT(doorOpen->doorOpen);
    TEST_ASSERT(map.GetCell(doorOpen->mapX, doorOpen->mapY) == TileType::Empty);

    player.position = {7.5f, 4.5f};
    SyncPlayerEntity(manager, player);
    GameplayEvents doorOverlapEvents{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true,
                             doorOverlapEvents);
    const Entity* doorStillOpen = manager.GetEntityById(doorId);
    TEST_ASSERT(doorStillOpen != nullptr);
    TEST_ASSERT(doorStillOpen->doorOpen);
    TEST_ASSERT(map.GetCell(doorStillOpen->mapX, doorStillOpen->mapY) == TileType::Empty);

    player.position = {6.6f, 4.5f};
    SyncPlayerEntity(manager, player);
    GameplayEvents doorCloseEvents{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true,
                             doorCloseEvents);
    const Entity* doorClosed = manager.GetEntityById(doorId);
    TEST_ASSERT(doorClosed != nullptr);
    TEST_ASSERT(!doorClosed->doorOpen);
    TEST_ASSERT(map.GetCell(doorClosed->mapX, doorClosed->mapY) == TileType::Door);
}

void TestContentFileLoaders() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

    ThemeTextureAssignments wall{};
    SpriteTextureDefinitions sprites{};
    TEST_ASSERT(LoadThemeTextureAssignments(ResolveRepoPath(ThemeConfigPath("classic")), wall, errors));
    TEST_ASSERT(LoadSpriteTextureDefinitions(ResolveRepoPath("game/defs/sprites.cfg"), sprites, errors));
    TEST_ASSERT(!wall.wall.empty());
    TEST_ASSERT(wall.exit == "game/assets/themes/classic/exit.ppm");
    TEST_ASSERT(sprites.Path(SpriteTextureId::Guard) == "game/assets/sprites/guard.ppm");
    TEST_ASSERT(sprites.Path(SpriteTextureId::KeyGold) == "game/assets/sprites/key_gold.ppm");

    Map map{};
    TEST_ASSERT(LoadMapFromFile(ReferencePath("game/maps/level01.map"), map, errors));
    TEST_ASSERT(map.Width() == ReferenceLevel1::kWidth);
    TEST_ASSERT(map.Height() == ReferenceLevel1::kHeight);
    TEST_ASSERT(map.GetCell(ReferenceLevel1::kDoorX, ReferenceLevel1::kDoorY) == TileType::Door);

    std::vector<EntityPlacement> placements;
    TEST_ASSERT(
        LoadEntityPlacementsFromFile(ReferencePath("game/defs/level01.entities.json"), placements, errors, warnings));
    TEST_ASSERT(!placements.empty());
    TEST_ASSERT(errors.empty() && warnings.empty());

    bool foundPlayer = false;
    bool foundDoor = false;
    for (const EntityPlacement& p : placements) {
        if (p.type == PlacementType::Player) {
            foundPlayer = true;
        } else if (p.type == PlacementType::Door) {
            foundDoor = true;
            TEST_ASSERT(p.mapX >= 0 && p.mapY >= 0);
        }
    }
    TEST_ASSERT(foundPlayer);
    TEST_ASSERT(foundDoor);
}

void TestAudioConfigLoader() {
    std::vector<std::string> errors;
    AudioEffectDefinitions audio{};
    TEST_ASSERT(LoadAudioEffectDefinitions(ResolveRepoPath("game/defs/audio.cfg"), audio, errors));
    TEST_ASSERT(!audio.doorOpen.empty());
    TEST_ASSERT(!audio.pickup.empty());
    TEST_ASSERT(!audio.enemyAlert.empty());
    TEST_ASSERT(audio.masterVolume >= 0.0f);
    TEST_ASSERT(audio.masterVolume <= 1.0f);
}

void TestAssetManagerFallbackLoad() {
    AssetManager assets{};
    Texture fallback{};
    TextureLoader::BuildFallbackChecker(fallback, 8, 8);
    assets.SetTexture("__missing", fallback);

    std::vector<std::string> errors;
    TEST_ASSERT(assets.LoadTextureFromPPM("wall", ResolveRepoPath("game/assets/themes/classic/wall.ppm"), errors));
    TEST_ASSERT(assets.GetTexture("wall") != nullptr);

    const bool missingLoaded =
        assets.LoadTextureFromPPM("bad", ResolveRepoPath("game/assets/does_not_exist.ppm"), errors);
    TEST_ASSERT(!missingLoaded);
    TEST_ASSERT(!errors.empty());
}

void TestGameStateTransitions() {
    GameState state = GameState::Menu;
    TEST_ASSERT(std::string(GameStateToString(state)) == "Menu");

    state = GameState::Gameplay;
    TEST_ASSERT(std::string(GameStateToString(state)) == "Gameplay");

    state = GameState::Paused;
    TEST_ASSERT(std::string(GameStateToString(state)) == "Paused");

    state = GameState::GameOver;
    TEST_ASSERT(std::string(GameStateToString(state)) == "GameOver");

    state = GameState::Victory;
    TEST_ASSERT(std::string(GameStateToString(state)) == "Victory");

    // Phase 5 states are appended: the C API values of the old ones are unchanged.
    TEST_ASSERT(static_cast<int>(GameState::Victory) == 4);
    TEST_ASSERT(static_cast<int>(GameState::LevelComplete) == 5);
    TEST_ASSERT(static_cast<int>(GameState::CampaignComplete) == 6);
    TEST_ASSERT(std::string(GameStateToString(GameState::LevelComplete)) == "LevelComplete");
    TEST_ASSERT(std::string(GameStateToString(GameState::CampaignComplete)) == "CampaignComplete");
    TEST_ASSERT(IsValidGameStateValue(0) && IsValidGameStateValue(6));
    TEST_ASSERT(!IsValidGameStateValue(-1) && !IsValidGameStateValue(7));
    TEST_ASSERT(static_cast<int>(InteropEventType::LevelLoaded) == 11);
    TEST_ASSERT(static_cast<int>(InteropEventType::LevelCompleted) == 12);  // appended
}

void TestPlayerCombatAndShooting() {
    Map map(16, 12);
    BuildBorderWalls(map);

    EntityManager manager{};
    Player player{};
    player.position = {2.0f, 2.0f};
    player.directionAngleRadians = 0.0f;  // Facing east (+X)

    Entity& enemy = manager.CreateEntity(EntityType::Enemy, Vec2{5.0f, 2.0f});
    enemy.health = 50;
    enemy.maxHealth = 50;

    int ammo = 10;
    constexpr int kDamage = 35;
    constexpr float kRange = 12.0f;
    GameplayEvents events{};

    // Shoot enemy: ammo should decrease to 9, enemy takes 35 damage (50 -> 15)
    bool shot = PerformPlayerAttack(manager, map, player, ammo, kDamage, kRange, events);
    TEST_ASSERT(shot);
    TEST_ASSERT(ammo == 9);
    TEST_ASSERT(events.attackHit);
    TEST_ASSERT(events.enemyDamaged);
    TEST_ASSERT(events.damagedEnemyId == enemy.id);
    TEST_ASSERT(enemy.health == 15);
    TEST_ASSERT(enemy.active);

    // Shoot again to kill enemy: ammo 8, enemy health 0, enemy dead and inactive
    GameplayEvents killEvents{};
    shot = PerformPlayerAttack(manager, map, player, ammo, kDamage, kRange, killEvents);
    TEST_ASSERT(shot);
    TEST_ASSERT(ammo == 8);
    TEST_ASSERT(killEvents.attackHit);
    TEST_ASSERT(killEvents.enemyDied);
    TEST_ASSERT(killEvents.deadEnemyId == enemy.id);
    TEST_ASSERT(enemy.health == 0);
    TEST_ASSERT(!enemy.active);

    // Shoot with 0 ammo: returns false
    ammo = 0;
    GameplayEvents noAmmoEvents{};
    shot = PerformPlayerAttack(manager, map, player, ammo, kDamage, kRange, noAmmoEvents);
    TEST_ASSERT(!shot);
    TEST_ASSERT(ammo == 0);

    // Shoot through wall: wall blocks ray
    ammo = 5;
    Entity& behindWallEnemy = manager.CreateEntity(EntityType::Enemy, Vec2{8.0f, 2.0f});
    behindWallEnemy.health = 50;
    map.SetCell(4, 2, TileType::Wall);  // Wall in between at x=4

    GameplayEvents wallEvents{};
    shot = PerformPlayerAttack(manager, map, player, ammo, kDamage, kRange, wallEvents);
    TEST_ASSERT(shot);
    TEST_ASSERT(ammo == 4);
    TEST_ASSERT(!wallEvents.attackHit);
    TEST_ASSERT(!wallEvents.enemyDamaged);
    TEST_ASSERT(behindWallEnemy.health == 50);
}

// Runs the AI for `seconds` (rounded to whole steps) in steps of `dt`.
void RunAi(EntityManager& manager, const Map& map, const Player& player, int& health, float seconds,
           GameplayEvents* collected = nullptr, const NoiseEvent* noise = nullptr, float dt = 0.05f) {
    const long steps = std::lround(seconds / dt);
    for (long step = 0; step < steps; ++step) {
        GameplayEvents events{};
        UpdateEnemyAi(manager, map, player, health, dt, events, noise);
        noise = nullptr;  // a shot is heard once
        if (collected != nullptr) {
            collected->enemyAlerted = collected->enemyAlerted || events.enemyAlerted;
            collected->playerDamaged = collected->playerDamaged || events.playerDamaged;
        }
    }
}

void TestEnemyAiBehavior() {
    // Known issue C10: sight needs a clear line, Alert and Attack are used, and an enemy that
    // loses the player searches and then gives up.
    Map map(16, 12);
    BuildBorderWalls(map);
    EntityManager manager{};
    Player player{};
    player.position = {2.5f, 2.5f};
    int health = 100;

    // Out of alertRadius: stays Idle.
    const int farId = manager.CreateEntity(EntityType::Enemy, Vec2{13.5f, 2.5f}).id;
    manager.GetEntityById(farId)->alertRadius = 5.0f;
    GameplayEvents events{};
    UpdateEnemyAi(manager, map, player, health, 0.1f, events);
    TEST_ASSERT(manager.GetEntityById(farId)->enemyState == EnemyState::Idle && !events.enemyAlerted);
    manager.DestroyEntity(farId);

    // Inside alertRadius but behind a wall: no waking through walls.
    for (int y = 1; y <= 4; ++y) {
        map.SetCell(4, y, TileType::Wall);
    }
    const int id = manager.CreateEntity(EntityType::Enemy, Vec2{6.5f, 2.5f}).id;
    Entity* enemy = manager.GetEntityById(id);
    enemy->alertRadius = 6.0f;
    enemy->reactionTime = 0.5f;
    enemy->moveSpeed = 1.6f;
    enemy->damage = 12;
    enemy->attackRadius = 1.1f;
    enemy->attackRate = 1.0f;
    RunAi(manager, map, player, health, 1.0f, &events);
    enemy = manager.GetEntityById(id);
    TEST_ASSERT(enemy->enemyState == EnemyState::Idle && !events.enemyAlerted);
    TEST_ASSERT_NEAR(enemy->position.x, 6.5f, 1e-6f);

    // Wall removed: it sees the player -> Alert; during the reaction time it neither moves nor
    // raises the alert (no sound yet).
    for (int y = 1; y <= 4; ++y) {
        map.SetCell(4, y, TileType::Empty);
    }
    GameplayEvents alert{};
    RunAi(manager, map, player, health, 0.1f, &alert);
    enemy = manager.GetEntityById(id);
    TEST_ASSERT(enemy->enemyState == EnemyState::Alert && !alert.enemyAlerted);
    RunAi(manager, map, player, health, 0.3f, &alert);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Alert && !alert.enemyAlerted);
    TEST_ASSERT_NEAR(manager.GetEntityById(id)->position.x, 6.5f, 1e-6f);
    // After the reaction time: Chase, and the alert event (the engine plays the alert sound).
    RunAi(manager, map, player, health, 0.15f, &alert);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Chase && alert.enemyAlerted);
    RunAi(manager, map, player, health, 0.5f);
    TEST_ASSERT(manager.GetEntityById(id)->position.x < 6.4f);  // closing in

    // Attack: in range with line of sight; the first hit lands at once, the next only after
    // attackRate seconds.
    int frames = 0;
    while (manager.GetEntityById(id)->enemyState != EnemyState::Attack && frames++ < 200) {
        RunAi(manager, map, player, health, 0.05f);
    }
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Attack);
    TEST_ASSERT(health == 88);
    TEST_ASSERT(Length(manager.GetEntityById(id)->position - player.position) <= 1.1f + 1e-4f);
    RunAi(manager, map, player, health, 0.8f);
    TEST_ASSERT(health == 88);
    RunAi(manager, map, player, health, 0.3f);
    TEST_ASSERT(health == 76);

    // The player steps out of range: back to Chase.
    player.position = {2.5f, 5.5f};
    RunAi(manager, map, player, health, 0.05f);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Chase);

    // The player disappears behind a full wall: Chase -> Search after kEnemyLoseTargetSeconds,
    // Search -> Idle after kEnemySearchSeconds.
    for (int y = 1; y < map.Height() - 1; ++y) {
        map.SetCell(9, y, TileType::Wall);
    }
    player.position = {13.5f, 6.5f};
    RunAi(manager, map, player, health, kEnemyLoseTargetSeconds - 0.2f);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Chase);
    RunAi(manager, map, player, health, 0.4f);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Search);
    RunAi(manager, map, player, health, kEnemySearchSeconds + 0.1f);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Idle);
    TEST_ASSERT(manager.GetEntityById(id)->position.x < 9.0f - kEnemyRadius);  // never went through the wall

    // Search -> Chase again when the player shows up.
    manager.GetEntityById(id)->enemyState = EnemyState::Search;
    manager.GetEntityById(id)->stateTimer = kEnemySearchSeconds;
    player.position = {manager.GetEntityById(id)->position.x + 2.0f, manager.GetEntityById(id)->position.y};
    if (player.position.x > 8.5f) {
        player.position.x = manager.GetEntityById(id)->position.x - 2.0f;
    }
    RunAi(manager, map, player, health, 0.05f);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Chase);

    // A shot alerts an idle enemy at once (no reaction delay).
    manager.GetEntityById(id)->enemyState = EnemyState::Idle;
    TEST_ASSERT(AlertEnemyByDamage(*manager.GetEntityById(id), player.position));
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Chase);
    TEST_ASSERT(manager.GetEntityById(id)->hitFlashTimer > 0.0f);
    TEST_ASSERT(!AlertEnemyByDamage(*manager.GetEntityById(id), player.position));  // already chasing
}

void TestPickupCollection() {
    Map map(16, 12);
    BuildBorderWalls(map);

    EntityManager manager{};
    Player player{};
    player.position = {5.0f, 5.0f};

    Entity& healthPickup = manager.CreateEntity(EntityType::Pickup, Vec2{5.1f, 5.0f});
    healthPickup.pickupType = PickupType::Health;
    healthPickup.pickupValue = 25;

    int playerHealth = 60;
    int playerAmmo = 10;

    GameplayEvents events{};
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, kPlayerMaxHealth, kPlayerMaxAmmo, false,
                             events);

    TEST_ASSERT(events.pickupCollected);
    TEST_ASSERT(events.pickupType == PickupType::Health);
    TEST_ASSERT(playerHealth == 85);
    TEST_ASSERT(healthPickup.collected);
    TEST_ASSERT(!healthPickup.active);

    // Ammo pickup
    Entity& ammoPickup = manager.CreateEntity(EntityType::Pickup, Vec2{5.0f, 5.2f});
    ammoPickup.pickupType = PickupType::Ammo;
    ammoPickup.pickupValue = 15;

    GameplayEvents ammoEvents{};
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, kPlayerMaxHealth, kPlayerMaxAmmo, false,
                             ammoEvents);

    TEST_ASSERT(ammoEvents.pickupCollected);
    TEST_ASSERT(ammoEvents.pickupType == PickupType::Ammo);
    TEST_ASSERT(playerAmmo == 25);
    TEST_ASSERT(ammoPickup.collected);
    TEST_ASSERT(!ammoPickup.active);
}

void TestCommandBridgeAndEvents() {
    CommandBridge bridge;
    TEST_ASSERT(!bridge.HasEvents());

    // Test Event pushing & polling
    InteropEvent evt{};
    evt.type = InteropEventType::StateChanged;
    evt.intArg1 = static_cast<int>(GameState::Gameplay);
    bridge.PushEvent(evt);

    TEST_ASSERT(bridge.HasEvents());
    InteropEvent polledEvt{};
    TEST_ASSERT(bridge.PollEvent(polledEvt));
    TEST_ASSERT(polledEvt.type == InteropEventType::StateChanged);
    TEST_ASSERT(polledEvt.intArg1 == static_cast<int>(GameState::Gameplay));
    TEST_ASSERT(!bridge.HasEvents());

    // Test Command queue
    InteropCommand cmd{};
    cmd.type = InteropCommandType::HealPlayer;
    cmd.intArg1 = 30;
    bridge.EnqueueCommand(cmd);

    InteropCommand polledCmd{};
    TEST_ASSERT(bridge.PollCommand(polledCmd));
    TEST_ASSERT(polledCmd.type == InteropCommandType::HealPlayer);
    TEST_ASSERT(polledCmd.intArg1 == 30);
    TEST_ASSERT(!bridge.PollCommand(polledCmd));
}

void TestLogFile() {
    const std::string logPath = "test_run.log";
    SetLogFilePath(logPath);
    LogInfo("Test info log entry");
    LogWarning("Test warning entry");
    LogError("Test error entry");

    TEST_ASSERT(std::filesystem::exists(logPath));
    std::ifstream file(logPath);
    TEST_ASSERT(file.is_open());
    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    TEST_ASSERT(content.find("[INFO]") != std::string::npos);
    TEST_ASSERT(content.find("Test info log entry") != std::string::npos);
    TEST_ASSERT(content.find("[WARN]") != std::string::npos);
    TEST_ASSERT(content.find("[ERROR]") != std::string::npos);

    SetLogFilePath("");
    std::filesystem::remove(logPath);
}

void TestLevelResetAndDynamicLoad() {
    std::vector<std::string> errors;
    Map map{};
    const std::string mapPath = ReferencePath("game/maps/level01.map");
    TEST_ASSERT(LoadMapFromFile(mapPath, map, errors));

    // Multi-cycle load simulation: every build of the same files gives the same level.
    std::vector<std::string> warnings;
    for (int cycle = 0; cycle < 10; ++cycle) {
        Map levelMap = map;
        std::vector<EntityPlacement> placements;
        errors.clear();
        TEST_ASSERT(LoadEntityPlacementsFromFile(ReferencePath("game/defs/level01.entities.json"), placements, errors,
                                                 warnings));
        LevelContents level{};
        TEST_ASSERT(BuildLevelFromPlacements(levelMap, placements, GameplayDefinitions{}, level, errors));
        TEST_ASSERT(level.entities.Entities().size() == ReferenceLevel1::kPlacementCount);
        TEST_ASSERT(level.sprites.size() == ReferenceLevel1::kSpriteCount);
        TEST_ASSERT(level.entities.Entities().front().id == 1);
        level.entities.Clear();
        TEST_ASSERT(level.entities.Entities().empty());
    }
}

void TestPlayableVerticalSliceSimulation() {
    // End-to-end game state, player combat, enemy interaction, trigger loop
    Map map(16, 12);
    BuildBorderWalls(map);

    GameState state = GameState::Menu;
    TEST_ASSERT(state == GameState::Menu);

    // Transition to Gameplay
    state = GameState::Gameplay;
    TEST_ASSERT(state == GameState::Gameplay);

    EntityManager manager{};
    Player player{};
    player.position = {2.0f, 2.0f};
    int playerHealth = 100;
    int playerAmmo = 24;

    // Spawn Enemy and shoot it down
    Entity& enemy = manager.CreateEntity(EntityType::Enemy, Vec2{4.0f, 2.0f});
    enemy.health = 35;
    GameplayEvents combatEvents{};
    PerformPlayerAttack(manager, map, player, playerAmmo, 35, 10.0f, combatEvents);
    TEST_ASSERT(combatEvents.enemyDied);
    TEST_ASSERT(!enemy.active);

    // Collect health pickup
    Entity& hp = manager.CreateEntity(EntityType::Pickup, Vec2{2.2f, 2.0f});
    hp.pickupType = PickupType::Health;
    GameplayEvents pickEvents{};
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, kPlayerMaxHealth, kPlayerMaxAmmo, false,
                             pickEvents);
    TEST_ASSERT(pickEvents.pickupCollected);

    // Open door
    Entity& door = manager.CreateEntity(EntityType::Door, Vec2{6.0f, 2.0f});
    door.mapX = 6;
    door.mapY = 2;
    map.SetCell(6, 2, TileType::Door);

    player.position = {5.2f, 2.0f};
    GameplayEvents doorEvents{};
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, kPlayerMaxHealth, kPlayerMaxAmmo, true,
                             doorEvents);
    TEST_ASSERT(doorEvents.doorToggled);
    TEST_ASSERT(door.doorOpen);
    TEST_ASSERT(map.GetCell(6, 2) == TileType::Empty);

    // Step into victory trigger
    manager.CreateEntity(EntityType::Trigger, Vec2{10.0f, 2.0f});
    player.position = {10.0f, 2.0f};
    GameplayEvents trigEvents{};
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, kPlayerMaxHealth, kPlayerMaxAmmo, false,
                             trigEvents);
    TEST_ASSERT(trigEvents.triggerActivated);
    state = GameState::Victory;
    TEST_ASSERT(state == GameState::Victory);
}

// The number of walkable cells a flood fill from `start` (steps to the four neighbours; walls
// and closed doors block) does not reach.
int CountUnreachableWalkableCells(const Map& map, Vec2i start) {
    const auto index = [&map](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(map.Width()) + static_cast<std::size_t>(x);
    };
    std::vector<bool> visited(static_cast<std::size_t>(map.Width()) * static_cast<std::size_t>(map.Height()), false);
    std::vector<Vec2i> queue{start};
    visited[index(start.x, start.y)] = true;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const Vec2i current = queue[head];
        const std::array<Vec2i, 4> neighbours = {
            Vec2i{current.x + 1, current.y},
            Vec2i{current.x - 1, current.y},
            Vec2i{current.x, current.y + 1},
            Vec2i{current.x, current.y - 1},
        };
        for (const Vec2i& n : neighbours) {
            const bool inside = n.x >= 0 && n.x < map.Width() && n.y >= 0 && n.y < map.Height();
            if (inside && !visited[index(n.x, n.y)] && !map.IsWall(n.x, n.y)) {
                visited[index(n.x, n.y)] = true;
                queue.push_back(n);
            }
        }
    }

    int unreached = 0;
    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            if (!map.IsWall(x, y) && !visited[index(x, y)]) {
                ++unreached;
            }
        }
    }
    return unreached;
}

void TestMapPerimeterAndReachabilityValidation() {
    Map map{};
    std::vector<std::string> errors;
    const std::string mapPath = ReferencePath("game/maps/level01.map");
    TEST_ASSERT(LoadMapFromFile(mapPath, map, errors));
    TEST_ASSERT(map.Width() > 0);
    TEST_ASSERT(map.Height() > 0);

    // Verify perimeter enclosure (solid walls on all boundaries)
    for (int x = 0; x < map.Width(); ++x) {
        TEST_ASSERT(map.IsWall(x, 0));
        TEST_ASSERT(map.IsWall(x, map.Height() - 1));
    }
    for (int y = 0; y < map.Height(); ++y) {
        TEST_ASSERT(map.IsWall(0, y));
        TEST_ASSERT(map.IsWall(map.Width() - 1, y));
    }

    TEST_ASSERT(CountUnreachableWalkableCells(map, ReferenceLevel1::kSpawnCell) == 0);
}

void TestAssetInventoryAndErrorResilience() {
    const std::vector<std::string> textures = {
        "game/assets/themes/classic/wall.ppm",       "game/assets/themes/classic/wall_alt_a.ppm",
        "game/assets/themes/classic/wall_alt_b.ppm", "game/assets/themes/classic/floor.ppm",
        "game/assets/themes/classic/ceiling.ppm",    "game/assets/sprites/decoration.ppm",
    };

    for (const auto& relPath : textures) {
        Texture tex{};
        const bool loaded = TextureLoader::LoadPPM(ResolveRepoPath(relPath), tex);
        TEST_ASSERT(loaded);
        TEST_ASSERT(tex.Width() > 0);
        TEST_ASSERT(tex.Height() > 0);
        TEST_ASSERT(tex.Pixels() != nullptr);
    }
}

void TestEngineInteropCApi() {
    void* handle = nw_engine_create();
    TEST_ASSERT(handle != nullptr);

    TEST_ASSERT(nw_engine_get_state(handle) == 0);  // Menu
    nw_engine_set_state(handle, 4);                 // Victory (deprecated) selects LevelComplete
    TEST_ASSERT(nw_engine_get_state(handle) == static_cast<int>(GameState::LevelComplete));
    nw_engine_set_state(handle, 99);  // out of range: ignored
    TEST_ASSERT(nw_engine_get_state(handle) == static_cast<int>(GameState::LevelComplete));
    nw_engine_set_state(handle, 1);  // Gameplay
    TEST_ASSERT(nw_engine_get_state(handle) == 1);

    nw_engine_set_player_pos(handle, 4.5f, 3.5f, 1.57f);
    float px = 0.0f;
    float py = 0.0f;
    float pAngle = 0.0f;
    nw_engine_get_player_pos(handle, &px, &py, &pAngle);
    TEST_ASSERT_NEAR(px, 4.5f, 0.0001f);
    TEST_ASSERT_NEAR(py, 3.5f, 0.0001f);
    TEST_ASSERT_NEAR(pAngle, 1.57f, 0.0001f);

    nw_engine_set_player_stats(handle, 80, 42);
    int health = 0;
    int ammo = 0;
    nw_engine_get_player_stats(handle, &health, &ammo);
    TEST_ASSERT(health == 80);
    TEST_ASSERT(ammo == 42);

    const int spawnedId = nw_engine_spawn_entity(handle, 1, 6.0f, 7.0f);
    TEST_ASSERT(spawnedId > 0);
    TEST_ASSERT(nw_engine_get_entity_count(handle) >= 1);

    int entId = 0;
    int entType = 0;
    int entActive = 0;
    float entX = 0.0f;
    float entY = 0.0f;
    TEST_ASSERT(nw_engine_get_entity(handle, 0, &entId, &entType, &entX, &entY, &entActive) == 1);

    // Send Interop Command
    InteropCommand cmd{};
    cmd.type = InteropCommandType::HealPlayer;
    cmd.intArg1 = 15;
    TEST_ASSERT(nw_engine_send_command(handle, &cmd) == 1);

    nw_engine_show_message(handle, "Test Interop Message", 2.0f);

    TEST_ASSERT(nw_engine_destroy_entity(handle, spawnedId) == 1);

    nw_engine_destroy(handle);
}

// A fresh, empty directory under the system temp folder; removed when the object dies.
class ScratchDir {
public:
    explicit ScratchDir(const std::string& name) {
        path_ = std::filesystem::temp_directory_path() / ("neowolf-test-" + name);
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_);
    }
    ~ScratchDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;
    ScratchDir(ScratchDir&&) = delete;
    ScratchDir& operator=(ScratchDir&&) = delete;

    const std::filesystem::path& Path() const { return path_; }

private:
    std::filesystem::path path_;
};

// A complete content root in `dir` for engine tests that need the reference levels: the
// shipped game/ (gameplay.json, themes, sprites, sounds) with tests/fixtures/reference/game
// (its campaign.json and level01..03) copied over it. Returns `dir`.
std::filesystem::path MakeReferenceContentRoot(const std::filesystem::path& dir) {
    const auto options = std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing;
    std::filesystem::create_directories(dir / "game");
    std::filesystem::copy(RepoRoot() / "game", dir / "game", options);
    std::filesystem::copy(PathFromUtf8(ReferencePath("game")), dir / "game", options);
    return dir;
}

// Options for a headless engine (no window, no error dialogs) on the content root `dataDir`;
// `startLevel` as for --level (0 = the main menu).
EngineOptions HeadlessEngineOptions(const std::filesystem::path& dataDir, int startLevel = 0) {
    EngineOptions options{};
    options.dataDir = PathToUtf8(dataDir);
    options.headless = true;
    options.errorDialogs = false;
    options.startLevel = startLevel;
    return options;
}

// Headless engine options on the shipped content (the repository root).
EngineOptions ShippedEngineOptions(int startLevel = 0) {
    return HeadlessEngineOptions(RepoRoot(), startLevel);
}

// Headless engine options on a reference content root inside `scratch` (see above).
EngineOptions ReferenceEngineOptions(const ScratchDir& scratch, int startLevel) {
    return HeadlessEngineOptions(MakeReferenceContentRoot(scratch.Path() / "content"), startLevel);
}

// The shipped campaign manifest, game/defs/campaign.json (no levels if it does not load).
CampaignDefinition ShippedCampaign() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    CampaignDefinition campaign{};
    if (!LoadCampaign(ResolveRepoPath("game/defs/campaign.json"), RepoRoot(), campaign, errors, warnings)) {
        return CampaignDefinition{};
    }
    return campaign;
}

// An empty content root in `dir`: the game/maps and game/defs folders the content-root
// search looks for.
void MakeContentRoot(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir / "game" / "maps");
    std::filesystem::create_directories(dir / "game" / "defs");
}

bool CandidateListContains(const std::vector<ContentRootCandidate>& list, const std::filesystem::path& dir) {
    return std::any_of(list.begin(), list.end(), [&dir](const ContentRootCandidate& candidate) {
        return candidate.directory == dir.lexically_normal();
    });
}

void TestContentRootResolution() {
    const ScratchDir scratch("content-root");
    const std::filesystem::path& base = scratch.Path();
    // <base>/repo/game/{maps,defs} is content; the "exe" lives in <base>/repo/build/preset/.
    const std::filesystem::path repo = base / "repo";
    MakeContentRoot(repo);
    const std::filesystem::path exeDir = repo / "build" / "preset";
    std::filesystem::create_directories(exeDir);
    const std::filesystem::path elsewhere = base / "elsewhere" / "sub";
    std::filesystem::create_directories(elsewhere);

    // The sentinel needs both game/maps and game/defs.
    TEST_ASSERT(IsContentRoot(repo));
    TEST_ASSERT(!IsContentRoot(exeDir));
    const std::filesystem::path halfRoot = base / "half";
    std::filesystem::create_directories(halfRoot / "game" / "maps");
    TEST_ASSERT(!IsContentRoot(halfRoot));

    // Automatic order: exe dir, its parents (up to 3), <exe>/../Resources, cwd, cwd parent.
    ContentRootInputs inputs{};
    inputs.basePath = PathToUtf8(exeDir) + "/";  // SDL_GetBasePath ends with a separator
    inputs.workingDir = PathToUtf8(elsewhere);
    const std::vector<ContentRootCandidate> candidates = BuildContentRootCandidates(inputs);
    TEST_ASSERT(candidates.size() == 7);
    TEST_ASSERT(candidates[0].directory == exeDir.lexically_normal());
    TEST_ASSERT(candidates[1].directory == (repo / "build").lexically_normal());
    TEST_ASSERT(candidates[2].directory == repo.lexically_normal());
    TEST_ASSERT(candidates[3].directory == base.lexically_normal());
    TEST_ASSERT(candidates[4].directory == (repo / "build" / "Resources").lexically_normal());
    TEST_ASSERT(candidates[5].directory == elsewhere.lexically_normal());
    TEST_ASSERT(candidates[6].directory == (base / "elsewhere").lexically_normal());

    ContentRootResult result = FindContentRoot(candidates);
    TEST_ASSERT(result.found);
    TEST_ASSERT(result.root == repo.lexically_normal());
    TEST_ASSERT(result.searched.size() == 3);  // exe dir, build/, repo/
    TEST_ASSERT(result.source.find("executable") != std::string::npos);

    // Content in a macOS app bundle's Resources folder (base path = Contents/MacOS).
    const std::filesystem::path bundle = base / "Neo-Wolf.app" / "Contents";
    std::filesystem::create_directories(bundle / "MacOS");
    MakeContentRoot(bundle / "Resources");
    ContentRootInputs bundleInputs{};
    bundleInputs.basePath = PathToUtf8(bundle / "MacOS");
    result = FindContentRoot(BuildContentRootCandidates(bundleInputs));
    TEST_ASSERT(result.found);
    TEST_ASSERT(result.root == (bundle / "Resources").lexically_normal());
    // What SDL_GetBasePath() really returns inside a .app: Contents/Resources (Phase 9 package).
    bundleInputs.basePath = PathToUtf8(bundle / "Resources") + "/";
    result = FindContentRoot(BuildContentRootCandidates(bundleInputs));
    TEST_ASSERT(result.found);
    TEST_ASSERT(result.root == (bundle / "Resources").lexically_normal());
    TEST_ASSERT(result.source == "app bundle Resources");

    // The working directory is used when nothing near the executable matches.
    ContentRootInputs cwdInputs{};
    cwdInputs.basePath = PathToUtf8(elsewhere);
    cwdInputs.workingDir = PathToUtf8(repo / "build");
    result = FindContentRoot(BuildContentRootCandidates(cwdInputs));
    TEST_ASSERT(result.found);
    TEST_ASSERT(result.root == repo.lexically_normal());
    TEST_ASSERT(result.source == "parent of the working directory");

    // --data is exclusive: an empty directory fails even though other candidates would match.
    const std::filesystem::path empty = base / "empty";
    std::filesystem::create_directories(empty);
    ContentRootInputs dataInputs = inputs;
    dataInputs.dataOption = PathToUtf8(empty);
    dataInputs.dataEnv = PathToUtf8(repo);
    const std::vector<ContentRootCandidate> dataCandidates = BuildContentRootCandidates(dataInputs);
    TEST_ASSERT(dataCandidates.size() == 1);
    TEST_ASSERT(dataCandidates[0].source == "--data");
    result = FindContentRoot(dataCandidates);
    TEST_ASSERT(!result.found);
    TEST_ASSERT(result.searched.size() == 1);
    TEST_ASSERT(DescribeContentRootSearch(result).find(PathToUtf8(empty.lexically_normal())) != std::string::npos);

    // A relative --data is relative to the working directory.
    ContentRootInputs relativeInputs{};
    relativeInputs.workingDir = PathToUtf8(base);
    relativeInputs.dataOption = "repo";
    result = FindContentRoot(BuildContentRootCandidates(relativeInputs));
    TEST_ASSERT(result.found);
    TEST_ASSERT(result.root == repo.lexically_normal());

    // NEOWOLF_DATA_DIR is used (alone) when --data is absent.
    ContentRootInputs envInputs = inputs;
    envInputs.dataEnv = PathToUtf8(repo);
    const std::vector<ContentRootCandidate> envCandidates = BuildContentRootCandidates(envInputs);
    TEST_ASSERT(envCandidates.size() == 1);
    TEST_ASSERT(envCandidates[0].source == "NEOWOLF_DATA_DIR");
    TEST_ASSERT(FindContentRoot(envCandidates).found);

    // Nothing found: every candidate is reported as searched.
    ContentRootInputs noneInputs{};
    noneInputs.basePath = PathToUtf8(empty);
    noneInputs.workingDir = PathToUtf8(empty);
    const std::vector<ContentRootCandidate> noneCandidates = BuildContentRootCandidates(noneInputs);
    TEST_ASSERT(CandidateListContains(noneCandidates, empty));
    result = FindContentRoot(noneCandidates);
    TEST_ASSERT(!result.found);
    TEST_ASSERT(result.searched.size() == noneCandidates.size());

    // Joining: root / relative, with absolute paths and an empty root passed through.
    TEST_ASSERT(PathFromUtf8(JoinContentPath(repo, "game/maps/level01.map")) == repo / "game" / "maps" / "level01.map");
    TEST_ASSERT(JoinContentPath(std::filesystem::path(), "game/maps/level01.map") == "game/maps/level01.map");
    const std::string absolute = PathToUtf8(empty / "x.map");
    TEST_ASSERT(JoinContentPath(repo, absolute) == absolute);

    // Non-ASCII directory names survive the UTF-8 round trip and work as a content root.
    const std::string utf8Name = "d\xC3\xA9j\xC3\xA0 vu \xE2\x9C\x93";  // "déjà vu ✓"
    const std::filesystem::path unicodeRoot = base / PathFromUtf8(utf8Name);
    MakeContentRoot(unicodeRoot);
    TEST_ASSERT(PathToUtf8(PathFromUtf8(utf8Name)) == utf8Name);
    ContentRootInputs unicodeInputs{};
    unicodeInputs.dataOption = PathToUtf8(unicodeRoot);
    result = FindContentRoot(BuildContentRootCandidates(unicodeInputs));
    TEST_ASSERT(result.found);
    {
        std::ofstream mapFile(unicodeRoot / "game" / "maps" / "t.map");
        mapFile << "###\n#.#\n###\n";
    }
    Map unicodeMap{};
    std::vector<std::string> errors;
    TEST_ASSERT(LoadMapFromFile(JoinContentPath(result.root, "game/maps/t.map"), unicodeMap, errors));
    TEST_ASSERT(unicodeMap.Width() == 3);
}

void TestCommandLineParsing() {
    // No arguments: run with defaults.
    CommandLineResult r = ParseCommandLine({});
    TEST_ASSERT(r.action == CommandLineAction::Run);
    TEST_ASSERT(r.options.dataDir.empty());
    TEST_ASSERT(r.options.level == 0);  // the main menu
    TEST_ASSERT(r.options.frames == 0);
    TEST_ASSERT(r.options.windowMode == WindowModeOption::Default);
    TEST_ASSERT(r.options.prefDir.empty() && r.options.screenshotPath.empty());
    TEST_ASSERT(r.options.preview == PreviewScreen::None);
    TEST_ASSERT(r.options.renderSize.width == kDefaultRenderSize.width &&
                r.options.renderSize.height == kDefaultRenderSize.height);

    // Phase 5: --pref-dir, and the developer tools --preview / --screenshot.
    r = ParseCommandLine(
        {"--pref-dir", "/tmp/nw prefs", "--preview", "levelselect", "--screenshot=shot.bmp", "--frames", "5"});
    TEST_ASSERT(r.action == CommandLineAction::Run);
    TEST_ASSERT(r.options.prefDir == "/tmp/nw prefs");
    TEST_ASSERT(r.options.preview == PreviewScreen::LevelSelect);
    TEST_ASSERT(r.options.screenshotPath == "shot.bmp");
    for (const PreviewScreen screen : kPreviewScreens) {
        r = ParseCommandLine({"--preview", PreviewScreenName(screen)});
        TEST_ASSERT(r.action == CommandLineAction::Run && r.options.preview == screen);
    }

    r = ParseCommandLine({"--data", "/some dir", "--level", "7", "--frames", "120", "--fullscreen"});
    TEST_ASSERT(r.action == CommandLineAction::Run);
    TEST_ASSERT(r.options.dataDir == "/some dir");
    TEST_ASSERT(r.options.level == 7);
    TEST_ASSERT(r.options.frames == 120);
    TEST_ASSERT(r.options.windowMode == WindowModeOption::Fullscreen);

    r = ParseCommandLine({"--data=C:\\Games\\Neo Wolf", "--level=12", "--windowed"});
    TEST_ASSERT(r.action == CommandLineAction::Run);
    TEST_ASSERT(r.options.dataDir == "C:\\Games\\Neo Wolf");
    TEST_ASSERT(r.options.level == 12);
    TEST_ASSERT(r.options.windowMode == WindowModeOption::Windowed);

    TEST_ASSERT(ParseCommandLine({"--help"}).action == CommandLineAction::ShowHelp);
    TEST_ASSERT(ParseCommandLine({"-h"}).action == CommandLineAction::ShowHelp);
    TEST_ASSERT(ParseCommandLine({"--version"}).action == CommandLineAction::ShowVersion);
    TEST_ASSERT(ParseCommandLine({"--frames", "5", "--version"}).action == CommandLineAction::ShowVersion);
    // macOS Finder's legacy process serial number argument is ignored.
    TEST_ASSERT(ParseCommandLine({"-psn_0_12345"}).action == CommandLineAction::Run);

    // Invalid input -> Error with a message (main() prints usage and exits 2).
    const std::vector<std::vector<std::string>> invalid = {
        {"--bogus"},
        {"stray"},
        {"--data"},
        {"--data", ""},
        {"--level"},
        {"--level", "0"},
        {"--level", "100"},
        {"--level", "-3"},
        {"--level", "2x"},
        {"--level", "abc"},
        {"--frames", "0"},
        {"--frames", "-1"},
        {"--frames", "99999999999"},
        {"--windowed", "--fullscreen"},
        {"--version=2"},
        {"--pref-dir"},
        {"--pref-dir", ""},
        {"--screenshot"},
        {"--preview"},
        {"--preview", "credits"},
        {"--preview", "none"},
    };
    for (const std::vector<std::string>& args : invalid) {
        const CommandLineResult bad = ParseCommandLine(args);
        TEST_ASSERT(bad.action == CommandLineAction::Error);
        TEST_ASSERT(!bad.error.empty());
    }
    TEST_ASSERT(ParseCommandLine({"--bogus"}).error.find("--bogus") != std::string::npos);

    TEST_ASSERT(ParseCommandLine({"--preview", "x"}).error.find("levelselect") != std::string::npos);

    // Version and usage text.
    TEST_ASSERT(VersionString() == std::string("Neo-Wolf ") + NW_EXPECTED_VERSION);
    const std::string usage = CommandLineUsage("neo_wolf");
    for (const char* option :
         {"--help", "--version", "--data", "--pref-dir", "--level", "--frames", "--windowed", "--fullscreen",
          "--render-size", "--preview", "--screenshot", "campaign", "bootIntoTestLevel"}) {
        TEST_ASSERT(usage.find(option) != std::string::npos);
    }
}

std::string ReadWholeFile(const std::string& path) {
    std::ifstream file(PathFromUtf8(path), std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

// Writes `text` with every "\n" turned into "\r\n" (binary mode, so nothing is translated).
void WriteWithCrlf(const std::filesystem::path& path, const std::string& text) {
    std::string crlf;
    for (const char c : text) {
        if (c == '\n') {
            crlf += '\r';
        }
        if (c != '\r') {
            crlf += c;
        }
    }
    std::ofstream file(path, std::ios::binary);
    file << crlf;
}

void TestCrlfContentLoading() {
    const ScratchDir scratch("crlf");
    std::vector<std::string> errors;

    // Map: the reference level 1 plus a comment and a blank line, in LF and CRLF form.
    const std::string mapText = "; comment line\n\n" + ReadWholeFile(ReferencePath("game/maps/level01.map"));
    TEST_ASSERT(mapText.find('\r') == std::string::npos);  // content files stay LF
    const std::filesystem::path lfMapPath = scratch.Path() / "lf.map";
    const std::filesystem::path crlfMapPath = scratch.Path() / "crlf.map";
    {
        std::ofstream lf(lfMapPath, std::ios::binary);
        lf << mapText;
    }
    WriteWithCrlf(crlfMapPath, mapText);
    TEST_ASSERT(ReadWholeFile(PathToUtf8(crlfMapPath)).find("\r\n") != std::string::npos);

    Map lfMap{};
    Map crlfMap{};
    TEST_ASSERT(LoadMapFromFile(PathToUtf8(lfMapPath), lfMap, errors));
    TEST_ASSERT(LoadMapFromFile(PathToUtf8(crlfMapPath), crlfMap, errors));
    TEST_ASSERT(errors.empty());
    TEST_ASSERT(lfMap.Width() == ReferenceLevel1::kWidth && lfMap.Height() == ReferenceLevel1::kHeight);
    TEST_ASSERT(crlfMap.Width() == lfMap.Width() && crlfMap.Height() == lfMap.Height());
    for (int y = 0; y < lfMap.Height(); ++y) {
        for (int x = 0; x < lfMap.Width(); ++x) {
            TEST_ASSERT(crlfMap.GetCell(x, y) == lfMap.GetCell(x, y));
        }
    }

    // Entity placements, in both formats (JSON canonical, CSV legacy).
    std::vector<std::string> warnings;
    for (const char* entitiesFile : {"level01.entities.json", "level01.entities.csv"}) {
        const std::string lfEntitiesPath = ReferencePath(std::string("game/defs/") + entitiesFile);
        const std::filesystem::path crlfEntitiesPath = scratch.Path() / (std::string("crlf-") + entitiesFile);
        WriteWithCrlf(crlfEntitiesPath, ReadWholeFile(lfEntitiesPath));
        std::vector<EntityPlacement> lfPlacements;
        std::vector<EntityPlacement> crlfPlacements;
        TEST_ASSERT(LoadEntityPlacementsFromFile(lfEntitiesPath, lfPlacements, errors, warnings));
        TEST_ASSERT(LoadEntityPlacementsFromFile(PathToUtf8(crlfEntitiesPath), crlfPlacements, errors, warnings));
        TEST_ASSERT(errors.empty() && warnings.empty());
        TEST_ASSERT(lfPlacements.size() == ReferenceLevel1::kPlacementCount);
        TEST_ASSERT(crlfPlacements.size() == lfPlacements.size());
        for (std::size_t i = 0; i < lfPlacements.size(); ++i) {
            TEST_ASSERT(crlfPlacements[i].type == lfPlacements[i].type);
            TEST_ASSERT_NEAR(crlfPlacements[i].position.x, lfPlacements[i].position.x, 0.0001f);
            TEST_ASSERT_NEAR(crlfPlacements[i].position.y, lfPlacements[i].position.y, 0.0001f);
            TEST_ASSERT(crlfPlacements[i].mapX == lfPlacements[i].mapX);
            TEST_ASSERT(crlfPlacements[i].mapY == lfPlacements[i].mapY);
            TEST_ASSERT(crlfPlacements[i].pickupKind == lfPlacements[i].pickupKind);
        }
    }

    // Key/value configs (AudioConfig.cpp and ContentConfig.cpp parsers).
    const std::filesystem::path crlfAudioPath = scratch.Path() / "audio.cfg";
    WriteWithCrlf(crlfAudioPath, ReadWholeFile(ResolveRepoPath("game/defs/audio.cfg")));
    AudioEffectDefinitions lfAudio{};
    AudioEffectDefinitions crlfAudio{};
    TEST_ASSERT(LoadAudioEffectDefinitions(ResolveRepoPath("game/defs/audio.cfg"), lfAudio, errors));
    TEST_ASSERT(LoadAudioEffectDefinitions(PathToUtf8(crlfAudioPath), crlfAudio, errors));
    TEST_ASSERT(crlfAudio.doorOpen == lfAudio.doorOpen);
    TEST_ASSERT(crlfAudio.weaponFire == lfAudio.weaponFire);
    TEST_ASSERT(crlfAudio.doorOpen.find('\r') == std::string::npos);
    TEST_ASSERT_NEAR(crlfAudio.masterVolume, lfAudio.masterVolume, 0.0001f);

    const std::filesystem::path crlfWallPath = scratch.Path() / "bunker.cfg";
    WriteWithCrlf(crlfWallPath, ReadWholeFile(ResolveRepoPath(ThemeConfigPath("bunker"))));
    ThemeTextureAssignments lfWall{};
    ThemeTextureAssignments crlfWall{};
    TEST_ASSERT(LoadThemeTextureAssignments(ResolveRepoPath(ThemeConfigPath("bunker")), lfWall, errors));
    TEST_ASSERT(LoadThemeTextureAssignments(PathToUtf8(crlfWallPath), crlfWall, errors));
    TEST_ASSERT(crlfWall.wall == lfWall.wall);
    TEST_ASSERT(crlfWall.door == lfWall.door);
    TEST_ASSERT(crlfWall.ceiling == lfWall.ceiling);
    TEST_ASSERT(crlfWall.exit == lfWall.exit && crlfWall.exit.find('\r') == std::string::npos);

    // The campaign manifest (JSON, read like gameplay.json).
    const std::filesystem::path crlfCampaignPath = scratch.Path() / "campaign.json";
    WriteWithCrlf(crlfCampaignPath, ReadWholeFile(ResolveRepoPath("game/defs/campaign.json")));
    CampaignDefinition crlfCampaign{};
    std::vector<std::string> campaignWarnings;
    TEST_ASSERT(LoadCampaign(PathToUtf8(crlfCampaignPath), RepoRoot(), crlfCampaign, errors, campaignWarnings));
    // The same levels as the LF original, whatever their number.
    const CampaignDefinition lfCampaign = ShippedCampaign();
    TEST_ASSERT(!lfCampaign.levels.empty() && crlfCampaign.levels.size() == lfCampaign.levels.size());
    TEST_ASSERT(crlfCampaign.title == lfCampaign.title);
    for (std::size_t i = 0; i < lfCampaign.levels.size(); ++i) {
        TEST_ASSERT(crlfCampaign.levels[i].id == lfCampaign.levels[i].id);
        TEST_ASSERT(crlfCampaign.levels[i].name == lfCampaign.levels[i].name);
        TEST_ASSERT(crlfCampaign.levels[i].theme == lfCampaign.levels[i].theme);
        TEST_ASSERT(crlfCampaign.levels[i].map == lfCampaign.levels[i].map);
    }
    TEST_ASSERT(errors.empty());
}

void TestLogFileRotation() {
    const ScratchDir scratch("log-rotation");
    const std::string logPath = PathToUtf8(scratch.Path() / "neowolf.log");
    const std::string prevPath = PathToUtf8(scratch.Path() / "neowolf.prev.log");

    TEST_ASSERT(StartFreshLogFile(logPath, prevPath));
    LogInfo("first run");
    TEST_ASSERT(GetLogFilePath() == logPath);
    TEST_ASSERT(!std::filesystem::exists(PathFromUtf8(prevPath)));

    TEST_ASSERT(StartFreshLogFile(logPath, prevPath));
    LogInfo("second run");
    TEST_ASSERT(StartFreshLogFile(logPath, prevPath));
    LogInfo("third run");
    SetLogFilePath("");

    const std::string current = ReadWholeFile(logPath);
    const std::string previous = ReadWholeFile(prevPath);
    TEST_ASSERT(current.find("third run") != std::string::npos);
    TEST_ASSERT(current.find("second run") == std::string::npos);  // truncated at each start
    TEST_ASSERT(previous.find("second run") != std::string::npos);
    TEST_ASSERT(previous.find("first run") == std::string::npos);  // only one previous run kept

    // An uncreatable path turns file logging off instead of failing later.
    TEST_ASSERT(!StartFreshLogFile(PathToUtf8(scratch.Path() / "missing-dir" / "x.log"), ""));
    TEST_ASSERT(GetLogFilePath().empty());
}

// --- Phase 4: data pipeline ---------------------------------------------------------------

void WriteTextFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
}

// True if one of `messages` contains every one of `parts`.
bool AnyMessageContains(const std::vector<std::string>& messages, std::initializer_list<const char*> parts) {
    for (const std::string& message : messages) {
        bool all = true;
        for (const char* part : parts) {
            all = all && message.find(part) != std::string::npos;
        }
        if (all) {
            return true;
        }
    }
    return false;
}

// The first entity of `type` in creation order, or nullptr. The result is const when the
// manager is.
template <typename Manager>
auto FindFirstEntity(Manager& manager, EntityType type) -> decltype(&manager.Entities().front()) {
    for (auto& entity : manager.Entities()) {
        if (entity.type == type) {
            return &entity;
        }
    }
    return nullptr;
}

int CountEntities(const EntityManager& manager, EntityType type) {
    int count = 0;
    for (const Entity& entity : manager.Entities()) {
        count += entity.type == type ? 1 : 0;
    }
    return count;
}

const SpriteEntity* FindSpriteFor(const std::vector<SpriteEntity>& sprites, int entityId) {
    for (const SpriteEntity& sprite : sprites) {
        if (sprite.entityId == entityId) {
            return &sprite;
        }
    }
    return nullptr;
}

void TestGameplayDefinitionsExactValues() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    GameplayDefinitions g{};
    g.weapon.damage = -1;  // overwritten by a successful load
    TEST_ASSERT(LoadGameplayDefinitions(ResolveRepoPath("game/defs/gameplay.json"), g, errors, warnings));
    TEST_ASSERT(errors.empty());
    TEST_ASSERT(warnings.empty());

    // The keys that collided in the old first-match parser (known issue C2).
    TEST_ASSERT(g.weapon.damage == 35);
    TEST_ASSERT(g.enemies.guard.damage == 12);
    TEST_ASSERT_NEAR(g.player.moveSpeed, 3.0f, 1e-6f);
    TEST_ASSERT_NEAR(g.enemies.guard.moveSpeed, 1.6f, 1e-6f);
    TEST_ASSERT(g.enemies.guard.health == 50);

    TEST_ASSERT(g.player.initialHealth == 100);
    TEST_ASSERT(g.player.maxHealth == 100);
    TEST_ASSERT_NEAR(g.player.turnSpeed, 1.8f, 1e-6f);
    TEST_ASSERT_NEAR(g.player.collisionRadius, 0.22f, 1e-6f);
    TEST_ASSERT_NEAR(g.weapon.fireRate, 0.25f, 1e-6f);
    TEST_ASSERT_NEAR(g.weapon.range, 12.0f, 1e-6f);
    TEST_ASSERT(g.weapon.initialAmmo == 24);
    TEST_ASSERT(g.weapon.maxAmmo == 99);
    TEST_ASSERT_NEAR(g.weapon.noiseRadius, 10.0f, 1e-6f);
    TEST_ASSERT_NEAR(g.enemies.guard.alertRadius, 6.5f, 1e-6f);
    TEST_ASSERT_NEAR(g.enemies.guard.attackRadius, 1.1f, 1e-6f);
    TEST_ASSERT_NEAR(g.enemies.guard.attackRate, 1.0f, 1e-6f);
    TEST_ASSERT_NEAR(g.enemies.guard.reactionTime, 0.5f, 1e-6f);
    TEST_ASSERT(g.enemies.guard.score == 100);
    TEST_ASSERT(g.pickups.health.value == 25);
    TEST_ASSERT(g.pickups.ammo.value == 12);

    // The same key name in two sections gives two independent values; missing keys keep
    // the documented defaults.
    const std::string partial =
        "{\"enemies\": {\"guard\": {\"damage\": 7, \"moveSpeed\": 0.5}}, \"weapon\": {\"damage\": 70}, "
        "\"player\": {\"moveSpeed\": 4.5}}";
    GameplayDefinitions p{};
    TEST_ASSERT(ParseGameplayDefinitions(partial, "partial.json", p, errors, warnings));
    TEST_ASSERT(p.enemies.guard.damage == 7);
    TEST_ASSERT(p.weapon.damage == 70);
    TEST_ASSERT_NEAR(p.player.moveSpeed, 4.5f, 1e-6f);
    TEST_ASSERT_NEAR(p.enemies.guard.moveSpeed, 0.5f, 1e-6f);
    TEST_ASSERT(p.enemies.guard.health == kDefaultEnemyDefinition.health);
    TEST_ASSERT(p.enemies.brute.health == kDefaultEnemyDefinitions.brute.health);
    TEST_ASSERT(p.player.maxHealth == kDefaultPlayerDefinition.maxHealth);
    TEST_ASSERT(p.pickups.ammo.value == kDefaultPickupDefinitions.ammo.value);

    GameplayDefinitions empty{};
    empty.enemies.guard.health = 1;
    TEST_ASSERT(ParseGameplayDefinitions("{}", "empty.json", empty, errors, warnings));
    TEST_ASSERT(empty.enemies.guard.health == kDefaultEnemyDefinition.health);
    TEST_ASSERT(errors.empty() && warnings.empty());

    // Backward compatibility: the pre-Phase 6 "enemy" section is read as the guard, with a
    // deprecation warning.
    GameplayDefinitions legacy{};
    TEST_ASSERT(ParseGameplayDefinitions("{\"enemy\": {\"health\": 70, \"damage\": 3}}", "legacy.json", legacy, errors,
                                         warnings));
    TEST_ASSERT(legacy.enemies.guard.health == 70 && legacy.enemies.guard.damage == 3);
    TEST_ASSERT(legacy.enemies.brute.health == kDefaultEnemyDefinitions.brute.health);
    TEST_ASSERT(errors.empty() && warnings.size() == 1);
    TEST_ASSERT(AnyMessageContains(warnings, {"legacy.json: section \"enemy\" is deprecated", "enemies.guard"}));
}

void TestGameplayDefinitionsInvalid() {
    struct Case {
        const char* json;
        const char* expected;  // must appear in an error message
    };
    const std::vector<Case> cases = {
        {"{\"player\": {\"moveSpeed\": 3.0,}}", "invalid JSON"},
        {"", "invalid JSON"},
        {"[1, 2]", "the top level must be an object"},
        {"{\"player\": 5}", "player must be an object, got 5"},
        {"{\"weapon\": {\"damage\": \"35\"}}", "weapon.damage must be an integer from 1 to 10000, got \"35\""},
        {"{\"weapon\": {\"damage\": 35.5}}", "weapon.damage must be an integer from 1 to 10000, got 35.5"},
        {"{\"weapon\": {\"damage\": true}}", "weapon.damage must be an integer"},
        {"{\"weapon\": {\"damage\": 0}}", "weapon.damage must be an integer from 1 to 10000, got 0"},
        {"{\"weapon\": {\"damage\": 99999999999999999999}}", "weapon.damage must be"},
        {"{\"weapon\": {\"range\": 0}}", "weapon.range must be a number > 0 and <= 1000, got 0"},
        {"{\"enemy\": {\"moveSpeed\": -1}}", "enemy.moveSpeed must be a number >= 0 and <= 20, got -1"},
        {"{\"enemy\": {\"attackRate\": 0}}", "enemy.attackRate must be a number > 0"},
        {"{\"enemies\": {\"brute\": {\"moveSpeed\": 21}}}",
         "enemies.brute.moveSpeed must be a number >= 0 and <= 20, got 21"},
        {"{\"enemies\": {\"runner\": {\"reactionTime\": -1}}}", "enemies.runner.reactionTime must be a number >= 0"},
        {"{\"enemies\": {\"guard\": {\"score\": -5}}}",
         "enemies.guard.score must be an integer from 0 to 10000, got -5"},
        {"{\"enemies\": {\"guard\": {\"health\": 0}}}", "enemies.guard.health must be an integer from 1"},
        {"{\"enemies\": 5}", "enemies must be an object, got 5"},
        {"{\"enemies\": {\"brute\": []}}", "enemies.brute must be an object, got an array"},
        {"{\"enemy\": {}, \"enemies\": {}}", "use either \"enemies\" or the old \"enemy\" section, not both"},
        {"{\"weapon\": {\"noiseRadius\": -1}}", "weapon.noiseRadius must be a number >= 0"},
        {"{\"player\": {\"collisionRadius\": 0.5}}", "player.collisionRadius must be a number > 0 and < 0.5"},
        {"{\"player\": {\"turnSpeed\": null}}", "player.turnSpeed must be a number > 0 and <= 20, got null"},
        {"{\"pickups\": {\"ammo\": {\"value\": 0}}}", "pickups.ammo.value must be an integer from 1"},
        {"{\"pickups\": {\"health\": []}}", "pickups.health must be an object, got an array"},
        {"{\"enemy\": {\"damage\": 12, \"damage\": 10}}", "duplicate key \"damage\" inside \"enemy\""},
        {"{\"player\": {}, \"player\": {}}", "duplicate key \"player\" at the top level"},
        {"{\"player\": {\"initialHealth\": 150}}", "player.initialHealth (150) must not exceed player.maxHealth (100)"},
        {"{\"weapon\": {\"initialAmmo\": 100}}", "weapon.initialAmmo (100) must not exceed weapon.maxAmmo (99)"},
    };
    for (const Case& c : cases) {
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        GameplayDefinitions g{};
        g.weapon.damage = 777;  // must survive a failed load
        const bool ok = ParseGameplayDefinitions(c.json, "bad/gameplay.json", g, errors, warnings);
        if (ok || !AnyMessageContains(errors, {"bad/gameplay.json: ", c.expected})) {
            std::string all;
            for (const std::string& e : errors) {
                all += "\n         " + e;
            }
            FailTest(__FILE__, __LINE__,
                     std::string("case ") + c.json + ": expected an error containing '" + c.expected + "', got:" + all);
        }
        TEST_ASSERT(g.weapon.damage == 777);
    }

    // A missing file is an error.
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    GameplayDefinitions g{};
    TEST_ASSERT(!LoadGameplayDefinitions(ResolveRepoPath("game/defs") + "/no-such-gameplay.json", g, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"Unable to open gameplay definitions", "no-such-gameplay.json"}));

    // Unknown keys are warnings only; the pre-Phase 4 keys say where they moved.
    errors.clear();
    const std::string withUnknown =
        "{\"player\": {\"speed\": 9}, \"pickups\": {\"health\": {\"value\": 30, \"maxPlayerHealth\": 100}}, \"music\": "
        "1, "
        "\"enemies\": {\"ninja\": {}, \"guard\": {\"armour\": 3}}}";
    TEST_ASSERT(ParseGameplayDefinitions(withUnknown, "old.json", g, errors, warnings));
    TEST_ASSERT(errors.empty());
    TEST_ASSERT(g.pickups.health.value == 30);
    TEST_ASSERT(warnings.size() == 5);
    TEST_ASSERT(AnyMessageContains(warnings, {"old.json: unknown key player.speed ignored"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"unknown key enemies.ninja ignored"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"unknown key enemies.guard.armour ignored"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"pickups.health.maxPlayerHealth", "moved to player.maxHealth"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"unknown key music ignored"}));
}

void TestEntityPlacementsJsonAndCsv() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<EntityPlacement> fromJson;
    std::vector<EntityPlacement> fromCsv;
    TEST_ASSERT(
        LoadEntityPlacementsFromFile(ReferencePath("game/defs/level01.entities.json"), fromJson, errors, warnings));
    TEST_ASSERT(
        LoadEntityPlacementsFromFile(ReferencePath("game/defs/level01.entities.csv"), fromCsv, errors, warnings));
    TEST_ASSERT(errors.empty());
    TEST_ASSERT(warnings.empty());

    // level01: exactly these seven, in file order.
    const std::array<PlacementType, ReferenceLevel1::kPlacementCount> expectedTypes = {
        PlacementType::Player,  PlacementType::Enemy,      PlacementType::Pickup, PlacementType::Door,
        PlacementType::Trigger, PlacementType::Decoration, PlacementType::Pickup};
    TEST_ASSERT(fromJson.size() == ReferenceLevel1::kPlacementCount);
    for (std::size_t i = 0; i < fromJson.size(); ++i) {
        TEST_ASSERT(fromJson[i].type == expectedTypes[i]);
    }
    TEST_ASSERT_NEAR(fromJson[0].position.x, ReferenceLevel1::kSpawnX, 1e-6f);
    TEST_ASSERT_NEAR(fromJson[0].angleDegrees, 0.0f, 1e-6f);
    TEST_ASSERT(fromJson[2].pickupKind == PickupType::Health);
    TEST_ASSERT(!fromJson[2].pickupValue.has_value());
    TEST_ASSERT(fromJson[3].mapX == ReferenceLevel1::kDoorX && fromJson[3].mapY == ReferenceLevel1::kDoorY);
    TEST_ASSERT(fromJson[4].mapX == ReferenceLevel1::kTriggerX && fromJson[4].mapY == ReferenceLevel1::kTriggerY);
    TEST_ASSERT(fromJson[2].source.find("entities[2]") != std::string::npos);
    TEST_ASSERT(fromJson[1].variant == EnemyVariant::Guard);
    TEST_ASSERT(fromJson[6].pickupKind == PickupType::Ammo);

    // The CSV export is equivalent, field by field.
    const auto expectEquivalent = [](const std::vector<EntityPlacement>& json,
                                     const std::vector<EntityPlacement>& csv) {
        TEST_ASSERT(csv.size() == json.size());
        for (std::size_t i = 0; i < json.size(); ++i) {
            TEST_ASSERT(csv[i].type == json[i].type);
            TEST_ASSERT_NEAR(csv[i].position.x, json[i].position.x, 1e-6f);
            TEST_ASSERT_NEAR(csv[i].position.y, json[i].position.y, 1e-6f);
            TEST_ASSERT(csv[i].mapX == json[i].mapX && csv[i].mapY == json[i].mapY);
            TEST_ASSERT_NEAR(csv[i].angleDegrees, json[i].angleDegrees, 1e-6f);
            TEST_ASSERT(csv[i].pickupKind == json[i].pickupKind);
            TEST_ASSERT(csv[i].pickupValue == json[i].pickupValue);
            TEST_ASSERT(csv[i].variant == json[i].variant);
            TEST_ASSERT(csv[i].key == json[i].key && csv[i].lock == json[i].lock);
            TEST_ASSERT(csv[i].message == json[i].message);
        }
    };
    expectEquivalent(fromJson, fromCsv);

    // Every shipped campaign level whose entity file has a CSV export next to it: the export
    // is up to date (`devkit sync-entities` after editing the JSON).
    const CampaignDefinition campaign = ShippedCampaign();
    TEST_ASSERT(!campaign.levels.empty());
    std::vector<CampaignLevel> levelsWithExports = campaign.levels;
    if (campaign.testLevel) {
        levelsWithExports.push_back(*campaign.testLevel);
    }
    for (const CampaignLevel& level : levelsWithExports) {
        const std::string jsonExtension = ".json";
        if (level.entities.size() <= jsonExtension.size() ||
            level.entities.compare(level.entities.size() - jsonExtension.size(), jsonExtension.size(), jsonExtension) !=
                0) {
            continue;
        }
        const std::string jsonPath = ResolveRepoPath(level.entities);
        const std::string csvPath = jsonPath.substr(0, jsonPath.size() - jsonExtension.size()) + ".csv";
        if (!std::filesystem::exists(PathFromUtf8(csvPath))) {
            continue;
        }
        std::vector<EntityPlacement> shippedJson;
        std::vector<EntityPlacement> shippedCsv;
        TEST_ASSERT(LoadEntityPlacementsFromFile(jsonPath, shippedJson, errors, warnings));
        TEST_ASSERT(LoadEntityPlacementsFromFile(csvPath, shippedCsv, errors, warnings));
        TEST_ASSERT(errors.empty() && warnings.empty());
        expectEquivalent(shippedJson, shippedCsv);
    }

    // Every optional field, in both formats.
    const std::string fullJson =
        "{\"level\": \"t\", \"entities\": ["
        "{\"type\": \"player\", \"x\": 1.5, \"y\": 1.5, \"angle\": 90},"
        "{\"type\": \"enemy\", \"x\": 3, \"y\": 1.5, \"variant\": \"brute\"},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 2, \"kind\": \"ammo\", \"value\": 30},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 3, \"kind\": \"key\", \"key\": \"gold\"},"
        "{\"type\": \"door\", \"x\": 4.5, \"y\": 1.5, \"mapX\": 4, \"mapY\": 1, \"lock\": \"gold\"},"
        "{\"type\": \"trigger\", \"x\": 5.5, \"y\": 1.5, \"mapX\": 5, \"mapY\": 1, \"message\": \"WEAPON RANGE: "
        "12\"}]}";
    const std::string fullCsv =
        "# type,x,y,mapX,mapY,kind,value,angle,variant,key,lock,message\n"
        "player,1.5,1.5,,,,,90\n"
        "enemy,3,1.5,,,,,,brute\n"
        "pickup,2,2,,,ammo,30\n"
        "pickup,2,3,,,key,,,,gold\n"
        "door,4.5,1.5,4,1,,,,,,gold\n"
        "trigger,5.5,1.5,5,1,,,,,,,WEAPON RANGE: 12\n";
    TEST_ASSERT(ParseEntityPlacementsJson(fullJson, "t.json", fromJson, errors, warnings));
    TEST_ASSERT(ParseEntityPlacementsCsv(fullCsv, "t.csv", fromCsv, errors, warnings));
    TEST_ASSERT(errors.empty() && warnings.empty());
    TEST_ASSERT(fromJson.size() == 6 && fromCsv.size() == 6);
    expectEquivalent(fromJson, fromCsv);
    TEST_ASSERT(fromJson[5].message == "WEAPON RANGE: 12" && fromJson[4].message.empty());

    // A trigger's "message": a non-empty string of at most 40 characters without a comma, a
    // control character or a space at either end (the CSV export trims cells); on another type it
    // is ignored with a warning.
    const std::vector<std::pair<std::string, const char*>> badMessages = {
        {"\"\"", "got \"\""},
        {"\"A, B\"", "without a comma"},
        {"\" ARMORY\"", "a space at either end"},
        {"\"   \"", "a space at either end"},
        {"\"A\\tB\"", "a control character"},
        {"\"TWO\\nLINES\"", "a control character"},
        {"7", "got 7"},
        {"\"" + std::string(kMaxTriggerMessageLength + 1, 'A') + "\"", "at most 40 characters"},
    };
    for (const auto& [value, expected] : badMessages) {
        errors.clear();
        warnings.clear();
        std::vector<EntityPlacement> triggers;
        const std::string text =
            "{\"entities\": [{\"type\": \"trigger\", \"x\": 1.5, \"y\": 1.5, \"mapX\": 1, "
            "\"mapY\": 1, \"message\": " +
            value + "}]}";
        TEST_ASSERT(!ParseEntityPlacementsJson(text, "m.json", triggers, errors, warnings));
        if (!AnyMessageContains(errors, {"entities[0] (trigger): field \"message\" must be", expected})) {
            FailTest(__FILE__, __LINE__, std::string("expected a message error containing: ") + expected);
        }
    }
    // A CSV cell is not checked for UTF-8: a message must be well-formed UTF-8 (the JSON twin
    // could not hold it otherwise), and invalid bytes in any text cell give an error, not a throw.
    const std::vector<std::pair<std::string, const char*>> badCsvBytes = {
        {"trigger,1.5,1.5,1,1,,,,,,,CAF\xE9", "field \"message\" must be"},       // Latin-1 e-acute
        {"trigger,1.5,1.5,1,1,,,,,,,\xED\xA0\x80", "field \"message\" must be"},  // a surrogate
        {"trigger,1.5,1.5,1,1,,,,,,,\xC0\xAF", "field \"message\" must be"},      // overlong '/'
        {"door,1.5,1.5,1,1,,,,,,\xE9", "field \"lock\" must be"},
    };
    for (const auto& [row, expected] : badCsvBytes) {
        errors.clear();
        warnings.clear();
        std::vector<EntityPlacement> placements;
        TEST_ASSERT(!ParseEntityPlacementsCsv("player,1.5,1.5\n" + row + "\n", "u.csv", placements, errors, warnings));
        if (!AnyMessageContains(errors, {expected})) {
            FailTest(__FILE__, __LINE__, std::string("expected an error containing: ") + expected);
        }
    }
    errors.clear();
    warnings.clear();
    std::vector<EntityPlacement> accented;
    TEST_ASSERT(ParseEntityPlacementsCsv("player,1.5,1.5\ntrigger,1.5,1.5,1,1,,,,,,,CAF\xC3\xA9 \xF0\x9F\x99\x82\n",
                                         "u.csv", accented, errors, warnings));
    TEST_ASSERT(errors.empty() && accented.size() == 2 && accented[1].message == "CAF\xC3\xA9 \xF0\x9F\x99\x82");
    errors.clear();
    warnings.clear();
    std::vector<EntityPlacement> doors;
    TEST_ASSERT(ParseEntityPlacementsJson(
        R"({"entities": [{"type": "door", "x": 1.5, "y": 1.5, "mapX": 1, "mapY": 1, "message": "HI"}]})", "m.json",
        doors, errors, warnings));
    TEST_ASSERT(AnyMessageContains(warnings, {"field \"message\" only applies to trigger"}));
    for (const std::vector<EntityPlacement>* list : {&fromJson, &fromCsv}) {
        TEST_ASSERT_NEAR((*list)[0].angleDegrees, 90.0f, 1e-6f);
        TEST_ASSERT((*list)[1].variant == EnemyVariant::Brute);
        TEST_ASSERT((*list)[2].pickupKind == PickupType::Ammo);
        TEST_ASSERT((*list)[2].pickupValue == 30);
        TEST_ASSERT((*list)[3].pickupKind == PickupType::Key && (*list)[3].key == KeyColor::Gold);
        TEST_ASSERT((*list)[4].lock == KeyColor::Gold);
    }

    // Errors name the file, the entity and the field; valid entries are still returned.
    const std::string badJson =
        "{\"level\": \"bad\", \"entities\": ["
        "{\"type\": \"player\", \"x\": 1.5, \"y\": 1.5},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 2},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 2, \"kind\": \"armor\"},"
        "{\"type\": \"door\", \"x\": 3, \"y\": 3},"
        "{\"type\": \"ghost\", \"x\": 1, \"y\": 1},"
        "{\"type\": \"enemy\", \"x\": \"1\", \"y\": 1},"
        "{\"type\": \"enemy\", \"x\": 1, \"y\": 1, \"colour\": \"red\", \"kind\": \"ammo\"},"
        "{\"x\": 1, \"y\": 1}, 7]}";
    std::vector<EntityPlacement> placements;
    TEST_ASSERT(!ParseEntityPlacementsJson(badJson, "bad.entities.json", placements, errors, warnings));
    TEST_ASSERT(placements.size() == 2);
    TEST_ASSERT(AnyMessageContains(errors, {"bad.entities.json: entities[1] (pickup): field \"kind\" is required"}));
    TEST_ASSERT(AnyMessageContains(
        errors, {"entities[2] (pickup): field \"kind\" must be \"health\", \"ammo\" or \"key\", got \"armor\""}));
    TEST_ASSERT(AnyMessageContains(errors, {"entities[3] (door): fields \"mapX\" and \"mapY\" are required"}));
    TEST_ASSERT(AnyMessageContains(errors, {"entities[4]: field \"type\" must be one of", "\"ghost\""}));
    TEST_ASSERT(AnyMessageContains(errors, {"entities[5] (enemy): field \"x\" must be a number, got \"1\""}));
    TEST_ASSERT(AnyMessageContains(errors, {"entities[7]: field \"type\" is required"}));
    TEST_ASSERT(AnyMessageContains(errors, {"entities[8]: each entity must be an object, got 7"}));
    TEST_ASSERT(errors.size() == 7);
    TEST_ASSERT(AnyMessageContains(warnings, {"entities[6] (enemy): unknown field \"colour\" ignored"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"entities[6] (enemy): field \"kind\" only applies to pickup; ignored"}));

    errors.clear();
    TEST_ASSERT(!ParseEntityPlacementsJson("{\"level\": \"x\"}", "noarray.json", placements, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"noarray.json: field \"entities\" is required"}));
    errors.clear();
    TEST_ASSERT(!ParseEntityPlacementsJson("player,1,1\n", "csv-as.json", placements, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"csv-as.json: invalid JSON"}));

    errors.clear();
    const std::string badCsv = "# header\npickup,2,2\ndoor,3,3,x,1\nplayer,1\n";
    TEST_ASSERT(!ParseEntityPlacementsCsv(badCsv, "bad.entities.csv", placements, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"bad.entities.csv:2 (entity 0) (pickup): field \"kind\" is required"}));
    TEST_ASSERT(
        AnyMessageContains(errors, {"bad.entities.csv:3 (entity 1): column \"mapX\" must be an integer, got 'x'"}));
    TEST_ASSERT(AnyMessageContains(errors, {"bad.entities.csv:4 (entity 2): expected at least 3 columns"}));

    errors.clear();
    TEST_ASSERT(!LoadEntityPlacementsFromFile("level01.entities.txt", placements, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"level01.entities.txt: unsupported entity file type"}));
    errors.clear();
    TEST_ASSERT(!LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs") + "/missing.entities.json", placements,
                                              errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"Unable to open entity placement file", "missing.entities.json"}));
}

void TestDecorationsAndSpriteSync() {
    TEST_ASSERT(static_cast<int>(EntityType::Decoration) == 5);  // appended: earlier values unchanged
    TEST_ASSERT(static_cast<int>(EntityType::Trigger) == 4);

    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    Map map{};
    TEST_ASSERT(LoadMapFromFile(ReferencePath("game/maps/level01.map"), map, errors));
    std::vector<EntityPlacement> placements;
    TEST_ASSERT(
        LoadEntityPlacementsFromFile(ReferencePath("game/defs/level01.entities.json"), placements, errors, warnings));
    LevelContents level{};
    TEST_ASSERT(BuildLevelFromPlacements(map, placements, GameplayDefinitions{}, level, errors));

    TEST_ASSERT(CountEntities(level.entities, EntityType::Trigger) == 1);  // only the real exit
    TEST_ASSERT(CountEntities(level.entities, EntityType::Decoration) == 1);
    const Entity* decoration = FindFirstEntity(level.entities, EntityType::Decoration);
    TEST_ASSERT(decoration != nullptr);
    const int decorationId = decoration->id;
    TEST_ASSERT_NEAR(decoration->position.x, 4.2f, 1e-6f);
    const SpriteEntity* decorationSprite = FindSpriteFor(level.sprites, decorationId);
    TEST_ASSERT(decorationSprite != nullptr);
    TEST_ASSERT(decorationSprite->type == SpriteType::Decoration);
    TEST_ASSERT(decorationSprite->textureId == 2);
    TEST_ASSERT(map.GetCell(ReferenceLevel1::kTriggerX, ReferenceLevel1::kTriggerY) == TileType::Trigger);
    TEST_ASSERT(map.GetCell(ReferenceLevel1::kDoorX, ReferenceLevel1::kDoorY) == TileType::Door);

    // Standing on the decoration does nothing (it used to be a Trigger and won the level).
    Player player = level.player;
    player.position = decoration->position;
    SyncPlayerEntity(level.entities, player);
    int health = 100;
    int ammo = 24;
    GameplayEvents events{};
    UpdateEntityInteractions(level.entities, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true, events);
    TEST_ASSERT(!events.triggerActivated);
    TEST_ASSERT(!events.pickupCollected);
    TEST_ASSERT(!events.doorToggled);

    // Sprite sync over ticks: the decoration survives, a collected pickup is hidden (not
    // dropped), a moving enemy's sprite follows it.
    Entity* pickup = nullptr;
    Entity* enemy = nullptr;
    for (Entity& entity : level.entities.Entities()) {
        pickup = entity.type == EntityType::Pickup ? &entity : pickup;
        enemy = entity.type == EntityType::Enemy ? &entity : enemy;
    }
    TEST_ASSERT(pickup != nullptr && enemy != nullptr);
    player.position = pickup->position;
    GameplayEvents pickupEvents{};
    UpdateEntityInteractions(level.entities, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, false,
                             pickupEvents);
    TEST_ASSERT(pickupEvents.pickupCollected);
    enemy->position = {7.0f, 2.5f};
    for (int tick = 0; tick < 3; ++tick) {
        SyncSpritesWithEntities(level.entities, level.sprites);
    }
    TEST_ASSERT(level.sprites.size() == ReferenceLevel1::kSpriteCount);
    decorationSprite = FindSpriteFor(level.sprites, decorationId);
    TEST_ASSERT(decorationSprite != nullptr && decorationSprite->active && decorationSprite->visible);
    const SpriteEntity* pickupSprite = FindSpriteFor(level.sprites, pickup->id);
    TEST_ASSERT(pickupSprite != nullptr && !pickupSprite->visible);
    const SpriteEntity* enemySprite = FindSpriteFor(level.sprites, enemy->id);
    TEST_ASSERT(enemySprite != nullptr && enemySprite->visible);
    TEST_ASSERT_NEAR(enemySprite->position.x, 7.0f, 1e-6f);

    // Destroyed entities lose their sprite; new ones (e.g. from the C API) get one.
    const int enemyId = enemy->id;
    TEST_ASSERT(level.entities.DestroyEntity(enemyId));
    const int spawnedId = level.entities.CreateEntity(EntityType::Decoration, {3.0f, 3.0f}).id;
    SyncSpritesWithEntities(level.entities, level.sprites);
    TEST_ASSERT(FindSpriteFor(level.sprites, enemyId) == nullptr);
    const SpriteEntity* spawnedSprite = FindSpriteFor(level.sprites, spawnedId);
    TEST_ASSERT(spawnedSprite != nullptr && spawnedSprite->type == SpriteType::Decoration);
    TEST_ASSERT(level.sprites.size() == ReferenceLevel1::kSpriteCount);
    TEST_ASSERT(FindSpriteFor(level.sprites, decorationId) != nullptr);
}

void TestPickupKindFromData() {
    const std::string text =
        "{\"entities\": ["
        "{\"type\": \"player\", \"x\": 1.5, \"y\": 1.5},"
        "{\"type\": \"pickup\", \"x\": 2.5, \"y\": 1.5, \"kind\": \"ammo\"},"
        "{\"type\": \"pickup\", \"x\": 3.5, \"y\": 1.5, \"kind\": \"ammo\"},"
        "{\"type\": \"pickup\", \"x\": 4.5, \"y\": 1.5, \"kind\": \"health\", \"value\": 5},"
        "{\"type\": \"pickup\", \"x\": 5.5, \"y\": 1.5, \"kind\": \"health\"}]}";
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<EntityPlacement> placements;
    TEST_ASSERT(ParseEntityPlacementsJson(text, "pickups.json", placements, errors, warnings));

    GameplayDefinitions gameplay{};
    gameplay.pickups.health.value = 30;
    gameplay.pickups.ammo.value = 7;
    Map map(8, 4);
    BuildBorderWalls(map);
    LevelContents level{};
    TEST_ASSERT(BuildLevelFromPlacements(map, placements, gameplay, level, errors));

    // Kind and value come from the data (the old code alternated by entity count).
    std::vector<const Entity*> pickups;
    for (const Entity& entity : level.entities.Entities()) {
        if (entity.type == EntityType::Pickup) {
            pickups.push_back(&entity);
        }
    }
    TEST_ASSERT(pickups.size() == 4);
    TEST_ASSERT(pickups[0]->pickupType == PickupType::Ammo && pickups[0]->pickupValue == 7);
    TEST_ASSERT(pickups[1]->pickupType == PickupType::Ammo && pickups[1]->pickupValue == 7);
    TEST_ASSERT(pickups[2]->pickupType == PickupType::Health && pickups[2]->pickupValue == 5);
    TEST_ASSERT(pickups[3]->pickupType == PickupType::Health && pickups[3]->pickupValue == 30);

    // Collecting applies that value, capped by the maximum.
    Player player = level.player;
    player.position = pickups[0]->position;
    int health = 90;
    int ammo = 10;
    GameplayEvents events{};
    UpdateEntityInteractions(level.entities, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, false,
                             events);
    TEST_ASSERT(events.pickupCollected && events.pickupType == PickupType::Ammo && events.pickupValue == 7);
    TEST_ASSERT(ammo == 17);
    player.position = pickups[3]->position;
    GameplayEvents healthEvents{};
    UpdateEntityInteractions(level.entities, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, false,
                             healthEvents);
    TEST_ASSERT(healthEvents.pickupValue == 30);
    TEST_ASSERT(health == 100);
}

// A content root with level01.map and the given gameplay.json and level01.entities.json.
void WriteContentRoot(const std::filesystem::path& root, const std::string& gameplayJson,
                      const std::string& entitiesJson) {
    MakeContentRoot(root);
    std::filesystem::copy_file(PathFromUtf8(ReferencePath("game/maps/level01.map")),
                               root / "game" / "maps" / "level01.map",
                               std::filesystem::copy_options::overwrite_existing);
    WriteTextFile(root / "game" / "defs" / "gameplay.json", gameplayJson);
    WriteTextFile(root / "game" / "defs" / "level01.entities.json", entitiesJson);
}

void TestEditedDataChangesGame() {
    const ScratchDir scratch("edited-data");
    const std::filesystem::path root = scratch.Path() / "content";
    const std::string gameplay = R"({
  "player": { "initialHealth": 50, "maxHealth": 150, "moveSpeed": 4.5 },
  "weapon": { "damage": 20, "fireRate": 0.5, "initialAmmo": 7, "maxAmmo": 30 },
  "enemies": { "guard": { "health": 80, "damage": 9, "alertRadius": 3.0 } },
  "pickups": { "health": { "value": 40 }, "ammo": { "value": 6 } }
})";
    const std::string entities = R"({
  "level": "level01",
  "entities": [
    { "type": "player", "x": 2.5, "y": 2.5, "angle": 90 },
    { "type": "pickup", "x": 2.6, "y": 2.5, "kind": "health" },
    { "type": "pickup", "x": 13.5, "y": 1.5, "kind": "ammo", "value": 11 },
    { "type": "enemy", "x": 13.5, "y": 10.5, "variant": "guard" },
    { "type": "decoration", "x": 4.5, "y": 1.5 },
    { "type": "door", "x": 8.5, "y": 7.5, "mapX": 8, "mapY": 7 },
    { "type": "trigger", "x": 13.5, "y": 8.5, "mapX": 13, "mapY": 8 }
  ]
})";
    WriteContentRoot(root, gameplay, entities);

    // The real engine path: Engine::LoadGameplay + Engine::LoadLevel (as Init does), then a
    // frame of Engine::Update. No window is needed.
    Engine engine;
    engine.contentRoot_ = root;
    TEST_ASSERT(engine.LoadGameplay("game/defs/gameplay.json"));
    TEST_ASSERT(engine.LoadLevel("game/maps/level01.map", "game/defs/level01.entities.json"));

    TEST_ASSERT(engine.playerHealth_ == 50);
    TEST_ASSERT(engine.playerMaxHealth_ == 150);
    TEST_ASSERT(engine.playerAmmo_ == 7);
    TEST_ASSERT(engine.playerMaxAmmo_ == 30);
    TEST_ASSERT_NEAR(engine.player_.moveSpeed, 4.5f, 1e-6f);
    TEST_ASSERT_NEAR(engine.player_.turnSpeedRadians, kDefaultPlayerDefinition.turnSpeed, 1e-6f);  // not in the file
    TEST_ASSERT_NEAR(engine.player_.directionAngleRadians, kPi / 2.0f, 1e-5f);
    TEST_ASSERT(engine.entityManager_.Entities().size() == 7);
    const Entity* enemy = FindFirstEntity(engine.entityManager_, EntityType::Enemy);
    TEST_ASSERT(enemy != nullptr);
    TEST_ASSERT(enemy->health == 80 && enemy->maxHealth == 80 && enemy->damage == 9);
    TEST_ASSERT_NEAR(enemy->alertRadius, 3.0f, 1e-6f);
    int ammoPickupValue = 0;
    for (const Entity& entity : engine.entityManager_.Entities()) {
        if (entity.type == EntityType::Pickup && entity.pickupType == PickupType::Ammo) {
            ammoPickupValue = entity.pickupValue;
        }
    }
    TEST_ASSERT(ammoPickupValue == 11);  // the placement's value overrides pickups.ammo.value

    // One frame: the player stands on the health pickup.
    engine.running_ = true;
    engine.SetState(GameState::Gameplay);
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.GetState() == GameState::Gameplay);
    TEST_ASSERT(engine.playerHealth_ == 90);
    TEST_ASSERT(engine.hudMessage_ == "HEALTH RESTORED (+40 HP)");
    const Entity* decoration = FindFirstEntity(engine.entityManager_, EntityType::Decoration);
    TEST_ASSERT(decoration != nullptr);
    const SpriteEntity* decorationSprite = FindSpriteFor(engine.sprites_, decoration->id);
    TEST_ASSERT(decorationSprite != nullptr && decorationSprite->visible);  // survives the update tick
    TEST_ASSERT(engine.sprites_.size() == 4);

    // Shooting uses the weapon values.
    engine.player_.position = {12.5f, 10.5f};
    engine.player_.directionAngleRadians = 0.0f;  // facing the enemy at (13.5, 10.5)
    engine.PlayerAttack();
    TEST_ASSERT(engine.playerAmmo_ == 6);
    TEST_ASSERT_NEAR(engine.playerAttackCooldown_, 0.5f, 1e-6f);
    TEST_ASSERT(engine.hudMessage_ == "Hit enemy! (-20 HP)");
    TEST_ASSERT(FindFirstEntity(engine.entityManager_, EntityType::Enemy)->health == 60);

    // Edit the file again and reload: the new value applies to the next level load.
    WriteTextFile(root / "game" / "defs" / "gameplay.json", "{\"enemy\": {\"health\": 25}}");
    TEST_ASSERT(engine.LoadGameplay("game/defs/gameplay.json"));
    engine.ResetLevel();
    TEST_ASSERT(FindFirstEntity(engine.entityManager_, EntityType::Enemy)->health == 25);
    TEST_ASSERT(engine.playerHealth_ == kDefaultPlayerDefinition.initialHealth);

    // A broken file is rejected and the previous values stay.
    WriteTextFile(root / "game" / "defs" / "gameplay.json", "{\"enemy\": {\"health\": \"lots\"}}");
    TEST_ASSERT(!engine.LoadGameplay("game/defs/gameplay.json"));
    TEST_ASSERT(engine.gameplay_.enemies.guard.health == 25);

    engine.running_ = false;
}

void TestLevelLoadFailures() {
    const ScratchDir scratch("level-load-failures");
    const std::filesystem::path root = scratch.Path() / "content";
    WriteContentRoot(root, ReadWholeFile(ResolveRepoPath("game/defs/gameplay.json")),
                     ReadWholeFile(ReferencePath("game/defs/level01.entities.json")));
    std::filesystem::copy_file(PathFromUtf8(ReferencePath("game/defs/level01.entities.csv")),
                               root / "game" / "defs" / "level01.entities.csv");

    Engine engine;
    engine.contentRoot_ = root;
    TEST_ASSERT(engine.LoadGameplay("game/defs/gameplay.json"));
    const std::string mapPath = "game/maps/level01.map";
    TEST_ASSERT(engine.LoadLevel(mapPath, "game/defs/level01.entities.json"));
    TEST_ASSERT(engine.entityManager_.Entities().size() == ReferenceLevel1::kPlacementCount);
    TEST_ASSERT(engine.LoadLevel(mapPath, "game/defs/level01.entities.csv"));  // legacy format still loads
    TEST_ASSERT(engine.entityManager_.Entities().size() == ReferenceLevel1::kPlacementCount);

    const auto writeEntities = [&](const std::string& name, const std::string& text) {
        WriteTextFile(root / "game" / "defs" / name, text);
        return "game/defs/" + name;
    };
    const std::string player = "{\"type\": \"player\", \"x\": 2.5, \"y\": 2.5}";
    const std::vector<std::string> failing = {
        "game/defs/missing.entities.json",  // no built-in layout any more
        writeEntities("syntax.entities.json", "{\"entities\": [ {\"type\": \"player\", \"x\": 2.5 "),
        writeEntities("nokind.entities.json",
                      "{\"entities\": [" + player + ", {\"type\": \"pickup\", \"x\": 3.5, \"y\": 2.5}]}"),
        writeEntities("noplayer.entities.json", "{\"entities\": [{\"type\": \"enemy\", \"x\": 3.5, \"y\": 2.5}]}"),
        writeEntities("twoplayers.entities.json", "{\"entities\": [" + player + ", " + player + "]}"),
        writeEntities("outside.entities.json",
                      "{\"entities\": [" + player + ", {\"type\": \"enemy\", \"x\": 30.5, \"y\": 2.5}]}"),
        writeEntities("badcell.entities.json",
                      "{\"entities\": [" + player +
                          ", {\"type\": \"door\", \"x\": 8.5, \"y\": 7.5, \"mapX\": 16, \"mapY\": 7}]}"),
    };
    for (const std::string& entitiesPath : failing) {
        TEST_ASSERT(!engine.LoadLevel(mapPath, entitiesPath));
        // A failed load leaves the current level as it was.
        TEST_ASSERT(engine.entityManager_.Entities().size() == ReferenceLevel1::kPlacementCount);
        TEST_ASSERT(engine.currentEntitiesPath_ == "game/defs/level01.entities.csv");
        TEST_ASSERT(engine.map_.GetCell(ReferenceLevel1::kDoorX, ReferenceLevel1::kDoorY) == TileType::Door);
    }
    TEST_ASSERT(!engine.LoadLevel("game/maps/missing.map", "game/defs/level01.entities.json"));

    // Builder messages name the file, the entity and the problem.
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    Map map{};
    TEST_ASSERT(LoadMapFromFile(PathToUtf8(root / "game" / "maps" / "level01.map"), map, errors));
    std::vector<EntityPlacement> placements;
    TEST_ASSERT(LoadEntityPlacementsFromFile(PathToUtf8(root / "game" / "defs" / "badcell.entities.json"), placements,
                                             errors, warnings));
    LevelContents level{};
    const Map before = map;
    TEST_ASSERT(!BuildLevelFromPlacements(map, placements, GameplayDefinitions{}, level, errors));
    TEST_ASSERT(AnyMessageContains(
        errors, {"badcell.entities.json: entities[1] (door): map cell (16, 7) is outside the 16x12 map"}));
    TEST_ASSERT(map.GetCell(ReferenceLevel1::kDoorX, ReferenceLevel1::kDoorY) ==
                before.GetCell(ReferenceLevel1::kDoorX, ReferenceLevel1::kDoorY));
    errors.clear();
    TEST_ASSERT(LoadEntityPlacementsFromFile(PathToUtf8(root / "game" / "defs" / "noplayer.entities.json"), placements,
                                             errors, warnings));
    TEST_ASSERT(!BuildLevelFromPlacements(map, placements, GameplayDefinitions{}, level, errors));
    TEST_ASSERT(AnyMessageContains(errors, {"no player placement"}));
}

// --- Phase 5: campaign & progression ------------------------------------------------------

// One frame of input for GameFlow tests: `pressed` actions go down this frame (a fresh press),
// everything else is up.
InputState Press(std::initializer_list<InputAction> pressed) {
    InputState input{};
    input.BeginFrame();
    for (const InputAction action : pressed) {
        input.SetActionState(action, true);
    }
    return input;
}

const InputState kNoInput{};

// A GameFlow on the main menu for a campaign of `levels` levels, with saved progress up to
// level `highest` if `hasProgress`.
GameFlow MakeFlow(int levels, bool hasProgress = false, int highest = 1) {
    GameFlowConfig config{};
    config.levelCount = levels;
    config.initial = PlayerStats{100, 24};
    config.maximum = PlayerStats{kPlayerMaxHealth, kPlayerMaxAmmo};
    GameFlow flow;
    flow.Configure(config);
    flow.SetProgress(hasProgress, highest);
    return flow;
}

const FlowRequest* FindRequest(const std::vector<FlowRequest>& requests, FlowRequestType type) {
    for (const FlowRequest& request : requests) {
        if (request.type == type) {
            return &request;
        }
    }
    return nullptr;
}

bool HasRequest(const std::vector<FlowRequest>& requests, FlowRequestType type) {
    return FindRequest(requests, type) != nullptr;
}

// The events of a frame in which the current level was completed with `health` and `ammo`.
FlowEvents LevelCompletedEvents(int health, int ammo) {
    FlowEvents events{};
    events.levelCompleted = true;
    events.stats = PlayerStats{health, ammo};
    events.results.kills = 1;
    events.results.totalEnemies = 2;
    events.results.seconds = 12.5;
    return events;
}

void TestWallTextureOrientation() {
    // A wall texture must read left-to-right from every side (before Phase 5 every wall was
    // mirrored, which showed on the exit switch's "EXIT" label). Texture: left half red,
    // right half blue; the player stands 0.6 cells from one face of an exit cell and looks at it.
    Texture leftRed(2, 1);
    leftRed.SetPixel(0, 0, PackARGB32(MakeColor(255, 0, 0)));
    leftRed.SetPixel(1, 0, PackARGB32(MakeColor(0, 0, 255)));
    Map map(7, 7);
    BuildBorderWalls(map);
    map.SetCell(3, 3, TileType::Exit);
    RaycasterRenderParams params{};
    params.exitTexture = &leftRed;
    params.wallTexture = &leftRed;

    struct View {
        Vec2 position;
        float angle = 0.0f;
        const char* name = nullptr;
    };
    const std::vector<View> views = {
        {{3.5f, 2.4f}, kPi / 2.0f, "looking +y at the north face"},
        {{3.5f, 4.6f}, 3.0f * kPi / 2.0f, "looking -y at the south face"},
        {{2.4f, 3.5f}, 0.0f, "looking +x at the west face"},
        {{4.6f, 3.5f}, kPi, "looking -x at the east face"},
    };
    for (const View& view : views) {
        Player player{};
        player.position = view.position;
        player.directionAngleRadians = view.angle;
        Framebuffer framebuffer(64, 48);
        std::vector<RaycastHit> hits;
        Raycaster::RenderWalls(map, player, framebuffer, params, hits);
        TEST_ASSERT(hits[16].tileType == TileType::Exit && hits[48].tileType == TileType::Exit);
        const std::uint32_t left = PixelAt(framebuffer, 16, 24);
        const std::uint32_t right = PixelAt(framebuffer, 48, 24);
        const bool leftIsRed = RedOf(left) > BlueOf(left);
        const bool rightIsBlue = BlueOf(right) > RedOf(right);
        if (!leftIsRed || !rightIsBlue) {
            FailTest(__FILE__, __LINE__, std::string("mirrored wall texture when ") + view.name);
        }
    }
}

void TestGameFlowMenu() {
    // No progress: New Game, Level Select, Quit (Continue only with saved progress).
    GameFlow flow = MakeFlow(3);
    TEST_ASSERT(flow.State() == GameState::Menu && flow.Page() == MenuPage::Main);
    std::vector<MenuItem> items = flow.MenuItems();
    TEST_ASSERT(items.size() == 3);
    TEST_ASSERT(items[0] == MenuItem::NewGame && items[1] == MenuItem::LevelSelect && items[2] == MenuItem::Quit);
    TEST_ASSERT(std::string(MenuItemLabel(MenuItem::LevelSelect)) == "LEVEL SELECT");

    // W/S and Up/Down (MoveForward/MoveBackward) move the cursor, wrapping around.
    flow.Update(Press({InputAction::MoveForward}), {});
    TEST_ASSERT(flow.MenuCursor() == 2);
    flow.Update(Press({InputAction::MoveBackward}), {});
    TEST_ASSERT(flow.MenuCursor() == 0);
    flow.Update(Press({InputAction::MoveBackward}), {});
    TEST_ASSERT(flow.MenuCursor() == 1);

    // Level Select: unlocked levels can start, locked ones cannot; Esc goes back.
    std::vector<FlowRequest> requests = flow.Update(Press({InputAction::Select}), {});
    TEST_ASSERT(requests.empty());
    TEST_ASSERT(flow.State() == GameState::Menu && flow.Page() == MenuPage::LevelSelect);
    TEST_ASSERT(flow.LevelCursor() == 0);
    TEST_ASSERT(flow.IsLevelUnlocked(0) && !flow.IsLevelUnlocked(1) && !flow.IsLevelUnlocked(2));
    flow.Update(Press({InputAction::MoveBackward}), {});
    TEST_ASSERT(flow.LevelCursor() == 1);
    requests = flow.Update(Press({InputAction::Select}), {});
    TEST_ASSERT(requests.empty() && flow.State() == GameState::Menu);  // locked
    flow.Update(Press({InputAction::Back}), {});
    TEST_ASSERT(flow.Page() == MenuPage::Main && flow.State() == GameState::Menu);

    // Esc on the main menu quits, and so does the Quit item.
    requests = flow.Update(Press({InputAction::Back}), {});
    TEST_ASSERT(HasRequest(requests, FlowRequestType::Quit));
    GameFlow quitFlow = MakeFlow(3);
    quitFlow.Update(Press({InputAction::MoveForward}), {});  // wraps to Quit
    requests = quitFlow.Update(Press({InputAction::Select}), {});
    TEST_ASSERT(requests.size() == 1 && requests[0].type == FlowRequestType::Quit);

    // New Game: level 1 with the gameplay.json initial values (fixes C7: a full reload).
    GameFlow newGame = MakeFlow(3, true, 3);
    requests = newGame.Update(Press({InputAction::Select}), {});
    const FlowRequest* load = FindRequest(requests, FlowRequestType::LoadLevel);
    TEST_ASSERT(load != nullptr && load->levelIndex == 0);
    TEST_ASSERT(load->stats.health == 100 && load->stats.ammo == 24);
    TEST_ASSERT(newGame.State() == GameState::Gameplay && newGame.CurrentLevel() == 0);
    TEST_ASSERT(newGame.HighestUnlocked() == 3);  // New Game keeps the unlocked levels

    // Nothing happens without input.
    GameFlow idle = MakeFlow(2);
    TEST_ASSERT(idle.Update(kNoInput, {}).empty() && idle.State() == GameState::Menu);
}

void TestGameFlowProgression() {
    GameFlow flow = MakeFlow(3);
    std::vector<FlowRequest> requests = flow.Update(Press({InputAction::Select}), {});  // New Game
    TEST_ASSERT(flow.State() == GameState::Gameplay && flow.EntrySnapshot().health == 100);

    // Level 1 complete: results kept, level 2 unlocked and saved.
    requests = flow.Update(kNoInput, LevelCompletedEvents(150, 120));  // above the maxima: clamped
    TEST_ASSERT(flow.State() == GameState::LevelComplete);
    const FlowRequest* save = FindRequest(requests, FlowRequestType::SaveProgress);
    TEST_ASSERT(save != nullptr && save->highestUnlocked == 2);
    TEST_ASSERT(flow.HasProgress() && flow.HighestUnlocked() == 2);
    TEST_ASSERT(flow.LastResults().kills == 1 && flow.LastResults().totalEnemies == 2);
    TEST_ASSERT_NEAR(flow.LastResults().seconds, 12.5, 1e-9);
    TEST_ASSERT(flow.CarriedStats().health == 100 && flow.CarriedStats().ammo == 99);  // clamped to maxima
    TEST_ASSERT(!flow.IsLastLevel());

    // Space does not continue (only Enter), so a shot fired at the switch cannot skip the screen.
    TEST_ASSERT(flow.Update(Press({InputAction::Attack, InputAction::Select}), {}).empty());
    TEST_ASSERT(flow.State() == GameState::LevelComplete);

    // Enter: level 2 with the carried stats, which become the new death snapshot.
    requests = flow.Update(Press({InputAction::Confirm, InputAction::Select}), {});
    const FlowRequest* next = FindRequest(requests, FlowRequestType::LoadLevel);
    TEST_ASSERT(next != nullptr && next->levelIndex == 1);
    TEST_ASSERT(next->stats.health == 100 && next->stats.ammo == 99);
    TEST_ASSERT(flow.State() == GameState::Gameplay && flow.CurrentLevel() == 1);
    TEST_ASSERT(flow.EntrySnapshot().health == 100 && flow.EntrySnapshot().ammo == 99);

    // Level 2 complete with lower stats: they carry over unchanged.
    flow.Update(kNoInput, LevelCompletedEvents(37, 5));
    TEST_ASSERT(flow.HighestUnlocked() == 3);
    requests = flow.Update(Press({InputAction::Confirm}), {});
    next = FindRequest(requests, FlowRequestType::LoadLevel);
    TEST_ASSERT(next != nullptr && next->levelIndex == 2 && next->stats.health == 37 && next->stats.ammo == 5);
    TEST_ASSERT(flow.IsLastLevel());

    // Last level: LevelComplete first, then Enter -> CampaignComplete, then Enter -> Menu.
    requests = flow.Update(kNoInput, LevelCompletedEvents(10, 0));
    TEST_ASSERT(flow.State() == GameState::LevelComplete);
    save = FindRequest(requests, FlowRequestType::SaveProgress);
    TEST_ASSERT(save != nullptr && save->highestUnlocked == 3);  // never beyond the campaign
    requests = flow.Update(Press({InputAction::Confirm}), {});
    TEST_ASSERT(requests.empty() && flow.State() == GameState::CampaignComplete);
    flow.Update(Press({InputAction::Confirm}), {});
    TEST_ASSERT(flow.State() == GameState::Menu && flow.Page() == MenuPage::Main);
    // Continue is offered now.
    TEST_ASSERT(flow.MenuItems().size() == 4 && flow.MenuItems()[1] == MenuItem::Continue);

    // Esc on the level-complete screen returns to the menu (progress is already saved).
    GameFlow escFlow = MakeFlow(3);
    escFlow.Update(Press({InputAction::Select}), {});
    escFlow.Update(kNoInput, LevelCompletedEvents(50, 5));
    escFlow.Update(Press({InputAction::Back, InputAction::Pause}), {});
    TEST_ASSERT(escFlow.State() == GameState::Menu);

    // Carry-over clamping on its own: health at least 1, ammo at least 0.
    const PlayerStats low = flow.ClampCarried(PlayerStats{-5, -3});
    TEST_ASSERT(low.health == 1 && low.ammo == 0);
    const PlayerStats high = flow.ClampCarried(PlayerStats{1000, 1000});
    TEST_ASSERT(high.health == 100 && high.ammo == 99);
}

void TestGameFlowDeathAndPause() {
    GameFlow flow = MakeFlow(3, true, 2);
    flow.Update(Press({InputAction::MoveBackward}), {});  // Continue
    std::vector<FlowRequest> requests = flow.Update(Press({InputAction::Select}), {});
    const FlowRequest* load = FindRequest(requests, FlowRequestType::LoadLevel);
    TEST_ASSERT(load != nullptr && load->levelIndex == 1 &&
                load->stats.health == 100);  // highest unlocked, fresh stats

    // Pause: P/Esc (Pause action) or focus loss; resume with Esc/P/Enter.
    flow.Update(Press({InputAction::Pause, InputAction::Back}), {});
    TEST_ASSERT(flow.State() == GameState::Paused);
    flow.Update(Press({InputAction::Pause, InputAction::Back}), {});
    TEST_ASSERT(flow.State() == GameState::Gameplay);
    FlowEvents focus{};
    focus.focusLost = true;
    flow.Update(kNoInput, focus);
    TEST_ASSERT(flow.State() == GameState::Paused);
    flow.Update(Press({InputAction::Confirm}), {});
    TEST_ASSERT(flow.State() == GameState::Gameplay);
    // R in the pause screen restarts from the level-entry snapshot.
    flow.Update(Press({InputAction::Pause}), {});
    requests = flow.Update(Press({InputAction::Restart}), {});
    const FlowRequest* restart = FindRequest(requests, FlowRequestType::RestartLevel);
    TEST_ASSERT(restart != nullptr && restart->levelIndex == 1 && restart->stats.health == 100);
    TEST_ASSERT(flow.State() == GameState::Gameplay);
    // Q in the pause screen goes to the main menu.
    flow.Update(Press({InputAction::Pause}), {});
    TEST_ASSERT(flow.Update(Press({InputAction::QuitToMenu, InputAction::TurnLeft}), {}).empty());
    TEST_ASSERT(flow.State() == GameState::Menu && flow.Page() == MenuPage::Main);
    // Q during play is only "turn left".
    GameFlow turning = MakeFlow(1);
    turning.Update(Press({InputAction::Select}), {});
    turning.Update(Press({InputAction::QuitToMenu, InputAction::TurnLeft}), {});
    TEST_ASSERT(turning.State() == GameState::Gameplay);

    // Death: GameOver, then R or Enter restarts with the snapshot taken at level entry.
    GameFlow death = MakeFlow(3);
    death.Update(Press({InputAction::Select}), {});
    death.Update(kNoInput, LevelCompletedEvents(42, 7));
    death.Update(Press({InputAction::Confirm}), {});  // level 2 entered with 42 / 7
    FlowEvents died{};
    died.playerDied = true;
    death.Update(kNoInput, died);
    TEST_ASSERT(death.State() == GameState::GameOver);
    requests = death.Update(Press({InputAction::Restart}), {});
    restart = FindRequest(requests, FlowRequestType::RestartLevel);
    TEST_ASSERT(restart != nullptr && restart->levelIndex == 1);
    TEST_ASSERT(restart->stats.health == 42 && restart->stats.ammo == 7);
    death.Update(kNoInput, died);
    requests = death.Update(Press({InputAction::Confirm}), {});
    TEST_ASSERT(FindRequest(requests, FlowRequestType::RestartLevel) != nullptr);
    TEST_ASSERT(FindRequest(requests, FlowRequestType::RestartLevel)->stats.health == 42);
    // Esc on the game-over screen goes to the menu.
    death.Update(kNoInput, died);
    death.Update(Press({InputAction::Back, InputAction::Pause}), {});
    TEST_ASSERT(death.State() == GameState::Menu);
    // R during play restarts as well.
    death.Update(Press({InputAction::Select}), {});
    requests = death.Update(Press({InputAction::Restart}), {});
    TEST_ASSERT(FindRequest(requests, FlowRequestType::RestartLevel) != nullptr);

    // External state changes (C API): Victory means LevelComplete.
    GameFlow external = MakeFlow(2);
    external.ForceState(GameState::Victory);
    TEST_ASSERT(external.State() == GameState::LevelComplete);
    external.ForceState(GameState::Menu);
    TEST_ASSERT(external.State() == GameState::Menu && external.Page() == MenuPage::Main);
    // --level N: straight into the level, unlocks ignored.
    const FlowRequest direct = external.StartLevelDirect(1);
    TEST_ASSERT(direct.type == FlowRequestType::LoadLevel && direct.levelIndex == 1 && direct.stats.health == 100);
    TEST_ASSERT(external.State() == GameState::Gameplay && !external.IsLevelUnlocked(1));
    // Saved progress beyond the campaign is clamped.
    const GameFlow clamped = MakeFlow(3, true, 50);
    TEST_ASSERT(clamped.HighestUnlocked() == 3);
}

void TestCampaignManifestValidation() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    CampaignDefinition campaign{};
    TEST_ASSERT(LoadCampaign(ResolveRepoPath("game/defs/campaign.json"), RepoRoot(), campaign, errors, warnings));
    TEST_ASSERT(errors.empty() && warnings.empty());
    // The shipped manifest loads exactly as written, whatever its number of levels.
    const nlohmann::json manifest = nlohmann::json::parse(ReadWholeFile(ResolveRepoPath("game/defs/campaign.json")));
    TEST_ASSERT(manifest["levels"].is_array() && !manifest["levels"].empty());
    TEST_ASSERT(campaign.title == manifest["title"].get<std::string>());
    TEST_ASSERT(campaign.levels.size() == manifest["levels"].size());
    for (std::size_t i = 0; i < campaign.levels.size(); ++i) {
        const nlohmann::json& entry = manifest["levels"][i];
        TEST_ASSERT(campaign.levels[i].id == entry["id"].get<std::string>());
        TEST_ASSERT(campaign.levels[i].name == entry["name"].get<std::string>());
        TEST_ASSERT(campaign.levels[i].map == entry["map"].get<std::string>());
        TEST_ASSERT(campaign.levels[i].entities == entry["entities"].get<std::string>());
        TEST_ASSERT(campaign.levels[i].theme == entry["theme"].get<std::string>());
    }
    TEST_ASSERT(campaign.testLevel.has_value() == manifest.contains("testLevel"));
    if (campaign.testLevel) {
        TEST_ASSERT(campaign.testLevel->id == manifest["testLevel"]["id"].get<std::string>());
        TEST_ASSERT(campaign.testLevel->map == manifest["testLevel"]["map"].get<std::string>());
    }
    TEST_ASSERT(campaign.bootIntoTestLevel == manifest.value("bootIntoTestLevel", false));
    // The reference campaign (tests/fixtures/reference): exact values.
    {
        const ScratchDir scratch("campaign-reference");
        const std::filesystem::path root = MakeReferenceContentRoot(scratch.Path() / "content");
        CampaignDefinition reference{};
        TEST_ASSERT(
            LoadCampaign(PathToUtf8(root / "game" / "defs" / "campaign.json"), root, reference, errors, warnings));
        TEST_ASSERT(errors.empty() && warnings.empty());
        TEST_ASSERT(reference.levels.size() == 3);
        TEST_ASSERT(reference.levels[0].id == "level01" && reference.levels[0].theme == "bunker");
        TEST_ASSERT(reference.levels[1].theme == "stone" && reference.levels[2].theme == "tech");
        TEST_ASSERT(reference.levels[2].map == "game/maps/level03.map");
    }

    TEST_ASSERT(IsValidContentId("level01") && IsValidContentId("tech_2-b"));
    TEST_ASSERT(!IsValidContentId("") && !IsValidContentId("../x") && !IsValidContentId("Level"));

    const std::string level01 = R"({"id": "level01", "name": "A", "map": "game/maps/level01.map",
        "entities": "game/defs/level01.entities.json", "theme": "bunker"})";
    struct Case {
        std::string json;
        const char* expected;
    };
    const std::vector<Case> cases = {
        {"{ \"title\": \"T\", \"levels\": [ " + level01 + " ", "invalid JSON"},
        {"[]", "top level must be an object"},
        {R"({"levels": [)" + level01 + "]}", "field \"title\" is required"},
        {R"({"title": "T"})", "field \"levels\" is required"},
        {R"({"title": "T", "levels": []})", "needs at least one level"},
        {R"({"title": "T", "levels": {}})", "must be an array"},
        {R"({"title": "T", "title": "U", "levels": [)" + level01 + "]}", "duplicate key \"title\""},
        {R"({"title": "T", "levels": [)" + level01 + ", " + level01 + "]}", "ids must be unique"},
        {R"({"title": "T", "levels": [{"id": "x", "name": "A", "map": "game/maps/missing.map",
            "entities": "game/defs/level01.entities.json", "theme": "bunker"}]})",
         "map file not found"},
        {R"({"title": "T", "levels": [{"id": "x", "name": "A", "map": "game/maps/level01.map",
            "entities": "game/defs/nope.entities.json", "theme": "bunker"}]})",
         "entities file not found"},
        {R"({"title": "T", "levels": [{"id": "x", "name": "A", "map": "game/maps/level01.map",
            "entities": "game/defs/level01.entities.json", "theme": "marble"}]})",
         "theme \"marble\" not found"},
        {R"({"title": "T", "levels": [{"id": "x", "name": "A", "map": "game/maps/level01.map",
            "entities": "game/defs/level01.entities.json", "theme": "../bunker"}]})",
         "theme must use only"},
        {R"({"title": "T", "levels": [{"id": "Level 1", "name": "A", "map": "game/maps/level01.map",
            "entities": "game/defs/level01.entities.json", "theme": "bunker"}]})",
         "id must use only"},
        {R"({"title": "T", "levels": [{"id": "x", "name": 5, "map": "game/maps/level01.map",
            "entities": "game/defs/level01.entities.json", "theme": "bunker"}]})",
         "\"name\" must be a non-empty string"},
        {R"({"title": "T", "levels": [{"id": "x", "name": "A", "entities": "game/defs/level01.entities.json",
            "theme": "bunker"}]})",
         "field \"map\" is required"},
        {R"({"title": "T", "levels": [3]})", "must be an object"},
        // The test level ("testLevel") and "bootIntoTestLevel".
        {R"({"title": "T", "levels": [)" + level01 + R"(], "testLevel": 3})", "testLevel must be an object"},
        {R"({"title": "T", "levels": [)" + level01 + R"(], "testLevel": )" + level01 + "}",
         "testLevel (level01): id \"level01\" is used by an earlier level"},
        {R"({"title": "T", "levels": [)" + level01 + R"(], "testLevel": {"id": "lab", "name": "Lab",
            "entities": "game/defs/level01.entities.json", "theme": "bunker"}})",
         "testLevel (lab): field \"map\" is required"},
        {R"({"title": "T", "levels": [)" + level01 + R"(], "testLevel": {"id": "lab", "name": "Lab",
            "map": "game/maps/level01.map", "entities": "game/defs/level01.entities.json", "theme": "marble"}})",
         "testLevel (lab): theme \"marble\" not found"},
        {R"({"title": "T", "levels": [)" + level01 + R"(], "bootIntoTestLevel": true})",
         "\"bootIntoTestLevel\" is true, but there is no \"testLevel\""},
        {R"({"title": "T", "levels": [)" + level01 + R"(], "testLevel": {"id": "lab", "name": "Lab",
            "map": "game/maps/level01.map", "entities": "game/defs/level01.entities.json", "theme": "bunker"},
            "bootIntoTestLevel": "yes"})",
         "\"bootIntoTestLevel\" must be true or false, got \"yes\""},
    };
    for (const Case& c : cases) {
        errors.clear();
        warnings.clear();
        CampaignDefinition untouched = campaign;
        TEST_ASSERT(!ParseCampaign(c.json, "campaign.json", RepoRoot(), untouched, errors, warnings));
        if (!AnyMessageContains(errors, {"campaign.json", c.expected})) {
            FailTest(__FILE__, __LINE__, std::string("expected an error containing: ") + c.expected);
        }
        TEST_ASSERT(untouched.levels.size() == campaign.levels.size());  // unchanged on failure
    }

    // Unknown keys and very long names are warnings only.
    errors.clear();
    warnings.clear();
    CampaignDefinition loose{};
    TEST_ASSERT(ParseCampaign(
        R"({"title": "T", "extra": 1, "levels": [{"id": "x", "name": "A very long level name that will not fit",
        "map": "game/maps/level01.map", "entities": "game/defs/level01.entities.json", "theme": "bunker", "music": "x"}]})",
        "campaign.json", RepoRoot(), loose, errors, warnings));
    TEST_ASSERT(AnyMessageContains(warnings, {"unknown key \"extra\""}));
    TEST_ASSERT(AnyMessageContains(warnings, {"unknown key \"music\""}));
    TEST_ASSERT(AnyMessageContains(warnings, {"shortened on screen"}));
    TEST_ASSERT(loose.levels.size() == 1);

    // A valid test level, outside the campaign; bootIntoTestLevel defaults to false.
    errors.clear();
    warnings.clear();
    CampaignDefinition withTestLevel{};
    const std::string lab = R"("testLevel": {"id": "lab", "name": "Lab", "map": "game/maps/level01.map",
        "entities": "game/defs/level01.entities.json", "theme": "stone"})";
    std::string labCampaign = R"({"title": "T", "levels": [)";
    labCampaign += level01;
    labCampaign += "], ";
    labCampaign += lab;
    TEST_ASSERT(ParseCampaign(labCampaign + "}", "campaign.json", RepoRoot(), withTestLevel, errors, warnings));
    TEST_ASSERT(errors.empty() && warnings.empty());
    TEST_ASSERT(withTestLevel.levels.size() == 1 && withTestLevel.testLevel.has_value());
    TEST_ASSERT(withTestLevel.testLevel->id == "lab" && withTestLevel.testLevel->theme == "stone");
    TEST_ASSERT(!withTestLevel.bootIntoTestLevel);
    TEST_ASSERT(ParseCampaign(labCampaign + R"(, "bootIntoTestLevel": true})", "campaign.json", RepoRoot(),
                              withTestLevel, errors, warnings));
    TEST_ASSERT(errors.empty() && withTestLevel.bootIntoTestLevel);

    // A missing manifest.
    errors.clear();
    TEST_ASSERT(
        !LoadCampaign(ResolveRepoPath("game/defs") + "/missing-campaign.json", RepoRoot(), loose, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"Unable to open campaign manifest"}));
}

// A map with the player in the middle of an open room and an exit switch on the east wall.
Map MakeExitTestMap() {
    Map map(10, 7);
    BuildBorderWalls(map);
    map.SetCell(9, 3, TileType::Exit);
    return map;
}

void TestExitSwitchActivation() {
    // The map loader reads 'X' as the exit switch.
    const ScratchDir scratch("exit-switch");
    WriteTextFile(scratch.Path() / "x.map", "#####\n#..X#\n#####\n");
    Map loaded{};
    std::vector<std::string> errors;
    TEST_ASSERT(LoadMapFromFile(PathToUtf8(scratch.Path() / "x.map"), loaded, errors));
    TEST_ASSERT(loaded.GetCell(3, 1) == TileType::Exit && loaded.IsWall(3, 1));
    // The reference level 1 has one, on the east wall; every shipped campaign map has one.
    Map level01{};
    TEST_ASSERT(LoadMapFromFile(ReferencePath("game/maps/level01.map"), level01, errors));
    TEST_ASSERT(level01.GetCell(ReferenceLevel1::kExitX, ReferenceLevel1::kExitY) == TileType::Exit);
    const CampaignDefinition campaign = ShippedCampaign();
    TEST_ASSERT(!campaign.levels.empty());
    for (const CampaignLevel& level : campaign.levels) {
        Map shipped{};
        TEST_ASSERT(LoadMapFromFile(ResolveRepoPath(level.map), shipped, errors));
        int exits = 0;
        for (int y = 0; y < shipped.Height(); ++y) {
            for (int x = 0; x < shipped.Width(); ++x) {
                exits += shipped.GetCell(x, y) == TileType::Exit ? 1 : 0;
            }
        }
        if (exits == 0) {
            FailTest(__FILE__, __LINE__, level.map + " has no exit switch X");
        }
    }

    const Map map = MakeExitTestMap();
    Player player{};
    player.collisionRadius = 0.22f;

    // In reach (0.5 cells from the face) and facing it: found.
    player.position = {8.5f, 3.5f};
    player.directionAngleRadians = 0.0f;
    const ExitSwitchHit hit = FindExitSwitchInReach(map, player);
    TEST_ASSERT(hit.found && hit.mapX == 9 && hit.mapY == 3);
    TEST_ASSERT_NEAR(hit.distance, 0.5f, 1e-4f);
    // Roughly facing (15 degrees off) still works...
    player.directionAngleRadians = 0.26f;
    TEST_ASSERT(FindExitSwitchInReach(map, player).found);
    // ...but looking sideways (90 degrees) or away does not.
    player.directionAngleRadians = kPi / 2.0f;
    TEST_ASSERT(!FindExitSwitchInReach(map, player).found);
    player.directionAngleRadians = kPi;  // behind the player
    TEST_ASSERT(!FindExitSwitchInReach(map, player).found);
    // Too far: 1.5 cells from the face.
    player.position = {7.5f, 3.5f};
    player.directionAngleRadians = 0.0f;
    TEST_ASSERT(!FindExitSwitchInReach(map, player).found);
    // Just inside the 1.1-cell reach.
    player.position = {9.0f - kExitReach + 0.05f, 3.5f};
    TEST_ASSERT(FindExitSwitchInReach(map, player).found);
    // A wall in between blocks it.
    Map blocked = map;
    blocked.SetCell(8, 3, TileType::Wall);
    player.position = {7.5f, 3.5f};
    TEST_ASSERT(!FindExitSwitchInReach(blocked, player).found);

    // Through the interaction rules: only with Interact.
    EntityManager manager{};
    manager.CreateEntity(EntityType::Player, {8.5f, 3.5f});
    Map interactionMap = map;
    player.position = {8.5f, 3.5f};
    player.directionAngleRadians = 0.0f;
    int health = 100;
    int ammo = 24;
    GameplayEvents events{};
    UpdateEntityInteractions(manager, interactionMap, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, false,
                             events);
    TEST_ASSERT(!events.exitActivated);
    UpdateEntityInteractions(manager, interactionMap, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true,
                             events);
    TEST_ASSERT(events.exitActivated && events.exitMapX == 9 && events.exitMapY == 3);

    // The switch is solid: the player cannot walk into it.
    const Vec2 moved = MoveWithSliding(map, {8.5f, 3.5f}, {1.0f, 0.0f}, 0.22f);
    TEST_ASSERT(moved.x < 9.0f - 0.2f);

    // A trigger is walk-over only: it fires, but it is not the exit.
    EntityManager triggerManager{};
    Entity& trigger = triggerManager.CreateEntity(EntityType::Trigger, {5.5f, 3.5f});
    trigger.mapX = 5;
    trigger.mapY = 3;
    Map triggerMap = MakeExitTestMap();
    triggerMap.SetCell(5, 3, TileType::Trigger);
    player.position = {5.5f, 3.5f};
    GameplayEvents triggerEvents{};
    UpdateEntityInteractions(triggerManager, triggerMap, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true,
                             triggerEvents);
    TEST_ASSERT(triggerEvents.triggerActivated && !triggerEvents.exitActivated);
}

void TestProgressFileRoundTrip() {
    const ScratchDir scratch("progress");
    const std::string path = PathToUtf8(scratch.Path() / "progress.json");
    ProgressData data{};
    std::string message;
    TEST_ASSERT(LoadProgressFile(path, data, message) == ProgressLoadResult::Missing);

    // Round trip, then an overwrite (atomic: no temporary file is left behind).
    data.highestUnlocked = 2;
    std::string error;
    TEST_ASSERT(SaveProgressFile(path, data, error));
    ProgressData loaded{};
    TEST_ASSERT(LoadProgressFile(path, loaded, message) == ProgressLoadResult::Loaded);
    TEST_ASSERT(loaded.highestUnlocked == 2);
    data.highestUnlocked = 3;
    TEST_ASSERT(SaveProgressFile(path, data, error));
    TEST_ASSERT(LoadProgressFile(path, loaded, message) == ProgressLoadResult::Loaded);
    TEST_ASSERT(loaded.highestUnlocked == 3);
    TEST_ASSERT(!std::filesystem::exists(scratch.Path() / "progress.json.tmp"));
    const std::string text = ReadWholeFile(path);
    TEST_ASSERT(text.find("\"version\": 1") != std::string::npos);
    TEST_ASSERT(text.find("\"highestUnlocked\": 3") != std::string::npos);

    // Corrupt or foreign files are Invalid (the caller warns and starts without progress);
    // the output value is left untouched.
    const std::vector<const char*> corrupt = {
        "",
        "{ not json",
        "[1, 2]",
        "{\"version\": 1}",
        "{\"highestUnlocked\": 2}",
        "{\"version\": 2, \"highestUnlocked\": 2}",
        "{\"version\": 1, \"highestUnlocked\": 0}",
        "{\"version\": 1, \"highestUnlocked\": \"2\"}",
        "{\"version\": 1, \"highestUnlocked\": 1.5}",
        "{\"version\": 1, \"highestUnlocked\": 99999999999}",
        "{\"version\": 1, \"version\": 1, \"highestUnlocked\": 2}",
    };
    for (const char* content : corrupt) {
        WriteTextFile(scratch.Path() / "progress.json", content);
        ProgressData untouched{};
        untouched.highestUnlocked = 7;
        message.clear();
        TEST_ASSERT(LoadProgressFile(path, untouched, message) == ProgressLoadResult::Invalid);
        TEST_ASSERT(untouched.highestUnlocked == 7);
        TEST_ASSERT(!message.empty());
    }
    // Unknown extra keys are tolerated (forward compatible).
    WriteTextFile(scratch.Path() / "progress.json", "{\"version\": 1, \"highestUnlocked\": 2, \"note\": \"x\"}");
    TEST_ASSERT(LoadProgressFile(path, loaded, message) == ProgressLoadResult::Loaded && loaded.highestUnlocked == 2);

    // Saving into a folder that does not exist fails cleanly.
    TEST_ASSERT(!SaveProgressFile(PathToUtf8(scratch.Path() / "no-such-dir" / "progress.json"), data, error));
    TEST_ASSERT(!error.empty());

    // The engine: a corrupt file means "no progress" (no Continue); a valid one is used.
    WriteTextFile(scratch.Path() / "progress.json", "{ corrupt");
    {
        Engine engine;
        EngineOptions options = ShippedEngineOptions();
        options.prefDir = scratch.Path();
        TEST_ASSERT(engine.Init(options));
        const std::size_t testLevelItems = ShippedCampaign().testLevel.has_value() ? 1 : 0;
        TEST_ASSERT(!engine.flow_.HasProgress() && engine.flow_.MenuItems().size() == 3 + testLevelItems);
    }
    // Level 3 unlocked (or the last level of a shorter campaign: a save beyond it is clamped).
    const int unlocked = std::min(3, static_cast<int>(ShippedCampaign().levels.size()));
    data.highestUnlocked = unlocked;
    TEST_ASSERT(SaveProgressFile(path, data, error));
    {
        Engine engine;
        EngineOptions options = ShippedEngineOptions();
        options.prefDir = scratch.Path();
        TEST_ASSERT(engine.Init(options));
        TEST_ASSERT(engine.flow_.HasProgress() && engine.flow_.HighestUnlocked() == unlocked);
        TEST_ASSERT(engine.flow_.MenuItems()[1] == MenuItem::Continue);
    }
}

void TestPrefDirResolution() {
    // --pref-dir wins over NEOWOLF_PREF_DIR, which wins over SDL's per-user folder.
    PrefDirResult r = ChoosePrefDir("/opt/a", "/opt/b", "/home/u/.local/share/NeoWolf/NeoWolf/", "/work");
    TEST_ASSERT(r.source == "--pref-dir" && r.directory == PathFromUtf8("/opt/a").lexically_normal());
    r = ChoosePrefDir("", "/opt/b", "/home/u/x/", "/work");
    TEST_ASSERT(r.source == "NEOWOLF_PREF_DIR" && r.directory == PathFromUtf8("/opt/b").lexically_normal());
    r = ChoosePrefDir("", "", "/home/u/x/", "/work");
    TEST_ASSERT(r.source == "SDL_GetPrefPath" && r.directory == PathFromUtf8("/home/u/x").lexically_normal());
    r = ChoosePrefDir("rel/dir", "", "", "/work");
    TEST_ASSERT(r.directory == (PathFromUtf8("/work") / "rel" / "dir").lexically_normal());
    r = ChoosePrefDir("", "", "", "/work");
    TEST_ASSERT(r.directory.empty() && !r.error.empty());

    // ResolvePrefDir creates an explicit folder (and never asks SDL for the real one then).
    const ScratchDir scratch("pref-dir");
    const std::filesystem::path wanted = scratch.Path() / "nested" / "prefs";
    r = ResolvePrefDir(PathToUtf8(wanted));
    TEST_ASSERT(r.error.empty() && r.source == "--pref-dir");
    TEST_ASSERT(std::filesystem::is_directory(wanted));
}

void TestThemeLoadingAndSwap() {
    // Every shipped theme has the seven keys and existing files.
    for (const char* theme : {"bunker", "stone", "tech", "classic"}) {
        std::vector<std::string> errors;
        ThemeTextureAssignments assignments{};
        TEST_ASSERT(LoadThemeTextureAssignments(ResolveRepoPath(ThemeConfigPath(theme)), assignments, errors));
        for (const std::string* path :
             {&assignments.wall, &assignments.wallAltA, &assignments.wallAltB, &assignments.door, &assignments.floor,
              &assignments.ceiling, &assignments.exit}) {
            Texture texture{};
            TEST_ASSERT(TextureLoader::LoadPPM(ResolveRepoPath(*path), texture));
        }
    }
    // A theme without "exit" is reported.
    const ScratchDir scratch("themes");
    WriteTextFile(scratch.Path() / "partial.cfg",
                  "wall=a.ppm\nwall_alt_a=a.ppm\nwall_alt_b=a.ppm\ndoor=a.ppm\nfloor=a.ppm\nceiling=a.ppm\n");
    std::vector<std::string> errors;
    ThemeTextureAssignments partial{};
    TEST_ASSERT(!LoadThemeTextureAssignments(PathToUtf8(scratch.Path() / "partial.cfg"), partial, errors));
    TEST_ASSERT(AnyMessageContains(errors, {"missing required key: exit"}));

    // The engine swaps textures in place: the cached pointers stay valid and show the new theme.
    constexpr int kThemeTextureSize = 32;  // every shipped theme texture is 32x32
    Engine engine;
    engine.contentRoot_ = RepoRoot();
    TEST_ASSERT(engine.ApplyTheme("bunker"));
    const Texture* wallBefore = engine.wallTexture_;
    TEST_ASSERT(wallBefore != nullptr && wallBefore == engine.assets_.GetTexture("wall"));
    TEST_ASSERT(engine.exitTexture_ != nullptr && engine.exitTexture_->Width() == kThemeTextureSize);
    Texture bunkerWall{};
    TEST_ASSERT(TextureLoader::LoadPPM(ResolveRepoPath("game/assets/themes/bunker/wall.ppm"), bunkerWall));
    TEST_ASSERT(wallBefore->GetPixel(5, 5) == bunkerWall.GetPixel(5, 5));

    TEST_ASSERT(engine.ApplyTheme("stone"));
    TEST_ASSERT(engine.currentThemeId_ == "stone");
    TEST_ASSERT(engine.wallTexture_ == engine.assets_.GetTexture("wall"));
    Texture stoneWall{};
    TEST_ASSERT(TextureLoader::LoadPPM(ResolveRepoPath("game/assets/themes/stone/wall.ppm"), stoneWall));
    TEST_ASSERT(engine.wallTexture_->GetPixel(5, 5) == stoneWall.GetPixel(5, 5));
    TEST_ASSERT(wallBefore->GetPixel(5, 5) == stoneWall.GetPixel(5, 5));  // same object, new pixels
    bool differs = false;
    for (int y = 0; y < kThemeTextureSize && !differs; ++y) {
        for (int x = 0; x < kThemeTextureSize && !differs; ++x) {
            differs = stoneWall.GetPixel(x, y) != bunkerWall.GetPixel(x, y);
        }
    }
    TEST_ASSERT(differs);

    // A missing theme falls back to the checkerboard (and says so).
    TEST_ASSERT(!engine.ApplyTheme("no-such-theme"));
    TEST_ASSERT(engine.wallTexture_ != nullptr && engine.wallTexture_->Width() == engine.missingTexture_.Width());

    // Campaign levels load their own theme.
    Engine campaignEngine;
    TEST_ASSERT(campaignEngine.Init(ShippedEngineOptions()));
    const CampaignDefinition campaign = ShippedCampaign();
    TEST_ASSERT(!campaign.levels.empty());
    TEST_ASSERT(campaignEngine.currentThemeId_ == campaign.levels[0].theme);
    // Every shipped level, last to first (so consecutive loads switch themes in both directions).
    for (int index = static_cast<int>(campaign.levels.size()) - 1; index >= 0; --index) {
        const CampaignLevel& level = campaign.levels[static_cast<std::size_t>(index)];
        TEST_ASSERT(campaignEngine.LoadCampaignLevel(index, PlayerStats{50, 5}));
        TEST_ASSERT(campaignEngine.currentThemeId_ == level.theme);
        TEST_ASSERT(campaignEngine.playerHealth_ == 50 && campaignEngine.playerAmmo_ == 5);
        TEST_ASSERT(campaignEngine.CurrentCampaignLevel() != nullptr &&
                    campaignEngine.CurrentCampaignLevel()->id == level.id);
    }
}

// The render sizes the UI is checked at: the old fixed size, the default, 4:3 and 16:9 at UI
// scale 2, 3 and 4, the smallest allowed height and the largest size, and the edges of the
// shape limits: 5:4 at UI scale 1 and 4 (canvas 400x320, 337x270) and 64:27 at UI scale 1 and
// 4 (canvas 640x270, 480x202).
constexpr std::array<RenderSize, 11> kUiTestSizes = {{
    {320, 240},
    {640, 360},
    {640, 480},
    {960, 540},
    {1280, 720},
    {320, 200},
    {1920, 1080},
    {400, 320},
    {1350, 1080},
    {640, 270},
    {1920, 810},
}};

std::string SizeName(int width, int height) {
    return std::to_string(width) + "x" + std::to_string(height);
}

// Checks that everything drawn since the last ResetDrawBounds() lies inside the framebuffer,
// at least UiLayout::kMargin UI pixels (times the UI scale) from every edge, and that nothing
// was clipped.
void AssertInsideMargin(const Framebuffer& framebuffer, const std::string& what) {
    const DrawBounds& b = framebuffer.GetDrawBounds();
    const int m = UiLayout::kMargin * UiLayout::UiScale(framebuffer.Width(), framebuffer.Height());
    const bool ok = b.any && b.clippedRects == 0 && b.minX >= m && b.minY >= m &&
                    b.maxX <= framebuffer.Width() - 1 - m && b.maxY <= framebuffer.Height() - 1 - m;
    if (!ok) {
        std::ostringstream out;
        out << what << ": draw bounds (" << b.minX << "," << b.minY << ")-(" << b.maxX << "," << b.maxY << "), "
            << b.clippedRects << " clipped rect(s), framebuffer " << framebuffer.Width() << "x" << framebuffer.Height();
        FailTest(__FILE__, __LINE__, out.str());
    }
}

// Every preview screen, rendered by the real engine (headless) at `size`.
void CheckPreviewScreensInsideMargin(const RenderSize& size) {
    for (const PreviewScreen screen : kPreviewScreens) {
        Engine engine;
        EngineOptions options = ShippedEngineOptions();
        options.preview = screen;
        options.renderSize = size;
        TEST_ASSERT(engine.Init(options));
        TEST_ASSERT(engine.framebuffer_.Width() == size.width && engine.framebuffer_.Height() == size.height);
        engine.Render();
        AssertInsideMargin(engine.framebuffer_,
                           std::string(PreviewScreenName(screen)) + " at " + SizeName(size.width, size.height));
    }

    // Play with everything on: minimap, level banner, debug overlay, a long message. On the
    // shipped campaign's last level (the biggest map, so the minimap follows the player).
    Engine engine;
    EngineOptions options = ShippedEngineOptions(static_cast<int>(ShippedCampaign().levels.size()));
    options.renderSize = size;
    TEST_ASSERT(engine.Init(options));
    engine.minimapVisible_ = true;
    engine.debugOverlayVisible_ = true;
    engine.levelBannerTimer_ = 2.0f;
    engine.ShowMessage(std::string(120, 'W'), 5.0f);
    engine.playerHealth_ = 10000;
    engine.playerAmmo_ = 10000;
    engine.Render();
    AssertInsideMargin(engine.framebuffer_, "gameplay with minimap, banner, debug overlay and long message at " +
                                                SizeName(size.width, size.height));
}

// Direct UI calls with extreme content at `size`.
void CheckExtremeScreensInsideMargin(const RenderSize& size) {
    const std::string at = " at " + SizeName(size.width, size.height);
    Framebuffer framebuffer(size.width, size.height);
    const std::string longName(80, 'M');
    UiRenderer::LevelSelectView select{};
    select.campaignTitle = longName;
    for (int i = 1; i <= 25; ++i) {
        select.levels.push_back(UiRenderer::LevelSelectEntry{i, longName, i % 3 != 0});
    }
    for (const int selected : {0, 12, 24}) {
        select.selected = selected;
        framebuffer.ResetDrawBounds();
        UiRenderer::DrawLevelSelectScreen(framebuffer, select);
        AssertInsideMargin(framebuffer, "level select, 25 long names, selected " + std::to_string(selected) + at);
    }
    UiRenderer::MenuView menu{};
    menu.campaignTitle = longName;
    menu.items = {"NEW GAME", "CONTINUE", "LEVEL SELECT", "QUIT", longName};
    menu.selected = 4;
    framebuffer.ResetDrawBounds();
    UiRenderer::DrawMenuScreen(framebuffer, menu);
    AssertInsideMargin(framebuffer, "menu with a long item" + at);
    // The game's longest menu (saved progress and a test level: five items) keeps its controls
    // summary, the only text in the dim colour.
    menu.campaignTitle = "OPERATION NEO-WOLF";
    menu.items = {"NEW GAME", "CONTINUE", "LEVEL SELECT", "TEST CHAMBER", "QUIT"};
    framebuffer.ResetDrawBounds();
    UiRenderer::DrawMenuScreen(framebuffer, menu);
    AssertInsideMargin(framebuffer, "menu with five items" + at);
    const std::uint32_t dimText = PackARGB32(MakeColor(105, 115, 130));
    bool controlsShown = false;
    for (int y = 0; y < framebuffer.Height() && !controlsShown; ++y) {
        for (int x = 0; x < framebuffer.Width() && !controlsShown; ++x) {
            controlsShown = PixelAt(framebuffer, x, y) == dimText;
        }
    }
    if (!controlsShown) {
        FailTest(__FILE__, __LINE__, "the five-item menu leaves out its controls summary" + at);
    }
    UiRenderer::LevelCompleteView complete{};
    complete.levelNumber = 99;
    complete.levelName = longName;
    complete.results = LevelResults{9999, 9999, 9999, 9999, 99999.0};
    framebuffer.ResetDrawBounds();
    UiRenderer::DrawLevelCompleteScreen(framebuffer, complete);
    AssertInsideMargin(framebuffer, "level complete with extreme values" + at);
    framebuffer.ResetDrawBounds();
    UiRenderer::DrawGameOverScreen(framebuffer, UiRenderer::LevelLabel(99, longName));
    UiRenderer::DrawPauseOverlay(framebuffer, UiRenderer::LevelLabel(99, longName));
    UiRenderer::DrawCampaignCompleteScreen(framebuffer, longName, 99);
    AssertInsideMargin(framebuffer, "game over, pause, campaign complete with a long name" + at);
    // The test level's HUD: its title instead of "LEVEL N" (fitted beside the key cards) and in
    // the banner.
    UiRenderer::HudView hud{};
    hud.health = 10000;
    hud.ammo = 10000;
    hud.levelTitle = longName;
    hud.levelName = longName;
    hud.showLevelBanner = true;
    for (const KeyColor color : kKeyColors) {
        hud.keys.Add(color);
    }
    framebuffer.ResetDrawBounds();
    UiRenderer::DrawHud(framebuffer, hud);
    AssertInsideMargin(framebuffer, "HUD with a long test-level title" + at);
}

// Minimap: a small map is scaled to fill the panel; a big one follows the player and is cut
// at the panel edge. The layout is in framebuffer pixels: UI pixels times the UI scale.
void CheckMinimapLayout(const RenderSize& size) {
    const int width = size.width;
    const int height = size.height;
    const std::string at = " at " + SizeName(width, height);
    const int scale = UiLayout::UiScale(width, height);
    const int canvasWidth = width / scale;
    const int canvasHeight = height / scale;
    const int panelSize = std::max(UiRenderer::kMinimapMinPanel, std::min(canvasWidth, canvasHeight) / 3) * scale;
    const UiRenderer::MinimapLayout small = UiRenderer::ComputeMinimapLayout(width, height, 6, 5, 2.0f, 2.0f);
    TEST_ASSERT(!small.followsPlayer && small.cellSize == UiRenderer::kMinimapMaxCell * scale);
    TEST_ASSERT(small.panelSize == panelSize);
    const UiRenderer::MinimapLayout level = UiRenderer::ComputeMinimapLayout(width, height, 16, 12, 2.0f, 2.0f);
    TEST_ASSERT(!level.followsPlayer && level.cellSize * 16 <= level.panelSize - 4 * scale);
    // A 24x14 map (the size of shipped level 2) is still drawn whole on the 16:9 canvas.
    const UiRenderer::MinimapLayout wide = UiRenderer::ComputeMinimapLayout(width, height, 24, 14, 2.0f, 2.0f);
    TEST_ASSERT(!wide.followsPlayer && wide.cellSize * 24 <= wide.panelSize - 4 * scale);
    // Every shipped level's minimap has the panel and cell size (in UI pixels) it has at
    // 320x240: the same part of the map, whole or around the player, at every render size
    // whose canvas is less than 270 UI pixels high or wide. A bigger canvas (5:4 and 64:27 at
    // their largest UI scale) has a bigger panel: cells at least as big, and every map shown
    // whole at 320x240 still shown whole.
    const bool basePanel = panelSize == UiRenderer::kMinimapMinPanel * scale;
    for (const CampaignLevel& campaignLevel : ShippedCampaign().levels) {
        Map shipped{};
        std::vector<std::string> errors;
        TEST_ASSERT(LoadMapFromFile(ResolveRepoPath(campaignLevel.map), shipped, errors));
        const UiRenderer::MinimapLayout base =
            UiRenderer::ComputeMinimapLayout(320, 240, shipped.Width(), shipped.Height(), 2.0f, 2.0f);
        const UiRenderer::MinimapLayout scaled =
            UiRenderer::ComputeMinimapLayout(width, height, shipped.Width(), shipped.Height(), 2.0f, 2.0f);
        const bool same = scaled.followsPlayer == base.followsPlayer && scaled.cellSize == base.cellSize * scale &&
                          scaled.panelSize == base.panelSize * scale;
        const bool bigger = scaled.panelSize > base.panelSize * scale && scaled.cellSize >= base.cellSize * scale &&
                            (base.followsPlayer || !scaled.followsPlayer);
        if (basePanel ? !same : !bigger) {
            FailTest(__FILE__, __LINE__, "minimap of " + campaignLevel.id + at + " differs from 320x240");
        }
    }
    const UiRenderer::MinimapLayout big = UiRenderer::ComputeMinimapLayout(width, height, 200, 150, 100.0f, 75.0f);
    TEST_ASSERT(big.followsPlayer && big.cellSize == UiRenderer::kMinimapMinCell * scale);
    Framebuffer framebuffer(width, height);
    Map bigMap(200, 150);
    BuildBorderWalls(bigMap);
    bigMap.SetCell(101, 75, TileType::Exit);
    Player player{};
    for (const Vec2 position : {Vec2{100.0f, 75.0f}, Vec2{1.5f, 1.5f}, Vec2{198.5f, 148.5f}}) {
        player.position = position;
        framebuffer.ResetDrawBounds();
        const int right = UiRenderer::DrawMinimap(framebuffer, bigMap, player, {});
        TEST_ASSERT(right == big.panelX + panelSize);
        AssertInsideMargin(framebuffer, "minimap of a 200x150 map" + at);
    }
}

// The level banner's title and name stay clear of the debug overlay (top right; it can cover
// only the banner's top border and padding), with and without the minimap on the left, and
// the banner ends at least 3 UI pixels above the crosshair.
void CheckLevelBannerClear(const RenderSize& size) {
    const int width = size.width;
    const int height = size.height;
    const int scale = UiLayout::UiScale(width, height);
    Framebuffer overlay(width, height);
    overlay.Clear(0u);
    UiRenderer::DrawDebugOverlay(overlay, 999.9f, 123.45f, 123.45f, 359.9f, 123, 123);
    Framebuffer crosshair(width, height);
    crosshair.Clear(0u);
    UiRenderer::DrawCrosshair(crosshair);
    for (const bool minimap : {false, true}) {
        UiRenderer::HudView hud{};
        hud.health = 100;
        hud.ammo = 100;
        hud.levelNumber = 10;
        hud.levelName = std::string(80, 'M');
        if (minimap) {
            const UiRenderer::MinimapLayout layout =
                UiRenderer::ComputeMinimapLayout(width, height, 40, 40, 20.0f, 20.0f);
            hud.reservedLeft = layout.panelX + layout.panelSize;
        }
        Framebuffer without(width, height);
        Framebuffer with(width, height);
        without.Clear(0u);
        with.Clear(0u);
        UiRenderer::DrawHud(without, hud);
        hud.showLevelBanner = true;
        UiRenderer::DrawHud(with, hud);
        int bannerTop = height;
        for (int y = 0; y < height && bannerTop == height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (PixelAt(with, x, y) != PixelAt(without, x, y)) {
                    bannerTop = y;
                    break;
                }
            }
        }
        TEST_ASSERT(bannerTop < height);
        // The title starts 5 UI pixels below the top (4 in the compact banner of a short canvas).
        const int titleTop = bannerTop + 4 * scale;
        constexpr int kCrosshairGap = 3;
        for (int y = bannerTop; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (PixelAt(with, x, y) == PixelAt(without, x, y)) {
                    continue;
                }
                const bool underOverlay = y >= titleTop && PixelAt(overlay, x, y) != 0u;
                bool nearCrosshair = false;
                for (int d = 0; d <= kCrosshairGap * scale && y + d < height; ++d) {
                    nearCrosshair = nearCrosshair || PixelAt(crosshair, x, y + d) != 0u;
                }
                if (underOverlay || nearCrosshair) {
                    FailTest(__FILE__, __LINE__,
                             std::string(underOverlay ? "the debug overlay covers"
                                                      : "the crosshair is within 3 UI pixels of") +
                                 " the level banner at (" + std::to_string(x) + "," + std::to_string(y) + ") at " +
                                 SizeName(width, height) + (minimap ? " with the minimap" : ""));
                    return;
                }
            }
        }
    }
}

// The pause, level-complete and game-over panels end above the HUD bar they are drawn over.
void CheckOverlaysAboveHud(const RenderSize& size) {
    Framebuffer framebuffer(size.width, size.height);
    UiRenderer::HudView hud{};
    hud.health = 100;
    hud.ammo = 100;
    hud.levelNumber = 1;
    UiRenderer::DrawHud(framebuffer, hud);
    const int barTop = framebuffer.GetDrawBounds().minY;
    const std::string at = " at " + SizeName(size.width, size.height);
    framebuffer.ResetDrawBounds();
    UiRenderer::DrawPauseOverlay(framebuffer, "LEVEL 1: A");
    TEST_ASSERT(framebuffer.GetDrawBounds().maxY < barTop);
    framebuffer.ResetDrawBounds();
    UiRenderer::LevelCompleteView complete{};
    complete.levelNumber = 1;
    complete.levelName = "A";
    UiRenderer::DrawLevelCompleteScreen(framebuffer, complete);
    if (framebuffer.GetDrawBounds().maxY >= barTop) {
        FailTest(__FILE__, __LINE__, "the level-complete panel reaches the HUD bar" + at);
    }
    framebuffer.ResetDrawBounds();
    UiRenderer::DrawGameOverScreen(framebuffer, "LEVEL 1: A");
    TEST_ASSERT(framebuffer.GetDrawBounds().maxY < barTop);
}

// The UI at a whole multiple of 320x240 is the 320x240 UI with every pixel scaled up.
void CheckUiScalesUp(int factor) {
    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    UiRenderer::MenuView menu{};
    menu.campaignTitle = "OPERATION NEO-WOLF";
    menu.items = {"NEW GAME", "CONTINUE", "LEVEL SELECT", "QUIT"};
    const auto draw = [&](Framebuffer& framebuffer) {
        UiRenderer::DrawMenuScreen(framebuffer, menu);
        UiRenderer::DrawCrosshair(framebuffer);
        UiRenderer::HudView hud{};
        hud.health = 20;
        hud.ammo = 7;
        hud.levelNumber = 3;
        hud.keys.Add(KeyColor::Blue);
        UiRenderer::DrawHud(framebuffer, hud);
    };
    Framebuffer small(kWidth, kHeight);
    Framebuffer big(kWidth * factor, kHeight * factor);
    TEST_ASSERT(UiLayout::UiScale(big.Width(), big.Height()) == factor);
    draw(small);
    draw(big);
    for (int y = 0; y < big.Height(); ++y) {
        for (int x = 0; x < big.Width(); ++x) {
            if (PixelAt(big, x, y) != PixelAt(small, x / factor, y / factor)) {
                FailTest(__FILE__, __LINE__,
                         "pixel (" + std::to_string(x) + "," + std::to_string(y) + ") of the UI at " +
                             SizeName(big.Width(), big.Height()) + " differs from the 320x240 UI scaled up");
                return;
            }
        }
    }
}

void TestUiLayoutBounds() {
    // The UI scale (UiLayout::UiScale): a whole number, at least 1, leaving a canvas of at
    // least 320x180 UI pixels.
    TEST_ASSERT(UiLayout::UiScale(320, 240) == 1 && UiLayout::UiScale(320, 200) == 1);
    TEST_ASSERT(UiLayout::UiScale(640, 360) == 2 && UiLayout::UiScale(640, 480) == 2);
    TEST_ASSERT(UiLayout::UiScale(960, 540) == 3 && UiLayout::UiScale(1280, 720) == 4);
    TEST_ASSERT(UiLayout::UiScale(1920, 1080) == 6 && UiLayout::UiScale(1920, 200) == 1);

    for (const RenderSize& size : kUiTestSizes) {
        CheckPreviewScreensInsideMargin(size);
        CheckExtremeScreensInsideMargin(size);
        CheckMinimapLayout(size);
        CheckLevelBannerClear(size);
        CheckOverlaysAboveHud(size);
    }
    CheckUiScalesUp(2);
    CheckUiScalesUp(3);

    const std::string longName(80, 'M');
    TEST_ASSERT(UiRenderer::TextWidth(UiRenderer::FitText(longName, 100, 1), 1) <= 100);
    TEST_ASSERT(UiRenderer::FitText("SHORT", 100, 1) == "SHORT");
    TEST_ASSERT(UiRenderer::FormatDuration(42.9) == "0:42" && UiRenderer::FormatDuration(3725.0) == "1:02:05");
}

// --- Scripted play-through ------------------------------------------------------------------

// From now on every engine frame holds only the action `pressed` names at that moment
// (InputAction::Count: nothing held).
void HoldOnlyAction(Engine& engine, const InputAction& pressed) {
    engine.SetInputInjector([&pressed](InputState& input) {
        for (std::size_t i = 0; i < static_cast<std::size_t>(InputAction::Count); ++i) {
            input.SetActionState(static_cast<InputAction>(i), static_cast<InputAction>(i) == pressed);
        }
    });
}

// The Phase 7 autoplay bot (engine/autoplay/AutoplayBot.h) plays the shipped campaign through
// Engine::Update with injected input, from the main menu: New Game, every level of
// game/defs/campaign.json in order -> CampaignComplete, health and ammo carried over. The
// per-level runs are CTest autoplay.level.N and autoplay.campaign (neo_wolf_autoplay); this test
// checks the engine state they cannot see, then uses Continue on the main menu. It holds for any
// campaign size; fetching a key before its locked door is covered by autoplay_fixture_level.
void TestCampaignPlaythrough() {
    const CampaignDefinition campaign = ShippedCampaign();
    TEST_ASSERT(!campaign.levels.empty());
    const std::size_t levelCount = campaign.levels.size();
    const ScratchDir prefs("playthrough-prefs");
    Engine engine;
    EngineOptions options = ShippedEngineOptions();
    options.prefDir = prefs.Path();
    TEST_ASSERT(engine.Init(options));
    TEST_ASSERT(engine.GetState() == GameState::Menu && !engine.flow_.HasProgress());

    const AutoplayRunReport report = RunAutoplaySession(engine, AutoplayConfig{}, true);
    if (!report.Success()) {
        FailTest(__FILE__, __LINE__, "the bot did not finish the campaign:\n" + FormatAutoplayTable(report));
    }
    TEST_ASSERT(engine.GetState() == GameState::CampaignComplete);
    TEST_ASSERT(report.levels.size() == levelCount && report.carryOverChecked && report.carryOverProblems.empty());
    // New Game starts with the gameplay.json values; later levels start with what the
    // previous one ended with (clamped to the maxima).
    TEST_ASSERT(report.levels[0].healthStart == engine.gameplay_.player.initialHealth);
    TEST_ASSERT(report.levels[0].ammoStart == engine.gameplay_.weapon.initialAmmo);
    for (std::size_t i = 1; i < report.levels.size(); ++i) {
        TEST_ASSERT(report.levels[i].healthStart ==
                    std::min(report.levels[i - 1].healthEnd, engine.gameplay_.player.maxHealth));
        TEST_ASSERT(report.levels[i].ammoStart ==
                    std::min(report.levels[i - 1].ammoEnd, engine.gameplay_.weapon.maxAmmo));
    }
    // Per level, against its own files: the report names the level, counts its enemies, and
    // every locked door the bot opened has a lock of that colour in the level and was opened
    // with a key the bot collected there (keys do not carry over).
    for (std::size_t i = 0; i < levelCount; ++i) {
        const AutoplayLevelReport& level = report.levels[i];
        TEST_ASSERT(level.id == campaign.levels[i].id && level.levelNumber == static_cast<int>(i) + 1);
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        std::vector<EntityPlacement> placements;
        TEST_ASSERT(
            LoadEntityPlacementsFromFile(ResolveRepoPath(campaign.levels[i].entities), placements, errors, warnings));
        int enemies = 0;
        std::vector<std::string> locks;
        for (const EntityPlacement& placement : placements) {
            enemies += placement.type == PlacementType::Enemy ? 1 : 0;
            if (placement.type == PlacementType::Door && placement.lock != KeyColor::None) {
                locks.emplace_back(KeyColorName(placement.lock));
            }
        }
        TEST_ASSERT(level.totalEnemies == enemies);
        for (const std::string& used : level.keysUsed) {
            TEST_ASSERT(std::find(locks.begin(), locks.end(), used) != locks.end());
            TEST_ASSERT(std::find(level.keysCollected.begin(), level.keysCollected.end(), used) !=
                        level.keysCollected.end());
        }
    }
    TEST_ASSERT(engine.flow_.LastResults().totalEnemies == report.levels.back().totalEnemies);
    TEST_ASSERT(engine.flow_.LastResults().shotsFired == report.levels.back().shotsFired);

    // Progress was saved: every level unlocked.
    ProgressData saved{};
    std::string message;
    TEST_ASSERT(LoadProgressFile(PathToUtf8(prefs.Path() / kProgressFileName), saved, message) ==
                ProgressLoadResult::Loaded);
    TEST_ASSERT(saved.highestUnlocked == static_cast<int>(levelCount));

    // Credits -> Enter -> menu, where Continue is offered and starts the last level with fresh stats.
    InputAction pressed = InputAction::Count;
    HoldOnlyAction(engine, pressed);
    const auto tap = [&](InputAction action) {
        pressed = InputAction::Count;
        engine.Update(kFrameSeconds);  // release everything first: a press is an edge
        pressed = action;
        engine.Update(kFrameSeconds);
    };
    tap(InputAction::Confirm);
    TEST_ASSERT(engine.GetState() == GameState::Menu);
    TEST_ASSERT(engine.flow_.MenuItems()[1] == MenuItem::Continue);
    tap(InputAction::MoveBackward);  // NEW GAME -> CONTINUE
    TEST_ASSERT(engine.flow_.MenuCursor() == 1);
    tap(InputAction::Select);
    TEST_ASSERT(engine.GetState() == GameState::Gameplay &&
                engine.currentLevelIndex_ == static_cast<int>(levelCount) - 1);
    TEST_ASSERT(engine.playerHealth_ == engine.gameplay_.player.initialHealth);
}

void TestNewGameResetsWorld() {
    // Known issue C7: Game Over -> Menu -> New Game used to keep 0 HP and the old world.
    // On the reference campaign: level 1 has a guard and the spawn at (2.5, 2.5).
    const ScratchDir prefs("new-game-prefs");
    const ScratchDir content("new-game-content");
    Engine engine;
    EngineOptions options = ReferenceEngineOptions(content, 0);
    options.prefDir = prefs.Path();
    TEST_ASSERT(engine.Init(options));

    InputAction pressed = InputAction::Count;
    HoldOnlyAction(engine, pressed);
    const auto tap = [&](InputAction action) {
        pressed = action;
        engine.Update(kFrameSeconds);
        pressed = InputAction::Count;
        engine.Update(kFrameSeconds);
    };

    tap(InputAction::Select);  // New Game
    TEST_ASSERT(engine.GetState() == GameState::Gameplay);
    // Change the world: collect nothing, kill the enemy, move the player, then die.
    Entity* enemy = FindFirstEntity(engine.entityManager_, EntityType::Enemy);
    TEST_ASSERT(enemy != nullptr);
    enemy->enemyState = EnemyState::Dead;
    enemy->active = false;
    engine.player_.position = {5.5f, 5.5f};
    InteropCommand damage{};
    damage.type = InteropCommandType::DamagePlayer;
    damage.intArg1 = 1000;
    engine.bridge_.EnqueueCommand(damage);
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.playerHealth_ == 0 && engine.GetState() == GameState::GameOver);

    tap(InputAction::Back);  // Esc: menu
    TEST_ASSERT(engine.GetState() == GameState::Menu);
    tap(InputAction::Select);  // New Game again
    TEST_ASSERT(engine.GetState() == GameState::Gameplay);
    TEST_ASSERT(engine.playerHealth_ == engine.gameplay_.player.initialHealth);
    TEST_ASSERT(engine.playerAmmo_ == engine.gameplay_.weapon.initialAmmo);
    TEST_ASSERT_NEAR(engine.player_.position.x, ReferenceLevel1::kSpawnX, 0.2f);
    const Entity* freshEnemy = FindFirstEntity(engine.entityManager_, EntityType::Enemy);
    TEST_ASSERT(freshEnemy != nullptr && freshEnemy->active && freshEnemy->enemyState != EnemyState::Dead);
    TEST_ASSERT(engine.CurrentLevelResults().kills == 0);

    // The Quit menu item stops the engine (the game then exits with code 0).
    engine.SetState(GameState::Menu);
    tap(InputAction::MoveForward);  // wraps to the last item, Quit
    TEST_ASSERT(engine.flow_.MenuItems()[static_cast<std::size_t>(engine.flow_.MenuCursor())] == MenuItem::Quit);
    tap(InputAction::Select);
    TEST_ASSERT(!engine.IsRunning());
}

// The test level (campaign.json "testLevel", the Test Chamber) in the game flow: its menu item,
// its exit switch restarts it, and it never unlocks a level or asks for progress to be saved.
void TestGameFlowTestLevel() {
    GameFlowConfig config{};
    config.levelCount = 3;
    config.initial = PlayerStats{100, 24};
    config.maximum = PlayerStats{kPlayerMaxHealth, kPlayerMaxAmmo};
    config.hasTestLevel = true;
    GameFlow flow;
    flow.Configure(config);
    flow.SetProgress(false, 1);
    TEST_ASSERT(flow.HasTestLevel());
    std::vector<MenuItem> items = flow.MenuItems();
    TEST_ASSERT(items.size() == 4 && items[2] == MenuItem::TestLevel && items[3] == MenuItem::Quit);
    flow.SetProgress(true, 2);
    items = flow.MenuItems();
    TEST_ASSERT(items.size() == 5 && items[1] == MenuItem::Continue && items[3] == MenuItem::TestLevel);
    TEST_ASSERT(std::string(MenuItemLabel(MenuItem::TestLevel)) == "TEST LEVEL");

    // The menu item starts the test level with the initial values.
    flow.Update(Press({InputAction::MoveBackward}), {});
    flow.Update(Press({InputAction::MoveBackward}), {});
    flow.Update(Press({InputAction::MoveBackward}), {});
    TEST_ASSERT(flow.MenuCursor() == 3);
    std::vector<FlowRequest> requests = flow.Update(Press({InputAction::Select}), {});
    const FlowRequest* load = FindRequest(requests, FlowRequestType::LoadLevel);
    TEST_ASSERT(load != nullptr && load->levelIndex == kTestLevelIndex);
    TEST_ASSERT(load->stats.health == 100 && load->stats.ammo == 24);
    TEST_ASSERT(flow.State() == GameState::Gameplay && flow.InTestLevel() && !flow.IsLastLevel());

    // Its exit switch restarts it: no level-complete screen, nothing unlocked or saved.
    requests = flow.Update(kNoInput, LevelCompletedEvents(37, 5));
    TEST_ASSERT(requests.size() == 1 && requests[0].type == FlowRequestType::RestartLevel);
    TEST_ASSERT(requests[0].levelIndex == kTestLevelIndex && requests[0].stats.health == 100);
    TEST_ASSERT(requests[0].stats.ammo == 24);
    TEST_ASSERT(!HasRequest(requests, FlowRequestType::SaveProgress));
    TEST_ASSERT(flow.State() == GameState::Gameplay && flow.InTestLevel());
    TEST_ASSERT(flow.HighestUnlocked() == 2 && flow.LastResults().kills == 0);

    // Death: the game-over screen; Restart (or Enter) starts it again, Esc goes to the menu.
    FlowEvents died{};
    died.playerDied = true;
    flow.Update(kNoInput, died);
    TEST_ASSERT(flow.State() == GameState::GameOver);
    requests = flow.Update(Press({InputAction::Restart}), {});
    const FlowRequest* restart = FindRequest(requests, FlowRequestType::RestartLevel);
    TEST_ASSERT(restart != nullptr && restart->levelIndex == kTestLevelIndex && restart->stats.health == 100);
    TEST_ASSERT(flow.InTestLevel() && flow.State() == GameState::Gameplay);

    // Pause and quit to the main menu, as in any level.
    flow.Update(Press({InputAction::Pause}), {});
    TEST_ASSERT(flow.State() == GameState::Paused);
    flow.Update(Press({InputAction::QuitToMenu}), {});
    TEST_ASSERT(flow.State() == GameState::Menu && flow.Page() == MenuPage::Main);

    // New Game afterwards is the campaign as usual: level 1, and its exit saves progress.
    requests = flow.Update(Press({InputAction::Select}), {});
    TEST_ASSERT(!flow.InTestLevel() && flow.CurrentLevel() == 0);
    requests = flow.Update(kNoInput, LevelCompletedEvents(80, 20));
    TEST_ASSERT(flow.State() == GameState::LevelComplete && HasRequest(requests, FlowRequestType::SaveProgress));

    // At boot (campaign.json "bootIntoTestLevel"): straight into play.
    GameFlow boot;
    boot.Configure(config);
    const FlowRequest direct = boot.StartTestLevel();
    TEST_ASSERT(direct.type == FlowRequestType::LoadLevel && direct.levelIndex == kTestLevelIndex);
    TEST_ASSERT(boot.State() == GameState::Gameplay && boot.InTestLevel());

    // The level-complete screen set from outside (C API, command bridge): Enter restarts the test
    // level instead of advancing to the campaign-complete screen.
    boot.ForceState(GameState::Victory);
    TEST_ASSERT(boot.State() == GameState::LevelComplete);
    requests = boot.Update(Press({InputAction::Confirm}), {});
    TEST_ASSERT(requests.size() == 1 && requests[0].type == FlowRequestType::RestartLevel);
    TEST_ASSERT(requests[0].levelIndex == kTestLevelIndex && boot.State() == GameState::Gameplay);
    TEST_ASSERT(!boot.HasProgress());

    // A level loaded by path afterwards is not the test level: its exit ends it as before. After
    // a campaign level the flow keeps its position (docs/C_API.md, nw_engine_load_level).
    boot.LeaveTestLevel();
    TEST_ASSERT(!boot.InTestLevel() && boot.CurrentLevel() == -1);
    GameFlow campaign;
    campaign.Configure(config);
    campaign.StartLevelDirect(1);
    campaign.LeaveTestLevel();
    TEST_ASSERT(campaign.CurrentLevel() == 1);
    requests = boot.Update(kNoInput, LevelCompletedEvents(10, 2));
    TEST_ASSERT(boot.State() == GameState::LevelComplete && !HasRequest(requests, FlowRequestType::RestartLevel));

    // A level that fails to load leaves the engine on the level before it: CancelLevelStart puts
    // the flow back on that level (and its restart stats), so the test level stays the test level.
    GameFlow failed;
    failed.Configure(config);
    failed.StartTestLevel();
    failed.ForceState(GameState::Menu);
    requests = failed.Update(Press({InputAction::Select}), {});  // New Game
    TEST_ASSERT(failed.CurrentLevel() == 0 && HasRequest(requests, FlowRequestType::LoadLevel));
    failed.CancelLevelStart();
    TEST_ASSERT(failed.State() == GameState::Menu && failed.InTestLevel());
    TEST_ASSERT(failed.EntrySnapshot().health == 100 && failed.EntrySnapshot().ammo == 24);
    failed.ForceState(GameState::Gameplay);
    requests = failed.Update(kNoInput, LevelCompletedEvents(10, 2));
    TEST_ASSERT(requests.size() == 1 && requests[0].type == FlowRequestType::RestartLevel);
    TEST_ASSERT(requests[0].levelIndex == kTestLevelIndex && !failed.HasProgress());
}

// A content root with the reference campaign and a small test level ("sandbox": a room with a
// sign and an exit switch on the east wall) that the game starts in.
std::filesystem::path MakeTestLevelContentRoot(const std::filesystem::path& dir) {
    const std::filesystem::path root = MakeReferenceContentRoot(dir);
    WriteTextFile(root / "game" / "maps" / "sandbox.map", "#########\n#.......X\n#...T...#\n#########\n");
    WriteTextFile(root / "game" / "defs" / "sandbox.entities.json",
                  R"({"level": "sandbox", "entities": [
                      {"type": "player", "x": 1.5, "y": 1.5, "angle": 0},
                      {"type": "trigger", "x": 4.5, "y": 2.5, "mapX": 4, "mapY": 2, "message": "Sandbox sign"}]})");
    nlohmann::json campaign =
        nlohmann::json::parse(ReadWholeFile(PathToUtf8(root / "game" / "defs" / "campaign.json")));
    campaign["testLevel"] = {{"id", "sandbox"},
                             {"name", "Sandbox Room"},
                             {"map", "game/maps/sandbox.map"},
                             {"entities", "game/defs/sandbox.entities.json"},
                             {"theme", "stone"}};
    campaign["bootIntoTestLevel"] = true;
    WriteTextFile(root / "game" / "defs" / "campaign.json", campaign.dump(2));
    return root;
}

void TestTestLevel() {
    const ScratchDir scratch("test-level");
    const std::filesystem::path root = MakeTestLevelContentRoot(scratch.Path() / "content");
    const std::filesystem::path prefs = scratch.Path() / "prefs";
    std::filesystem::create_directories(prefs);
    const std::filesystem::path progressPath = prefs / kProgressFileName;

    // Without EngineOptions::testLevelBoot (the C API, tests, the bot): the main menu as before,
    // with the test level's item showing its name.
    {
        Engine engine;
        TEST_ASSERT(engine.Init(HeadlessEngineOptions(root)));
        TEST_ASSERT(engine.GetState() == GameState::Menu && !engine.InTestLevel() && engine.currentLevelIndex_ == 0);
        const std::vector<MenuItem> items = engine.flow_.MenuItems();
        TEST_ASSERT(std::find(items.begin(), items.end(), MenuItem::TestLevel) != items.end());
        TEST_ASSERT(engine.MenuItemText(MenuItem::TestLevel) == "SANDBOX ROOM");
        TEST_ASSERT(engine.MenuItemText(MenuItem::NewGame) == "NEW GAME");
    }
    // --level and --preview win over bootIntoTestLevel.
    {
        Engine engine;
        EngineOptions options = HeadlessEngineOptions(root, 2);
        options.testLevelBoot = true;
        TEST_ASSERT(engine.Init(options));
        TEST_ASSERT(engine.GetState() == GameState::Gameplay && engine.currentLevelIndex_ == 1);
        TEST_ASSERT(!engine.InTestLevel() && !engine.flow_.InTestLevel());
    }
    {
        Engine engine;
        EngineOptions options = HeadlessEngineOptions(root);
        options.testLevelBoot = true;
        options.preview = PreviewScreen::Menu;
        TEST_ASSERT(engine.Init(options));
        TEST_ASSERT(engine.GetState() == GameState::Menu && !engine.InTestLevel());
    }

    // The game: straight into the test level, at the initial health and ammo.
    Engine engine;
    EngineOptions options = HeadlessEngineOptions(root);
    options.testLevelBoot = true;
    options.prefDir = prefs;
    TEST_ASSERT(engine.Init(options));
    TEST_ASSERT(engine.GetState() == GameState::Gameplay && engine.InTestLevel() && engine.flow_.InTestLevel());
    TEST_ASSERT(engine.CurrentCampaignLevel() == nullptr && engine.currentThemeId_ == "stone");
    TEST_ASSERT(engine.playerHealth_ == engine.gameplay_.player.initialHealth);
    TEST_ASSERT(engine.playerAmmo_ == engine.gameplay_.weapon.initialAmmo);
    TEST_ASSERT(UiRenderer::TestLevelLabel("Sandbox Room") == "SANDBOX ROOM");
    engine.framebuffer_.ResetDrawBounds();
    engine.Render();
    AssertInsideMargin(engine.framebuffer_, "the test level's HUD and banner");

    InputAction pressed = InputAction::Count;
    HoldOnlyAction(engine, pressed);
    const auto tap = [&](InputAction action) {
        pressed = action;
        engine.Update(kFrameSeconds);
        pressed = InputAction::Count;
        engine.Update(kFrameSeconds);
    };

    // A trigger with a "message" shows it on the HUD.
    engine.player_.position = {4.5f, 2.5f};
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.hudMessage_ == "Sandbox sign");

    // The exit switch restarts the test level (and says so) and reports level 0; nothing is saved.
    InteropEvent event{};
    while (engine.bridge_.PollEvent(event)) {
    }
    engine.playerHealth_ = 40;
    engine.player_.position = {7.5f, 1.5f};
    engine.player_.directionAngleRadians = 0.0f;
    tap(InputAction::Interact);
    TEST_ASSERT(engine.GetState() == GameState::Gameplay && engine.InTestLevel());
    TEST_ASSERT_NEAR(engine.player_.position.x, 1.5f, 0.01f);
    TEST_ASSERT(engine.playerHealth_ == engine.gameplay_.player.initialHealth);
    bool completed = false;
    while (engine.bridge_.PollEvent(event)) {
        if (event.type == InteropEventType::LevelCompleted) {
            completed = true;
            TEST_ASSERT(event.intArg1 == 0);
        }
    }
    TEST_ASSERT(completed);
    TEST_ASSERT(engine.hudMessage_ == "Exit switch: Sandbox Room restarted");
    TEST_ASSERT(!std::filesystem::exists(progressPath));
    TEST_ASSERT(!engine.flow_.HasProgress() && engine.flow_.HighestUnlocked() == 1);

    // Death: game over; R restarts the test level.
    InteropCommand damage{};
    damage.type = InteropCommandType::DamagePlayer;
    damage.intArg1 = 1000;
    engine.bridge_.EnqueueCommand(damage);
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.GetState() == GameState::GameOver);
    tap(InputAction::Restart);
    TEST_ASSERT(engine.GetState() == GameState::Gameplay && engine.InTestLevel());
    TEST_ASSERT(engine.playerHealth_ == engine.gameplay_.player.initialHealth);

    // The C API's reset keeps the test level.
    engine.ResetLevel();
    TEST_ASSERT(engine.InTestLevel() && engine.CurrentCampaignLevel() == nullptr);

    // Pause, quit to the menu, and back in through the menu item (New Game, Level Select, the
    // test level, Quit).
    tap(InputAction::Pause);
    TEST_ASSERT(engine.GetState() == GameState::Paused);
    tap(InputAction::QuitToMenu);
    TEST_ASSERT(engine.GetState() == GameState::Menu);
    tap(InputAction::MoveForward);
    tap(InputAction::MoveForward);
    TEST_ASSERT(engine.flow_.MenuItems()[static_cast<std::size_t>(engine.flow_.MenuCursor())] == MenuItem::TestLevel);
    tap(InputAction::Select);
    TEST_ASSERT(engine.GetState() == GameState::Gameplay && engine.InTestLevel());
    TEST_ASSERT(!std::filesystem::exists(progressPath));

    // The level-complete screen set from outside (C API SetGameState): the test level's name and
    // results so far; Enter restarts it and nothing is saved.
    engine.SetState(GameState::LevelComplete);
    engine.framebuffer_.ResetDrawBounds();
    engine.Render();
    AssertInsideMargin(engine.framebuffer_, "the test level's level-complete screen");
    tap(InputAction::Confirm);
    TEST_ASSERT(engine.GetState() == GameState::Gameplay && engine.InTestLevel());
    TEST_ASSERT(!std::filesystem::exists(progressPath));

    // A campaign level that fails to load (an invalid entity file is only found at load time)
    // leaves the test level loaded, and the flow follows it: back to the menu, and the test
    // level's exit switch, resumed through the C API, still restarts it and saves nothing.
    const nlohmann::json manifest =
        nlohmann::json::parse(ReadWholeFile(PathToUtf8(root / "game" / "defs" / "campaign.json")));
    const std::filesystem::path level1Entities = root / manifest["levels"][0]["entities"].get<std::string>();
    const std::string level1Text = ReadWholeFile(PathToUtf8(level1Entities));
    WriteTextFile(level1Entities, "{ \"entities\": [ }");
    engine.SetState(GameState::Menu);
    tap(InputAction::Select);  // New Game: level 1 fails to load
    TEST_ASSERT(engine.GetState() == GameState::Menu && engine.InTestLevel() && engine.flow_.InTestLevel());
    WriteTextFile(level1Entities, level1Text);
    engine.SetState(GameState::Gameplay);
    engine.player_.position = {7.5f, 1.5f};
    engine.player_.directionAngleRadians = 0.0f;
    tap(InputAction::Interact);
    TEST_ASSERT(engine.GetState() == GameState::Gameplay && engine.InTestLevel());
    TEST_ASSERT(!std::filesystem::exists(progressPath));
    TEST_ASSERT(!engine.flow_.HasProgress() && engine.flow_.HighestUnlocked() == 1);

    // A level loaded by path (C API, command bridge) replaces the test level for the flow too:
    // its exit switch ends that level instead of loading the test level again.
    TEST_ASSERT(engine.LoadLevel("game/maps/sandbox.map", "game/defs/sandbox.entities.json"));
    TEST_ASSERT(!engine.InTestLevel() && !engine.flow_.InTestLevel() && engine.flow_.CurrentLevel() == -1);
    engine.player_.position = {7.5f, 1.5f};
    engine.player_.directionAngleRadians = 0.0f;
    tap(InputAction::Interact);
    TEST_ASSERT(engine.GetState() == GameState::LevelComplete && !engine.InTestLevel());
    engine.Shutdown();
}

void TestKeyBindings() {
    std::array<InputAction, PlatformSDL::kMaxActionsPerKey> actions = {InputAction::Count, InputAction::Count};
    // B13: Ctrl fires, but not while Cmd/Windows is held (Cmd+Ctrl+F is the fullscreen chord).
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_LCTRL, KMOD_NONE, actions) == 1);
    TEST_ASSERT(actions[0] == InputAction::Attack);
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_LCTRL, KMOD_LGUI, actions) == 0);
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_RCTRL, KMOD_RGUI | KMOD_LCTRL, actions) == 0);
#if defined(__APPLE__)
    TEST_ASSERT(PlatformSDL::IsFullscreenShortcut(SDL_SCANCODE_F, KMOD_LGUI | KMOD_LCTRL));
#endif
    TEST_ASSERT(!PlatformSDL::IsFullscreenShortcut(SDL_SCANCODE_F, KMOD_NONE));
    TEST_ASSERT(PlatformSDL::IsFullscreenShortcut(SDL_SCANCODE_F11, KMOD_NONE));
    TEST_ASSERT(PlatformSDL::IsFullscreenShortcut(SDL_SCANCODE_RETURN, KMOD_LALT));
    // The chord's press is cancelled for the frame (no shot, no release event).
    InputState input{};
    input.BeginFrame();
    input.SetActionState(InputAction::Attack, true);
    input.CancelAction(InputAction::Attack);
    TEST_ASSERT(!input.IsDown(InputAction::Attack) && !input.WasPressed(InputAction::Attack) &&
                !input.WasReleased(InputAction::Attack));

    // Menu, pause and minimap keys.
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_SPACE, KMOD_NONE, actions) == 2);
    TEST_ASSERT(actions[0] == InputAction::Attack && actions[1] == InputAction::Select);
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_RETURN, KMOD_NONE, actions) == 2);
    TEST_ASSERT(actions[0] == InputAction::Confirm && actions[1] == InputAction::Select);
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_Q, KMOD_NONE, actions) == 2);
    TEST_ASSERT(actions[0] == InputAction::TurnLeft && actions[1] == InputAction::QuitToMenu);
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_ESCAPE, KMOD_NONE, actions) == 2);
    TEST_ASSERT(actions[0] == InputAction::Pause && actions[1] == InputAction::Back);
    for (const SDL_Scancode key : {SDL_SCANCODE_M, SDL_SCANCODE_TAB}) {
        TEST_ASSERT(PlatformSDL::MapKeyToActions(key, KMOD_NONE, actions) == 1);
        TEST_ASSERT(actions[0] == InputAction::ToggleMinimap);
    }
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_F3, KMOD_NONE, actions) == 1);
    TEST_ASSERT(actions[0] == InputAction::ToggleDebugView);
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_UP, KMOD_NONE, actions) == 1);
    TEST_ASSERT(actions[0] == InputAction::MoveForward);
    TEST_ASSERT(PlatformSDL::MapKeyToActions(SDL_SCANCODE_K, KMOD_NONE, actions) == 0);

    // The minimap is off by default and toggles in play.
    Engine engine;
    TEST_ASSERT(engine.Init(ShippedEngineOptions(1)));
    TEST_ASSERT(!engine.minimapVisible_);
    bool down = true;
    engine.SetInputInjector([&down](InputState& state) { state.SetActionState(InputAction::ToggleMinimap, down); });
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.minimapVisible_);
    down = false;
    engine.Update(kFrameSeconds);
    down = true;
    engine.Update(kFrameSeconds);
    TEST_ASSERT(!engine.minimapVisible_);
}

// --- Phase 6: AI, enemy types, keys and locked doors -----------------------------------------

void TestLineOfSight() {
    Map map(12, 8);
    BuildBorderWalls(map);
    const Vec2 a{2.5f, 3.5f};
    const Vec2 b{9.5f, 3.5f};
    TEST_ASSERT(HasLineOfSight(map, a, b) && HasLineOfSight(map, b, a));
    for (const TileType solid : {TileType::Wall, TileType::WallAltA, TileType::WallAltB, TileType::Exit}) {
        map.SetCell(6, 3, solid);
        TEST_ASSERT(!HasLineOfSight(map, a, b) && !HasLineOfSight(map, b, a));
    }
    map.SetCell(6, 3, TileType::Door);  // a closed door blocks sight
    TEST_ASSERT(!HasLineOfSight(map, a, b));
    map.SetCell(6, 3, TileType::Empty);  // the same door, open
    TEST_ASSERT(HasLineOfSight(map, a, b));
    map.SetCell(6, 3, TileType::Trigger);  // a trigger tile is walkable and see-through
    TEST_ASSERT(HasLineOfSight(map, a, b));

    // Diagonal lines are blocked by the cells they cross, and only by those.
    map.SetCell(6, 4, TileType::Wall);
    TEST_ASSERT(!HasLineOfSight(map, {2.5f, 1.5f}, {9.5f, 6.5f}));
    TEST_ASSERT(HasLineOfSight(map, {2.5f, 6.5f}, {9.5f, 5.5f}));
    // Same cell, and a point next to the eye.
    TEST_ASSERT(HasLineOfSight(map, {2.2f, 2.2f}, {2.8f, 2.7f}));
    // A body passing a wall corner: the centre line is clear, a wide body's edge is not.
    TEST_ASSERT(HasLineOfSight(map, {2.5f, 5.3f}, {9.5f, 5.3f}));
    TEST_ASSERT(!HasClearCorridor(map, {2.5f, 5.3f}, {9.5f, 5.3f}, 0.4f));
    TEST_ASSERT(HasClearCorridor(map, {2.5f, 5.5f}, {9.5f, 5.5f}, kEnemyRadius));
}

void TestEnemyHearing() {
    // Two rooms joined by a door in a wall at x = 7. Hearing follows open cells only.
    Map map(14, 7);
    BuildBorderWalls(map);
    for (int y = 1; y <= 5; ++y) {
        map.SetCell(7, y, TileType::Wall);
    }
    map.SetCell(7, 3, TileType::Door);
    EntityManager manager{};
    Player player{};
    player.position = {3.5f, 3.5f};
    const int id = manager.CreateEntity(EntityType::Enemy, Vec2{10.5f, 3.5f}).id;
    manager.GetEntityById(id)->alertRadius = 2.0f;  // too short to see the player: only hearing counts
    int health = 100;
    const NoiseEvent shot{true, player.position, 10.0f};

    GameplayEvents events{};
    UpdateEnemyAi(manager, map, player, health, 0.05f, events, &shot);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Idle);  // the closed door blocks the sound

    map.SetCell(7, 3, TileType::Empty);                          // door open
    UpdateEnemyAi(manager, map, player, health, 0.05f, events);  // no shot: nothing heard
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Idle);
    UpdateEnemyAi(manager, map, player, health, 0.05f, events, &shot);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Alert);
    TEST_ASSERT_NEAR(manager.GetEntityById(id)->lastKnownPlayerPosition.x, 3.5f, 1e-6f);

    // Out of the noise radius (counted along the path, 7 cells here): not heard.
    manager.GetEntityById(id)->enemyState = EnemyState::Idle;
    const NoiseEvent faint{true, player.position, 6.0f};
    UpdateEnemyAi(manager, map, player, health, 0.05f, events, &faint);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Idle);

    // Sound goes around walls, so the path length counts, not the straight distance: with the
    // door closed again and a gap at the bottom, the enemy is 7 cells away in a straight line but
    // 11 along the path.
    map.SetCell(7, 3, TileType::Door);
    map.SetCell(7, 5, TileType::Empty);
    const std::vector<int> distances = ComputeSoundDistances(map, CellOf(player.position), 20);
    TEST_ASSERT(distances[static_cast<std::size_t>(3 * map.Width() + 10)] == 11);
    TEST_ASSERT(distances[static_cast<std::size_t>(3 * map.Width() + 7)] == -1);  // the closed door itself
    const NoiseEvent medium{true, player.position, 9.0f};
    UpdateEnemyAi(manager, map, player, health, 0.05f, events, &medium);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Idle);
    const NoiseEvent loud{true, player.position, 11.0f};
    UpdateEnemyAi(manager, map, player, health, 0.05f, events, &loud);
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Alert);

    // Through the engine: a shot in the reference level 3 wakes the brute (10 cells along the
    // corridor) but not the guard locked in the room behind the closed door.
    const ScratchDir referenceContent("hearing-reference");
    Engine engine;
    const EngineOptions options = ReferenceEngineOptions(referenceContent, 3);
    TEST_ASSERT(engine.Init(options));
    const Entity* guard = nullptr;
    const Entity* brute = nullptr;
    for (const Entity& entity : engine.entityManager_.Entities()) {
        guard = entity.type == EntityType::Enemy && entity.variant == EnemyVariant::Guard ? &entity : guard;
        brute = entity.type == EntityType::Enemy && entity.variant == EnemyVariant::Brute ? &entity : brute;
    }
    TEST_ASSERT(guard != nullptr && brute != nullptr);
    const int guardId = guard->id;
    const int bruteId = brute->id;
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.entityManager_.GetEntityById(bruteId)->enemyState == EnemyState::Idle);
    engine.PlayerAttack();
    TEST_ASSERT(engine.pendingNoise_.active);
    engine.Update(kFrameSeconds);
    TEST_ASSERT(!engine.pendingNoise_.active);  // heard once
    TEST_ASSERT(engine.entityManager_.GetEntityById(bruteId)->enemyState == EnemyState::Alert);
    TEST_ASSERT(engine.entityManager_.GetEntityById(guardId)->enemyState == EnemyState::Idle);
}

// True if the path is a chain of neighbouring walkable cells that never cuts a wall corner.
bool IsValidPath(const Map& map, const Vec2i& start, const std::vector<Vec2i>& path) {
    Vec2i at = start;
    for (const Vec2i& next : path) {
        const int dx = next.x - at.x;
        const int dy = next.y - at.y;
        if (std::abs(dx) > 1 || std::abs(dy) > 1 || (dx == 0 && dy == 0) || !IsWalkableCell(map, next.x, next.y)) {
            return false;
        }
        if (dx != 0 && dy != 0 && (!IsWalkableCell(map, at.x + dx, at.y) || !IsWalkableCell(map, at.x, at.y + dy))) {
            return false;
        }
        at = next;
    }
    return true;
}

void TestEnemyPathfinding() {
    // A wall at x = 5 from the top to row 5; the only way round is through row 6.
    Map map(10, 8);
    BuildBorderWalls(map);
    for (int y = 1; y <= 5; ++y) {
        map.SetCell(5, y, TileType::Wall);
    }
    std::vector<Vec2i> path;
    TEST_ASSERT(FindPath(map, {2, 2}, {8, 2}, path));
    TEST_ASSERT(!path.empty() && path.back() == (Vec2i{8, 2}));
    TEST_ASSERT(IsValidPath(map, {2, 2}, path));
    bool viaRowSix = false;
    for (const Vec2i& cell : path) {
        viaRowSix = viaRowSix || (cell.x == 5 && cell.y == 6);
    }
    TEST_ASSERT(viaRowSix);
    // The shortest path without cutting corners: (3,3) (4,4) (4,5) (4,6) (5,6) (6,6) (7,5) (8,4) (8,3) (8,2).
    TEST_ASSERT(path.size() == 10);

    TEST_ASSERT(FindPath(map, {3, 3}, {3, 3}, path) && path.empty());
    TEST_ASSERT(!FindPath(map, {2, 2}, {5, 2}, path) && path.empty());  // the goal is a wall
    map.SetCell(5, 6, TileType::Door);                                  // a closed door: no way round
    TEST_ASSERT(!FindPath(map, {2, 2}, {8, 2}, path));
    map.SetCell(5, 6, TileType::Empty);  // opened
    TEST_ASSERT(FindPath(map, {2, 2}, {8, 2}, path));

    // An enemy chasing a last known position behind the wall walks round it (cell-centre
    // steering with sliding) and never sticks to the corner or enters a wall.
    EntityManager manager{};
    Player player{};
    player.position = {8.5f, 2.5f};
    const int id = manager.CreateEntity(EntityType::Enemy, Vec2{2.5f, 2.5f}).id;
    Entity* enemy = manager.GetEntityById(id);
    enemy->enemyState = EnemyState::Chase;
    enemy->lastKnownPlayerPosition = player.position;
    enemy->moveSpeed = 2.0f;
    enemy->alertRadius = 6.0f;
    int health = 100;
    bool reached = false;
    for (int frame = 0; frame < 400 && !reached; ++frame) {
        GameplayEvents events{};
        UpdateEnemyAi(manager, map, player, health, kAiFrameSeconds, events);
        enemy = manager.GetEntityById(id);
        TEST_ASSERT(!IsBlockedByWall(map, enemy->position, kEnemyRadius - 0.01f));
        TEST_ASSERT(enemy->repathTimer <= kEnemyRepathInterval + 1e-5f);
        reached = enemy->enemyState == EnemyState::Attack;
    }
    TEST_ASSERT(reached);  // 400 frames = 6.7 s; the path is about 8 cells at 2 cells/s

    // Enemies do not open doors: with the gap closed by a door, the enemy stays on its side and
    // eventually gives up.
    map.SetCell(5, 6, TileType::Door);
    manager.GetEntityById(id)->position = {2.5f, 2.5f};
    manager.GetEntityById(id)->enemyState = EnemyState::Chase;
    manager.GetEntityById(id)->path.clear();
    for (int frame = 0; frame < 60 * 9; ++frame) {
        GameplayEvents events{};
        UpdateEnemyAi(manager, map, player, health, kAiFrameSeconds, events);
        TEST_ASSERT(manager.GetEntityById(id)->position.x < 5.0f - kEnemyRadius + 1e-4f);
    }
    TEST_ASSERT(manager.GetEntityById(id)->enemyState == EnemyState::Idle);
}

void TestEnemyArchetypesFromData() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    GameplayDefinitions gameplay{};
    TEST_ASSERT(LoadGameplayDefinitions(ResolveRepoPath("game/defs/gameplay.json"), gameplay, errors, warnings));
    TEST_ASSERT(errors.empty() && warnings.empty());
    const EnemyDefinition& guard = gameplay.enemies.guard;
    const EnemyDefinition& brute = gameplay.enemies.brute;
    const EnemyDefinition& runner = gameplay.enemies.runner;
    // The shipped values (game/defs/gameplay.json).
    TEST_ASSERT(brute.health == 140 && brute.damage == 25 && brute.score == 300);
    TEST_ASSERT_NEAR(brute.moveSpeed, 1.0f, 1e-6f);
    TEST_ASSERT_NEAR(brute.attackRate, 1.6f, 1e-6f);
    TEST_ASSERT_NEAR(brute.reactionTime, 0.8f, 1e-6f);
    TEST_ASSERT(runner.health == 25 && runner.damage == 6 && runner.score == 150);
    TEST_ASSERT_NEAR(runner.moveSpeed, 2.8f, 1e-6f);
    TEST_ASSERT_NEAR(runner.alertRadius, 8.0f, 1e-6f);
    TEST_ASSERT_NEAR(runner.reactionTime, 0.2f, 1e-6f);
    // The archetypes differ the way the design says: tank / fragile, slow / fast.
    TEST_ASSERT(brute.health > guard.health && guard.health > runner.health);
    TEST_ASSERT(brute.damage > guard.damage && guard.damage > runner.damage);
    TEST_ASSERT(brute.moveSpeed < guard.moveSpeed && guard.moveSpeed < runner.moveSpeed);

    // Placements pick their archetype; no variant means guard. Each gets its own sprite.
    Map map(8, 8);
    BuildBorderWalls(map);
    std::vector<EntityPlacement> placements(5);
    placements[0].type = PlacementType::Player;
    placements[0].position = {1.5f, 1.5f};
    const std::array<EnemyVariant, 4> variants = {EnemyVariant::Guard, EnemyVariant::Brute, EnemyVariant::Runner,
                                                  EnemyVariant::Guard};
    for (std::size_t i = 0; i < variants.size(); ++i) {
        placements[i + 1].type = PlacementType::Enemy;
        placements[i + 1].position = {2.5f + static_cast<float>(i), 4.5f};
        placements[i + 1].variant = variants[i];
    }
    LevelContents level{};
    TEST_ASSERT(BuildLevelFromPlacements(map, placements, gameplay, level, errors));
    const std::array<SpriteTextureId, 4> textures = {SpriteTextureId::Guard, SpriteTextureId::Brute,
                                                     SpriteTextureId::Runner, SpriteTextureId::Guard};
    std::size_t index = 0;
    for (const Entity& entity : level.entities.Entities()) {
        if (entity.type != EntityType::Enemy) {
            continue;
        }
        const EnemyDefinition& expected = gameplay.enemies.For(variants[index]);
        TEST_ASSERT(entity.variant == variants[index]);
        TEST_ASSERT(entity.health == expected.health && entity.maxHealth == expected.health);
        TEST_ASSERT(entity.damage == expected.damage && entity.score == expected.score);
        TEST_ASSERT_NEAR(entity.moveSpeed, expected.moveSpeed, 1e-6f);
        TEST_ASSERT_NEAR(entity.alertRadius, expected.alertRadius, 1e-6f);
        TEST_ASSERT_NEAR(entity.attackRadius, expected.attackRadius, 1e-6f);
        TEST_ASSERT_NEAR(entity.attackRate, expected.attackRate, 1e-6f);
        TEST_ASSERT_NEAR(entity.reactionTime, expected.reactionTime, 1e-6f);
        const SpriteEntity* sprite = FindSpriteFor(level.sprites, entity.id);
        TEST_ASSERT(sprite != nullptr && sprite->type == SpriteType::Enemy);
        TEST_ASSERT(sprite->textureId == static_cast<int>(textures[index]));
        ++index;
    }
    TEST_ASSERT(index == 4);

    // Edited data changes an archetype (no recompiling).
    GameplayDefinitions edited{};
    TEST_ASSERT(ParseGameplayDefinitions("{\"enemies\": {\"brute\": {\"health\": 400, \"moveSpeed\": 0.5}}}",
                                         "edit.json", edited, errors, warnings));
    Entity entity{};
    ApplyEnemyArchetype(entity, EnemyVariant::Brute, edited.enemies);
    TEST_ASSERT(entity.health == 400 && entity.variant == EnemyVariant::Brute);
    TEST_ASSERT_NEAR(entity.moveSpeed, 0.5f, 1e-6f);
    TEST_ASSERT(entity.damage == kDefaultEnemyDefinitions.brute.damage);  // not in the file: the default

    // The reference campaign: level 3 has one enemy of each type, each with its own texture loaded.
    const ScratchDir referenceContent("archetypes-reference");
    Engine engine;
    const EngineOptions options = ReferenceEngineOptions(referenceContent, 3);
    TEST_ASSERT(engine.Init(options));
    std::array<int, kEnemyVariantCount> counts{};
    for (const Entity& e : engine.entityManager_.Entities()) {
        if (e.type == EntityType::Enemy) {
            ++counts[static_cast<std::size_t>(e.variant)];
        }
    }
    TEST_ASSERT(counts[0] == 1 && counts[1] == 1 && counts[2] == 1);
    for (int i = 0; i < kSpriteTextureCount; ++i) {
        const Texture* texture = engine.spriteTextures_[static_cast<std::size_t>(i)];
        TEST_ASSERT(texture != nullptr && texture->Width() > 0 && texture->Width() != engine.missingTexture_.Width());
        for (int j = 0; j < i; ++j) {
            TEST_ASSERT(texture != engine.spriteTextures_[static_cast<std::size_t>(j)]);
        }
    }
}

void TestEntityVariantValidation() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<EntityPlacement> placements;
    const std::string bad =
        "{\"level\": \"v\", \"entities\": ["
        "{\"type\": \"player\", \"x\": 1.5, \"y\": 1.5},"
        "{\"type\": \"enemy\", \"x\": 2, \"y\": 2, \"variant\": \"ninja\"},"
        "{\"type\": \"enemy\", \"x\": 2, \"y\": 2, \"variant\": 3},"
        "{\"type\": \"enemy\", \"x\": 2, \"y\": 2, \"variant\": \"Guard\"},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 2, \"kind\": \"key\"},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 2, \"kind\": \"key\", \"key\": \"green\"},"
        "{\"type\": \"door\", \"x\": 3.5, \"y\": 1.5, \"mapX\": 3, \"mapY\": 1, \"lock\": \"purple\"},"
        "{\"type\": \"enemy\", \"x\": 2, \"y\": 2, \"variant\": \"runner\", \"lock\": \"red\"},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 2, \"kind\": \"health\", \"key\": \"red\"},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 2, \"kind\": \"key\", \"key\": \"red\", \"value\": 5}]}";
    TEST_ASSERT(!ParseEntityPlacementsJson(bad, "v.entities.json", placements, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"v.entities.json: entities[1] (enemy): field \"variant\" must be "
                                            "\"guard\", \"brute\" or \"runner\", got \"ninja\""}));
    TEST_ASSERT(AnyMessageContains(errors, {"entities[2] (enemy): field \"variant\" must be", "got 3"}));
    TEST_ASSERT(AnyMessageContains(errors, {"entities[3] (enemy): field \"variant\" must be", "\"Guard\""}));
    TEST_ASSERT(AnyMessageContains(
        errors, {"entities[4] (pickup): field \"key\" is required (\"red\", \"blue\" or \"gold\")"}));
    TEST_ASSERT(AnyMessageContains(
        errors, {"entities[5] (pickup): field \"key\" must be \"red\", \"blue\" or \"gold\", got \"green\""}));
    TEST_ASSERT(AnyMessageContains(errors, {"entities[6] (door): field \"lock\" must be", "\"purple\""}));
    TEST_ASSERT(errors.size() == 6);
    TEST_ASSERT(AnyMessageContains(warnings, {"entities[7] (enemy): field \"lock\" only applies to door"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"entities[8] (pickup): field \"key\" only applies to pickups of kind"}));
    TEST_ASSERT(
        AnyMessageContains(warnings, {"entities[9] (pickup): field \"value\" only applies to health and ammo"}));
    TEST_ASSERT(placements.size() == 4);  // the player, the runner and the two pickups with warnings only

    // The CSV export uses the same rules.
    errors.clear();
    TEST_ASSERT(!ParseEntityPlacementsCsv("player,1.5,1.5\nenemy,2,2,,,,,,ninja\npickup,2,2,,,key,,,,teal\n", "v.csv",
                                          placements, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"v.csv:2 (entity 1) (enemy): field \"variant\" must be"}));
    TEST_ASSERT(AnyMessageContains(errors, {"v.csv:3 (entity 2) (pickup): field \"key\" must be", "\"teal\""}));

    // An unknown variant fails the level load in the engine.
    const ScratchDir scratch("variant-validation");
    const std::filesystem::path root = scratch.Path() / "content";
    WriteContentRoot(root, "{}",
                     "{\"entities\": [{\"type\": \"player\", \"x\": 2.5, \"y\": 2.5},"
                     "{\"type\": \"enemy\", \"x\": 6.5, \"y\": 2.5, \"variant\": \"dragon\"}]}");
    Engine engine;
    engine.contentRoot_ = root;
    TEST_ASSERT(!engine.LoadLevel("game/maps/level01.map", "game/defs/level01.entities.json"));
}

// Places a door at (cellX, cellY) with `lock` (entity and map) and returns its id.
int AddDoor(EntityManager& manager, Map& map, int cellX, int cellY, KeyColor lock, bool open = false) {
    Entity& door = manager.CreateEntity(EntityType::Door,
                                        Vec2{static_cast<float>(cellX) + 0.5f, static_cast<float>(cellY) + 0.5f});
    door.mapX = cellX;
    door.mapY = cellY;
    door.doorLock = lock;
    door.doorOpen = open;
    map.SetCell(cellX, cellY, open ? TileType::Empty : TileType::Door);
    map.SetDoorLock(cellX, cellY, lock);
    return door.id;
}

void TestLockedDoorsAndKeys() {
    Map map(10, 6);
    BuildBorderWalls(map);
    EntityManager manager{};
    const int redDoor = AddDoor(manager, map, 5, 2, KeyColor::Red);
    const int blueDoor = AddDoor(manager, map, 5, 4, KeyColor::Blue);
    Player player{};
    player.position = {4.5f, 2.5f};
    int health = 100;
    int ammo = 10;
    KeyRing keys{};

    // Without the key: the door stays shut and says which key it needs.
    GameplayEvents locked{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true, keys, locked);
    TEST_ASSERT(locked.doorNeedsKey == KeyColor::Red && !locked.doorToggled && !locked.exitActivated);
    TEST_ASSERT(!manager.GetEntityById(redDoor)->doorOpen && map.GetCell(5, 2) == TileType::Door);

    // Walking over the red key card picks it up.
    Entity& card = manager.CreateEntity(EntityType::Pickup, Vec2{4.6f, 2.5f});
    card.pickupType = PickupType::Key;
    card.keyColor = KeyColor::Red;
    GameplayEvents pickup{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, false, keys, pickup);
    TEST_ASSERT(pickup.pickupCollected && pickup.pickupType == PickupType::Key && pickup.pickupKey == KeyColor::Red);
    TEST_ASSERT(keys.Has(KeyColor::Red) && !keys.Has(KeyColor::Blue) && !keys.Has(KeyColor::Gold));
    TEST_ASSERT(health == 100 && ammo == 10);

    // With the key it opens (and closes again like any door).
    GameplayEvents open{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true, keys, open);
    TEST_ASSERT(open.doorToggled && open.doorOpened && open.doorNeedsKey == KeyColor::None);
    TEST_ASSERT(manager.GetEntityById(redDoor)->doorOpen && map.GetCell(5, 2) == TileType::Empty);

    // The red key does not open the blue door.
    player.position = {4.5f, 4.5f};
    GameplayEvents wrongKey{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true, keys,
                             wrongKey);
    TEST_ASSERT(wrongKey.doorNeedsKey == KeyColor::Blue && !manager.GetEntityById(blueDoor)->doorOpen);
    // Facing an exit switch in reach, Interact uses the switch, not a door that is also in reach.
    map.SetCell(3, 4, TileType::Exit);
    player.directionAngleRadians = kPi;
    GameplayEvents exitPress{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true, keys,
                             exitPress);
    TEST_ASSERT(exitPress.exitActivated && exitPress.doorNeedsKey == KeyColor::None && !exitPress.doorToggled);
    map.SetCell(3, 4, TileType::Empty);
    // Callers without keys (the old signature) never open locked doors.
    GameplayEvents noKeys{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true, noKeys);
    TEST_ASSERT(noKeys.doorNeedsKey == KeyColor::Blue);

    // Through the engine (reference level 2): "NEED BLUE KEY", the key card, the door opens;
    // keys are cleared when the level restarts. Level 2 has an unlocked door at (5, 3), the blue
    // locked door at (9, 3) and the blue key card at (7.5, 5.3).
    constexpr int kBlueDoorX = 9;
    constexpr int kBlueDoorY = 3;
    const ScratchDir referenceContent("locked-doors-reference");
    Engine engine;
    const EngineOptions options = ReferenceEngineOptions(referenceContent, 2);
    TEST_ASSERT(engine.Init(options));
    TEST_ASSERT(engine.map_.GetCell(kBlueDoorX, kBlueDoorY) == TileType::Door &&
                engine.map_.GetDoorLock(kBlueDoorX, kBlueDoorY) == KeyColor::Blue);
    TEST_ASSERT(engine.map_.GetDoorLock(5, 3) == KeyColor::None);
    for (Entity& entity : engine.entityManager_.Entities()) {
        if (entity.type == EntityType::Enemy) {
            entity.enemyState = EnemyState::Dead;  // keep the test about doors
            entity.active = false;
        }
    }
    bool interact = false;
    engine.SetInputInjector([&interact](InputState& input) { input.SetActionState(InputAction::Interact, interact); });
    const auto press = [&] {
        interact = false;
        engine.Update(kFrameSeconds);
        interact = true;
        engine.Update(kFrameSeconds);
        interact = false;
    };
    engine.player_.position = {8.6f, 3.5f};  // within 1.05 cells of the door's centre
    engine.player_.directionAngleRadians = 0.0f;
    press();
    TEST_ASSERT(engine.hudMessage_ == "NEED BLUE KEY");
    TEST_ASSERT(engine.map_.GetCell(kBlueDoorX, kBlueDoorY) == TileType::Door);
    engine.player_.position = {7.5f, 5.3f};  // the blue key card
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.keys_.Has(KeyColor::Blue));
    TEST_ASSERT(engine.hudMessage_ == "BLUE KEY CARD");
    engine.player_.position = {8.6f, 3.5f};
    press();
    TEST_ASSERT(engine.map_.GetCell(kBlueDoorX, kBlueDoorY) == TileType::Empty);
    engine.ResetLevel();
    TEST_ASSERT(!engine.keys_.Has(KeyColor::Blue));
    TEST_ASSERT(engine.map_.GetCell(kBlueDoorX, kBlueDoorY) == TileType::Door);
}

void TestDoorCloseBlocked() {
    Map map(10, 6);
    BuildBorderWalls(map);
    EntityManager manager{};
    const int doorId = AddDoor(manager, map, 5, 2, KeyColor::None, true);
    Player player{};
    player.position = {4.6f, 2.5f};  // within reach of the door, not in its cell
    int health = 100;
    int ammo = 10;
    TEST_ASSERT(!IsDoorwayOccupied(manager, player, 5, 2));

    // A living enemy in the doorway: the door does not close.
    const int enemyId = manager.CreateEntity(EntityType::Enemy, Vec2{5.5f, 2.5f}).id;
    TEST_ASSERT(IsDoorwayOccupied(manager, player, 5, 2));
    GameplayEvents blocked{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true, blocked);
    TEST_ASSERT(blocked.doorBlocked && !blocked.doorToggled);
    TEST_ASSERT(manager.GetEntityById(doorId)->doorOpen && map.GetCell(5, 2) == TileType::Empty);
    // Only touching the cell counts: an enemy half outside still blocks.
    manager.GetEntityById(enemyId)->position = {6.2f, 2.5f};
    TEST_ASSERT(IsDoorwayOccupied(manager, player, 5, 2));
    manager.GetEntityById(enemyId)->position = {6.3f, 2.5f};
    TEST_ASSERT(!IsDoorwayOccupied(manager, player, 5, 2));

    // The same enemy dead (a corpse) does not block: the door closes over it.
    manager.GetEntityById(enemyId)->position = {5.5f, 2.5f};
    manager.GetEntityById(enemyId)->enemyState = EnemyState::Dead;
    manager.GetEntityById(enemyId)->active = false;
    GameplayEvents closed{};
    UpdateEntityInteractions(manager, map, player, health, ammo, kPlayerMaxHealth, kPlayerMaxAmmo, true, closed);
    TEST_ASSERT(closed.doorToggled && !closed.doorOpened && !closed.doorBlocked);
    TEST_ASSERT(map.GetCell(5, 2) == TileType::Door);

    // Through the engine: the HUD says BLOCKED (reference level 1's door, a guard in the doorway).
    const ScratchDir referenceContent("door-blocked-reference");
    Engine engine;
    const EngineOptions options = ReferenceEngineOptions(referenceContent, 1);
    TEST_ASSERT(engine.Init(options));
    bool interact = false;
    engine.SetInputInjector([&interact](InputState& input) { input.SetActionState(InputAction::Interact, interact); });
    engine.player_.position = {7.6f, 7.5f};  // west of the door, facing it
    engine.player_.directionAngleRadians = 0.0f;
    interact = true;
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.map_.GetCell(ReferenceLevel1::kDoorX, ReferenceLevel1::kDoorY) == TileType::Empty);  // opened
    Entity* guard = FindFirstEntity(engine.entityManager_, EntityType::Enemy);
    TEST_ASSERT(guard != nullptr);
    guard->position = {ReferenceLevel1::kDoorX + 0.5f, ReferenceLevel1::kDoorY + 0.5f};  // in the doorway
    guard->enemyState = EnemyState::Alert;  // standing still for its reaction time
    guard->stateTimer = 10.0f;
    interact = false;
    engine.Update(kFrameSeconds);
    interact = true;
    engine.Update(kFrameSeconds);
    TEST_ASSERT(engine.hudMessage_ == "BLOCKED");
    TEST_ASSERT(engine.map_.GetCell(ReferenceLevel1::kDoorX, ReferenceLevel1::kDoorY) == TileType::Empty);
}

void TestCorpseAndHitFlash() {
    Map map(12, 6);
    BuildBorderWalls(map);
    EntityManager manager{};
    std::vector<SpriteEntity> sprites;
    Player player{};
    player.position = {2.5f, 2.5f};
    player.directionAngleRadians = 0.0f;
    const int frontId = manager.CreateEntity(EntityType::Enemy, Vec2{5.5f, 2.5f}).id;
    const int backId = manager.CreateEntity(EntityType::Enemy, Vec2{8.5f, 2.5f}).id;
    manager.GetEntityById(frontId)->health = 30;
    SyncSpritesWithEntities(manager, sprites);

    // A hit: the enemy flashes for a moment and starts chasing.
    int ammo = 10;
    GameplayEvents hit{};
    PerformPlayerAttack(manager, map, player, ammo, 20, 12.0f, hit);
    TEST_ASSERT(hit.enemyDamaged && hit.damagedEnemyId == frontId && hit.enemyAlerted);
    TEST_ASSERT(manager.GetEntityById(frontId)->hitFlashTimer > 0.0f);
    SyncSpritesWithEntities(manager, sprites);
    TEST_ASSERT(FindSpriteFor(sprites, frontId)->flash && !FindSpriteFor(sprites, backId)->flash);

    // The flash brightens the sprite on screen.
    Texture plain(4, 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            plain.SetPixel(x, y, Framebuffer::PackRGBA(120, 40, 40));
        }
    }
    SpriteTextureSet set{};
    set.slots.fill(&plain);
    set.missing = &plain;
    const auto centrePixel = [&](bool flash) {
        Framebuffer framebuffer(64, 48);
        framebuffer.Clear(0xFF000000u);
        const std::vector<SpriteEntity> one{SpriteEntity{{4.5f, 2.5f}, 0, -1, SpriteType::Enemy, true, true, flash}};
        SpriteRenderer::Render(map, player, framebuffer, std::vector<float>(64, 100.0f), one, set);
        return PixelAt(framebuffer, 32, 24);
    };
    const std::uint32_t normal = centrePixel(false);
    const std::uint32_t flashed = centrePixel(true);
    TEST_ASSERT(normal != 0xFF000000u);
    TEST_ASSERT(GreenOf(flashed) > GreenOf(normal) + 50);  // green channel: 40 -> much brighter

    // The flash wears off.
    int health = 100;
    GameplayEvents events{};
    UpdateEnemyAi(manager, map, player, health, kEnemyHitFlashSeconds + 0.01f, events);
    SyncSpritesWithEntities(manager, sprites);
    TEST_ASSERT(!FindSpriteFor(sprites, frontId)->flash);

    // Killed: a corpse that stays visible with its corpse texture, inactive for the game.
    manager.GetEntityById(frontId)->position = {5.5f, 2.5f};
    GameplayEvents kill{};
    PerformPlayerAttack(manager, map, player, ammo, 20, 12.0f, kill);
    TEST_ASSERT(kill.enemyDied && kill.deadEnemyId == frontId);
    const Entity* corpse = manager.GetEntityById(frontId);
    TEST_ASSERT(IsEnemyCorpse(*corpse) && !IsEnemyAlive(*corpse) && !corpse->active && corpse->visible);
    SyncSpritesWithEntities(manager, sprites);
    const SpriteEntity* corpseSprite = FindSpriteFor(sprites, frontId);
    TEST_ASSERT(corpseSprite != nullptr && corpseSprite->visible && corpseSprite->active);
    TEST_ASSERT(corpseSprite->type == SpriteType::Corpse);
    TEST_ASSERT(corpseSprite->textureId == static_cast<int>(SpriteTextureId::GuardDead));
    TEST_ASSERT(!SpriteRenderer::BuildSortedSpriteIndices(player, sprites).empty());

    // The corpse blocks nothing: the next shot along the same line hits the enemy behind it, a
    // living enemy can stand on it (no separation push), and it does not occupy a doorway.
    GameplayEvents through{};
    PerformPlayerAttack(manager, map, player, ammo, 20, 12.0f, through);
    TEST_ASSERT(through.enemyDamaged && through.damagedEnemyId == backId);
    manager.GetEntityById(backId)->position = {5.5f, 2.5f};
    SeparateEnemies(manager, map);
    TEST_ASSERT_NEAR(manager.GetEntityById(backId)->position.x, 5.5f, 1e-6f);
    manager.GetEntityById(backId)->position = {9.5f, 4.5f};
    TEST_ASSERT(!IsDoorwayOccupied(manager, player, 5, 2));
    std::vector<Vec2i> path;
    TEST_ASSERT(FindPath(map, {9, 4}, {2, 2}, path));  // paths cross the corpse's cell

    // Every enemy type has its own corpse texture.
    EntityManager single{};
    Entity& runner = single.CreateEntity(EntityType::Enemy, Vec2{3.0f, 3.0f});
    runner.variant = EnemyVariant::Runner;
    runner.enemyState = EnemyState::Dead;
    runner.active = false;
    std::vector<SpriteEntity> runnerSprites;
    SyncSpritesWithEntities(single, runnerSprites);
    TEST_ASSERT(runnerSprites.size() == 1 &&
                runnerSprites[0].textureId == static_cast<int>(SpriteTextureId::RunnerDead));
    single.Entities()[0].variant = EnemyVariant::Brute;
    SyncSpritesWithEntities(single, runnerSprites);
    TEST_ASSERT(runnerSprites[0].textureId == static_cast<int>(SpriteTextureId::BruteDead));
}

void TestEnemySeparation() {
    Map map(12, 8);
    BuildBorderWalls(map);
    EntityManager manager{};
    // Two enemies on the same spot, and two squeezed into a corner.
    const int a = manager.CreateEntity(EntityType::Enemy, Vec2{5.5f, 4.5f}).id;
    const int b = manager.CreateEntity(EntityType::Enemy, Vec2{5.5f, 4.5f}).id;
    const int c = manager.CreateEntity(EntityType::Enemy, Vec2{1.3f, 1.3f}).id;
    const int d = manager.CreateEntity(EntityType::Enemy, Vec2{1.35f, 1.3f}).id;
    for (int i = 0; i < 20; ++i) {
        SeparateEnemies(manager, map);
    }
    const auto gap = [&](int first, int second) {
        return Length(manager.GetEntityById(first)->position - manager.GetEntityById(second)->position);
    };
    TEST_ASSERT(gap(a, b) >= 2.0f * kEnemyRadius - 0.01f);
    TEST_ASSERT(gap(c, d) >= 2.0f * kEnemyRadius - 0.05f);
    for (const int id : {a, b, c, d}) {
        TEST_ASSERT(!IsBlockedByWall(map, manager.GetEntityById(id)->position, kEnemyRadius - 0.01f));
    }

    // Six enemies chasing the same player crowd round without overlapping.
    EntityManager crowd{};
    Player player{};
    player.position = {9.5f, 4.5f};
    for (int i = 0; i < 6; ++i) {
        Entity& enemy = crowd.CreateEntity(EntityType::Enemy, Vec2{2.5f, 1.5f + static_cast<float>(i)});
        enemy.enemyState = EnemyState::Chase;
        enemy.moveSpeed = 2.5f;
        enemy.damage = 0;
    }
    int health = 100;
    for (int frame = 0; frame < 60 * 5; ++frame) {
        GameplayEvents events{};
        UpdateEnemyAi(crowd, map, player, health, kAiFrameSeconds, events);
    }
    const std::vector<Entity>& all = crowd.Entities();
    for (std::size_t i = 0; i < all.size(); ++i) {
        TEST_ASSERT(!IsBlockedByWall(map, all[i].position, kEnemyRadius - 0.01f));
        TEST_ASSERT(Length(all[i].position - player.position) < 2.5f);  // they all got close
        for (std::size_t j = i + 1; j < all.size(); ++j) {
            TEST_ASSERT(Length(all[i].position - all[j].position) >= 2.0f * kEnemyRadius - 0.08f);
        }
    }
}

void TestHudKeyIconsLayout() {
    const int width = 320;
    const int height = 240;
    const int barY = height - UiLayout::kMargin - UiLayout::kHudBarHeight;
    // UiRenderer::DrawHud: the ammo label starts at kHudAmmoX; the level label ends one text
    // inset before the bar's right end.
    const int ammoTextEnd = UiLayout::kHudAmmoX + UiRenderer::TextWidth("AMMO 10000", 1);
    const int levelLabelStart = width - UiLayout::kMargin - UiLayout::kTextInset - UiRenderer::TextWidth("LEVEL 99", 1);
    int previousRight = ammoTextEnd;
    for (const KeyColor color : kKeyColors) {
        const UiRenderer::UiRect card = UiRenderer::KeyCardRect(width, height, color);
        // Inside the bar (its 1-pixel border excluded), right of the longest ammo text and of the
        // previous card, left of the level label.
        TEST_ASSERT(card.y > barY && card.y + card.height < barY + UiLayout::kHudBarHeight - 1);
        TEST_ASSERT(card.x > previousRight && card.x + card.width < levelLabelStart);
        previousRight = card.x + card.width;
    }

    // Every combination of held keys: inside the 4-pixel margin, each held key drawn in its
    // colour, no card for a key that is not held.
    constexpr unsigned long kKeyCombinations = 8;  // every subset of the three key colours
    for (unsigned long mask = 0; mask < kKeyCombinations; ++mask) {
        const std::bitset<kKeyColorCount> heldKeys(mask);
        Framebuffer framebuffer(width, height);
        UiRenderer::HudView view{};
        view.health = 10000;
        view.ammo = 10000;
        view.levelNumber = 99;
        view.levelName = "KEYS";
        view.message = "NEED GOLD KEY";
        for (int i = 0; i < kKeyColorCount; ++i) {
            if (heldKeys.test(static_cast<std::size_t>(i))) {
                view.keys.Add(kKeyColors[i]);
            }
        }
        framebuffer.ResetDrawBounds();
        UiRenderer::DrawHud(framebuffer, view);
        AssertInsideMargin(framebuffer, "HUD with key mask " + std::to_string(mask));
        for (int i = 0; i < kKeyColorCount; ++i) {
            const UiRenderer::UiRect card = UiRenderer::KeyCardRect(width, height, kKeyColors[i]);
            const std::uint32_t inside = PixelAt(framebuffer, card.x + 1, card.y + 1);
            const bool held = heldKeys.test(static_cast<std::size_t>(i));
            TEST_ASSERT((inside == UiRenderer::KeyCardColor(kKeyColors[i])) == held);
        }
    }
    // The three key colours differ (and each card also carries its initial).
    TEST_ASSERT(UiRenderer::KeyCardColor(KeyColor::Red) != UiRenderer::KeyCardColor(KeyColor::Blue));
    TEST_ASSERT(UiRenderer::KeyCardColor(KeyColor::Blue) != UiRenderer::KeyCardColor(KeyColor::Gold));
    TEST_ASSERT(UiRenderer::KeyCardColor(KeyColor::Gold) != UiRenderer::KeyCardColor(KeyColor::Red));

    // In the engine: the key preview holds all three keys and renders inside the layout.
    Engine engine;
    EngineOptions options = ShippedEngineOptions();
    options.preview = PreviewScreen::Items;
    TEST_ASSERT(engine.Init(options));
    TEST_ASSERT(engine.keys_.Has(KeyColor::Red) && engine.keys_.Has(KeyColor::Blue) &&
                engine.keys_.Has(KeyColor::Gold));
    engine.Render();
    AssertInsideMargin(engine.framebuffer_, "items preview");
    // Inside the card's border, which is one UI pixel (UI scale framebuffer pixels) wide.
    const int scale = UiLayout::UiScale(engine.framebuffer_.Width(), engine.framebuffer_.Height());
    const UiRenderer::UiRect gold =
        UiRenderer::KeyCardRect(engine.framebuffer_.Width(), engine.framebuffer_.Height(), KeyColor::Gold);
    TEST_ASSERT(PixelAt(engine.framebuffer_, gold.x + scale, gold.y + scale) ==
                UiRenderer::KeyCardColor(KeyColor::Gold));
}

void TestAiStress20Enemies() {
    // Performance sanity check: 20 enemies chasing in a 48x48 map with pillars, 600 frames of
    // Engine::Update + Engine::Render (headless, the real renderer at the default render size).
    const ScratchDir scratch("ai-stress");
    const std::filesystem::path root = scratch.Path() / "content";
    std::string mapText;
    constexpr int kSize = 48;
    for (int y = 0; y < kSize; ++y) {
        for (int x = 0; x < kSize; ++x) {
            const bool border = x == 0 || y == 0 || x == kSize - 1 || y == kSize - 1;
            const bool pillar = x % 6 == 3 && y % 6 == 3;
            const bool wall = x == 24 && y > 4 && y < 40;  // a long wall to path around
            mapText += border || pillar || wall ? '#' : '.';
        }
        mapText += '\n';
    }
    std::string entities = "{\"entities\": [{\"type\": \"player\", \"x\": 12.5, \"y\": 24.5, \"angle\": 0}";
    const std::array<const char*, 3> variants = {"guard", "brute", "runner"};
    for (int i = 0; i < 20; ++i) {
        const int column = i % 5;  // five enemies per row, rows 9 cells apart
        const int row = i / 5;
        entities += ",{\"type\": \"enemy\", \"x\": " + std::to_string(30.5 + column * 3) +
                    ", \"y\": " + std::to_string(8.5 + row * 9) + ", \"variant\": \"" +
                    variants[static_cast<std::size_t>(i % 3)] + "\"}";
    }
    entities += "]}";
    MakeContentRoot(root);
    WriteTextFile(root / "game" / "maps" / "stress.map", mapText);
    WriteTextFile(root / "game" / "defs" / "stress.entities.json", entities);

    Engine engine;
    TEST_ASSERT(engine.Init(ShippedEngineOptions(1)));
    engine.contentRoot_ = root;
    TEST_ASSERT(engine.LoadLevel("game/maps/stress.map", "game/defs/stress.entities.json"));
    engine.SetState(GameState::Gameplay);
    engine.playerHealth_ = 1000000;  // survive 10 s of 20 attackers
    for (Entity& entity : engine.entityManager_.Entities()) {
        if (entity.type == EntityType::Enemy) {
            entity.enemyState = EnemyState::Chase;  // everyone hunts the player at once
            entity.lastKnownPlayerPosition = engine.player_.position;
        }
    }
    int frame = 0;
    engine.SetInputInjector([&frame](InputState& input) {
        input.SetActionState(InputAction::TurnLeft, (frame / 90) % 2 == 0);
        input.SetActionState(InputAction::StrafeLeft, (frame / 120) % 2 == 0);
        input.SetActionState(InputAction::Attack, frame % 20 == 0);
    });

    double totalMs = 0.0;
    double worstMs = 0.0;
    constexpr int kFrames = 600;
    for (frame = 0; frame < kFrames; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        engine.Update(kFrameSeconds);
        engine.Render();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        totalMs += ms;
        worstMs = std::max(worstMs, ms);
    }
    TEST_ASSERT(engine.GetState() == GameState::Gameplay);
    int engaged = 0;
    for (const Entity& entity : engine.entityManager_.Entities()) {
        if (!IsEnemyAlive(entity)) {
            continue;
        }
        TEST_ASSERT(!IsBlockedByWall(engine.map_, entity.position, kEnemyRadius - 0.01f));
        engaged += entity.enemyState == EnemyState::Attack || entity.enemyState == EnemyState::Chase ? 1 : 0;
    }
    TEST_ASSERT(engaged > 0);
    const double averageMs = totalMs / kFrames;
    std::cout << "    ai_stress_20_enemies: 48x48 map, 20 enemies, " << kFrames << " frames: update+render average "
              << averageMs << " ms, worst " << worstMs << " ms per frame\n";
#ifdef NDEBUG
    TEST_ASSERT(averageMs < 8.0);  // half a 60 Hz frame (optimised builds)
#else
    TEST_ASSERT(averageMs < 100.0);  // unoptimised builds: only catch pathological slowdowns
#endif
}

// --- Phase 7: autoplay bot, C15, C18, loader parity -------------------------------------------

struct FixtureLevel {
    std::string id;
    std::string map;
    std::string entities;
};

// A small content root: gameplay.json, an empty theme (textures fall back to the checkerboard)
// and the given campaign levels.
void WriteAutoplayFixture(const std::filesystem::path& root, const std::vector<FixtureLevel>& levels,
                          const std::string& gameplayJson = "{}") {
    MakeContentRoot(root);
    WriteTextFile(root / "game" / "defs" / "gameplay.json", gameplayJson);
    WriteTextFile(root / "game" / "defs" / "themes" / "fixture.cfg", "# no textures: checkerboards\n");
    std::string campaign = "{ \"title\": \"Fixture\", \"levels\": [";
    for (std::size_t i = 0; i < levels.size(); ++i) {
        const FixtureLevel& level = levels[i];
        WriteTextFile(root / "game" / "maps" / (level.id + ".map"), level.map);
        WriteTextFile(root / "game" / "defs" / (level.id + ".entities.json"), level.entities);
        campaign += std::string(i == 0 ? "" : ",") + "{ \"id\": \"" + level.id + "\", \"name\": \"" + level.id +
                    "\", \"map\": \"game/maps/" + level.id + ".map\", \"entities\": \"game/defs/" + level.id +
                    ".entities.json\", \"theme\": \"fixture\" }";
    }
    WriteTextFile(root / "game" / "defs" / "campaign.json", campaign + "] }\n");
}

// Spawn room | unlocked door | room with the blue key and a guard; a blue locked door leads
// down to the corridor with the exit switch. A health pickup by the spawn: the bot starts with
// 30 health, at or below its low-health threshold, so it fetches the pickup first.
const char* const kAutoplayFixtureMap =
    "##########\n"
    "#....#...#\n"
    "#....D...#\n"
    "#....#...#\n"
    "###D######\n"
    "#........X\n"
    "##########\n";
const char* const kAutoplayFixtureEntities = R"({ "entities": [
  { "type": "player", "x": 1.5, "y": 1.5, "angle": 0 },
  { "type": "door", "x": 5.5, "y": 2.5, "mapX": 5, "mapY": 2 },
  { "type": "door", "x": 3.5, "y": 4.5, "mapX": 3, "mapY": 4, "lock": "blue" },
  { "type": "pickup", "x": 7.5, "y": 1.5, "kind": "key", "key": "blue" },
  { "type": "enemy", "x": 7.5, "y": 3.5, "variant": "guard" },
  { "type": "pickup", "x": 2.5, "y": 3.5, "kind": "health" }
] })";

void TestAutoplayFixtureLevel() {
    const ScratchDir scratch("autoplay-fixture");
    const std::filesystem::path root = scratch.Path() / "content";
    // The fixture's gameplay.json only lowers the starting health; everything else is a default.
    constexpr int kStartHealth = 30;
    WriteAutoplayFixture(root, {{"keys", kAutoplayFixtureMap, kAutoplayFixtureEntities}},
                         R"({ "player": { "initialHealth": )" + std::to_string(kStartHealth) + " } }");
    AutoplayRunOptions options{};
    options.dataDir = PathToUtf8(root);
    options.prefDir = scratch.Path() / "prefs";
    options.level = 1;
    const AutoplayRunReport first = RunAutoplay(options);
    if (!first.Success()) {
        FailTest(__FILE__, __LINE__, "the bot did not complete the fixture level:\n" + FormatAutoplayTable(first));
    }
    TEST_ASSERT(first.levels.size() == 1 && !first.wholeCampaign && !first.carryOverChecked);
    const AutoplayLevelReport& level = first.levels[0];
    TEST_ASSERT(level.result == AutoplayResult::Complete && level.reason.empty());
    TEST_ASSERT(level.id == "keys" && level.levelNumber == 1);
    TEST_ASSERT(level.healthStart == kStartHealth);
    TEST_ASSERT(level.keysCollected == std::vector<std::string>{"blue"});
    TEST_ASSERT(level.keysUsed == std::vector<std::string>{"blue"});
    TEST_ASSERT(level.kills == 1 && level.totalEnemies == 1);
    TEST_ASSERT(level.pickups == 2 && level.totalPickups == 2);  // the health detour and the key
    TEST_ASSERT(level.doorsOpened >= 2);
    TEST_ASSERT(level.shotsFired >= 2 &&
                level.shotsFired == kDefaultWeaponDefinition.initialAmmo - level.ammoEnd);  // a guard takes 2 hits
    TEST_ASSERT(level.healthEnd ==
                std::min(kDefaultPlayerDefinition.maxHealth, kStartHealth + kDefaultPickupDefinitions.health.value) -
                    level.damageTaken);
    TEST_ASSERT(level.frames > 0 && level.gameSeconds > 1.0);
    TEST_ASSERT_NEAR(level.gameSeconds, static_cast<double>(level.frames) / 60.0, 1e-6);

    // Deterministic: the same content gives the same run, frame for frame, and the same JSON.
    const AutoplayRunReport second = RunAutoplay(options);
    TEST_ASSERT(FormatAutoplayJson(first) == FormatAutoplayJson(second));
    TEST_ASSERT(second.levels[0].frames == level.frames);

    // A time limit shorter than the walk: result "timeout".
    options.config.timeLimitSeconds = 1.0;
    const AutoplayRunReport timedOut = RunAutoplay(options);
    TEST_ASSERT(!timedOut.Success() && timedOut.levels.size() == 1);
    TEST_ASSERT(timedOut.levels[0].result == AutoplayResult::Timeout);
    TEST_ASSERT(timedOut.levels[0].reason.find("time limit of 1.0 s") != std::string::npos);
}

void TestAutoplayUnwinnableLevels() {
    // The committed fixture (also checked by the DevKit validator): the exit is walled off.
    AutoplayRunOptions options{};
    const ScratchDir scratch("autoplay-unwinnable");
    options.dataDir = PathToUtf8(RepoRoot() / "tests" / "fixtures" / "unwinnable");
    options.prefDir = scratch.Path() / "prefs";
    options.level = 1;
    const AutoplayRunReport walled = RunAutoplay(options);
    TEST_ASSERT(walled.error.empty() && !walled.Success() && walled.levels.size() == 1);
    TEST_ASSERT(walled.levels[0].result == AutoplayResult::Stuck);
    TEST_ASSERT(walled.levels[0].reason.find("no route to an exit switch") != std::string::npos);
    TEST_ASSERT(walled.levels[0].frames < 10);  // it gives up at once, not after the time limit

    // The exit behind a red locked door and no red key: the reason names the door.
    const std::filesystem::path root = scratch.Path() / "content";
    WriteAutoplayFixture(root, {{"locked", "#######\n#..D..X\n#######\n",
                                 R"({ "entities": [ { "type": "player", "x": 1.5, "y": 1.5 },
                                     { "type": "door", "x": 3.5, "y": 1.5, "mapX": 3, "mapY": 1, "lock": "red" } ] })"}});
    options.dataDir = PathToUtf8(root);
    const AutoplayRunReport locked = RunAutoplay(options);
    TEST_ASSERT(locked.levels.size() == 1 && locked.levels[0].result == AutoplayResult::Stuck);
    TEST_ASSERT(locked.levels[0].reason.find("red door at (3, 1)") != std::string::npos);

    // --all stops at the first failure; the later levels are reported as not played.
    WriteAutoplayFixture(
        root, {{"locked", "#######\n#..D..X\n#######\n",
                R"({ "entities": [ { "type": "player", "x": 1.5, "y": 1.5 },
                                     { "type": "door", "x": 3.5, "y": 1.5, "mapX": 3, "mapY": 1, "lock": "red" } ] })"},
               {"open", "#####\n#..X#\n#####\n", R"({ "entities": [ { "type": "player", "x": 1.5, "y": 1.5 } ] })"}});
    options.level = 0;
    const AutoplayRunReport campaign = RunAutoplay(options);
    TEST_ASSERT(!campaign.Success() && campaign.levels.size() == 2 && !campaign.campaignCompleted);
    TEST_ASSERT(campaign.levels[0].result == AutoplayResult::Stuck);
    TEST_ASSERT(campaign.levels[1].result == AutoplayResult::NotPlayed && campaign.levels[1].id == "open");

    // A content root the engine cannot start with: an error, not a crash.
    options.dataDir = PathToUtf8(scratch.Path() / "nothing-here");
    const AutoplayRunReport broken = RunAutoplay(options);
    TEST_ASSERT(!broken.Success() && !broken.error.empty() && broken.levels.empty());
    TEST_ASSERT(FormatAutoplayTable(broken).find("ERROR: the engine did not start") != std::string::npos);
}

void TestAutoplayCommandLine() {
    using Action = AutoplayCommandLine::Action;
    const AutoplayCommandLine level = ParseAutoplayCommandLine({"--level", "2", "--data", "/tmp/x"});
    TEST_ASSERT(level.action == Action::Run && level.level == 2 && !level.all && level.dataDir == "/tmp/x");
    TEST_ASSERT_NEAR(level.timeLimitSeconds, AutoplayConfig{}.timeLimitSeconds, 1e-9);
    const AutoplayCommandLine all = ParseAutoplayCommandLine(
        {"--all", "--json", "out/report.json", "--time-limit", "60.5", "--pref-dir", "p", "--verbose"});
    TEST_ASSERT(all.action == Action::Run && all.all && all.level == 0 && all.jsonPath == "out/report.json");
    TEST_ASSERT(all.prefDir == "p" && all.verbose);
    TEST_ASSERT_NEAR(all.timeLimitSeconds, 60.5, 1e-9);
    TEST_ASSERT(ParseAutoplayCommandLine({"--help"}).action == Action::ShowHelp);
    TEST_ASSERT(ParseAutoplayCommandLine({"--all", "--version"}).action == Action::ShowVersion);
    const std::vector<std::vector<std::string>> bad = {
        {},
        {"--level"},
        {"--level", "0"},
        {"--level", "100"},
        {"--level", "two"},
        {"--level", "1", "--level", "2"},
        {"--all", "--level", "1"},
        {"--all", "--time-limit", "0"},
        {"--all", "--time-limit", "-5"},
        {"--all", "--time-limit", "soon"},
        {"--all", "--json", ""},
        {"--all", "--frobnicate"},
    };
    for (const std::vector<std::string>& args : bad) {
        const AutoplayCommandLine parsed = ParseAutoplayCommandLine(args);
        TEST_ASSERT(parsed.action == Action::Error && !parsed.error.empty());
    }
    TEST_ASSERT(AutoplayUsage("neo_wolf_autoplay").find("Exit codes: 0") != std::string::npos);

    // The JSON report has the documented fields.
    AutoplayRunReport report{};
    report.campaignTitle = "T";
    report.campaignLevels = 1;
    report.fixedStepSeconds = kFrameSeconds;
    report.timeLimitSeconds = 300.0;
    AutoplayLevelReport entry{};
    entry.levelNumber = 1;
    entry.id = "level01";
    entry.result = AutoplayResult::Complete;
    entry.gameSeconds = 6.8833333;
    entry.keysUsed = {"blue"};
    report.levels.push_back(entry);
    const nlohmann::json json = nlohmann::json::parse(FormatAutoplayJson(report));
    TEST_ASSERT(json["success"] == true && json["mode"] == "level");
    TEST_ASSERT(json["levels"][0]["result"] == "complete" && json["levels"][0]["gameSeconds"] == 6.883);
    TEST_ASSERT(json["levels"][0]["keysUsed"][0] == "blue" && json["carryOver"]["checked"] == false);
    for (const char* key : {"frames", "damageTaken", "healthEnd", "ammoEnd", "kills", "totalEnemies", "pickups",
                            "totalPickups", "keysCollected", "shotsFired", "doorsOpened", "replans", "reason"}) {
        TEST_ASSERT(json["levels"][0].contains(key));
    }
    TEST_ASSERT(FormatAutoplayTable(report).find("Result: PASS (1 of 1 level(s) completed)") != std::string::npos);
}

void TestPlayerEnemyCollision() {
    // Known issue C15: living enemies block the player; corpses do not.
    Map map(12, 6);
    BuildBorderWalls(map);
    const float radius = kDefaultPlayerDefinition.collisionRadius;
    const float contact = radius + kEnemyRadius;
    const std::vector<BodyObstacle> body{{Vec2{6.0f, 2.5f}, kEnemyRadius}};

    // Walking into an enemy stops at contact distance.
    const Vec2 position = MoveWithSlidingAndBodies(map, {2.5f, 2.5f}, {5.0f, 0.0f}, radius, body);
    TEST_ASSERT(position.x < 6.0f - contact + 1e-3f && position.x > 6.0f - contact - 0.06f);
    // Without bodies (or past a corpse) the same move goes all the way.
    TEST_ASSERT(MoveWithSlidingAndBodies(map, {2.5f, 2.5f}, {5.0f, 0.0f}, radius, {}).x > 7.4f);
    // Already overlapping (an enemy placed too close): backing away works, pushing in does not.
    const Vec2 overlapping{5.8f, 2.5f};
    TEST_ASSERT(MoveWithSlidingAndBodies(map, overlapping, {-0.5f, 0.0f}, radius, body).x < 5.35f);
    TEST_ASSERT_NEAR(MoveWithSlidingAndBodies(map, overlapping, {0.3f, 0.0f}, radius, body).x, 5.8f, 1e-6f);
    // A diagonal push slides past the body (the axis that is not blocked still moves).
    const Vec2 slid = MoveWithSlidingAndBodies(map, {5.5f, 2.5f}, {0.4f, 0.4f}, radius, body);
    TEST_ASSERT(slid.y > 2.85f && Length(slid - body[0].position) >= contact - 1e-3f);

    // The enemy side: an enemy pushed into the player is moved back out (the player stays).
    EntityManager manager{};
    const Entity& guard = manager.CreateEntity(EntityType::Enemy, Vec2{3.0f, 2.5f});
    Entity& corpse = manager.CreateEntity(EntityType::Enemy, Vec2{2.6f, 2.5f});
    corpse.enemyState = EnemyState::Dead;
    corpse.active = false;
    Player player{};
    player.position = {2.5f, 2.5f};
    player.collisionRadius = radius;
    SeparateEnemiesFromPlayer(manager, map, player);
    TEST_ASSERT(Length(guard.position - player.position) >= contact - 1e-4f);
    TEST_ASSERT_NEAR(corpse.position.x, 2.6f, 1e-6f);  // corpses are left alone
    TEST_ASSERT(LivingEnemyBodies(manager).size() == 1);

    // In the engine: walk the reference level 1's hall straight at its guard. The player never
    // gets closer than the two radii, the guard attacks; once it is dead the player walks over
    // its corpse.
    const ScratchDir prefs("c15-prefs");
    const ScratchDir referenceContent("c15-reference");
    Engine engine;
    EngineOptions options = ReferenceEngineOptions(referenceContent, 1);
    options.prefDir = prefs.Path();
    TEST_ASSERT(engine.Init(options));
    const Entity* enemy = FindFirstEntity(engine.entityManager_, EntityType::Enemy);
    TEST_ASSERT(enemy != nullptr);
    const int enemyId = enemy->id;
    engine.player_.directionAngleRadians = 0.0f;  // east, along row 2 towards the guard
    engine.SetInputInjector([](InputState& input) { input.SetActionState(InputAction::MoveForward, true); });
    float closest = 100.0f;
    for (int frame = 0; frame < 240; ++frame) {
        engine.Update(kFrameSeconds);
        closest =
            std::min(closest, Length(engine.entityManager_.GetEntityById(enemyId)->position - engine.player_.position));
    }
    TEST_ASSERT(closest >= contact - 0.01f);
    TEST_ASSERT(engine.player_.position.x < ReferenceLevel1::kGuardX);
    // Kill it the way a shot does; its corpse no longer blocks.
    Entity* dead = engine.entityManager_.GetEntityById(enemyId);
    dead->enemyState = EnemyState::Dead;
    dead->active = false;
    const float corpseX = dead->position.x;
    TEST_ASSERT(std::fabs(dead->position.y - engine.player_.position.y) < contact);  // still in the way
    for (int frame = 0; frame < 120 && engine.GetState() == GameState::Gameplay; ++frame) {
        engine.Update(kFrameSeconds);
    }
    if (!(engine.player_.position.x > corpseX + 0.3f)) {
        std::ostringstream out;
        out << "the player did not walk past the corpse at (" << corpseX << ", " << dead->position.y << "): player at ("
            << engine.player_.position.x << ", " << engine.player_.position.y << ")";
        FailTest(__FILE__, __LINE__, out.str());
    }
}

void TestSpriteVerticalCrop() {
    // A sprite taller than the screen (very close) is cropped at the top and bottom, not
    // squeezed (found while investigating known issue C18). Texture rows: red, green, blue, white.
    Texture texture(1, 4);
    texture.SetPixel(0, 0, Framebuffer::PackRGBA(255, 0, 0));
    texture.SetPixel(0, 1, Framebuffer::PackRGBA(0, 255, 0));
    texture.SetPixel(0, 2, Framebuffer::PackRGBA(0, 0, 255));
    texture.SetPixel(0, 3, Framebuffer::PackRGBA(255, 255, 255));
    SpriteTextureSet textures{};
    textures.Set(SpriteTextureId::Decoration, &texture);
    textures.missing = &texture;
    Map map(8, 8);
    Player player{};
    player.position = {2.0f, 4.0f};
    player.directionAngleRadians = 0.0f;
    constexpr int kWidth = 320;
    constexpr int kHeight = 240;
    Framebuffer framebuffer(kWidth, kHeight);
    const std::vector<float> depth(kWidth, 100.0f);
    const auto render = [&](float distance) {
        framebuffer.Clear(0);
        SpriteEntity sprite{};
        sprite.position = {2.0f + distance, 4.0f};
        sprite.textureId = static_cast<int>(SpriteTextureId::Decoration);
        SpriteRenderer::Render(map, player, framebuffer, depth, {sprite}, textures);
    };
    const int centreX = kWidth / 2;
    // 960 pixels tall: the screen shows texture rows 0.37..0.62, i.e. green at the top and blue
    // at the bottom (the old code squeezed the whole texture in: red at the top, white below).
    render(0.25f);
    const std::uint32_t top = PixelAt(framebuffer, centreX, 0);
    const std::uint32_t bottom = PixelAt(framebuffer, centreX, kHeight - 1);
    TEST_ASSERT(GreenOf(top) > 150 && RedOf(top) < 60);         // green
    TEST_ASSERT(BlueOf(bottom) > 150 && GreenOf(bottom) < 60);  // blue
    // A sprite that fits on screen still shows the whole texture, red on top and white below.
    // It is 120 pixels tall at this distance: rows 58 above and below the centre are just
    // inside its first and last texture rows.
    render(2.0f);
    const std::uint32_t nearTop = PixelAt(framebuffer, centreX, kHeight / 2 - 58);
    const std::uint32_t nearBottom = PixelAt(framebuffer, centreX, kHeight / 2 + 58);
    TEST_ASSERT(RedOf(nearTop) > 150 && GreenOf(nearTop) < 60);
    TEST_ASSERT(BlueOf(nearBottom) > 150 && GreenOf(nearBottom) > 150);
}

void TestContentParityFixtures() {
    // tests/fixtures/parity/: the same files go through the DevKit (tools/devkit/tests/
    // test_parity.py); both must match expected.json, so the two parsers agree.
    const std::filesystem::path dir = PathFromUtf8(ResolveRepoPath("tests/fixtures/parity"));
    const nlohmann::json expected = nlohmann::json::parse(ReadWholeFile(PathToUtf8(dir / "expected.json")));
    int checked = 0;
    for (const auto& [name, want] : expected["maps"].items()) {
        Map map{};
        std::vector<std::string> errors;
        const bool ok = LoadMapFromFile(PathToUtf8(dir / name), map, errors);
        if (ok != want["ok"].get<bool>()) {
            FailTest(__FILE__, __LINE__,
                     name + ": the engine " + (ok ? "accepts" : "rejects") + " it, expected.json says otherwise");
        }
        if (ok) {
            TEST_ASSERT(map.Width() == want["width"].get<int>() && map.Height() == want["height"].get<int>());
        }
        ++checked;
    }
    for (const auto& [name, want] : expected["entities"].items()) {
        std::vector<EntityPlacement> placements;
        std::vector<LightZone> lights;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        const bool ok = LoadEntityPlacementsFromFile(PathToUtf8(dir / name), placements, lights, errors, warnings);
        if (ok != want["ok"].get<bool>()) {
            FailTest(__FILE__, __LINE__,
                     name + ": the engine " + (ok ? "accepts" : "rejects") + " it, expected.json says otherwise");
        }
        if (ok) {
            std::vector<std::string> types;
            types.reserve(placements.size());
            for (const EntityPlacement& placement : placements) {
                types.emplace_back(PlacementTypeName(placement.type));
            }
            TEST_ASSERT(types == want["types"].get<std::vector<std::string>>());
            // Light zones [x, y, w, h, level], in order (none when the entry does not list them).
            const nlohmann::json wantLights = want.value("lights", nlohmann::json::array());
            TEST_ASSERT(lights.size() == wantLights.size());
            for (std::size_t i = 0; i < lights.size() && i < wantLights.size(); ++i) {
                TEST_ASSERT(lights[i].x == wantLights[i][0].get<int>() && lights[i].y == wantLights[i][1].get<int>());
                TEST_ASSERT(lights[i].width == wantLights[i][2].get<int>() &&
                            lights[i].height == wantLights[i][3].get<int>());
                TEST_ASSERT_NEAR(lights[i].level, wantLights[i][4].get<float>(), 1e-6f);
            }
        }
        ++checked;
    }
    // A map and an entity file together: the level builds or not (light zones inside the
    // map), and the light level of every cell afterwards.
    for (const auto& [name, want] : expected["levels"].items()) {
        Map map{};
        std::vector<EntityPlacement> placements;
        std::vector<LightZone> lights;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
        TEST_ASSERT(LoadMapFromFile(PathToUtf8(dir / want["map"].get<std::string>()), map, errors));
        TEST_ASSERT(LoadEntityPlacementsFromFile(PathToUtf8(dir / want["entities"].get<std::string>()), placements,
                                                 lights, errors, warnings));
        LevelContents level{};
        const bool ok = BuildLevelFromPlacements(map, placements, lights, GameplayDefinitions{}, level, errors);
        if (ok != want["ok"].get<bool>()) {
            FailTest(
                __FILE__, __LINE__,
                "levels." + name + ": the engine " + (ok ? "builds" : "rejects") + " it, expected.json says otherwise");
        }
        if (ok) {
            const nlohmann::json& rows = want["light"];
            TEST_ASSERT(static_cast<int>(rows.size()) == map.Height());
            for (int y = 0; y < map.Height() && y < static_cast<int>(rows.size()); ++y) {
                TEST_ASSERT(static_cast<int>(rows[static_cast<std::size_t>(y)].size()) == map.Width());
                for (int x = 0; x < map.Width(); ++x) {
                    TEST_ASSERT_NEAR(map.GetLightLevel(x, y),
                                     rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)].get<float>(),
                                     1e-6f);
                }
            }
        }
        ++checked;
    }
    TEST_ASSERT(checked >= 40);
}

void TestLightZones() {
    // Known issue C21: the optional "lights" array of an entity JSON file sets the map's light
    // levels. Zones apply in order (later ones override), uncovered cells keep 1.0.
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<EntityPlacement> placements;
    std::vector<LightZone> lights;
    const std::string text = R"({"level": "t", "entities": [{"type": "player", "x": 1.5, "y": 1.5}],
  "lights": [{"x": 2, "y": 1, "w": 4, "h": 3, "level": 0.5},
             {"x": 4, "y": 2, "w": 2, "h": 2, "level": 0.2},
             {"x": 1, "y": 4, "w": 1, "h": 1, "level": 1}]})";
    TEST_ASSERT(ParseEntityPlacementsJson(text, "lights.json", placements, lights, errors, warnings));
    TEST_ASSERT(errors.empty() && warnings.empty());
    TEST_ASSERT(placements.size() == 1 && lights.size() == 3);
    TEST_ASSERT(lights[0].x == 2 && lights[0].y == 1 && lights[0].width == 4 && lights[0].height == 3);
    TEST_ASSERT_NEAR(lights[0].level, 0.5f, 1e-6f);
    TEST_ASSERT_NEAR(lights[1].level, 0.2f, 1e-6f);  // the minimum is allowed ...
    TEST_ASSERT_NEAR(lights[2].level, 1.0f, 1e-6f);  // ... and so is the maximum (an integer)
    TEST_ASSERT(lights[1].source == "lights.json: lights[1]");

    Map map(8, 6);
    BuildBorderWalls(map);
    LevelContents level{};
    TEST_ASSERT(BuildLevelFromPlacements(map, placements, lights, GameplayDefinitions{}, level, errors));
    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            float expected = 1.0f;  // not covered by any zone
            if (x >= 2 && x <= 5 && y >= 1 && y <= 3) {
                expected = 0.5f;
            }
            if (x >= 4 && x <= 5 && y >= 2 && y <= 3) {
                expected = 0.2f;  // the second zone overrides the first where they overlap
            }
            TEST_ASSERT_NEAR(map.GetLightLevel(x, y), expected, 1e-6f);
        }
    }

    // Without "lights" (and through the overloads without zones) every cell stays at 1.0.
    Map plain(8, 6);
    BuildBorderWalls(plain);
    TEST_ASSERT(ParseEntityPlacementsJson(text, "lights.json", placements, errors, warnings));
    TEST_ASSERT(BuildLevelFromPlacements(plain, placements, GameplayDefinitions{}, level, errors));
    for (int y = 0; y < plain.Height(); ++y) {
        for (int x = 0; x < plain.Width(); ++x) {
            TEST_ASSERT_NEAR(plain.GetLightLevel(x, y), 1.0f, 1e-6f);
        }
    }

    // Invalid zones are errors in the format of the entity errors; valid zones and the
    // placements are still returned; an unknown field is a warning.
    const std::string bad = R"({"entities": [{"type": "player", "x": 1.5, "y": 1.5}], "lights": [
  {"x": 1, "y": 1, "w": 1, "h": 1, "level": 0.1},
  {"x": 1, "y": 1, "w": 1, "h": 1, "level": 1.5},
  {"x": 1, "y": 1, "w": 1, "h": 1, "level": "dark"},
  {"x": 1, "y": 1, "w": 1, "h": 1},
  {"x": 1, "y": 1, "w": 0, "h": 1, "level": 0.5},
  {"x": -1, "y": 1, "w": 1, "h": 1, "level": 0.5},
  {"x": 1.5, "y": 1, "w": 1, "h": 1, "level": 0.5},
  {"y": 1, "w": 1, "h": 1, "level": 0.5},
  7,
  {"x": 1, "y": 1, "w": 1, "h": 1, "level": 0.2, "colour": "red"}]})";
    TEST_ASSERT(!ParseEntityPlacementsJson(bad, "bad.json", placements, lights, errors, warnings));
    TEST_ASSERT(placements.size() == 1 && lights.size() == 1);
    TEST_ASSERT(errors.size() == 9);
    TEST_ASSERT(
        AnyMessageContains(errors, {"bad.json: lights[0]: field \"level\" must be a number from 0.2 to 1.0, got 0.1"}));
    TEST_ASSERT(
        AnyMessageContains(errors, {"bad.json: lights[1]: field \"level\" must be a number from 0.2 to 1.0, got 1.5"}));
    TEST_ASSERT(
        AnyMessageContains(errors, {"lights[2]: field \"level\" must be a number from 0.2 to 1.0, got \"dark\""}));
    TEST_ASSERT(AnyMessageContains(errors, {"lights[3]: field \"level\" is required"}));
    TEST_ASSERT(AnyMessageContains(errors, {"lights[4]: field \"w\" must be an integer from 1 to 100000, got 0"}));
    TEST_ASSERT(AnyMessageContains(errors, {"lights[5]: field \"x\" must be an integer from 0 to 100000, got -1"}));
    TEST_ASSERT(AnyMessageContains(errors, {"lights[6]: field \"x\" must be an integer from 0 to 100000, got 1.5"}));
    TEST_ASSERT(AnyMessageContains(errors, {"lights[7]: field \"x\" is required"}));
    TEST_ASSERT(AnyMessageContains(errors, {"lights[8]: each light zone must be an object, got 7"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"bad.json: lights[9]: unknown field \"colour\" ignored"}));
    TEST_ASSERT(!AnyMessageContains(warnings, {"top-level field \"lights\""}));  // a known field now
    errors.clear();
    warnings.clear();
    TEST_ASSERT(!ParseEntityPlacementsJson(R"({"entities": [], "lights": {"x": 1}})", "obj.json", placements, lights,
                                           errors, warnings));
    TEST_ASSERT(
        AnyMessageContains(errors, {"obj.json: field \"lights\" must be an array of light zones, got an object"}));
    errors.clear();
    // The overload without zones still rejects a file with an invalid zone.
    TEST_ASSERT(!ParseEntityPlacementsJson(bad, "bad.json", placements, errors, warnings));
    errors.clear();
    warnings.clear();

    // A zone that leaves the map fails the build, and nothing is applied (not even the valid
    // zone before it).
    const std::string outside = R"({"entities": [{"type": "player", "x": 1.5, "y": 1.5}], "lights": [
  {"x": 1, "y": 1, "w": 2, "h": 2, "level": 0.3},
  {"x": 6, "y": 1, "w": 3, "h": 1, "level": 0.4},
  {"x": 1, "y": 5, "w": 1, "h": 2, "level": 0.4}]})";
    TEST_ASSERT(ParseEntityPlacementsJson(outside, "out.json", placements, lights, errors, warnings));
    Map untouched(8, 6);
    BuildBorderWalls(untouched);
    TEST_ASSERT(!BuildLevelFromPlacements(untouched, placements, lights, GameplayDefinitions{}, level, errors));
    TEST_ASSERT(errors.size() == 2);
    TEST_ASSERT(
        AnyMessageContains(errors, {"out.json: lights[1]: cells (6, 1) to (8, 1) are not all inside the 8x6 map"}));
    TEST_ASSERT(
        AnyMessageContains(errors, {"out.json: lights[2]: cells (1, 5) to (1, 6) are not all inside the 8x6 map"}));
    TEST_ASSERT_NEAR(untouched.GetLightLevel(1, 1), 1.0f, 1e-6f);
    errors.clear();

    // The real load path: Engine::LoadLevel applies the zones; a CSV has none; the next level
    // load starts from a fresh, fully lit map.
    const ScratchDir scratch("light-zones");
    const std::filesystem::path root = scratch.Path() / "content";
    WriteContentRoot(root, "{}", R"({"level": "level01", "entities": [
    { "type": "player", "x": 2.5, "y": 2.5 },
    { "type": "door", "x": 8.5, "y": 7.5, "mapX": 8, "mapY": 7 }],
  "lights": [{ "x": 1, "y": 4, "w": 6, "h": 3, "level": 0.35 }]})");
    WriteTextFile(root / "game" / "defs" / "level01.entities.csv", "player,2.5,2.5\ndoor,8.5,7.5,8,7\n");
    Engine engine;
    engine.contentRoot_ = root;
    TEST_ASSERT(engine.LoadGameplay("game/defs/gameplay.json"));
    TEST_ASSERT(engine.LoadLevel("game/maps/level01.map", "game/defs/level01.entities.json"));
    TEST_ASSERT_NEAR(engine.map_.GetLightLevel(1, 4), 0.35f, 1e-6f);
    TEST_ASSERT_NEAR(engine.map_.GetLightLevel(6, 6), 0.35f, 1e-6f);
    TEST_ASSERT_NEAR(engine.map_.GetLightLevel(7, 6), 1.0f, 1e-6f);
    TEST_ASSERT_NEAR(engine.map_.GetLightLevel(1, 3), 1.0f, 1e-6f);
    TEST_ASSERT(engine.LoadLevel("game/maps/level01.map", "game/defs/level01.entities.csv"));
    TEST_ASSERT_NEAR(engine.map_.GetLightLevel(1, 4), 1.0f, 1e-6f);

    // Walls are lit like the cell in front of the face seen: a wall between a lit cell (west)
    // and a dark one (east) is bright from the west and dark from the east.
    Texture white(1, 1);
    white.SetPixel(0, 0, PackARGB32(MakeColor(255, 255, 255)));
    Map split(7, 5);
    BuildBorderWalls(split);
    split.SetCell(3, 1, TileType::Wall);
    split.SetCell(3, 2, TileType::Wall);
    split.SetCell(3, 3, TileType::Wall);
    for (int y = 1; y <= 3; ++y) {
        for (int x = 4; x <= 5; ++x) {
            split.SetLightLevel(x, y, 0.2f);
        }
    }
    RaycasterRenderParams params{};
    params.wallTexture = &white;
    const auto wallRed = [&](Vec2 position, float angle) {
        Player viewer{};
        viewer.position = position;
        viewer.directionAngleRadians = angle;
        Framebuffer framebuffer(64, 48);
        std::vector<RaycastHit> hits;
        Raycaster::RenderWalls(split, viewer, framebuffer, params, hits);
        TEST_ASSERT(hits[32].hit && hits[32].mapX == 3);
        return RedOf(PixelAt(framebuffer, 32, 24));
    };
    const int fromLitSide = wallRed({1.5f, 2.5f}, 0.0f);  // looking +x at the west face
    const int fromDarkSide = wallRed({5.5f, 2.5f}, kPi);  // looking -x at the east face
    TEST_ASSERT(fromLitSide > 150);
    TEST_ASSERT(fromDarkSide < 60);
}

void TestRenderSizeOption() {
    // --render-size WxH: valid sizes, both separators, the inline form.
    struct ValidCase {
        const char* text = nullptr;
        RenderSize size;
    };
    for (const ValidCase& valid :
         {ValidCase{"640x360", {640, 360}}, ValidCase{"320x240", {320, 240}}, ValidCase{"320x200", {320, 200}},
          ValidCase{"1920X1080", {1920, 1080}}, ValidCase{"1280x720", {1280, 720}},
          ValidCase{"1280x1024", {1280, 1024}}, ValidCase{"1920x810", {1920, 810}}}) {
        const CommandLineResult r = ParseCommandLine({"--render-size", valid.text});
        TEST_ASSERT(r.action == CommandLineAction::Run);
        TEST_ASSERT(r.options.renderSize.width == valid.size.width && r.options.renderSize.height == valid.size.height);
    }
    const CommandLineResult inlineForm = ParseCommandLine({"--render-size=960x540", "--frames", "3"});
    TEST_ASSERT(inlineForm.action == CommandLineAction::Run && inlineForm.options.renderSize.width == 960 &&
                inlineForm.options.renderSize.height == 540 && inlineForm.options.frames == 3);

    // Invalid: missing or malformed, odd, outside the limits, narrower than 5:4 or wider than
    // 64:27 (1920x808). The message names the option and the limits.
    for (const char* text : {"",          "640",     "640x",      "x360",    "640x360x2",       "-640x360",
                             "640 x 360", "640*360", "axb",       "641x360", "640x361",         "318x240",
                             "1922x1080", "640x198", "1920x1082", "0x0",     "99999999999x360", "1920x200",
                             "320x1080",  "640x514", "1920x808"}) {
        const CommandLineResult bad = ParseCommandLine({"--render-size", text});
        TEST_ASSERT(bad.action == CommandLineAction::Error);
        if (bad.error.find("--render-size") == std::string::npos || bad.error.find("1920") == std::string::npos ||
            bad.error.find("640x360") == std::string::npos) {
            FailTest(__FILE__, __LINE__, std::string("--render-size ") + text + ": unclear error: " + bad.error);
        }
    }
    TEST_ASSERT(ParseCommandLine({"--render-size"}).action == CommandLineAction::Error);

    TEST_ASSERT(IsValidRenderSize(kDefaultRenderSize));
    TEST_ASSERT(kDefaultRenderSize.width == 640 && kDefaultRenderSize.height == 360);

    // The engine renders at the size it is given, and refuses an invalid one (C API, tests).
    Engine engine;
    EngineOptions options = ShippedEngineOptions(1);
    options.renderSize = {960, 540};
    TEST_ASSERT(engine.Init(options));
    TEST_ASSERT(engine.framebuffer_.Width() == 960 && engine.framebuffer_.Height() == 540);
    engine.Update(kFrameSeconds);
    engine.Render();
    TEST_ASSERT(engine.latestWallDepthBuffer_.size() == 960);
    engine.Shutdown();
    Engine invalid;
    EngineOptions invalidOptions = ShippedEngineOptions(1);
    invalidOptions.renderSize = {641, 360};
    TEST_ASSERT(!invalid.Init(invalidOptions));
    invalid.Shutdown();

    // The initial window: the largest whole multiple of the render size within 1280x960, at
    // least 1x.
    const auto windowIs = [](const RenderSize& render, int width, int height) {
        const RenderSize window = InitialWindowSize(render);
        return window.width == width && window.height == height;
    };
    TEST_ASSERT(windowIs({640, 360}, 1280, 720) && windowIs({320, 240}, 1280, 960));
    TEST_ASSERT(windowIs({640, 480}, 1280, 960) && windowIs({320, 200}, 1280, 800));
    TEST_ASSERT(windowIs({960, 540}, 960, 540) && windowIs({1920, 1080}, 1920, 1080));
    TEST_ASSERT(windowIs({1920, 810}, 1920, 810));

    // PlatformSDL::FitWindow: the window fits within 90% of the desktop with its shape, never
    // below the framebuffer unless the framebuffer does not fit, and the minimum size follows.
    const auto fit = [](const RenderSize& render, int usableWidth, int usableHeight) {
        PlatformConfig config{};
        const RenderSize window = InitialWindowSize(render);
        config.windowWidth = window.width;
        config.windowHeight = window.height;
        config.logicalWidth = render.width;
        config.logicalHeight = render.height;
        return PlatformSDL::FitWindow(config, usableWidth, usableHeight);
    };
    const auto fitIs = [](const WindowFit& f, int width, int height, int minWidth, int minHeight) {
        return f.width == width && f.height == height && f.minWidth == minWidth && f.minHeight == minHeight;
    };
    TEST_ASSERT(fitIs(fit({640, 360}, 2560, 1440), 1280, 720, 640, 360));     // room enough
    TEST_ASSERT(fitIs(fit({640, 360}, 0, 0), 1280, 720, 640, 360));           // desktop unknown
    TEST_ASSERT(fitIs(fit({640, 360}, 1280, 800), 1152, 648, 640, 360));      // shrunk with its shape
    TEST_ASSERT(fitIs(fit({320, 240}, 1366, 768), 921, 691, 320, 240));       // 4:3 on a short laptop
    TEST_ASSERT(fitIs(fit({1920, 1080}, 1920, 1080), 1728, 972, 1728, 972));  // bigger than the desktop
    TEST_ASSERT(fitIs(fit({1280, 720}, 1280, 720), 1152, 648, 1152, 648));    // framebuffer does not fit
    TEST_ASSERT(fitIs(fit({640, 360}, 800, 600), 720, 405, 640, 360));        // between 1x and 2x
}

void TestProjectionSquarePixels() {
    // The projection keeps the vertical field of view and makes pixels square at every render
    // size ("Hor+", RenderMath.h): a wall one unit high at distance d is screenHeight / d pixels
    // high, a wall face one unit wide as many pixels wide, and a sprite or particle at the same
    // distance matches the wall. The player looks +x along y = 2.5 at a one-unit exit face
    // (cell (5, 2), face x = 5, distance 2.5) between plain walls.
    Map map(7, 5);
    BuildBorderWalls(map);
    map.SetCell(5, 1, TileType::Wall);
    map.SetCell(5, 2, TileType::Exit);
    map.SetCell(5, 3, TileType::Wall);
    Texture white(4, 4);
    Texture red(4, 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            white.SetPixel(x, y, Framebuffer::PackRGBA(255, 255, 255));
            red.SetPixel(x, y, Framebuffer::PackRGBA(255, 0, 0));
        }
    }
    RaycasterRenderParams params{};  // no floor, ceiling or missing texture: only walls are drawn
    params.wallTexture = &white;
    params.exitTexture = &white;
    Player player{};
    player.position = {2.5f, 2.5f};
    player.directionAngleRadians = 0.0f;
    constexpr float kDistance = 2.5f;
    constexpr std::uint32_t kBlack = 0xFF000000u;

    for (const RenderSize& size : {RenderSize{320, 240}, RenderSize{640, 360}, RenderSize{640, 480}}) {
        const std::string at = " at " + std::to_string(size.width) + "x" + std::to_string(size.height);
        const int expected = static_cast<int>(std::lround(static_cast<float>(size.height) / kDistance));
        const int centreX = size.width / 2;
        const int centreY = size.height / 2;
        Framebuffer framebuffer(size.width, size.height);
        framebuffer.Clear(kBlack);
        std::vector<RaycastHit> hits;
        Raycaster::RenderWalls(map, player, framebuffer, params, hits);

        // Same vertical field of view: the wall is 1 / 2.5 of the screen height.
        int wallRows = 0;
        for (int y = 0; y < size.height; ++y) {
            wallRows += PixelAt(framebuffer, centreX, y) != kBlack ? 1 : 0;
        }
        // Square pixels: the one-unit exit face is as wide as the wall is high.
        int exitColumns = 0;
        for (const RaycastHit& hit : hits) {
            exitColumns += hit.hit && hit.tileType == TileType::Exit ? 1 : 0;
        }
        if (wallRows != expected || std::abs(exitColumns - expected) > 2) {
            FailTest(__FILE__, __LINE__,
                     "wall " + std::to_string(exitColumns) + " x " + std::to_string(wallRows) + " pixels, expected " +
                         std::to_string(expected) + " x " + std::to_string(expected) + at);
        }

        // The horizontal field of view across the whole face: the ray through column x meets the
        // face x = 5 at a sideways offset of (x - width / 2) / height * distance, so a column is
        // as wide as a row is high at every column, not only the centre.
        for (int x = 0; x < size.width; ++x) {
            const float sideways = static_cast<float>(x - centreX) / static_cast<float>(size.height) * kDistance;
            if (std::abs(sideways) > 1.4f) {
                continue;  // past the cells (5, 1)-(5, 3) the ray meets the border walls
            }
            const RaycastHit& hit = hits[static_cast<std::size_t>(x)];
            if (!hit.hit || std::abs(hit.hitPosition.x - 5.0f) > 1e-3f ||
                std::abs(hit.hitPosition.y - (2.5f + sideways)) > 1e-3f) {
                FailTest(__FILE__, __LINE__,
                         "column " + std::to_string(x) + " meets the wall at y " + std::to_string(hit.hitPosition.y) +
                             ", expected " + std::to_string(2.5f + sideways) + at);
                break;
            }
        }

        // A sprite at the same distance is a square of the same size.
        SpriteTextureSet textures{};
        textures.slots.fill(&red);
        textures.missing = &red;
        const std::vector<float> farDepth(static_cast<std::size_t>(size.width), 100.0f);
        framebuffer.Clear(kBlack);
        SpriteEntity sprite{};
        sprite.position = {2.5f + kDistance, 2.5f};
        SpriteRenderer::Render(map, player, framebuffer, farDepth, {sprite}, textures);
        int spriteColumns = 0;
        for (int x = 0; x < size.width; ++x) {
            spriteColumns += PixelAt(framebuffer, x, centreY) != kBlack ? 1 : 0;
        }
        int spriteRows = 0;
        for (int y = 0; y < size.height; ++y) {
            spriteRows += PixelAt(framebuffer, centreX, y) != kBlack ? 1 : 0;
        }
        TEST_ASSERT(std::abs(spriteColumns - exitColumns) <= 2 && std::abs(spriteRows - wallRows) <= 2);

        // A sprite off to the side (0.8 units at the same distance, still whole on screen) is
        // centred on the column whose ray meets the wall 0.8 units to the side.
        constexpr float kSideways = 0.8f;
        framebuffer.Clear(kBlack);
        SpriteEntity sideSprite{};
        sideSprite.position = {2.5f + kDistance, 2.5f + kSideways};
        SpriteRenderer::Render(map, player, framebuffer, farDepth, {sideSprite}, textures);
        int sideLeft = size.width;
        int sideRight = -1;
        for (int x = 0; x < size.width; ++x) {
            if (PixelAt(framebuffer, x, centreY) != kBlack) {
                sideLeft = std::min(sideLeft, x);
                sideRight = std::max(sideRight, x);
            }
        }
        const int sideCentre = (sideLeft + sideRight + 1) / 2;
        const RaycastHit& sideHit = hits[static_cast<std::size_t>(std::clamp(sideCentre, 0, size.width - 1))];
        if (sideRight < 0 ||
            std::abs(sideHit.hitPosition.y - (2.5f + kSideways)) > 1.5f * kDistance / static_cast<float>(size.height) ||
            std::abs((sideRight - sideLeft + 1) - spriteColumns) > 2) {
            FailTest(__FILE__, __LINE__,
                     "side sprite at columns " + std::to_string(sideLeft) + "-" + std::to_string(sideRight) +
                         ", the wall there is at y " + std::to_string(sideHit.hitPosition.y) + at);
        }

        // A particle of 0.2 world units is a square of 0.2 * screenHeight / distance pixels.
        Particle particle{};
        particle.position = sprite.position;
        particle.size = 0.2f;
        particle.lifetimeRemaining = 1.0f;
        particle.lifetimeTotal = 1.0f;
        particle.active = true;
        framebuffer.ResetDrawBounds();
        ParticleRenderer::Render(map, player, framebuffer, farDepth, {particle});
        const DrawBounds& bounds = framebuffer.GetDrawBounds();
        const int particleSize = static_cast<int>(0.2f * static_cast<float>(size.height) / kDistance);
        TEST_ASSERT(bounds.any && bounds.maxX - bounds.minX + 1 == particleSize &&
                    bounds.maxY - bounds.minY + 1 == particleSize);
    }
}

struct TestCase {
    const char* id;    // used on the command line and as the CTest name (native.<id>)
    const char* name;  // human-readable description
    void (*fn)();
};

// Every test case, in run order. Keep in sync with NEOWOLF_NATIVE_TESTS in CMakeLists.txt.
const std::vector<TestCase>& AllTests() {
    static const std::vector<TestCase> tests = {
        {"vec2_math_helpers", "Vec2 and math helpers", &TestVec2AndMathHelpers},
        {"input_state_transitions", "InputState transitions", &TestInputStateTransitions},
        {"map_queries", "Map queries", &TestMapQueries},
        {"raycast_hit_distance", "Raycast hit distance", &TestRaycastHitDistance},
        {"texture_loader_fallback", "Texture loader and fallback", &TestTextureLoaderAndFallback},
        {"entity_manager_basics", "Entity manager basics", &TestEntityManagerBasics},
        {"player_collision_rules", "Player collision rules", &TestPlayerCollisionRules},
        {"sprite_distance_sorting", "Sprite distance sorting", &TestSpriteDistanceSorting},
        {"entity_interaction_rules", "Entity interaction rules", &TestEntityInteractionRules},
        {"content_file_loaders", "Content file loaders", &TestContentFileLoaders},
        {"audio_config_loader", "Audio config loader", &TestAudioConfigLoader},
        {"asset_manager_fallback_load", "Asset manager fallback load", &TestAssetManagerFallbackLoad},
        {"game_state_transitions", "GameState transitions", &TestGameStateTransitions},
        {"player_combat_shooting", "Player combat and shooting", &TestPlayerCombatAndShooting},
        {"enemy_ai_behavior", "Enemy AI behavior", &TestEnemyAiBehavior},
        {"pickup_collection", "Pickup collection", &TestPickupCollection},
        {"command_bridge_events", "Command bridge and events", &TestCommandBridgeAndEvents},
        {"file_logging", "File logging", &TestLogFile},
        {"level_reset_dynamic_load", "Level reset and dynamic load", &TestLevelResetAndDynamicLoad},
        {"vertical_slice_simulation", "Playable vertical slice simulation", &TestPlayableVerticalSliceSimulation},
        {"map_perimeter_reachability", "Map perimeter and reachability validation",
         &TestMapPerimeterAndReachabilityValidation},
        {"asset_inventory_resilience", "Asset inventory and error resilience", &TestAssetInventoryAndErrorResilience},
        {"engine_interop_c_api", "Engine Interop C API", &TestEngineInteropCApi},
        {"content_root_resolution", "Content root resolution (candidates, sentinel, searched list)",
         &TestContentRootResolution},
        {"command_line_parsing", "Command-line parsing", &TestCommandLineParsing},
        {"crlf_content_loading", "CRLF content files load like LF ones", &TestCrlfContentLoading},
        {"log_file_rotation", "Log file rotation", &TestLogFileRotation},
        {"gameplay_definitions_exact", "gameplay.json exact values per section", &TestGameplayDefinitionsExactValues},
        {"gameplay_definitions_invalid", "Malformed or invalid gameplay.json is rejected",
         &TestGameplayDefinitionsInvalid},
        {"entity_placements_json_csv", "JSON entity placements, errors, CSV equivalence",
         &TestEntityPlacementsJsonAndCsv},
        {"decorations_and_sprite_sync", "Decorations are not triggers; sprites survive updates",
         &TestDecorationsAndSpriteSync},
        {"pickup_kind_from_data", "Pickup kind and value come from data", &TestPickupKindFromData},
        {"edited_data_changes_game", "Edited gameplay and entity files change the game", &TestEditedDataChangesGame},
        {"level_load_failures", "Missing or invalid entity files fail the level load", &TestLevelLoadFailures},
        {"game_flow_menu", "Game flow: main menu, level select, quit, new game", &TestGameFlowMenu},
        {"game_flow_progression", "Game flow: level complete, carry-over, campaign complete", &TestGameFlowProgression},
        {"game_flow_death_pause", "Game flow: death snapshot, pause, continue, external states",
         &TestGameFlowDeathAndPause},
        {"campaign_manifest_validation", "campaign.json loads and invalid manifests are rejected",
         &TestCampaignManifestValidation},
        {"exit_switch_activation", "Exit switch: reach, facing, walls; triggers do not end the level",
         &TestExitSwitchActivation},
        {"progress_file_roundtrip", "progress.json round trip, atomic save, corrupt files", &TestProgressFileRoundTrip},
        {"pref_dir_resolution", "Per-user folder: --pref-dir, NEOWOLF_PREF_DIR, SDL default", &TestPrefDirResolution},
        {"theme_loading_and_swap", "Texture themes load and swap in place", &TestThemeLoadingAndSwap},
        {"ui_layout_bounds", "Every screen stays inside the margins at every tested render size", &TestUiLayoutBounds},
        {"campaign_playthrough", "Scripted play-through of every campaign level -> CampaignComplete",
         &TestCampaignPlaythrough},
        {"line_of_sight", "Line of sight: walls and closed doors block, open doors do not", &TestLineOfSight},
        {"enemy_hearing", "Enemies hear shots through open cells only", &TestEnemyHearing},
        {"enemy_pathfinding", "Grid path finding and enemies walking round walls", &TestEnemyPathfinding},
        {"enemy_archetypes_from_data", "Guard, brute and runner stats from gameplay.json",
         &TestEnemyArchetypesFromData},
        {"entity_variant_validation", "Unknown variants, key and lock colours are errors",
         &TestEntityVariantValidation},
        {"locked_doors_and_keys", "Locked doors open only with their key; keys reset per level",
         &TestLockedDoorsAndKeys},
        {"door_close_blocked", "Doors do not close on the player or a living enemy", &TestDoorCloseBlocked},
        {"corpse_and_hit_flash", "Corpses stay visible and block nothing; hits flash", &TestCorpseAndHitFlash},
        {"enemy_separation", "Living enemies do not overlap", &TestEnemySeparation},
        {"hud_key_icons_layout", "HUD key-card icons inside the layout", &TestHudKeyIconsLayout},
        {"ai_stress_20_enemies", "20 enemies in a 48x48 map: robust and fast", &TestAiStress20Enemies},
        {"new_game_resets_world", "Menu -> New Game fully resets the world (C7)", &TestNewGameResetsWorld},
        {"game_flow_test_level", "Game flow: the test level's menu item, exit restart, no progress",
         &TestGameFlowTestLevel},
        {"test_level", "The test level: boot, --level and --preview, exit restart, no progress, menu", &TestTestLevel},
        {"key_bindings", "Key bindings, B13 fullscreen chord, minimap toggle", &TestKeyBindings},
        {"wall_texture_orientation", "Wall textures read left to right from every side", &TestWallTextureOrientation},
        {"autoplay_fixture_level", "Autoplay bot: keys, doors, a guard, a health detour; deterministic",
         &TestAutoplayFixtureLevel},
        {"autoplay_unwinnable_levels", "Autoplay bot reports unwinnable levels with a reason",
         &TestAutoplayUnwinnableLevels},
        {"autoplay_command_line", "neo_wolf_autoplay command line and report formats", &TestAutoplayCommandLine},
        {"player_enemy_collision", "Living enemies block the player, corpses do not (C15)", &TestPlayerEnemyCollision},
        {"sprite_vertical_crop", "Close sprites are cropped, not squeezed", &TestSpriteVerticalCrop},
        {"content_parity_fixtures", "Engine loaders match tests/fixtures/parity/expected.json",
         &TestContentParityFixtures},
        {"light_zones", "Entity-file light zones: order, range, bounds, default 1.0, wall faces (C21)",
         &TestLightZones},
        {"render_size_option", "--render-size option and engine render size", &TestRenderSizeOption},
        {"projection_square_pixels", "Projection keeps the vertical FOV and square pixels",
         &TestProjectionSquarePixels},
    };
    return tests;
}

void PrintUsage(const char* program) {
    std::cout << "Usage: " << program << " [--list] [--help] [TEST...]\n"
              << "  --list   print the id of every test case, one per line\n"
              << "  TEST     run only matching test cases: an exact id, otherwise every test whose\n"
              << "           id or description contains TEST (case-sensitive)\n"
              << "With no TEST arguments every test case runs. Exit code 0 means all selected tests\n"
              << "passed; 1 means a test failed or a filter matched nothing; 2 means bad usage.\n";
}

bool Matches(const TestCase& test, const std::string& filter) {
    return std::string(test.id).find(filter) != std::string::npos ||
           std::string(test.name).find(filter) != std::string::npos;
}

// Adds the test cases `filter` selects to `selected` (each once): an exact id selects only that
// test, otherwise every test whose id or description contains it. False if none matches.
bool SelectByFilter(const std::string& filter, std::vector<const TestCase*>& selected) {
    std::vector<const TestCase*> matches;
    for (const TestCase& test : AllTests()) {
        if (filter == test.id) {
            matches = {&test};
            break;
        }
        if (Matches(test, filter)) {
            matches.push_back(&test);
        }
    }
    for (const TestCase* match : matches) {
        if (std::find(selected.begin(), selected.end(), match) == selected.end()) {
            selected.push_back(match);
        }
    }
    return !matches.empty();
}

// Runs one test case: its failure message, or an empty string if it passed.
std::string RunTestCase(const TestCase& test) {
    try {
        test.fn();
    } catch (const TestFailure& failure) {
        return failure.message;
    } catch (const std::exception& ex) {
        return std::string("unexpected exception: ") + ex.what();
    } catch (...) {
        return "unexpected non-standard exception";
    }
    return "";
}

// Runs the selected test cases and prints a summary; returns the exit code (0: all passed).
int RunTestCases(const std::vector<const TestCase*>& selected) {
    int passed = 0;
    std::vector<std::string> failures;
    for (const TestCase* test : selected) {
        std::cout << "[RUN ] " << test->id << " (" << test->name << ")\n" << std::flush;
        const std::string error = RunTestCase(*test);
        if (error.empty()) {
            std::cout << "[PASS] " << test->id << '\n';
            ++passed;
        } else {
            std::cout << "[FAIL] " << test->id << '\n' << std::flush;
            std::cerr << "       " << error << '\n' << std::flush;
            failures.push_back(std::string(test->id) + ": " + error);
        }
    }

    std::cout << "\n" << passed << " of " << selected.size() << " test case(s) passed";
    if (failures.empty()) {
        std::cout << ".\n";
        return 0;
    }
    std::cout << "; " << failures.size() << " failed:\n";
    for (const std::string& failure : failures) {
        std::cout << "  " << failure << '\n';
    }
    return 1;
}
}  // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): tests run inside RunTestCase; only output can throw here
int main(int argc, char* argv[]) {
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--list") {
            for (const TestCase& test : AllTests()) {
                std::cout << test.id << '\n';
            }
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            return 0;
        }
        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << '\n';
            PrintUsage(argv[0]);
            return 2;
        }
        filters.push_back(arg);
    }

    std::vector<const TestCase*> selected;
    for (const std::string& filter : filters) {
        if (!SelectByFilter(filter, selected)) {
            std::cerr << "No test case matches '" << filter << "' (use --list to see test ids).\n";
            return 1;
        }
    }
    if (filters.empty()) {
        for (const TestCase& test : AllTests()) {
            selected.push_back(&test);
        }
    }
    return RunTestCases(selected);
}
