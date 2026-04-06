#include <cmath>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <exception>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/assets/TextureLoader.h"
#include "engine/assets/AssetManager.h"
#include "engine/assets/ContentConfig.h"
#include "engine/core/InputState.h"
#include "engine/core/MathUtils.h"
#include "engine/render/Raycaster.h"
#include "engine/render/SpriteRenderer.h"
#include "engine/core/Vec2.h"
#include "engine/world/Entity.h"
#include "engine/world/EntityPlacementLoader.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/Map.h"
#include "engine/world/MapLoader.h"
#include "engine/world/MovementRules.h"
#include "engine/world/Player.h"
#include "engine/world/Sprite.h"

namespace {
void Assert(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        throw std::runtime_error(
            std::string(file) + ":" + std::to_string(line) + " assertion failed: " + expression);
    }
}

void AssertNear(
    float actual,
    float expected,
    float epsilon,
    const char* expression,
    const char* file,
    int line) {
    if (std::fabs(actual - expected) > epsilon) {
        throw std::runtime_error(
            std::string(file) + ":" + std::to_string(line) + " assertion failed: " + expression +
            " (actual=" + std::to_string(actual) + ", expected=" + std::to_string(expected) + ")");
    }
}

#define TEST_ASSERT(expr) Assert((expr), #expr, __FILE__, __LINE__)
#define TEST_ASSERT_NEAR(actual, expected, epsilon) \
    AssertNear((actual), (expected), (epsilon), #actual " ~= " #expected, __FILE__, __LINE__)

std::string ResolveRepoPath(const std::string& relativePathFromRepoRoot) {
    if (std::filesystem::exists(relativePathFromRepoRoot)) {
        return relativePathFromRepoRoot;
    }

    const std::string fromBuildDir = "../" + relativePathFromRepoRoot;
    if (std::filesystem::exists(fromBuildDir)) {
        return fromBuildDir;
    }

    return relativePathFromRepoRoot;
}

void TestVec2AndMathHelpers() {
    const Vec2 a{3.0f, 4.0f};
    const Vec2 b{1.0f, 2.0f};

    const Vec2 sum = Add(a, b);
    TEST_ASSERT_NEAR(sum.x, 4.0f, 0.0001f);
    TEST_ASSERT_NEAR(sum.y, 6.0f, 0.0001f);

    const Vec2 diff = Subtract(a, b);
    TEST_ASSERT_NEAR(diff.x, 2.0f, 0.0001f);
    TEST_ASSERT_NEAR(diff.y, 2.0f, 0.0001f);

    const Vec2 scaled = Scale(b, 3.0f);
    TEST_ASSERT_NEAR(scaled.x, 3.0f, 0.0001f);
    TEST_ASSERT_NEAR(scaled.y, 6.0f, 0.0001f);

    TEST_ASSERT_NEAR(Length(a), 5.0f, 0.0001f);

    const Vec2 normalized = Normalize(a);
    TEST_ASSERT_NEAR(normalized.x, 0.6f, 0.0001f);
    TEST_ASSERT_NEAR(normalized.y, 0.8f, 0.0001f);

    const Vec2 normalizedZero = Normalize(Vec2{0.0f, 0.0f});
    TEST_ASSERT_NEAR(normalizedZero.x, 0.0f, 0.0001f);
    TEST_ASSERT_NEAR(normalizedZero.y, 0.0f, 0.0001f);

    TEST_ASSERT_NEAR(Dot(a, b), 11.0f, 0.0001f);

    TEST_ASSERT(Clamp(10, 0, 5) == 5);
    TEST_ASSERT(Clamp(-2, 0, 5) == 0);
    TEST_ASSERT_NEAR(Clamp(0.4f, 0.0f, 1.0f), 0.4f, 0.0001f);

    TEST_ASSERT_NEAR(DegreesToRadians(180.0f), kPi, 0.0001f);
    TEST_ASSERT_NEAR(RadiansToDegrees(kPi), 180.0f, 0.0001f);
    TEST_ASSERT_NEAR(NormalizeAngleRadians(-kPi / 2.0f), 1.5f * kPi, 0.0001f);
    TEST_ASSERT_NEAR(NormalizeAngleDegrees(-90.0f), 270.0f, 0.0001f);
}

void TestInputStateTransitions() {
    InputState input{};

    TEST_ASSERT(!input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));

    input.SetActionState(InputAction::MoveForward, true);
    TEST_ASSERT(input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));

    input.BeginFrame();
    TEST_ASSERT(input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));

    input.SetActionState(InputAction::MoveForward, true);
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));

    input.SetActionState(InputAction::MoveForward, false);
    TEST_ASSERT(!input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(input.WasReleased(InputAction::MoveForward));

    input.BeginFrame();
    TEST_ASSERT(!input.IsDown(InputAction::MoveForward));
    TEST_ASSERT(!input.WasPressed(InputAction::MoveForward));
    TEST_ASSERT(!input.WasReleased(InputAction::MoveForward));
}

