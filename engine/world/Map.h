#pragma once

#include <cstdint>
#include <vector>

#include "engine/world/KeyColor.h"

enum class TileType : std::uint8_t {
    Empty = 0,
    Wall,
    Door,
    Trigger,
    WallAltA,
    WallAltB,
    Exit  // 'X': solid wall with the exit-switch texture; Interact (E) facing it completes the level
};

class Map {
public:
    Map() = default;
    Map(int width, int height);

    bool Resize(int width, int height);

    int Width() const;
    int Height() const;

    bool IsInsideMap(int x, int y) const;
    TileType GetCell(int x, int y) const;
    bool SetCell(int x, int y, TileType type);
    // Solid for movement and rays: walls (#, A, B), closed doors (D), the exit switch (X), and
    // every cell outside the map.
    bool IsWall(int x, int y) const;
    float GetLightLevel(int x, int y) const;
    bool SetLightLevel(int x, int y, float lightLevel);
    // The lock of the door in a cell (Phase 6), for drawing: None for unlocked doors and every
    // other cell. The door entity holds the same value and decides whether the door opens.
    KeyColor GetDoorLock(int x, int y) const;
    bool SetDoorLock(int x, int y, KeyColor lock);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<TileType> cells_{};
    std::vector<float> lightLevels_{};
    std::vector<KeyColor> doorLocks_{};
};
