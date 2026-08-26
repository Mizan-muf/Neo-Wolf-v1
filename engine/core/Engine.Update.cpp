#include "engine/core/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/platform/PlatformSDL.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/MovementRules.h"

void Engine::Update(double deltaSeconds) {
    if (!running_) {
        return;
    }

    // Process incoming interop commands
    bridge_.ProcessCommands(*this);

    if (platform_ != nullptr) {
        input_.BeginFrame();
        platform_->PumpEvents(input_);
        if (input_.quitRequested) {
            running_ = false;
            return;
        }
    }

    time_.deltaSeconds = deltaSeconds;
    time_.totalSeconds += deltaSeconds;
    time_.frameIndex += 1;
    const float dt = static_cast<float>(deltaSeconds);

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

    // State machine updates
    if (state_ == GameState::Menu) {
        if (input_.WasPressed(InputAction::Confirm) ||
            input_.WasPressed(InputAction::Interact) ||
            input_.WasPressed(InputAction::Attack)) {
            SetState(GameState::Gameplay);
        }
        return;
    }

    if (state_ == GameState::Paused) {
        if (input_.WasPressed(InputAction::Pause) || input_.WasPressed(InputAction::Confirm)) {
            SetState(GameState::Gameplay);
        } else if (input_.WasPressed(InputAction::Restart)) {
            ResetLevel();
            SetState(GameState::Gameplay);
        }
        return;
    }

    if (state_ == GameState::GameOver || state_ == GameState::Victory) {
        if (input_.WasPressed(InputAction::Restart)) {
            ResetLevel();
            SetState(GameState::Gameplay);
        } else if (input_.WasPressed(InputAction::Confirm) || input_.WasPressed(InputAction::Pause)) {
            SetState(GameState::Menu);
        }
        return;
    }

    // Active Gameplay State
    if (input_.WasPressed(InputAction::Pause)) {
        SetState(GameState::Paused);
        return;
    }

    if (input_.WasPressed(InputAction::Restart)) {
        ResetLevel();
        return;
    }

    if (input_.WasPressed(InputAction::ToggleDebugView)) {
        debugTopDownEnabled_ = !debugTopDownEnabled_;
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
        player_.position = MoveWithSliding(map_, player_.position, moveDelta, player_.collisionRadius);
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
        events);

    if (events.pickupCollected) {
        audio_.PlayEffect("pickup");
        particleManager_.SpawnPickupSparkle(player_.position);
        if (events.pickupType == PickupType::Health) {
            ShowMessage("HEALTH RESTORED (+25 HP)", 1.5f);
        } else {
            ShowMessage("AMMO REPLENISHED (+10 AMMO)", 1.5f);
        }

        InteropEvent pEvt{};
        pEvt.type = InteropEventType::PickupCollected;
        pEvt.intArg1 = static_cast<int>(events.pickupType);
        pEvt.intArg2 = events.pickupValue;
        bridge_.PushEvent(pEvt);
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
        ShowMessage("OBJECTIVE COMPLETED! VICTORY!", 3.0f);
        InteropEvent tEvt{};
        tEvt.type = InteropEventType::TriggerActivated;
        tEvt.intArg1 = events.triggerId;
        bridge_.PushEvent(tEvt);
        SetState(GameState::Victory);
        return;
    }

    // Enemy AI & Combat
    UpdateEnemyAi(entityManager_, map_, player_, playerHealth_, dt, events);

    if (events.enemyAlerted && enemyAlertCooldown_ <= 0.0f) {
        audio_.PlayEffect("enemy_alert");
        enemyAlertCooldown_ = 2.0f;
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
            SetState(GameState::GameOver);
            return;
        }
    }

    // Sync Sprites
    sprites_.clear();
    for (const Entity& ent : entityManager_.Entities()) {
        if (!ent.active || !ent.visible) {
            continue;
        }
        if (ent.type == EntityType::Enemy) {
            sprites_.push_back(SpriteEntity{ent.position, 0, ent.id, SpriteType::Enemy, true, true});
        } else if (ent.type == EntityType::Pickup) {
            sprites_.push_back(SpriteEntity{ent.position, 1, ent.id, SpriteType::Pickup, true, true});
        }
    }

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
