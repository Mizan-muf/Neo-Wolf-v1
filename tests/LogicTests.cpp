#include <algorithm>
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

#include "engine/assets/AssetManager.h"
#include "engine/assets/AudioConfig.h"
#include "engine/assets/ContentConfig.h"
#include "engine/assets/GameplayConfig.h"
#include "engine/assets/Texture.h"
#include "engine/assets/TextureLoader.h"
#include "engine/core/CommandLine.h"
#include "engine/core/Engine.h"
#include "engine/core/GameState.h"
#include "engine/core/InputState.h"
#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/core/Paths.h"
#include "engine/core/Vec2.h"
#include "engine/interop/CommandBridge.h"
#include "engine/interop/EngineInterop.h"
#include "engine/particles/Particle.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/Raycaster.h"
#include "engine/render/SpriteRenderer.h"
#include "engine/ui/UiRenderer.h"
#include "engine/world/Entity.h"
#include "engine/world/EntityPlacementLoader.h"
#include "engine/world/GameplayDefinitions.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/LevelBuilder.h"
#include "engine/world/Map.h"
#include "engine/world/MapLoader.h"
#include "engine/world/MovementRules.h"
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
    throw TestFailure{out.str()};
}
} // namespace

#define TEST_ASSERT(expr)                                                \
    do {                                                                 \
        if (!(expr)) {                                                   \
            FailTest(__FILE__, __LINE__, "TEST_ASSERT(" #expr ") failed"); \
        }                                                                \
    } while (false)

#define TEST_ASSERT_NEAR(a, b, eps)                                                        \
    do {                                                                                   \
        const auto nwActualA = (a);                                                        \
        const auto nwActualB = (b);                                                        \
        if (!(std::fabs(nwActualA - nwActualB) <= (eps))) {                                \
            std::ostringstream nwDetail;                                                   \
            nwDetail << "TEST_ASSERT_NEAR(" #a ", " #b ", " #eps ") failed: " << nwActualA \
                     << " vs " << nwActualB;                                               \
            FailTest(__FILE__, __LINE__, nwDetail.str());                                  \
        }                                                                                  \
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
    const std::string candidate = "../" + relativePath;
    if (std::filesystem::exists(candidate)) {
        return candidate;
    }
    return relativePath;
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
    const std::string validPath = ResolveRepoPath("game/assets/wall_main.ppm");
    const bool loaded = TextureLoader::LoadPPM(validPath, fileTexture);
    TEST_ASSERT(loaded);
    TEST_ASSERT(fileTexture.Width() > 0);
    TEST_ASSERT(fileTexture.Height() > 0);
}

void TestEntityManagerBasics() {
    EntityManager manager{};
    TEST_ASSERT(manager.Entities().empty());

    Entity& e1 = manager.CreateEntity(EntityType::Enemy, Vec2{3.0f, 4.0f});
    const int id1 = e1.id;
    TEST_ASSERT(id1 > 0);
    TEST_ASSERT(e1.type == EntityType::Enemy);
    TEST_ASSERT_NEAR(e1.position.x, 3.0f, 0.0001f);
    TEST_ASSERT_NEAR(e1.position.y, 4.0f, 0.0001f);

    Entity& e2 = manager.CreateEntity(EntityType::Pickup, Vec2{7.0f, 8.0f});
    TEST_ASSERT(e2.id > id1);
    TEST_ASSERT(manager.Entities().size() == 2);

    Entity* fetched = manager.GetEntityById(id1);
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

    int health = 100;
    int ammo = 24;
    GameplayEvents events{};

    SyncPlayerEntity(manager, player);
    UpdateEntityInteractions(manager, map, player, health, ammo, 100, 99, false, events);
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
    UpdateEntityInteractions(manager, map, player, health, ammo, 100, 99, true, doorOpenEvents);
    const Entity* doorOpen = manager.GetEntityById(doorId);
    TEST_ASSERT(doorOpen != nullptr);
    TEST_ASSERT(doorOpen->doorOpen);
    TEST_ASSERT(map.GetCell(doorOpen->mapX, doorOpen->mapY) == TileType::Empty);

    player.position = {7.5f, 4.5f};
    SyncPlayerEntity(manager, player);
    GameplayEvents doorOverlapEvents{};
    UpdateEntityInteractions(manager, map, player, health, ammo, 100, 99, true, doorOverlapEvents);
    const Entity* doorStillOpen = manager.GetEntityById(doorId);
    TEST_ASSERT(doorStillOpen != nullptr);
    TEST_ASSERT(doorStillOpen->doorOpen);
    TEST_ASSERT(map.GetCell(doorStillOpen->mapX, doorStillOpen->mapY) == TileType::Empty);

    player.position = {6.6f, 4.5f};
    SyncPlayerEntity(manager, player);
    GameplayEvents doorCloseEvents{};
    UpdateEntityInteractions(manager, map, player, health, ammo, 100, 99, true, doorCloseEvents);
    const Entity* doorClosed = manager.GetEntityById(doorId);
    TEST_ASSERT(doorClosed != nullptr);
    TEST_ASSERT(!doorClosed->doorOpen);
    TEST_ASSERT(map.GetCell(doorClosed->mapX, doorClosed->mapY) == TileType::Door);
}

void TestContentFileLoaders() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;

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
    TEST_ASSERT(LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs/level01.entities.json"), placements, errors, warnings));
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
    TEST_ASSERT(assets.LoadTextureFromPPM("wall", ResolveRepoPath("game/assets/wall_main.ppm"), errors));
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
}

void TestPlayerCombatAndShooting() {
    Map map(16, 12);
    BuildBorderWalls(map);

    EntityManager manager{};
    Player player{};
    player.position = {2.0f, 2.0f};
    player.directionAngleRadians = 0.0f; // Facing east (+X)

    Entity& enemy = manager.CreateEntity(EntityType::Enemy, Vec2{5.0f, 2.0f});
    enemy.health = 50;
    enemy.maxHealth = 50;

    int ammo = 10;
    GameplayEvents events{};

    // Shoot enemy: ammo should decrease to 9, enemy takes 35 damage (50 -> 15)
    bool shot = PerformPlayerAttack(manager, map, player, ammo, 35, 12.0f, events);
    TEST_ASSERT(shot);
    TEST_ASSERT(ammo == 9);
    TEST_ASSERT(events.attackHit);
    TEST_ASSERT(events.enemyDamaged);
    TEST_ASSERT(events.damagedEnemyId == enemy.id);
    TEST_ASSERT(enemy.health == 15);
    TEST_ASSERT(enemy.active);

    // Shoot again to kill enemy: ammo 8, enemy health 0, enemy dead and inactive
    GameplayEvents killEvents{};
    shot = PerformPlayerAttack(manager, map, player, ammo, 35, 12.0f, killEvents);
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
    shot = PerformPlayerAttack(manager, map, player, ammo, 35, 12.0f, noAmmoEvents);
    TEST_ASSERT(!shot);
    TEST_ASSERT(ammo == 0);

    // Shoot through wall: wall blocks ray
    ammo = 5;
    Entity& behindWallEnemy = manager.CreateEntity(EntityType::Enemy, Vec2{8.0f, 2.0f});
    behindWallEnemy.health = 50;
    map.SetCell(4, 2, TileType::Wall); // Wall in between at x=4

    GameplayEvents wallEvents{};
    shot = PerformPlayerAttack(manager, map, player, ammo, 35, 12.0f, wallEvents);
    TEST_ASSERT(shot);
    TEST_ASSERT(ammo == 4);
    TEST_ASSERT(!wallEvents.attackHit);
    TEST_ASSERT(!wallEvents.enemyDamaged);
    TEST_ASSERT(behindWallEnemy.health == 50);
}

void TestEnemyAiBehavior() {
    Map map(16, 12);
    BuildBorderWalls(map);

    EntityManager manager{};
    Player player{};
    player.position = {2.0f, 2.0f};

    // Enemy outside alert radius (dist = 10.0 > alertRadius = 5.0)
    Entity& farEnemy = manager.CreateEntity(EntityType::Enemy, Vec2{12.0f, 2.0f});
    farEnemy.alertRadius = 5.0f;
    farEnemy.enemyState = EnemyState::Idle;

    int playerHealth = 100;
    GameplayEvents events{};
    UpdateEnemyAi(manager, map, player, playerHealth, 0.1f, events);

    TEST_ASSERT(farEnemy.enemyState == EnemyState::Idle);
    TEST_ASSERT(!events.enemyAlerted);
    TEST_ASSERT(playerHealth == 100);

    // Enemy inside alert radius (dist = 4.0 <= alertRadius = 6.0)
    Entity& nearEnemy = manager.CreateEntity(EntityType::Enemy, Vec2{6.0f, 2.0f});
    nearEnemy.alertRadius = 6.0f;
    nearEnemy.moveSpeed = 2.0f;
    nearEnemy.enemyState = EnemyState::Idle;

    GameplayEvents alertEvents{};
    UpdateEnemyAi(manager, map, player, playerHealth, 0.5f, alertEvents);

    TEST_ASSERT(nearEnemy.enemyState == EnemyState::Chase);
    TEST_ASSERT(alertEvents.enemyAlerted);
    TEST_ASSERT(nearEnemy.position.x < 6.0f); // Moved closer to player

    // Enemy within attack range (dist = 0.8 <= attackRadius = 1.1)
    Entity& attackingEnemy = manager.CreateEntity(EntityType::Enemy, Vec2{2.5f, 2.0f});
    attackingEnemy.attackRadius = 1.1f;
    attackingEnemy.damage = 15;
    attackingEnemy.enemyState = EnemyState::Chase;
    attackingEnemy.attackCooldown = 0.0f;

    GameplayEvents attackEvents{};
    UpdateEnemyAi(manager, map, player, playerHealth, 0.1f, attackEvents);

    TEST_ASSERT(attackEvents.playerDamaged);
    TEST_ASSERT(attackEvents.playerDamageAmount == 15);
    TEST_ASSERT(playerHealth == 85);
    TEST_ASSERT(attackingEnemy.attackCooldown > 0.0f);
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
    int maxHealth = 100;
    int maxAmmo = 99;

    GameplayEvents events{};
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, maxHealth, maxAmmo, false, events);

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
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, maxHealth, maxAmmo, false, ammoEvents);

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
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
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
    const std::string mapPath = ResolveRepoPath("game/maps/level01.map");
    TEST_ASSERT(LoadMapFromFile(mapPath, map, errors));

    // Multi-cycle load simulation: every build of the same files gives the same level.
    std::vector<std::string> warnings;
    for (int cycle = 0; cycle < 10; ++cycle) {
        Map levelMap = map;
        std::vector<EntityPlacement> placements;
        errors.clear();
        TEST_ASSERT(LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs/level01.entities.json"), placements, errors, warnings));
        LevelContents level{};
        TEST_ASSERT(BuildLevelFromPlacements(levelMap, placements, GameplayDefinitions{}, level, errors));
        TEST_ASSERT(level.entities.Entities().size() == 6);
        TEST_ASSERT(level.sprites.size() == 3);
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
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, 100, 99, false, pickEvents);
    TEST_ASSERT(pickEvents.pickupCollected);

    // Open door
    Entity& door = manager.CreateEntity(EntityType::Door, Vec2{6.0f, 2.0f});
    door.mapX = 6;
    door.mapY = 2;
    map.SetCell(6, 2, TileType::Door);

    player.position = {5.2f, 2.0f};
    GameplayEvents doorEvents{};
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, 100, 99, true, doorEvents);
    TEST_ASSERT(doorEvents.doorToggled);
    TEST_ASSERT(door.doorOpen);
    TEST_ASSERT(map.GetCell(6, 2) == TileType::Empty);

    // Step into victory trigger
    manager.CreateEntity(EntityType::Trigger, Vec2{10.0f, 2.0f});
    player.position = {10.0f, 2.0f};
    GameplayEvents trigEvents{};
    UpdateEntityInteractions(manager, map, player, playerHealth, playerAmmo, 100, 99, false, trigEvents);
    TEST_ASSERT(trigEvents.triggerActivated);
    state = GameState::Victory;
    TEST_ASSERT(state == GameState::Victory);
}

