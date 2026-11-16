#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// The campaign manifest: game/defs/campaign.json (schema: docs/CONTENT_DEFINITIONS.md).
//
//   { "title": "...",
//     "levels": [ { "id": "level01", "name": "...", "map": "game/maps/level01.map",
//                   "entities": "game/defs/level01.entities.json", "theme": "bunker" } ] }
//
// Every field is required. Validation is fail-fast, like gameplay.json: invalid JSON, a
// repeated key, a missing or mistyped field, an empty or repeated level id, an id or theme
// that is not a simple name ([a-z0-9_-]), a map, entity or theme file that does not exist
// under the content root, or no level at all is an error. Unknown keys are warnings.

struct CampaignLevel {
    std::string id;        // unique, e.g. "level01"
    std::string name;      // shown in the HUD, level select and level-complete screen
    std::string map;       // content-relative .map path
    std::string entities;  // content-relative .entities.json (or .csv) path
    std::string theme;     // texture theme id: game/defs/themes/<theme>.cfg
};

struct CampaignDefinition {
    std::string title;
    std::vector<CampaignLevel> levels;
};

// Longest level name / title shown in full; longer ones are accepted with a warning and
// shortened on screen.
inline constexpr std::size_t kMaxCampaignNameLength = 32;

// True for a non-empty id made of a-z, 0-9, '_' and '-' (safe as a file name part).
bool IsValidContentId(const std::string& id);

// Parses campaign JSON text. File references are checked relative to `contentRoot` (an empty
// root means the working directory). `sourceName` starts every message. On failure
// `outCampaign` is unchanged.
bool ParseCampaign(const std::string& jsonText, const std::string& sourceName, const std::filesystem::path& contentRoot,
                   CampaignDefinition& outCampaign, std::vector<std::string>& errors,
                   std::vector<std::string>& warnings);

bool LoadCampaign(const std::string& filePath, const std::filesystem::path& contentRoot,
                  CampaignDefinition& outCampaign, std::vector<std::string>& errors,
                  std::vector<std::string>& warnings);
