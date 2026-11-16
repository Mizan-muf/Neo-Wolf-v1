#pragma once

#include <string>
#include <vector>

// A texture theme: game/defs/themes/<id>.cfg (docs/CONTENT_DEFINITIONS.md). Seven required
// keys, each a content-relative .ppm path.
struct ThemeTextureAssignments {
    std::string wall;      // '#'
    std::string wallAltA;  // 'A'
    std::string wallAltB;  // 'B'
    std::string door;      // 'D' (closed doors)
    std::string floor;
    std::string ceiling;
    std::string exit;      // 'X' (exit switch)
};

// "game/defs/themes/<themeId>.cfg" (content-relative).
std::string ThemeConfigPath(const std::string& themeId);

struct SpriteTextureDefinitions {
    std::string enemy;
    std::string pickup;
    std::string decoration;
};

// Reads a theme file. A syntax error makes the whole file unusable (false, nothing set); a
// missing key is an error for that key only (false, the other keys are set).
bool LoadThemeTextureAssignments(
    const std::string& filePath,
    ThemeTextureAssignments& outAssignments,
    std::vector<std::string>& errors);

bool LoadSpriteTextureDefinitions(
    const std::string& filePath,
    SpriteTextureDefinitions& outDefinitions,
    std::vector<std::string>& errors);
