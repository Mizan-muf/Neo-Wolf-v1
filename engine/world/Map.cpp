#include "engine/world/Map.h"

#include <algorithm>
#include <cstddef>

Map::Map(int width, int height) {
    Resize(width, height);
}

bool Map::Resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        width_ = 0;
        height_ = 0;
        cells_.clear();
        lightLevels_.clear();
        return false;
    }

    width_ = width;
    height_ = height;
    cells_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), TileType::Empty);
    lightLevels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), 1.0f);
    return true;
}

int Map::Width() const {
    return width_;
}

int Map::Height() const {
    return height_;
}

bool Map::IsInsideMap(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

TileType Map::GetCell(int x, int y) const {
    if (!IsInsideMap(x, y)) {
        return TileType::Empty;
    }

    return cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

bool Map::SetCell(int x, int y, TileType type) {
    if (!IsInsideMap(x, y)) {
        return false;
    }

    cells_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = type;
    return true;
}

bool Map::IsWall(int x, int y) const {
    const TileType tile = GetCell(x, y);
    return tile == TileType::Wall || tile == TileType::WallAltA || tile == TileType::WallAltB ||
           tile == TileType::Door;
}

float Map::GetLightLevel(int x, int y) const {
    if (!IsInsideMap(x, y) || lightLevels_.empty()) {
        return 1.0f;
    }

    return lightLevels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

bool Map::SetLightLevel(int x, int y, float lightLevel) {
    if (!IsInsideMap(x, y) || lightLevels_.empty()) {
        return false;
    }

    lightLevels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] =
        std::clamp(lightLevel, 0.0f, 1.0f);
    return true;
}