void TestMapPerimeterAndReachabilityValidation() {
    Map map{};
    std::vector<std::string> errors;
    const std::string mapPath = ResolveRepoPath("game/maps/level01.map");
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

    struct GridCoord {
        int x;
        int y;
    };

    // Reachability flood-fill from spawn (2, 2)
    std::vector<bool> visited(map.Width() * map.Height(), false);
    std::vector<GridCoord> queue;
    queue.push_back({2, 2});
    visited[2 * map.Width() + 2] = true;

    int head = 0;
    while (head < static_cast<int>(queue.size())) {
        const GridCoord current = queue[head++];
        const GridCoord neighbors[4] = {
            {current.x + 1, current.y},
            {current.x - 1, current.y},
            {current.x, current.y + 1},
            {current.x, current.y - 1},
        };

        for (const GridCoord& n : neighbors) {
            if (n.x >= 0 && n.x < map.Width() && n.y >= 0 && n.y < map.Height()) {
                const int idx = n.y * map.Width() + n.x;
                if (!visited[idx] && !map.IsWall(n.x, n.y)) {
                    visited[idx] = true;
                    queue.push_back(n);
                }
            }
        }
    }

    int unreachedWalkable = 0;
    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            if (!map.IsWall(x, y) && !visited[y * map.Width() + x]) {
                ++unreachedWalkable;
            }
        }
    }
    TEST_ASSERT(unreachedWalkable == 0);
}

