#include "engine/core/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/platform/PlatformSDL.h"
#include "engine/world/EnemyAi.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/LevelBuilder.h"
#include "engine/world/MovementRules.h"

namespace {
// A random offset in [-spread / 2, spread / 2), in steps of spread / 100.
float RandomDustOffset(float spread) {
    // NOLINTNEXTLINE(misc-predictable-rand): decoration only; another generator would change the dust layout
    return (static_cast<float>(std::rand() % 100) / 100.0f - 0.5f) * spread;
}
}  // namespace

void Engine::Update(double deltaSeconds) {
    if (!running_) {
        return;
    }

    // Commands queued through the C API run first, so this frame already sees their effect.
    bridge_.ProcessCommands(*this);

    input_.BeginFrame();
    if (platform_ != nullptr) {
        platform_->PumpEvents(input_);
    }
    if (inputInjector_) {
        inputInjector_(input_);
    }
    if (input_.quitRequested) {
        LogInfo("Quit requested (window closed or quit shortcut).");
        running_ = false;
        return;
    }

    time_.deltaSeconds = deltaSeconds;
    time_.totalSeconds += deltaSeconds;
    time_.frameIndex += 1;
    const auto dt = static_cast<float>(deltaSeconds);

    if (simulationFrozen_) {
        // A preview shows a still image; Escape still quits.
        if (input_.WasPressed(InputAction::Back)) {
            running_ = false;
        }
        return;
    }

    if (hudMessageTimer_ > 0.0f) {
        hudMessageTimer_ = std::max(0.0f, hudMessageTimer_ - dt);
        if (hudMessageTimer_ <= 0.0f) {
            hudMessage_.clear();
        }
    }

    if (playerAttackCooldown_ > 0.0f) {
        playerAttackCooldown_ = std::max(0.0f, playerAttackCooldown_ - dt);
    }

    if (enemyAlertCooldown_ > 0.0f) {
        enemyAlertCooldown_ = std::max(0.0f, enemyAlertCooldown_ - dt);
    }

    // The simulation runs only during play; the game flow (menus, pause, end screens) decides
    // what happens next and asks for level loads, saves or quitting.
    FlowEvents flowEvents{};
    flowEvents.focusLost = input_.focusLost;
    if (flow_.State() == GameState::Gameplay) {
        UpdateGameplay(dt, flowEvents);
    }
    if (flowEvents.focusLost && flow_.State() == GameState::Gameplay) {
        LogInfo("Window lost focus: pausing.");
    }
    ExecuteFlowRequests(flow_.Update(input_, flowEvents));
    PublishStateChange();
}

void Engine::UpdateGameplay(float dt, FlowEvents& flowEvents) {
    levelTimeSeconds_ += static_cast<double>(dt);
    if (levelBannerTimer_ > 0.0f) {
        levelBannerTimer_ = std::max(0.0f, levelBannerTimer_ - dt);
    }

    if (input_.WasPressed(InputAction::ToggleMinimap)) {
        minimapVisible_ = !minimapVisible_;
    }
    if (input_.WasPressed(InputAction::ToggleDebugView)) {
        debugOverlayVisible_ = !debugOverlayVisible_;
    }

    if (input_.WasPressed(InputAction::Attack)) {
        PlayerAttack();
    }

    MovePlayer(dt);
    SyncPlayerEntity(entityManager_, player_);

    GameplayEvents events{};
    UpdateEntityInteractions(entityManager_, map_, player_, playerHealth_, playerAmmo_, playerMaxHealth_,
                             playerMaxAmmo_, input_.WasPressed(InputAction::Interact), keys_, events);
    ReportInteractionEvents(events);
    if (events.exitActivated) {
        CompleteLevel(events, flowEvents);
        return;
    }

    // Enemy AI & Combat (the last shot is heard once, then forgotten).
    const int healthBeforeAttacks = playerHealth_;
    UpdateEnemyAi(entityManager_, map_, player_, playerHealth_, dt, events, &pendingNoise_);
    levelDamageTaken_ += std::max(0, healthBeforeAttacks - playerHealth_);
    pendingNoise_ = NoiseEvent{};

    if (events.enemyAlerted && PlayEnemyAlertSound()) {
        PushInteropEvent(InteropEventType::EnemyAlerted);
    }
    if (ReportPlayerDamage(events, flowEvents)) {
        return;
    }

    // Sprites follow their entities (positions, hidden when collected or dead).
    SyncSpritesWithEntities(entityManager_, sprites_);
    particleManager_.Update(dt);
    UpdateAmbientDust(dt);
}

