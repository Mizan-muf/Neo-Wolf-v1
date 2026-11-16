#include "engine/world/MovementRules.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kStepSize = 0.05f;  // map cells: movement is tested in steps at most this long

// Squared distance from `point` to the nearest point of map cell (cellX, cellY).
float DistanceSquaredToCell(const Vec2& point, int cellX, int cellY) {
    const auto minX = static_cast<float>(cellX);
    const float maxX = minX + 1.0f;
    const auto minY = static_cast<float>(cellY);
    const float maxY = minY + 1.0f;

    const float closestX = std::max(minX, std::min(point.x, maxX));
    const float closestY = std::max(minY, std::min(point.y, maxY));
    const float dx = point.x - closestX;
    const float dy = point.y - closestY;
    return dx * dx + dy * dy;
}
}  // namespace

bool IsBlockedByWall(const Map& map, const Vec2& position, float radius) {
    const int minX = static_cast<int>(std::floor(position.x - radius));
    const int maxX = static_cast<int>(std::floor(position.x + radius));
    const int minY = static_cast<int>(std::floor(position.y - radius));
    const int maxY = static_cast<int>(std::floor(position.y + radius));

    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (map.IsWall(x, y) && DistanceSquaredToCell(position, x, y) < radius * radius) {
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
    return std::any_of(bodies->begin(), bodies->end(), [&](const BodyObstacle& body) {
        const float minDistance = radius + body.radius;
        const float after = DistanceSquared(to, body.position);
        return after < minDistance * minDistance && after < DistanceSquared(from, body.position);
    });
}

// Moves `position` by `delta` along one axis (x when `alongX`, else y) in steps of at most
// kStepSize, stopping before the first step that is blocked.
void SlideAlongAxis(const Map& map, Vec2& position, float delta, bool alongX, float radius,
                    const std::vector<BodyObstacle>* bodies) {
    if (delta == 0.0f) {
        return;
    }
    const float sign = delta > 0.0f ? 1.0f : -1.0f;
    float remaining = std::abs(delta);
    while (remaining > 0.0f) {
        const float step = std::min(remaining, kStepSize);
        Vec2 candidate = position;
        float& coordinate = alongX ? candidate.x : candidate.y;
        coordinate = coordinate + sign * step;
        if (IsBlockedByWall(map, candidate, radius) || IsBlockedByBody(position, candidate, radius, bodies)) {
            return;
        }
        position = candidate;
        remaining -= step;
    }
}

// x first, then y: a blocked axis does not stop the other, so movers slide along walls.
Vec2 MoveAxisSeparated(const Map& map, const Vec2& position, const Vec2& displacement, float radius,
                       const std::vector<BodyObstacle>* bodies) {
    Vec2 nextPosition = position;
    SlideAlongAxis(map, nextPosition, displacement.x, true, radius, bodies);
    SlideAlongAxis(map, nextPosition, displacement.y, false, radius, bodies);
    return nextPosition;
}
}  // namespace

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
    return DistanceSquaredToCell(center, cellX, cellY) <= (radius * radius);
}