void TestAssetInventoryAndErrorResilience() {
    const std::vector<std::string> textures = {
        "game/assets/wall_main.ppm",
        "game/assets/wall_alt_a.ppm",
        "game/assets/wall_alt_b.ppm",
        "game/assets/floor.ppm",
        "game/assets/ceiling.ppm",
        "game/assets/sprite_enemy.ppm",
        "game/assets/sprite_pickup.ppm",
        "game/assets/sprite_decor.ppm",
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

void TestEngineInteropC_API() {
    void* handle = nw_engine_create();
    TEST_ASSERT(handle != nullptr);

    TEST_ASSERT(nw_engine_get_state(handle) == 0); // Menu
    nw_engine_set_state(handle, 1); // Gameplay
    TEST_ASSERT(nw_engine_get_state(handle) == 1);

    nw_engine_set_player_pos(handle, 4.5f, 3.5f, 1.57f);
    float px = 0.0f, py = 0.0f, pAngle = 0.0f;
    nw_engine_get_player_pos(handle, &px, &py, &pAngle);
    TEST_ASSERT_NEAR(px, 4.5f, 0.0001f);
    TEST_ASSERT_NEAR(py, 3.5f, 0.0001f);
    TEST_ASSERT_NEAR(pAngle, 1.57f, 0.0001f);

    nw_engine_set_player_stats(handle, 80, 42);
    int health = 0, ammo = 0;
    nw_engine_get_player_stats(handle, &health, &ammo);
    TEST_ASSERT(health == 80);
    TEST_ASSERT(ammo == 42);

    const int spawnedId = nw_engine_spawn_entity(handle, 1, 6.0f, 7.0f);
    TEST_ASSERT(spawnedId > 0);
    TEST_ASSERT(nw_engine_get_entity_count(handle) >= 1);

    int entId = 0, entType = 0, entActive = 0;
    float entX = 0.0f, entY = 0.0f;
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

    const std::filesystem::path& Path() const { return path_; }

private:
    std::filesystem::path path_;
};

void MakeContentRoot(const std::filesystem::path& dir) {
    std::filesystem::create_directories(dir / "game" / "maps");
    std::filesystem::create_directories(dir / "game" / "defs");
}

bool CandidateListContains(const std::vector<ContentRootCandidate>& list, const std::filesystem::path& dir) {
    for (const ContentRootCandidate& candidate : list) {
        if (candidate.directory == dir.lexically_normal()) {
            return true;
        }
    }
    return false;
}

void TestContentRootResolution() {
    ScratchDir scratch("content-root");
    const std::filesystem::path base = scratch.Path();
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
    TEST_ASSERT(r.options.level == 1);
    TEST_ASSERT(r.options.frames == 0);
    TEST_ASSERT(r.options.windowMode == WindowModeOption::Default);

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
    };
    for (const std::vector<std::string>& args : invalid) {
        const CommandLineResult bad = ParseCommandLine(args);
        TEST_ASSERT(bad.action == CommandLineAction::Error);
        TEST_ASSERT(!bad.error.empty());
    }
    TEST_ASSERT(ParseCommandLine({"--bogus"}).error.find("--bogus") != std::string::npos);

    // --level padding.
    TEST_ASSERT(LevelMapPath(1) == "game/maps/level01.map");
    TEST_ASSERT(LevelEntitiesPath(1) == "game/defs/level01.entities.json");
    TEST_ASSERT(LevelMapPath(9) == "game/maps/level09.map");
    TEST_ASSERT(LevelMapPath(10) == "game/maps/level10.map");
    TEST_ASSERT(LevelEntitiesPath(99) == "game/defs/level99.entities.json");

    // Version and usage text.
    TEST_ASSERT(VersionString() == std::string("Neo-Wolf ") + NW_EXPECTED_VERSION);
    const std::string usage = CommandLineUsage("neo_wolf");
    for (const char* option : {"--help", "--version", "--data", "--level", "--frames", "--windowed", "--fullscreen"}) {
        TEST_ASSERT(usage.find(option) != std::string::npos);
    }
}

std::string ReadWholeFile(const std::string& path) {
    std::ifstream file(PathFromUtf8(path), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
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
    ScratchDir scratch("crlf");
    std::vector<std::string> errors;

    // Map: the shipped level plus a comment and a blank line, in LF and CRLF form.
    const std::string mapText = "; comment line\n\n" + ReadWholeFile(ResolveRepoPath("game/maps/level01.map"));
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
    TEST_ASSERT(lfMap.Width() == 16 && lfMap.Height() == 12);
    TEST_ASSERT(crlfMap.Width() == lfMap.Width() && crlfMap.Height() == lfMap.Height());
    for (int y = 0; y < lfMap.Height(); ++y) {
        for (int x = 0; x < lfMap.Width(); ++x) {
            TEST_ASSERT(crlfMap.GetCell(x, y) == lfMap.GetCell(x, y));
        }
    }

    // Entity placements, in both formats (JSON canonical, CSV legacy).
    std::vector<std::string> warnings;
    for (const char* entitiesFile : {"level01.entities.json", "level01.entities.csv"}) {
        const std::string lfEntitiesPath = ResolveRepoPath(std::string("game/defs/") + entitiesFile);
        const std::filesystem::path crlfEntitiesPath = scratch.Path() / (std::string("crlf-") + entitiesFile);
        WriteWithCrlf(crlfEntitiesPath, ReadWholeFile(lfEntitiesPath));
        std::vector<EntityPlacement> lfPlacements;
        std::vector<EntityPlacement> crlfPlacements;
        TEST_ASSERT(LoadEntityPlacementsFromFile(lfEntitiesPath, lfPlacements, errors, warnings));
        TEST_ASSERT(LoadEntityPlacementsFromFile(PathToUtf8(crlfEntitiesPath), crlfPlacements, errors, warnings));
        TEST_ASSERT(errors.empty() && warnings.empty());
        TEST_ASSERT(lfPlacements.size() == 6);
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

    const std::filesystem::path crlfWallPath = scratch.Path() / "wall_textures.cfg";
    WriteWithCrlf(crlfWallPath, ReadWholeFile(ResolveRepoPath("game/defs/wall_textures.cfg")));
    WallTextureAssignments lfWall{};
    WallTextureAssignments crlfWall{};
    TEST_ASSERT(LoadWallTextureAssignments(ResolveRepoPath("game/defs/wall_textures.cfg"), lfWall, errors));
    TEST_ASSERT(LoadWallTextureAssignments(PathToUtf8(crlfWallPath), crlfWall, errors));
    TEST_ASSERT(crlfWall.wall == lfWall.wall);
    TEST_ASSERT(crlfWall.door == lfWall.door);
    TEST_ASSERT(crlfWall.ceiling == lfWall.ceiling);
    TEST_ASSERT(errors.empty());
}

void TestLogFileRotation() {
    ScratchDir scratch("log-rotation");
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

const Entity* FindFirstEntity(const EntityManager& manager, EntityType type) {
    for (const Entity& entity : manager.Entities()) {
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
    TEST_ASSERT(g.enemy.damage == 12);
    TEST_ASSERT_NEAR(g.player.moveSpeed, 3.0f, 1e-6f);
    TEST_ASSERT_NEAR(g.enemy.moveSpeed, 1.6f, 1e-6f);
    TEST_ASSERT(g.enemy.health == 50);

    TEST_ASSERT(g.player.initialHealth == 100);
    TEST_ASSERT(g.player.maxHealth == 100);
    TEST_ASSERT_NEAR(g.player.turnSpeed, 1.8f, 1e-6f);
    TEST_ASSERT_NEAR(g.player.collisionRadius, 0.22f, 1e-6f);
    TEST_ASSERT_NEAR(g.weapon.fireRate, 0.25f, 1e-6f);
    TEST_ASSERT_NEAR(g.weapon.range, 12.0f, 1e-6f);
    TEST_ASSERT(g.weapon.initialAmmo == 24);
    TEST_ASSERT(g.weapon.maxAmmo == 99);
    TEST_ASSERT_NEAR(g.enemy.alertRadius, 6.5f, 1e-6f);
    TEST_ASSERT_NEAR(g.enemy.attackRadius, 1.1f, 1e-6f);
    TEST_ASSERT_NEAR(g.enemy.attackRate, 1.0f, 1e-6f);
    TEST_ASSERT(g.pickups.health.value == 25);
    TEST_ASSERT(g.pickups.ammo.value == 12);

    // The same key name in two sections gives two independent values; missing keys keep
    // the documented defaults.
    const std::string partial =
        "{\"enemy\": {\"damage\": 7, \"moveSpeed\": 0.5}, \"weapon\": {\"damage\": 70}, \"player\": {\"moveSpeed\": 4.5}}";
    GameplayDefinitions p{};
    TEST_ASSERT(ParseGameplayDefinitions(partial, "partial.json", p, errors, warnings));
    TEST_ASSERT(p.enemy.damage == 7);
    TEST_ASSERT(p.weapon.damage == 70);
    TEST_ASSERT_NEAR(p.player.moveSpeed, 4.5f, 1e-6f);
    TEST_ASSERT_NEAR(p.enemy.moveSpeed, 0.5f, 1e-6f);
    TEST_ASSERT(p.enemy.health == kDefaultEnemyDefinition.health);
    TEST_ASSERT(p.player.maxHealth == kDefaultPlayerDefinition.maxHealth);
    TEST_ASSERT(p.pickups.ammo.value == kDefaultPickupDefinitions.ammo.value);

    GameplayDefinitions empty{};
    empty.enemy.health = 1;
    TEST_ASSERT(ParseGameplayDefinitions("{}", "empty.json", empty, errors, warnings));
    TEST_ASSERT(empty.enemy.health == kDefaultEnemyDefinition.health);
    TEST_ASSERT(errors.empty() && warnings.empty());
}

void TestGameplayDefinitionsInvalid() {
    struct Case {
        const char* json;
        const char* expected;  // must appear in an error message
    };
    const Case cases[] = {
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
            FailTest(__FILE__, __LINE__, std::string("case ") + c.json + ": expected an error containing '" +
                                             c.expected + "', got:" + all);
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
        "{\"player\": {\"speed\": 9}, \"pickups\": {\"health\": {\"value\": 30, \"maxPlayerHealth\": 100}}, \"music\": 1}";
    TEST_ASSERT(ParseGameplayDefinitions(withUnknown, "old.json", g, errors, warnings));
    TEST_ASSERT(errors.empty());
    TEST_ASSERT(g.pickups.health.value == 30);
    TEST_ASSERT(warnings.size() == 3);
    TEST_ASSERT(AnyMessageContains(warnings, {"old.json: unknown key player.speed ignored"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"pickups.health.maxPlayerHealth", "moved to player.maxHealth"}));
    TEST_ASSERT(AnyMessageContains(warnings, {"unknown key music ignored"}));
}

void TestEntityPlacementsJsonAndCsv() {
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<EntityPlacement> fromJson;
    std::vector<EntityPlacement> fromCsv;
    TEST_ASSERT(LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs/level01.entities.json"), fromJson, errors, warnings));
    TEST_ASSERT(LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs/level01.entities.csv"), fromCsv, errors, warnings));
    TEST_ASSERT(errors.empty());
    TEST_ASSERT(warnings.empty());

    // level01: exactly these six, in file order.
    const PlacementType expectedTypes[] = {PlacementType::Player, PlacementType::Enemy, PlacementType::Pickup,
                                           PlacementType::Door, PlacementType::Trigger, PlacementType::Decoration};
    TEST_ASSERT(fromJson.size() == 6);
    for (std::size_t i = 0; i < fromJson.size(); ++i) {
        TEST_ASSERT(fromJson[i].type == expectedTypes[i]);
    }
    TEST_ASSERT_NEAR(fromJson[0].position.x, 2.5f, 1e-6f);
    TEST_ASSERT_NEAR(fromJson[0].angleDegrees, 0.0f, 1e-6f);
    TEST_ASSERT(fromJson[2].pickupKind == PickupType::Health);
    TEST_ASSERT(!fromJson[2].pickupValue.has_value());
    TEST_ASSERT(fromJson[3].mapX == 8 && fromJson[3].mapY == 7);
    TEST_ASSERT(fromJson[4].mapX == 13 && fromJson[4].mapY == 8);
    TEST_ASSERT(fromJson[2].source.find("entities[2]") != std::string::npos);

    // The CSV export is equivalent, field by field.
    TEST_ASSERT(fromCsv.size() == fromJson.size());
    for (std::size_t i = 0; i < fromJson.size(); ++i) {
        TEST_ASSERT(fromCsv[i].type == fromJson[i].type);
        TEST_ASSERT_NEAR(fromCsv[i].position.x, fromJson[i].position.x, 1e-6f);
        TEST_ASSERT_NEAR(fromCsv[i].position.y, fromJson[i].position.y, 1e-6f);
        TEST_ASSERT(fromCsv[i].mapX == fromJson[i].mapX && fromCsv[i].mapY == fromJson[i].mapY);
        TEST_ASSERT_NEAR(fromCsv[i].angleDegrees, fromJson[i].angleDegrees, 1e-6f);
        TEST_ASSERT(fromCsv[i].pickupKind == fromJson[i].pickupKind);
        TEST_ASSERT(fromCsv[i].pickupValue == fromJson[i].pickupValue);
        TEST_ASSERT(fromCsv[i].variant == fromJson[i].variant);
    }

    // Every optional field, in both formats.
    const std::string fullJson =
        "{\"level\": \"t\", \"entities\": ["
        "{\"type\": \"player\", \"x\": 1.5, \"y\": 1.5, \"angle\": 90},"
        "{\"type\": \"enemy\", \"x\": 3, \"y\": 1.5, \"variant\": \"guard\"},"
        "{\"type\": \"pickup\", \"x\": 2, \"y\": 2, \"kind\": \"ammo\", \"value\": 30}]}";
    const std::string fullCsv =
        "# type,x,y,mapX,mapY,kind,value,angle,variant\n"
        "player,1.5,1.5,,,,,90\n"
        "enemy,3,1.5,,,,,,guard\n"
        "pickup,2,2,,,ammo,30\n";
    TEST_ASSERT(ParseEntityPlacementsJson(fullJson, "t.json", fromJson, errors, warnings));
    TEST_ASSERT(ParseEntityPlacementsCsv(fullCsv, "t.csv", fromCsv, errors, warnings));
    TEST_ASSERT(errors.empty() && warnings.empty());
    TEST_ASSERT(fromJson.size() == 3 && fromCsv.size() == 3);
    for (const std::vector<EntityPlacement>* list : {&fromJson, &fromCsv}) {
        TEST_ASSERT_NEAR((*list)[0].angleDegrees, 90.0f, 1e-6f);
        TEST_ASSERT((*list)[1].variant == "guard");
        TEST_ASSERT((*list)[2].pickupKind == PickupType::Ammo);
        TEST_ASSERT((*list)[2].pickupValue == 30);
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
    TEST_ASSERT(AnyMessageContains(errors, {"entities[2] (pickup): field \"kind\" must be \"health\" or \"ammo\", got \"armor\""}));
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
    TEST_ASSERT(AnyMessageContains(errors, {"bad.entities.csv:3 (entity 1): column \"mapX\" must be an integer, got 'x'"}));
    TEST_ASSERT(AnyMessageContains(errors, {"bad.entities.csv:4 (entity 2): expected at least 3 columns"}));

    errors.clear();
    TEST_ASSERT(!LoadEntityPlacementsFromFile("level01.entities.txt", placements, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"level01.entities.txt: unsupported entity file type"}));
    errors.clear();
    TEST_ASSERT(!LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs") + "/missing.entities.json", placements, errors, warnings));
    TEST_ASSERT(AnyMessageContains(errors, {"Unable to open entity placement file", "missing.entities.json"}));
}

void TestDecorationsAndSpriteSync() {
    TEST_ASSERT(static_cast<int>(EntityType::Decoration) == 5);  // appended: earlier values unchanged
    TEST_ASSERT(static_cast<int>(EntityType::Trigger) == 4);

    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    Map map{};
    TEST_ASSERT(LoadMapFromFile(ResolveRepoPath("game/maps/level01.map"), map, errors));
    std::vector<EntityPlacement> placements;
    TEST_ASSERT(LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs/level01.entities.json"), placements, errors, warnings));
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
    TEST_ASSERT(map.GetCell(13, 8) == TileType::Trigger);
    TEST_ASSERT(map.GetCell(8, 7) == TileType::Door);

    // Standing on the decoration does nothing (it used to be a Trigger and won the level).
    Player player = level.player;
    player.position = decoration->position;
    SyncPlayerEntity(level.entities, player);
    int health = 100;
    int ammo = 24;
    GameplayEvents events{};
    UpdateEntityInteractions(level.entities, map, player, health, ammo, 100, 99, true, events);
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
    UpdateEntityInteractions(level.entities, map, player, health, ammo, 100, 99, false, pickupEvents);
    TEST_ASSERT(pickupEvents.pickupCollected);
    enemy->position = {7.0f, 2.5f};
    for (int tick = 0; tick < 3; ++tick) {
        SyncSpritesWithEntities(level.entities, level.sprites);
    }
    TEST_ASSERT(level.sprites.size() == 3);
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
    TEST_ASSERT(level.sprites.size() == 3);
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
    UpdateEntityInteractions(level.entities, map, player, health, ammo, 100, 99, false, events);
    TEST_ASSERT(events.pickupCollected && events.pickupType == PickupType::Ammo && events.pickupValue == 7);
    TEST_ASSERT(ammo == 17);
    player.position = pickups[3]->position;
    GameplayEvents healthEvents{};
    UpdateEntityInteractions(level.entities, map, player, health, ammo, 100, 99, false, healthEvents);
    TEST_ASSERT(healthEvents.pickupValue == 30);
    TEST_ASSERT(health == 100);
}

// A content root with level01.map and the given gameplay.json and level01.entities.json.
void WriteContentRoot(const std::filesystem::path& root, const std::string& gameplayJson, const std::string& entitiesJson) {
    MakeContentRoot(root);
    std::filesystem::copy_file(PathFromUtf8(ResolveRepoPath("game/maps/level01.map")), root / "game" / "maps" / "level01.map",
                               std::filesystem::copy_options::overwrite_existing);
    WriteTextFile(root / "game" / "defs" / "gameplay.json", gameplayJson);
    WriteTextFile(root / "game" / "defs" / "level01.entities.json", entitiesJson);
}

void TestEditedDataChangesGame() {
    ScratchDir scratch("edited-data");
    const std::filesystem::path root = scratch.Path() / "content";
    const std::string gameplay = R"({
  "player": { "initialHealth": 50, "maxHealth": 150, "moveSpeed": 4.5 },
  "weapon": { "damage": 20, "fireRate": 0.5, "initialAmmo": 7, "maxAmmo": 30 },
  "enemy": { "health": 80, "damage": 9, "alertRadius": 3.0 },
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
    engine.Update(1.0 / 60.0);
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
    TEST_ASSERT(engine.gameplay_.enemy.health == 25);

    engine.running_ = false;
}

void TestLevelLoadFailures() {
    ScratchDir scratch("level-load-failures");
    const std::filesystem::path root = scratch.Path() / "content";
    WriteContentRoot(root, ReadWholeFile(ResolveRepoPath("game/defs/gameplay.json")),
                     ReadWholeFile(ResolveRepoPath("game/defs/level01.entities.json")));
    std::filesystem::copy_file(PathFromUtf8(ResolveRepoPath("game/defs/level01.entities.csv")),
                               root / "game" / "defs" / "level01.entities.csv");

    Engine engine;
    engine.contentRoot_ = root;
    TEST_ASSERT(engine.LoadGameplay("game/defs/gameplay.json"));
    const std::string mapPath = "game/maps/level01.map";
    TEST_ASSERT(engine.LoadLevel(mapPath, "game/defs/level01.entities.json"));
    TEST_ASSERT(engine.entityManager_.Entities().size() == 6);
    TEST_ASSERT(engine.LoadLevel(mapPath, "game/defs/level01.entities.csv"));  // legacy format still loads
    TEST_ASSERT(engine.entityManager_.Entities().size() == 6);

    const auto writeEntities = [&](const std::string& name, const std::string& text) {
        WriteTextFile(root / "game" / "defs" / name, text);
        return "game/defs/" + name;
    };
    const std::string player = "{\"type\": \"player\", \"x\": 2.5, \"y\": 2.5}";
    const std::string failing[] = {
        "game/defs/missing.entities.json",  // no built-in layout any more
        writeEntities("syntax.entities.json", "{\"entities\": [ {\"type\": \"player\", \"x\": 2.5 "),
        writeEntities("nokind.entities.json",
                      "{\"entities\": [" + player + ", {\"type\": \"pickup\", \"x\": 3.5, \"y\": 2.5}]}"),
        writeEntities("noplayer.entities.json", "{\"entities\": [{\"type\": \"enemy\", \"x\": 3.5, \"y\": 2.5}]}"),
        writeEntities("twoplayers.entities.json", "{\"entities\": [" + player + ", " + player + "]}"),
        writeEntities("outside.entities.json",
                      "{\"entities\": [" + player + ", {\"type\": \"enemy\", \"x\": 30.5, \"y\": 2.5}]}"),
        writeEntities("badcell.entities.json",
                      "{\"entities\": [" + player + ", {\"type\": \"door\", \"x\": 8.5, \"y\": 7.5, \"mapX\": 16, \"mapY\": 7}]}"),
    };
    for (const std::string& entitiesPath : failing) {
        TEST_ASSERT(!engine.LoadLevel(mapPath, entitiesPath));
        // A failed load leaves the current level as it was.
        TEST_ASSERT(engine.entityManager_.Entities().size() == 6);
        TEST_ASSERT(engine.currentEntitiesPath_ == "game/defs/level01.entities.csv");
        TEST_ASSERT(engine.map_.GetCell(8, 7) == TileType::Door);
    }
    TEST_ASSERT(!engine.LoadLevel("game/maps/missing.map", "game/defs/level01.entities.json"));

    // Builder messages name the file, the entity and the problem.
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    Map map{};
    TEST_ASSERT(LoadMapFromFile(PathToUtf8(root / "game" / "maps" / "level01.map"), map, errors));
    std::vector<EntityPlacement> placements;
    TEST_ASSERT(LoadEntityPlacementsFromFile(PathToUtf8(root / "game" / "defs" / "badcell.entities.json"), placements, errors, warnings));
    LevelContents level{};
    const Map before = map;
    TEST_ASSERT(!BuildLevelFromPlacements(map, placements, GameplayDefinitions{}, level, errors));
    TEST_ASSERT(AnyMessageContains(errors, {"badcell.entities.json: entities[1] (door): map cell (16, 7) is outside the 16x12 map"}));
    TEST_ASSERT(map.GetCell(8, 7) == before.GetCell(8, 7));
    errors.clear();
    TEST_ASSERT(LoadEntityPlacementsFromFile(PathToUtf8(root / "game" / "defs" / "noplayer.entities.json"), placements, errors, warnings));
    TEST_ASSERT(!BuildLevelFromPlacements(map, placements, GameplayDefinitions{}, level, errors));
    TEST_ASSERT(AnyMessageContains(errors, {"no player placement"}));
}

struct TestCase {
    const char* id;    // used on the command line and as the CTest name (native.<id>)
    const char* name;  // human-readable description
    void (*fn)();
};

// Keep in sync with NEOWOLF_NATIVE_TESTS in CMakeLists.txt.
const TestCase kTests[] = {
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
    {"engine_interop_c_api", "Engine Interop C API", &TestEngineInteropC_API},
    {"content_root_resolution", "Content root resolution (candidates, sentinel, searched list)",
     &TestContentRootResolution},
    {"command_line_parsing", "Command-line parsing", &TestCommandLineParsing},
    {"crlf_content_loading", "CRLF content files load like LF ones", &TestCrlfContentLoading},
    {"log_file_rotation", "Log file rotation", &TestLogFileRotation},
    {"gameplay_definitions_exact", "gameplay.json exact values per section", &TestGameplayDefinitionsExactValues},
    {"gameplay_definitions_invalid", "Malformed or invalid gameplay.json is rejected", &TestGameplayDefinitionsInvalid},
    {"entity_placements_json_csv", "JSON entity placements, errors, CSV equivalence", &TestEntityPlacementsJsonAndCsv},
    {"decorations_and_sprite_sync", "Decorations are not triggers; sprites survive updates", &TestDecorationsAndSpriteSync},
    {"pickup_kind_from_data", "Pickup kind and value come from data", &TestPickupKindFromData},
    {"edited_data_changes_game", "Edited gameplay and entity files change the game", &TestEditedDataChangesGame},
    {"level_load_failures", "Missing or invalid entity files fail the level load", &TestLevelLoadFailures},
};

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
} // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> filters;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--list") {
            for (const TestCase& test : kTests) {
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
        std::vector<const TestCase*> matches;
        for (const TestCase& test : kTests) {
            if (filter == test.id) {
                matches = {&test};
                break;
            }
            if (Matches(test, filter)) {
                matches.push_back(&test);
            }
        }
        if (matches.empty()) {
            std::cerr << "No test case matches '" << filter << "' (use --list to see test ids).\n";
            return 1;
        }
        for (const TestCase* match : matches) {
            if (std::find(selected.begin(), selected.end(), match) == selected.end()) {
                selected.push_back(match);
            }
        }
    }
    if (filters.empty()) {
        for (const TestCase& test : kTests) {
            selected.push_back(&test);
        }
    }

    int passed = 0;
    std::vector<std::string> failures;
    for (const TestCase* test : selected) {
        std::cout << "[RUN ] " << test->id << " (" << test->name << ")\n" << std::flush;
        std::string error;
        try {
            test->fn();
        } catch (const TestFailure& failure) {
            error = failure.message;
        } catch (const std::exception& ex) {
            error = std::string("unexpected exception: ") + ex.what();
        } catch (...) {
            error = "unexpected non-standard exception";
        }
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