void TestMapQueries() {
    Map map(4, 3);

    TEST_ASSERT(map.Width() == 4);
    TEST_ASSERT(map.Height() == 3);

    TEST_ASSERT(map.IsInsideMap(0, 0));
    TEST_ASSERT(map.IsInsideMap(3, 2));
    TEST_ASSERT(!map.IsInsideMap(-1, 0));
    TEST_ASSERT(!map.IsInsideMap(4, 1));
    TEST_ASSERT(!map.IsInsideMap(2, 3));

    TEST_ASSERT(map.GetCell(1, 1) == TileType::Empty);
    TEST_ASSERT(map.GetCell(-1, -1) == TileType::Empty);

    TEST_ASSERT(map.SetCell(1, 1, TileType::Wall));
    TEST_ASSERT(map.GetCell(1, 1) == TileType::Wall);
    TEST_ASSERT(map.IsWall(1, 1));

    TEST_ASSERT(map.SetCell(2, 1, TileType::Door));
    TEST_ASSERT(map.GetCell(2, 1) == TileType::Door);
    TEST_ASSERT(map.IsWall(2, 1));

    TEST_ASSERT(map.SetCell(2, 2, TileType::Trigger));
    TEST_ASSERT(map.GetCell(2, 2) == TileType::Trigger);
    TEST_ASSERT(!map.IsWall(2, 2));

    TEST_ASSERT(!map.SetCell(10, 10, TileType::Wall));
    TEST_ASSERT(!map.IsWall(-1, 0));

    TEST_ASSERT(map.SetCell(0, 1, TileType::WallAltA));
    TEST_ASSERT(map.IsWall(0, 1));
    TEST_ASSERT(map.SetCell(0, 2, TileType::WallAltB));
    TEST_ASSERT(map.IsWall(0, 2));

    TEST_ASSERT_NEAR(map.GetLightLevel(1, 1), 1.0f, 0.0001f);
    TEST_ASSERT(map.SetLightLevel(1, 1, 0.3f));
    TEST_ASSERT_NEAR(map.GetLightLevel(1, 1), 0.3f, 0.0001f);
    TEST_ASSERT(map.SetLightLevel(1, 2, 5.0f));
    TEST_ASSERT_NEAR(map.GetLightLevel(1, 2), 1.0f, 0.0001f);
    TEST_ASSERT(map.SetLightLevel(2, 2, -2.0f));
    TEST_ASSERT_NEAR(map.GetLightLevel(2, 2), 0.0f, 0.0001f);
}

void TestRaycastHitDistance() {
    Map map(5, 5);
    for (int x = 0; x < map.Width(); ++x) {
        map.SetCell(x, 0, TileType::Wall);
        map.SetCell(x, map.Height() - 1, TileType::Wall);
    }
    for (int y = 0; y < map.Height(); ++y) {
        map.SetCell(0, y, TileType::Wall);
        map.SetCell(map.Width() - 1, y, TileType::Wall);
    }
    map.SetCell(3, 1, TileType::Wall);
    map.SetCell(3, 2, TileType::Wall);
    map.SetCell(3, 3, TileType::Wall);

    const Vec2 origin{1.5f, 2.5f};
    const float playerAngle = 0.0f;

    const RaycastHit centerHit = Raycaster::CastRay(map, origin, Vec2{1.0f, 0.0f}, playerAngle);
    TEST_ASSERT(centerHit.hit);
    TEST_ASSERT(centerHit.mapX == 3);
    TEST_ASSERT(centerHit.mapY == 2);
    TEST_ASSERT(centerHit.side == RayHitSide::Vertical);
    TEST_ASSERT_NEAR(centerHit.distanceRaw, 1.5f, 0.0001f);
    TEST_ASSERT_NEAR(centerHit.distanceCorrected, 1.5f, 0.0001f);

    const float offAngle = DegreesToRadians(30.0f);
    const Vec2 offDir{std::cos(offAngle), std::sin(offAngle)};
    const RaycastHit offCenterHit = Raycaster::CastRay(map, origin, offDir, playerAngle);
    TEST_ASSERT(offCenterHit.hit);
    TEST_ASSERT(offCenterHit.distanceRaw > offCenterHit.distanceCorrected);
    TEST_ASSERT_NEAR(offCenterHit.distanceCorrected, 1.5f, 0.001f);
}

