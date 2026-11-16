#pragma once

#include <optional>
#include <string>
#include <vector>

#include "engine/core/Vec2.h"
#include "engine/world/Entity.h"

enum class PlacementType {
    Player = 0,
    Enemy,
    Pickup,
    Door,
    Trigger,
    Decoration
};

// One entry of a level's entity file (schema: docs/CONTENT_DEFINITIONS.md).
struct EntityPlacement {
    PlacementType type = PlacementType::Enemy;
    Vec2 position{};
    int mapX = -1;                      // door/trigger: the map cell it controls (required)
    int mapY = -1;
    float angleDegrees = 0.0f;          // player: initial view direction (0 = +x, 90 = +y)
    PickupType pickupKind = PickupType::Health;  // pickup: "kind" (required)
    std::optional<int> pickupValue{};   // pickup: "value"; empty = gameplay.json pickups.<kind>.value
    std::string variant{};              // enemy: reserved for enemy archetypes (Roadmap Phase 6)
    std::string source{};               // where it came from, for messages: "file: entities[3]" or "file:5"
};

// "player", "enemy", ... (the names used in entity files).
const char* PlacementTypeName(PlacementType type);

// Loads a level's entity placements. The format follows the extension (case-insensitive):
//   .json  the canonical format: {"level": "...", "entities": [{"type": ..., "x": ..., ...}]}
//   .csv   the legacy table: type,x,y[,mapX,mapY[,kind[,value[,angle[,variant]]]]]
//          (an empty column means "not given").
// Every problem is reported with the file name, the entity (index, and line for CSV) and the
// field. Errors (unreadable file, bad syntax, unknown type, missing or invalid required field)
// make the load fail; unknown or inapplicable fields are warnings. Returns false if this call
// added any error; outPlacements then holds only the entries that were valid.
bool LoadEntityPlacementsFromFile(
    const std::string& filePath,
    std::vector<EntityPlacement>& outPlacements,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings);

// The two parsers, from text; `sourceName` (normally the file path) starts every message.
bool ParseEntityPlacementsJson(
    const std::string& text,
    const std::string& sourceName,
    std::vector<EntityPlacement>& outPlacements,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings);

bool ParseEntityPlacementsCsv(
    const std::string& text,
    const std::string& sourceName,
    std::vector<EntityPlacement>& outPlacements,
    std::vector<std::string>& errors,
    std::vector<std::string>& warnings);
