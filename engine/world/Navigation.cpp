#include "engine/world/Navigation.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include "engine/render/Raycaster.h"

namespace {
constexpr float kSameSpotDistance = 1e-4f;  // map cells: closer points have no direction between them

// FindPath's parent entries that are not a cell index.
constexpr int kUnvisited = -1;
constexpr int kStartCell = -2;

// Neighbour offsets: the 4 orthogonal steps first, then the 4 diagonals.
constexpr std::size_t kOrthogonalNeighbours = 4;
constexpr std::array<int, 8> kNeighbourDx = {1, -1, 0, 0, 1, 1, -1, -1};
constexpr std::array<int, 8> kNeighbourDy = {0, 0, 1, -1, 1, -1, 1, -1};

std::size_t IndexOf(const Map& map, int x, int y) {
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(map.Width()) + static_cast<std::size_t>(x);
}
}  // namespace

Vec2i CellOf(const Vec2& position) {
    return Vec2i{static_cast<int>(std::floor(position.x)), static_cast<int>(std::floor(position.y))};
}

Vec2 CellCenter(const Vec2i& cell) {
    return Vec2{static_cast<float>(cell.x) + 0.5f, static_cast<float>(cell.y) + 0.5f};
}

bool IsWalkableCell(const Map& map, int x, int y) {
    return map.IsInsideMap(x, y) && !map.IsWall(x, y);
}

bool HasLineOfSight(const Map& map, const Vec2& from, const Vec2& to) {
    const Vec2i fromCell = CellOf(from);
    if (map.IsWall(fromCell.x, fromCell.y)) {
        return false;
    }
    const Vec2 delta = to - from;
    const float distance = Length(delta);
    if (distance < kSameSpotDistance || fromCell == CellOf(to)) {
        return true;
    }
    const Vec2 direction{delta.x / distance, delta.y / distance};
    const RaycastHit hit = Raycaster::CastRay(map, from, direction, std::atan2(direction.y, direction.x));
    return !hit.hit || hit.distanceRaw >= distance;
}

bool HasClearCorridor(const Map& map, const Vec2& from, const Vec2& to, float radius) {
    if (!HasLineOfSight(map, from, to)) {
        return false;
    }
    const Vec2 delta = to - from;
    const float distance = Length(delta);
    if (distance < kSameSpotDistance) {
        return true;
    }
    const Vec2 side{-delta.y / distance * radius, delta.x / distance * radius};
    return HasLineOfSight(map, from + side, to + side) && HasLineOfSight(map, from - side, to - side);
}

bool FindPath(const Map& map, const Vec2i& start, const Vec2i& goal, std::vector<Vec2i>& outPath) {
    outPath.clear();
    if (!IsWalkableCell(map, goal.x, goal.y) || !map.IsInsideMap(start.x, start.y)) {
        return false;
    }
    if (start == goal) {
        return true;
    }

    // parent[i]: index of the cell the search came from, kUnvisited or kStartCell.
    std::vector<int> parent(static_cast<std::size_t>(map.Width()) * static_cast<std::size_t>(map.Height()), kUnvisited);
    std::vector<Vec2i> queue;
    queue.reserve(parent.size());
    queue.push_back(start);
    parent[IndexOf(map, start.x, start.y)] = kStartCell;

    // Orthogonal steps first, so a straight path is preferred over an equally short zigzag.
    bool found = false;
    for (std::size_t head = 0; head < queue.size() && !found; ++head) {
        const Vec2i cell = queue[head];
        for (std::size_t d = 0; d < kNeighbourDx.size(); ++d) {
            const int nx = cell.x + kNeighbourDx[d];
            const int ny = cell.y + kNeighbourDy[d];
            if (!IsWalkableCell(map, nx, ny) || parent[IndexOf(map, nx, ny)] != kUnvisited) {
                continue;
            }
            if (d >= kOrthogonalNeighbours && (!IsWalkableCell(map, cell.x + kNeighbourDx[d], cell.y) ||
                                               !IsWalkableCell(map, cell.x, cell.y + kNeighbourDy[d]))) {
                continue;  // would cut a wall corner
            }
            parent[IndexOf(map, nx, ny)] = static_cast<int>(IndexOf(map, cell.x, cell.y));
            if (nx == goal.x && ny == goal.y) {
                found = true;
                break;
            }
            queue.push_back(Vec2i{nx, ny});
        }
    }
    if (!found) {
        return false;
    }

    for (int at = static_cast<int>(IndexOf(map, goal.x, goal.y)); parent[static_cast<std::size_t>(at)] != kStartCell;
         at = parent[static_cast<std::size_t>(at)]) {
        outPath.push_back(Vec2i{at % map.Width(), at / map.Width()});
    }
    // Collected goal-first; the caller walks it nearest-first.
    std::reverse(outPath.begin(), outPath.end());
    return true;
}

std::vector<int> ComputeSoundDistances(const Map& map, const Vec2i& source, int maxDistance) {
    std::vector<int> distance(static_cast<std::size_t>(map.Width()) * static_cast<std::size_t>(map.Height()), -1);
    if (!IsWalkableCell(map, source.x, source.y) || maxDistance < 0) {
        return distance;
    }
    std::vector<Vec2i> queue{source};
    distance[IndexOf(map, source.x, source.y)] = 0;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const Vec2i cell = queue[head];
        const int next = distance[IndexOf(map, cell.x, cell.y)] + 1;
        if (next > maxDistance) {
            continue;
        }
        for (std::size_t d = 0; d < kOrthogonalNeighbours; ++d) {
            const int nx = cell.x + kNeighbourDx[d];
            const int ny = cell.y + kNeighbourDy[d];
            if (IsWalkableCell(map, nx, ny) && distance[IndexOf(map, nx, ny)] == -1) {
                distance[IndexOf(map, nx, ny)] = next;
                queue.push_back(Vec2i{nx, ny});
            }
        }
    }
    return distance;
}