void TestTextureLoaderAndFallback() {
    const std::string filePath = "test_texture.ppm";
    {
        std::ofstream out(filePath, std::ios::trunc);
        TEST_ASSERT(out.is_open());
        out << "P3\n";
        out << "2 2\n";
        out << "255\n";
        out << "255 0 0   0 255 0\n";
        out << "0 0 255   255 255 0\n";
    }

    Texture texture{};
    TEST_ASSERT(TextureLoader::LoadPPM(filePath, texture));
    TEST_ASSERT(texture.Width() == 2);
    TEST_ASSERT(texture.Height() == 2);
    TEST_ASSERT(texture.GetPixel(0, 0) != texture.GetPixel(1, 0));
    TEST_ASSERT(texture.SampleNearest(0.1f, 0.1f) == texture.GetPixel(0, 0));

    Texture fallback{};
    TextureLoader::BuildFallbackChecker(fallback, 16, 16);
    TEST_ASSERT(fallback.Width() == 16);
    TEST_ASSERT(fallback.Height() == 16);
    TEST_ASSERT(fallback.GetPixel(0, 0) != fallback.GetPixel(8, 0));

    const bool deleted = std::remove(filePath.c_str()) == 0;
    TEST_ASSERT(deleted);
}

void TestEntityManagerBasics() {
    EntityManager manager{};
    Entity& a = manager.CreateEntity(EntityType::Enemy, Vec2{2.0f, 3.0f});
    const int aId = a.id;
    Entity& b = manager.CreateEntity(EntityType::Pickup, Vec2{4.0f, 5.0f});
    const int bId = b.id;
    TEST_ASSERT(a.id != b.id);
    TEST_ASSERT(manager.Entities().size() == 2);

    Entity* found = manager.GetEntityById(bId);
    TEST_ASSERT(found != nullptr);
    TEST_ASSERT(found->type == EntityType::Pickup);
    TEST_ASSERT_NEAR(found->position.x, 4.0f, 0.0001f);
    TEST_ASSERT_NEAR(found->position.y, 5.0f, 0.0001f);

    TEST_ASSERT(manager.DestroyEntity(aId));
    TEST_ASSERT(manager.GetEntityById(aId) == nullptr);
    TEST_ASSERT(manager.Entities().size() == 1);
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

void TestPlayerCollisionRules() {
    Map map(6, 6);
    BuildBorderWalls(map);
    map.SetCell(3, 2, TileType::Wall);

    Player player{};
    player.position = {2.5f, 2.5f};
    player.collisionRadius = 0.22f;

    ResolveCollisionAgainstWalls(map, player, Vec2{0.6f, 0.0f});
    TEST_ASSERT_NEAR(player.position.x, 2.5f, 0.0001f);
    TEST_ASSERT_NEAR(player.position.y, 2.5f, 0.0001f);

    ResolveCollisionAgainstWalls(map, player, Vec2{-0.4f, 0.0f});
    TEST_ASSERT(player.position.x < 2.2f);
    TEST_ASSERT_NEAR(player.position.y, 2.5f, 0.0001f);
}

void TestSpriteDistanceSorting() {
    Player player{};
    player.position = {2.0f, 2.0f};

    std::vector<SpriteEntity> sprites{
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

    Entity& pickup = manager.CreateEntity(EntityType::Pickup, Vec2{5.6f, 4.5f});
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

    SyncPlayerEntity(manager, player);
    UpdateEntityInteractions(manager, map, player, false);
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
    UpdateEntityInteractions(manager, map, player, true);
    const Entity* doorOpen = manager.GetEntityById(doorId);
    TEST_ASSERT(doorOpen != nullptr);
    TEST_ASSERT(doorOpen->doorOpen);
    TEST_ASSERT(map.GetCell(doorOpen->mapX, doorOpen->mapY) == TileType::Empty);

    player.position = {7.5f, 4.5f};
    SyncPlayerEntity(manager, player);
    UpdateEntityInteractions(manager, map, player, true);
    const Entity* doorStillOpen = manager.GetEntityById(doorId);
    TEST_ASSERT(doorStillOpen != nullptr);
    TEST_ASSERT(doorStillOpen->doorOpen);
    TEST_ASSERT(map.GetCell(doorStillOpen->mapX, doorStillOpen->mapY) == TileType::Empty);

    player.position = {6.6f, 4.5f};
    SyncPlayerEntity(manager, player);
    UpdateEntityInteractions(manager, map, player, true);
    const Entity* doorClosed = manager.GetEntityById(doorId);
    TEST_ASSERT(doorClosed != nullptr);
    TEST_ASSERT(!doorClosed->doorOpen);
    TEST_ASSERT(map.GetCell(doorClosed->mapX, doorClosed->mapY) == TileType::Door);
}

void TestContentFileLoaders() {
    std::vector<std::string> errors;

    WallTextureAssignments wall{};
    SpriteTextureDefinitions sprites{};
    TEST_ASSERT(LoadWallTextureAssignments(ResolveRepoPath("game/defs/wall_textures.cfg"), wall, errors));
    TEST_ASSERT(LoadSpriteTextureDefinitions(ResolveRepoPath("game/defs/sprites.cfg"), sprites, errors));
    TEST_ASSERT(!wall.wall.empty());
    TEST_ASSERT(!sprites.enemy.empty());

    Map map{};
    TEST_ASSERT(LoadMapFromFile(ResolveRepoPath("game/maps/level01.map"), map, errors));
    TEST_ASSERT(map.Width() == 16);
    TEST_ASSERT(map.Height() == 12);
    TEST_ASSERT(map.GetCell(8, 7) == TileType::Door);

    std::vector<EntityPlacement> placements;
    TEST_ASSERT(LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs/level01.entities.csv"), placements, errors));
    TEST_ASSERT(!placements.empty());

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

void TestAssetManagerFallbackLoad() {
    AssetManager assets{};
    Texture fallback{};
    TextureLoader::BuildFallbackChecker(fallback, 8, 8);
    assets.SetTexture("__missing", fallback);

    std::vector<std::string> errors;
    TEST_ASSERT(assets.LoadTextureFromPPM("wall", ResolveRepoPath("game/assets/wall_main.ppm"), errors));
    TEST_ASSERT(assets.GetTexture("wall") != nullptr);

    const bool missingLoaded =
        assets.LoadTextureFromPPM("bad", ResolveRepoPath("game/assets/does_not_exist.ppm"), errors);
    TEST_ASSERT(!missingLoaded);
    TEST_ASSERT(!errors.empty());
}
} // namespace

int main() {
    struct TestCase {
        const char* name;
        void (*fn)();
    };

    const std::vector<TestCase> tests{
        {"Vec2 and math helpers", &TestVec2AndMathHelpers},
        {"InputState transitions", &TestInputStateTransitions},
        {"Map queries", &TestMapQueries},
        {"Raycast hit distance", &TestRaycastHitDistance},
        {"Texture loader and fallback", &TestTextureLoaderAndFallback},
        {"Entity manager basics", &TestEntityManagerBasics},
        {"Player collision rules", &TestPlayerCollisionRules},
        {"Sprite distance sorting", &TestSpriteDistanceSorting},
        {"Entity interaction rules", &TestEntityInteractionRules},
        {"Content file loaders", &TestContentFileLoaders},
        {"Asset manager fallback load", &TestAssetManagerFallbackLoad},
    };

    std::size_t failures = 0;
    for (const TestCase& test : tests) {
        try {
            test.fn();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const std::exception& ex) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": " << ex.what() << '\n';
        } catch (...) {
            ++failures;
            std::cerr << "[FAIL] " << test.name << ": unknown exception\n";
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test case(s) failed.\n";
        return 1;
    }

    std::cout << "All test cases passed.\n";
    return 0;
}
