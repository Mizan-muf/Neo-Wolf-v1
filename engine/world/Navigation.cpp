#include "engine/world/Navigation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "engine/render/Raycaster.h"

namespace {
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
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance < 1e-4f || fromCell == CellOf(to)) {
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
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance < 1e-4f) {
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

    // parent[i]: index of the cell the search came from; -1 = not visited, -2 = the start.
    std::vector<int> parent(static_cast<std::size_t>(map.Width()) * static_cast<std::size_t>(map.Height()), -1);
    std::vector<Vec2i> queue;
    queue.reserve(parent.size());
    queue.push_back(start);
    parent[IndexOf(map, start.x, start.y)] = -2;

    // Orthogonal steps first, so a straight path is preferred over an equally short zigzag.
    constexpr int kDx[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    constexpr int kDy[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    bool found = false;
    for (std::size_t head = 0; head < queue.size() && !found; ++head) {
        const Vec2i cell = queue[head];
        for (int d = 0; d < 8; ++d) {
            const int nx = cell.x + kDx[d];
            const int ny = cell.y + kDy[d];
            if (!IsWalkableCell(map, nx, ny) || parent[IndexOf(map, nx, ny)] != -1) {
                continue;
            }
            if (d >= 4 &&
                (!IsWalkableCell(map, cell.x + kDx[d], cell.y) || !IsWalkableCell(map, cell.x, cell.y + kDy[d]))) {
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

    for (int at = static_cast<int>(IndexOf(map, goal.x, goal.y)); parent[static_cast<std::size_t>(at)] != -2;
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
    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const Vec2i cell = queue[head];
        const int next = distance[IndexOf(map, cell.x, cell.y)] + 1;
        if (next > maxDistance) {
            continue;
        }
        for (int d = 0; d < 4; ++d) {
            const int nx = cell.x + kDx[d];
            const int ny = cell.y + kDy[d];
            if (IsWalkableCell(map, nx, ny) && distance[IndexOf(map, nx, ny)] == -1) {
                distance[IndexOf(map, nx, ny)] = next;
                queue.push_back(Vec2i{nx, ny});
            }
        }
    }
    return distance;
}
