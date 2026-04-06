#include "engine/core/Engine.h"

#include <cmath>
#include <vector>

#include "engine/core/MathUtils.h"
#include "engine/platform/PlatformSDL.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/MovementRules.h"

void Engine::Update(double deltaSeconds) {
    if (!running_ || platform_ == nullptr) {
        return;
    }

    if (deltaSeconds < 0.0) {
        deltaSeconds = 0.0;
    }

    time_.deltaSeconds = deltaSeconds;
    time_.totalSeconds += deltaSeconds;
    ++time_.frameIndex;

    input_.BeginFrame();
    platform_->PumpEvents(input_);

    if (input_.quitRequested) {
        running_ = false;
        return;
    }

    if (input_.WasPressed(InputAction::ToggleDebugView)) {
        debugTopDownEnabled_ = !debugTopDownEnabled_;
    }
    if (input_.WasPressed(InputAction::Pause)) {
        paused_ = !paused_;
    }

    if (map_.Width() <= 0 || map_.Height() <= 0) {
        return;
    }

    const float dt = static_cast<float>(time_.deltaSeconds);
    if (dt <= 0.0f) {
        return;
    }

    if (enemyAlertCooldown_ > 0.0f) {
        enemyAlertCooldown_ -= dt;
    }

    if (paused_) {
        return;
    }

    float turnInput = 0.0f;
    if (input_.IsDown(InputAction::TurnLeft)) {
        turnInput -= 1.0f;
    }
    if (input_.IsDown(InputAction::TurnRight)) {
        turnInput += 1.0f;
    }
    player_.directionAngleRadians = NormalizeAngleRadians(
        player_.directionAngleRadians + turnInput * player_.turnSpeedRadians * dt);

    const Vec2 forward{
        std::cos(player_.directionAngleRadians),
        std::sin(player_.directionAngleRadians)};
    const Vec2 right{-forward.y, forward.x};

    Vec2 desiredMove{};
    if (input_.IsDown(InputAction::MoveForward)) {
        desiredMove = Add(desiredMove, forward);
    }
    if (input_.IsDown(InputAction::MoveBackward)) {
        desiredMove = Subtract(desiredMove, forward);
    }
    if (input_.IsDown(InputAction::StrafeRight)) {
        desiredMove = Add(desiredMove, right);
    }
    if (input_.IsDown(InputAction::StrafeLeft)) {
        desiredMove = Subtract(desiredMove, right);
    }

    if (Length(desiredMove) > 0.0001f) {
        const Vec2 beforeMove = player_.position;
        desiredMove = Normalize(desiredMove);
        const Vec2 displacement = Scale(desiredMove, player_.moveSpeed * dt);
        ResolveCollisionAgainstWalls(map_, player_, displacement);

        const float movedSq = DistanceSquared(beforeMove, player_.position);
        if (movedSq < 0.0001f) {
            const Vec2 sparkPos = Add(player_.position, Scale(forward, 0.32f));
            particleManager_.SpawnHitSpark(sparkPos);
        }
    }

    struct EntityStateSnapshot {
        int id = -1;
        bool collected = false;
        bool doorOpen = false;
    };
    std::vector<EntityStateSnapshot> beforeStates;
    beforeStates.reserve(entityManager_.Entities().size());
    for (const Entity& e : entityManager_.Entities()) {
        beforeStates.push_back({e.id, e.collected, e.doorOpen});
    }

    SyncPlayerEntity(entityManager_, player_);
    UpdateEntityInteractions(entityManager_, map_, player_, input_.WasPressed(InputAction::Interact));

    for (const EntityStateSnapshot& snapshot : beforeStates) {
        const Entity* after = entityManager_.GetEntityById(snapshot.id);
        if (after == nullptr) {
            continue;
        }

        if (!snapshot.collected && after->collected) {
            particleManager_.SpawnPickupSparkle(after->position);
            audio_.PlayEffect("pickup");
        }
        if (snapshot.doorOpen != after->doorOpen) {
            particleManager_.SpawnHitSpark(after->position);
            audio_.PlayEffect("door_open");
        }
    }

    if (enemyAlertCooldown_ <= 0.0f) {
        constexpr float kEnemyAlertRangeSq = 3.75f * 3.75f;
        bool shouldAlert = false;
        for (const Entity& entity : entityManager_.Entities()) {
            if (!entity.active || !entity.visible || entity.type != EntityType::Enemy) {
                continue;
            }

            if (DistanceSquared(entity.position, player_.position) <= kEnemyAlertRangeSq) {
                shouldAlert = true;
                break;
            }
        }

        if (shouldAlert) {
            audio_.PlayEffect("enemy_alert");
            enemyAlertCooldown_ = 1.2f;
        }
    }

    ambientDustSpawnTimer_ += dt;
    while (ambientDustSpawnTimer_ >= 0.12f) {
        ambientDustSpawnTimer_ -= 0.12f;
        const float seed = static_cast<float>((time_.frameIndex % 997) * 0.173);
        const Vec2 offset{std::sin(seed) * 0.75f, std::cos(seed * 0.7f) * 0.6f};
        particleManager_.SpawnAmbientDust(Add(player_.position, offset));
    }

    particleManager_.Update(dt);

    for (SpriteEntity& sprite : sprites_) {
        if (sprite.entityId < 0) {
            continue;
        }

        const Entity* owner = entityManager_.GetEntityById(sprite.entityId);
        if (owner == nullptr) {
            sprite.active = false;
            sprite.visible = false;
            continue;
        }

        sprite.position = owner->position;
        sprite.active = owner->active;
        sprite.visible = owner->visible;
    }
}
