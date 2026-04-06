#include "engine/core/Engine.h"

#include <cmath>

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
        desiredMove = Normalize(desiredMove);
        const Vec2 displacement = Scale(desiredMove, player_.moveSpeed * dt);
        ResolveCollisionAgainstWalls(map_, player_, displacement);
    }

    SyncPlayerEntity(entityManager_, player_);
    UpdateEntityInteractions(entityManager_, map_, player_, input_.WasPressed(InputAction::Interact));

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
