#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "engine/assets/AssetManager.h"
#include "engine/assets/AudioConfig.h"
#include "engine/assets/ContentConfig.h"
#include "engine/assets/Texture.h"
#include "engine/assets/TextureLoader.h"
#include "engine/core/GameState.h"
#include "engine/core/InputState.h"
#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
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
#include "engine/world/InteractionRules.h"
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

    constexpr float kPi = 3.14159265358979323846f;
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

    WallTextureAssignments wall{};
    SpriteTextureDefinitions sprites{};
    GameplayDefinitions gameplay{};
    TEST_ASSERT(LoadWallTextureAssignments(ResolveRepoPath("game/defs/wall_textures.cfg"), wall, errors));
    TEST_ASSERT(LoadSpriteTextureDefinitions(ResolveRepoPath("game/defs/sprites.cfg"), sprites, errors));
    TEST_ASSERT(LoadGameplayDefinitions(ResolveRepoPath("game/defs/gameplay.json"), gameplay, errors));
    TEST_ASSERT(!wall.wall.empty());
    TEST_ASSERT(!sprites.enemy.empty());
    TEST_ASSERT(gameplay.enemyHealth > 0);
    TEST_ASSERT(gameplay.weaponDamage > 0);

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

    // Multi-cycle load simulation
    for (int cycle = 0; cycle < 10; ++cycle) {
        EntityManager manager{};
        std::vector<EntityPlacement> placements;
        errors.clear();
        TEST_ASSERT(LoadEntityPlacementsFromFile(ResolveRepoPath("game/defs/level01.entities.csv"), placements, errors));
        for (const auto& p : placements) {
            if (p.type == PlacementType::Player) {
                manager.CreateEntity(EntityType::Player, p.position);
            } else if (p.type == PlacementType::Enemy) {
                manager.CreateEntity(EntityType::Enemy, p.position);
            } else if (p.type == PlacementType::Pickup) {
                manager.CreateEntity(EntityType::Pickup, p.position);
            }
        }
        TEST_ASSERT(manager.Entities().size() >= 3);
        manager.Clear();
        TEST_ASSERT(manager.Entities().empty());
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
