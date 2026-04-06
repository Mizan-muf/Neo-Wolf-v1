#pragma once

#include <cstdint>
#include <vector>

enum class TileType : std::uint8_t {
    Empty = 0,
    Wall,
    Door,
    Trigger,
    WallAltA,
    WallAltB
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
    bool IsWall(int x, int y) const;
    float GetLightLevel(int x, int y) const;
    bool SetLightLevel(int x, int y, float lightLevel);

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<TileType> cells_{};
    std::vector<float> lightLevels_{};
};
