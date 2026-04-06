#pragma once

#include "engine/core/Vec2.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

bool IsBlockedByWall(const Map& map, const Vec2& position, float radius);
void ResolveCollisionAgainstWalls(const Map& map, Player& player, Vec2 displacement);
float DistanceSquared(const Vec2& a, const Vec2& b);
bool IsCircleOverlappingCell(const Vec2& center, float radius, int cellX, int cellY);