void Engine::MovePlayer(float dt) {
    float turnDelta = 0.0f;
    if (input_.IsDown(InputAction::TurnLeft)) {
        turnDelta -= 1.0f;
    }
    if (input_.IsDown(InputAction::TurnRight)) {
        turnDelta += 1.0f;
    }
    player_.directionAngleRadians =
        NormalizeAngle(player_.directionAngleRadians + turnDelta * player_.turnSpeedRadians * dt);

    Vec2 wishDir{0.0f, 0.0f};
    const Vec2 forward{std::cos(player_.directionAngleRadians), std::sin(player_.directionAngleRadians)};
    const Vec2 right{-forward.y, forward.x};

    if (input_.IsDown(InputAction::MoveForward)) {
        wishDir = wishDir + forward;
    }
    if (input_.IsDown(InputAction::MoveBackward)) {
        wishDir = wishDir - forward;
    }
    if (input_.IsDown(InputAction::StrafeRight)) {
        wishDir = wishDir + right;
    }
    if (input_.IsDown(InputAction::StrafeLeft)) {
        wishDir = wishDir - right;
    }

    // Opposite keys cancel out; below this squared length there is no direction to walk in.
    constexpr float kMinWishLengthSquared = 1e-6f;
    const float lengthSq = wishDir.x * wishDir.x + wishDir.y * wishDir.y;
    if (lengthSq > kMinWishLengthSquared) {
        const float invLen = 1.0f / std::sqrt(lengthSq);
        wishDir = wishDir * invLen;
        const Vec2 moveDelta = wishDir * (player_.moveSpeed * dt);
        // Walls, closed doors and living enemies stop the player (C15); corpses do not.
        player_.position = MoveWithSlidingAndBodies(map_, player_.position, moveDelta, player_.collisionRadius,
                                                    LivingEnemyBodies(entityManager_));
    }
}

void Engine::ReportInteractionEvents(const GameplayEvents& events) {
    if (events.pickupCollected) {
        audio_.PlayEffect("pickup");
        particleManager_.SpawnPickupSparkle(player_.position);
        const std::string amount = std::to_string(events.pickupValue);
        if (events.pickupType == PickupType::Health) {
            ShowMessage("HEALTH RESTORED (+" + amount + " HP)", 1.5f);
        } else if (events.pickupType == PickupType::Ammo) {
            ShowMessage("AMMO REPLENISHED (+" + amount + " AMMO)", 1.5f);
        } else {
            ShowMessage(std::string(KeyColorLabel(events.pickupKey)) + " KEY CARD", 2.0f);
        }
        PushInteropEvent(InteropEventType::PickupCollected, static_cast<int>(events.pickupType), events.pickupValue);
    }

    if (events.doorNeedsKey != KeyColor::None) {
        ShowMessage("NEED " + std::string(KeyColorLabel(events.doorNeedsKey)) + " KEY", 1.5f);
    } else if (events.doorBlocked) {
        ShowMessage("BLOCKED", 1.0f);
    }

    if (events.doorToggled) {
        audio_.PlayEffect("door_open");
        PushInteropEvent(InteropEventType::DoorToggled, events.doorMapX, events.doorMapY);
    }

    if (events.triggerActivated) {
        // A walk-over marker: an interop event and a HUD message. It does not end the level.
        ShowMessage("CHECKPOINT REACHED", 2.0f);
        PushInteropEvent(InteropEventType::TriggerActivated, events.triggerId);
    }
}

void Engine::CompleteLevel(const GameplayEvents& events, FlowEvents& flowEvents) {
    const LevelResults results = CurrentLevelResults();
    LogInfo("Exit switch at (" + std::to_string(events.exitMapX) + ", " + std::to_string(events.exitMapY) +
            ") used: level complete (enemies " + std::to_string(results.kills) + "/" +
            std::to_string(results.totalEnemies) + ", pickups " + std::to_string(results.pickups) + "/" +
            std::to_string(results.totalPickups) + ", score " + std::to_string(results.score) + ", health " +
            std::to_string(playerHealth_) + ", ammo " + std::to_string(playerAmmo_) + ").");
    ShowMessage("EXIT SWITCH ACTIVATED", 2.0f);
    InteropEvent levelEvent{};
    levelEvent.type = InteropEventType::LevelCompleted;
    levelEvent.intArg1 = currentLevelIndex_ + 1;
    levelEvent.intArg2 = playerHealth_;
    levelEvent.floatArg1 = static_cast<float>(results.seconds);
    bridge_.PushEvent(levelEvent);
    flowEvents.levelCompleted = true;
    flowEvents.stats = CurrentStats();
    flowEvents.results = results;
}

bool Engine::ReportPlayerDamage(const GameplayEvents& events, FlowEvents& flowEvents) {
    if (!events.playerDamaged) {
        return false;
    }
    audio_.PlayEffect("enemy_alert");
    particleManager_.SpawnHitSpark(player_.position);
    ShowMessage("UNDER ATTACK! (-" + std::to_string(events.playerDamageAmount) + " HP)", 1.0f);
    PushInteropEvent(InteropEventType::PlayerDamaged, events.playerDamageAmount);

    if (!events.playerDied && playerHealth_ > 0) {
        return false;
    }
    playerHealth_ = 0;
    ShowMessage("YOU DIED!", 3.0f);
    PushInteropEvent(InteropEventType::PlayerDied);
    flowEvents.playerDied = true;
    return true;
}

// Cosmetic dust around the player, a few times a second.
void Engine::UpdateAmbientDust(float dt) {
    constexpr float kSpawnIntervalSeconds = 0.25f;
    constexpr float kSpreadTiles = 4.0f;  // a square this wide, centred on the player
    ambientDustSpawnTimer_ += dt;
    if (ambientDustSpawnTimer_ >= kSpawnIntervalSeconds) {
        ambientDustSpawnTimer_ = 0.0f;
        const float ox = RandomDustOffset(kSpreadTiles);
        const float oy = RandomDustOffset(kSpreadTiles);
        const Vec2 spawnPos = player_.position + Vec2{ox, oy};
        if (!map_.IsWall(static_cast<int>(std::floor(spawnPos.x)), static_cast<int>(std::floor(spawnPos.y)))) {
            particleManager_.SpawnAmbientDust(spawnPos);
        }
    }
}
