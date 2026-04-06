#pragma once

#include <string>
#include <vector>

#include "engine/core/Vec2.h"

enum class PlacementType {
    Player = 0,
    Enemy,
    Pickup,
    Door,
    Trigger,
    Decoration
};

struct EntityPlacement {
    PlacementType type = PlacementType::Enemy;
    Vec2 position{};
    int mapX = -1;
    int mapY = -1;
};

bool LoadEntityPlacementsFromFile(
    const std::string& filePath,
    std::vector<EntityPlacement>& outPlacements,
    std::vector<std::string>& errors);
