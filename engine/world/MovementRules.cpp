#include "engine/world/MovementRules.h"

#include <algorithm>
#include <cmath>

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
    player.position = MoveWithSliding(map, player.position, displacement, player.collisionRadius);
}

namespace {
// True when moving from `from` to `to` pushes a circle of `radius` deeper into one of `bodies`.
bool IsBlockedByBody(const Vec2& from, const Vec2& to, float radius, const std::vector<BodyObstacle>* bodies) {
    if (bodies == nullptr) {
        return false;
    }
    for (const BodyObstacle& body : *bodies) {
        const float minDistance = radius + body.radius;
        const float after = DistanceSquared(to, body.position);
        if (after < minDistance * minDistance && after < DistanceSquared(from, body.position)) {
            return true;
        }
    }
    return false;
}

Vec2 MoveAxisSeparated(const Map& map, const Vec2& position, const Vec2& displacement, float radius,
                       const std::vector<BodyObstacle>* bodies) {
    Vec2 nextPosition = position;

    constexpr float kStepSize = 0.05f;

    if (displacement.x != 0.0f) {
        const float totalDist = std::abs(displacement.x);
        const float sign = displacement.x > 0.0f ? 1.0f : -1.0f;
        float remaining = totalDist;
        while (remaining > 0.0f) {
            const float step = std::min(remaining, kStepSize);
            const Vec2 candidate{nextPosition.x + sign * step, nextPosition.y};
            if (IsBlockedByWall(map, candidate, radius) || IsBlockedByBody(nextPosition, candidate, radius, bodies)) {
                break;
            }
            nextPosition.x = candidate.x;
            remaining -= step;
        }
    }

    if (displacement.y != 0.0f) {
        const float totalDist = std::abs(displacement.y);
        const float sign = displacement.y > 0.0f ? 1.0f : -1.0f;
        float remaining = totalDist;
        while (remaining > 0.0f) {
            const float step = std::min(remaining, kStepSize);
            const Vec2 candidate{nextPosition.x, nextPosition.y + sign * step};
            if (IsBlockedByWall(map, candidate, radius) || IsBlockedByBody(nextPosition, candidate, radius, bodies)) {
                break;
            }
            nextPosition.y = candidate.y;
            remaining -= step;
        }
    }

    return nextPosition;
}
} // namespace

Vec2 MoveWithSliding(const Map& map, const Vec2& position, const Vec2& displacement, float radius) {
    return MoveAxisSeparated(map, position, displacement, radius, nullptr);
}

Vec2 MoveWithSlidingAndBodies(const Map& map, const Vec2& position, const Vec2& displacement, float radius,
                              const std::vector<BodyObstacle>& bodies) {
    return MoveAxisSeparated(map, position, displacement, radius, &bodies);
}

float DistanceSquared(const Vec2& a, const Vec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool IsCircleOverlappingCell(const Vec2& center, float radius, int cellX, int cellY) {
    const float minX = static_cast<float>(cellX);
    const float maxX = minX + 1.0f;
    const float minY = static_cast<float>(cellY);
    const float maxY = minY + 1.0f;

    const float closestX = std::max(minX, std::min(center.x, maxX));
    const float closestY = std::max(minY, std::min(center.y, maxY));
    const float dx = center.x - closestX;
    const float dy = center.y - closestY;
    return (dx * dx + dy * dy) <= (radius * radius);
}
