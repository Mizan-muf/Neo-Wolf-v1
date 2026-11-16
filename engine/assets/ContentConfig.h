#pragma once

#include <string>
#include <vector>

struct WallTextureAssignments {
    std::string wall;
    std::string wallAltA;
    std::string wallAltB;
    std::string door;
    std::string floor;
    std::string ceiling;
};

struct SpriteTextureDefinitions {
    std::string enemy;
    std::string pickup;
    std::string decoration;
};

bool LoadWallTextureAssignments(
    const std::string& filePath,
    WallTextureAssignments& outAssignments,
    std::vector<std::string>& errors);

bool LoadSpriteTextureDefinitions(
    const std::string& filePath,
    SpriteTextureDefinitions& outDefinitions,
    std::vector<std::string>& errors);
