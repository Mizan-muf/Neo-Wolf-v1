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

void Engine::Update(double deltaSeconds) {
    if (!running_) {
        return;
    }

    // Process incoming interop commands
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
    const float dt = static_cast<float>(deltaSeconds);

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

    // Player Turning
    float turnDelta = 0.0f;
    if (input_.IsDown(InputAction::TurnLeft)) {
        turnDelta -= 1.0f;
    }
    if (input_.IsDown(InputAction::TurnRight)) {
        turnDelta += 1.0f;
    }
    player_.directionAngleRadians = NormalizeAngle(player_.directionAngleRadians + turnDelta * player_.turnSpeedRadians * dt);

    // Player Movement
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

    const float lengthSq = wishDir.x * wishDir.x + wishDir.y * wishDir.y;
    if (lengthSq > 1e-6f) {
        const float invLen = 1.0f / std::sqrt(lengthSq);
        wishDir = wishDir * invLen;
        const Vec2 moveDelta = wishDir * (player_.moveSpeed * dt);
        // Walls, closed doors and living enemies stop the player (C15); corpses do not.
        player_.position = MoveWithSlidingAndBodies(map_, player_.position, moveDelta, player_.collisionRadius,
                                                    LivingEnemyBodies(entityManager_));
    }

    SyncPlayerEntity(entityManager_, player_);

    // Entity Interactions (Pickups, Doors, Triggers)
    GameplayEvents events{};
    UpdateEntityInteractions(
        entityManager_,
        map_,
        player_,
        playerHealth_,
        playerAmmo_,
        playerMaxHealth_,
        playerMaxAmmo_,
        input_.WasPressed(InputAction::Interact),
        keys_,
        events);

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

        InteropEvent pEvt{};
        pEvt.type = InteropEventType::PickupCollected;
        pEvt.intArg1 = static_cast<int>(events.pickupType);
        pEvt.intArg2 = events.pickupValue;
        bridge_.PushEvent(pEvt);
    }

    if (events.doorNeedsKey != KeyColor::None) {
        ShowMessage("NEED " + std::string(KeyColorLabel(events.doorNeedsKey)) + " KEY", 1.5f);
    } else if (events.doorBlocked) {
        ShowMessage("BLOCKED", 1.0f);
    }

    if (events.doorToggled) {
        audio_.PlayEffect("door_open");
        InteropEvent dEvt{};
        dEvt.type = InteropEventType::DoorToggled;
        dEvt.intArg1 = events.doorMapX;
        dEvt.intArg2 = events.doorMapY;
        bridge_.PushEvent(dEvt);
    }

    if (events.triggerActivated) {
        // A walk-over marker: an interop event and a HUD message. It does not end the level.
        ShowMessage("CHECKPOINT REACHED", 2.0f);
        InteropEvent tEvt{};
        tEvt.type = InteropEventType::TriggerActivated;
        tEvt.intArg1 = events.triggerId;
        bridge_.PushEvent(tEvt);
    }

    if (events.exitActivated) {
        const LevelResults results = CurrentLevelResults();
        LogInfo("Exit switch at (" + std::to_string(events.exitMapX) + ", " + std::to_string(events.exitMapY) +
                ") used: level complete (enemies " + std::to_string(results.kills) + "/" +
                std::to_string(results.totalEnemies) + ", pickups " + std::to_string(results.pickups) + "/" +
                std::to_string(results.totalPickups) + ", score " + std::to_string(results.score) + ", health " +
                std::to_string(playerHealth_) + ", ammo " + std::to_string(playerAmmo_) + ").");
        ShowMessage("EXIT SWITCH ACTIVATED", 2.0f);
        InteropEvent lEvt{};
        lEvt.type = InteropEventType::LevelCompleted;
        lEvt.intArg1 = currentLevelIndex_ + 1;
        lEvt.intArg2 = playerHealth_;
        lEvt.floatArg1 = static_cast<float>(results.seconds);
        bridge_.PushEvent(lEvt);
        flowEvents.levelCompleted = true;
        flowEvents.stats = CurrentStats();
        flowEvents.results = results;
        return;
    }

    // Enemy AI & Combat (the last shot is heard once, then forgotten).
    const int healthBeforeAttacks = playerHealth_;
    UpdateEnemyAi(entityManager_, map_, player_, playerHealth_, dt, events, &pendingNoise_);
    levelDamageTaken_ += std::max(0, healthBeforeAttacks - playerHealth_);
    pendingNoise_ = NoiseEvent{};

    if (events.enemyAlerted && enemyAlertCooldown_ <= 0.0f) {
        audio_.PlayEffect("enemy_alert");
        enemyAlertCooldown_ = kEnemyAlertSoundCooldown;
        InteropEvent aEvt{};
        aEvt.type = InteropEventType::EnemyAlerted;
        bridge_.PushEvent(aEvt);
    }

    if (events.playerDamaged) {
        audio_.PlayEffect("enemy_alert");
        particleManager_.SpawnHitSpark(player_.position);
        ShowMessage("UNDER ATTACK! (-" + std::to_string(events.playerDamageAmount) + " HP)", 1.0f);

        InteropEvent dmgEvt{};
        dmgEvt.type = InteropEventType::PlayerDamaged;
        dmgEvt.intArg1 = events.playerDamageAmount;
        bridge_.PushEvent(dmgEvt);

        if (events.playerDied || playerHealth_ <= 0) {
            playerHealth_ = 0;
            ShowMessage("YOU DIED!", 3.0f);
            InteropEvent dieEvt{};
            dieEvt.type = InteropEventType::PlayerDied;
            bridge_.PushEvent(dieEvt);
            flowEvents.playerDied = true;
            return;
        }
    }

    // Sprites follow their entities (positions, hidden when collected or dead).
    SyncSpritesWithEntities(entityManager_, sprites_);

    // Update Particles
    particleManager_.Update(dt);

    // Ambient Dust
    ambientDustSpawnTimer_ += dt;
    if (ambientDustSpawnTimer_ >= 0.25f) {
        ambientDustSpawnTimer_ = 0.0f;
        const float ox = (static_cast<float>(std::rand() % 100) / 100.0f - 0.5f) * 4.0f;
        const float oy = (static_cast<float>(std::rand() % 100) / 100.0f - 0.5f) * 4.0f;
        const Vec2 spawnPos = player_.position + Vec2{ox, oy};
        if (!map_.IsWall(static_cast<int>(std::floor(spawnPos.x)), static_cast<int>(std::floor(spawnPos.y)))) {
            particleManager_.SpawnAmbientDust(spawnPos);
        }
    }
}
