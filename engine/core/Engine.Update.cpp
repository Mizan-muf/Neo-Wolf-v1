#include "engine/core/Engine.h"

#include <algorithm>
#include <cmath>

#include "engine/core/MathUtils.h"
#include "engine/platform/PlatformSDL.h"

namespace {
bool IsBlockedByWall(const Map& map, const Vec2& position, float radius) {
    const int minX = static_cast<int>(std::floor(position.x - radius));
    const int maxX = static_cast<int>(std::floor(position.x + radius));
    const int minY = static_cast<int>(std::floor(position.y - radius));
    const int maxY = static_cast<int>(std::floor(position.y + radius));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (!map.IsWall(x, y)) {
                continue;
            }

            const float cellMinX = static_cast<float>(x);
            const float cellMaxX = cellMinX + 1.0f;
            const float cellMinY = static_cast<float>(y);
            const float cellMaxY = cellMinY + 1.0f;

            const float closestX = std::max(cellMinX, std::min(position.x, cellMaxX));
            const float closestY = std::max(cellMinY, std::min(position.y, cellMaxY));
            const float dx = position.x - closestX;
            const float dy = position.y - closestY;
            if ((dx * dx + dy * dy) < radius * radius) {
                return true;
            }
        }
    }

    return false;
}

void ResolveCollisionAgainstWalls(const Map& map, Player& player, Vec2 displacement) {
    Vec2 nextPosition = player.position;

    if (displacement.x != 0.0f) {
        const Vec2 candidate{nextPosition.x + displacement.x, nextPosition.y};
        if (!IsBlockedByWall(map, candidate, player.collisionRadius)) {
            nextPosition.x = candidate.x;
        }
    }

    if (displacement.y != 0.0f) {
        const Vec2 candidate{nextPosition.x, nextPosition.y + displacement.y};
        if (!IsBlockedByWall(map, candidate, player.collisionRadius)) {
            nextPosition.y = candidate.y;
        }
    }

    player.position = nextPosition;
}
} // namespace

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

    if (map_.Width() <= 0 || map_.Height() <= 0) {
        return;
    }

    const float dt = static_cast<float>(time_.deltaSeconds);
    if (dt <= 0.0f) {
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
}
