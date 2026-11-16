#pragma once

#include <vector>

#include "engine/core/Vec2.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

// A round body that blocks movement (living enemies block the player, known issue C15).
struct BodyObstacle {
    Vec2 position{};
    float radius = 0.0f;
};

bool IsBlockedByWall(const Map& map, const Vec2& position, float radius);
void ResolveCollisionAgainstWalls(const Map& map, Player& player, Vec2 displacement);
Vec2 MoveWithSliding(const Map& map, const Vec2& position, const Vec2& displacement, float radius);
// MoveWithSliding that is also stopped by `bodies`: a step is refused when it would bring the
// mover's circle closer than (radius + body radius) to a body and closer to it than before.
// Steps that keep or increase the distance are allowed, so a mover that already overlaps a
// body (e.g. one placed too close) can always back away, and it slides along a body as it
// slides along walls (each axis is tried separately).
Vec2 MoveWithSlidingAndBodies(const Map& map, const Vec2& position, const Vec2& displacement, float radius,
                              const std::vector<BodyObstacle>& bodies);
float DistanceSquared(const Vec2& a, const Vec2& b);
bool IsCircleOverlappingCell(const Vec2& center, float radius, int cellX, int cellY);

