#pragma once

// The key=value .cfg content files: texture themes, sprites.cfg (here) and audio.cfg
// (AudioConfig.h), and the reader they share.

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/world/Sprite.h"

// A texture theme: game/defs/themes/<id>.cfg (docs/CONTENT_DEFINITIONS.md). Ten required keys,
// each a content-relative .ppm path.
struct ThemeTextureAssignments {
    std::string wall;      // '#'
    std::string wallAltA;  // 'A'
    std::string wallAltB;  // 'B'
    std::string door;      // 'D' (closed doors)
    std::string doorRed;   // closed doors locked with the red key (Phase 6)
    std::string doorBlue;  // ... the blue key
    std::string doorGold;  // ... the gold key
    std::string floor;
    std::string ceiling;
    std::string exit;  // 'X' (exit switch)
};

// "game/defs/themes/<themeId>.cfg" (content-relative).
std::string ThemeConfigPath(const std::string& themeId);

// sprites.cfg keys, in SpriteTextureId order (all required).
inline constexpr std::array<const char*, kSpriteTextureCount> kSpriteTextureKeys = {
    "guard",      "health",      "decoration", "brute",   "runner",   "guard_dead",
    "brute_dead", "runner_dead", "ammo",       "key_red", "key_blue", "key_gold"};

// Content-relative .ppm paths, indexed by SpriteTextureId.
struct SpriteTextureDefinitions {
    std::array<std::string, kSpriteTextureCount> paths{};

    const std::string& Path(SpriteTextureId id) const { return paths[static_cast<std::size_t>(id)]; }
};

// Reads a theme file. A syntax error makes the whole file unusable (false, nothing set); a
// missing key is an error for that key only (false, the other keys are set).
bool LoadThemeTextureAssignments(const std::string& filePath, ThemeTextureAssignments& outAssignments,
                                 std::vector<std::string>& errors);

bool LoadSpriteTextureDefinitions(const std::string& filePath, SpriteTextureDefinitions& outDefinitions,
                                  std::vector<std::string>& errors);

// Reads a .cfg file: one `key = value` per line, blank lines and lines starting with '#'
// skipped, a CRLF file accepted. A line without '=' or with an empty key or value is an error
// (with file and line number); the other lines are still read. Returns false when the file
// cannot be opened or `errors` is not empty afterwards.
bool LoadKeyValueConfig(const std::string& filePath, std::unordered_map<std::string, std::string>& outValues,
                        std::vector<std::string>& errors);

// Copies the value of a required key into outValue; a missing key is an error.
bool RequireConfigKey(const std::unordered_map<std::string, std::string>& values, const std::string& key,
                      std::string& outValue, const std::string& filePath, std::vector<std::string>& errors);
