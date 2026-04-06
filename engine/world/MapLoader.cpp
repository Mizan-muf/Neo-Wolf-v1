#include "engine/world/MapLoader.h"

#include <fstream>
#include <string>
#include <vector>

#include "engine/world/Map.h"

namespace {
TileType DecodeTile(char c, bool& valid) {
    valid = true;
    switch (c) {
    case '.':
        return TileType::Empty;
    case '#':
        return TileType::Wall;
    case 'A':
        return TileType::WallAltA;
    case 'B':
        return TileType::WallAltB;
    case 'D':
        return TileType::Door;
    case 'T':
        return TileType::Trigger;
    default:
        valid = false;
        return TileType::Empty;
    }
}
} // namespace

bool LoadMapFromFile(const std::string& filePath, Map& outMap, std::vector<std::string>& errors) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        errors.push_back("Unable to open map file: " + filePath);
        return false;
    }

    std::vector<std::string> rows;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == ';') {
            continue;
        }
        rows.push_back(line);
    }

    if (rows.empty()) {
        errors.push_back("Map file has no rows: " + filePath);
        return false;
    }

    const int width = static_cast<int>(rows[0].size());
    const int height = static_cast<int>(rows.size());
    if (width <= 0 || height <= 0 || !outMap.Resize(width, height)) {
        errors.push_back("Failed to allocate map dimensions from: " + filePath);
        return false;
    }

    bool ok = true;
    for (int y = 0; y < height; ++y) {
        if (static_cast<int>(rows[y].size()) != width) {
            errors.push_back(filePath + ": inconsistent row width at y=" + std::to_string(y));
            ok = false;
            continue;
        }

        for (int x = 0; x < width; ++x) {
            bool valid = false;
            const TileType tile = DecodeTile(rows[y][x], valid);
            if (!valid) {
                errors.push_back(
                    filePath + ": invalid tile '" + std::string(1, rows[y][x]) + "' at (" +
                    std::to_string(x) + "," + std::to_string(y) + ")");
                ok = false;
                continue;
            }
            outMap.SetCell(x, y, tile);
        }
    }

    return ok;
}
