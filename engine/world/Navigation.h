#pragma once

// Grid queries for enemy AI (Roadmap Phase 6, known issue C10): line of sight, path finding
// and how far a sound carries. Every query treats walls, the exit switch and **closed doors**
// as solid (Map::IsWall); an open door is an ordinary empty cell.

#include <vector>

#include "engine/core/Vec2.h"
#include "engine/core/Vec2i.h"
#include "engine/world/Map.h"

// The map cell that contains `position`.
Vec2i CellOf(const Vec2& position);

// The centre of a map cell in world coordinates.
Vec2 CellCenter(const Vec2i& cell);

// A cell an enemy can stand in: inside the map and not solid.
bool IsWalkableCell(const Map& map, int x, int y);

// True when nothing solid lies on the straight line from `from` to `to`. Uses the raycaster's
// DDA (Raycaster::CastRay), so sight is blocked by exactly what blocks the view in 3D.
bool HasLineOfSight(const Map& map, const Vec2& from, const Vec2& to);

// Line of sight along the centre line and along both edges of a body of `radius` moving from
// `from` to `to`: true when a circle of that radius can move straight there without touching
// a wall corner.
bool HasClearCorridor(const Map& map, const Vec2& from, const Vec2& to, float radius);

// Breadth-first search over walkable cells, 8-connected; a diagonal step is only allowed when
// both cells beside it are walkable (no cutting wall corners). On success `outPath` holds the
// cells from the first step to `goal` (empty when start == goal) and the function returns
// true. Returns false (outPath empty) when the goal is solid or cannot be reached.
bool FindPath(const Map& map, const Vec2i& start, const Vec2i& goal, std::vector<Vec2i>& outPath);

// How far a sound made in `source` travels: breadth-first distances (4-connected, in cells)
// through walkable cells, up to `maxDistance`. The result has one entry per map cell
// (index y * width + x); -1 marks cells the sound does not reach. Closed doors block sound.
std::vector<int> ComputeSoundDistances(const Map& map, const Vec2i& source, int maxDistance);
