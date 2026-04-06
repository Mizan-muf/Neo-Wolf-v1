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
