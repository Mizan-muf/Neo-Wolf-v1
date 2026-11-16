#pragma once

// Reading a level's entity file (.json, or legacy .csv): the entity placements and light
// zones, checked field by field. BuildLevelFromPlacements (LevelBuilder.h) turns them into a level.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "engine/core/Vec2.h"
#include "engine/world/Entity.h"
#include "engine/world/GameplayDefinitions.h"
#include "engine/world/KeyColor.h"

enum class PlacementType { Player = 0, Enemy, Pickup, Door, Trigger, Decoration };

// One entry of a level's entity file (schema: docs/CONTENT_DEFINITIONS.md).
struct EntityPlacement {
    PlacementType type = PlacementType::Enemy;
    Vec2 position{};
    int mapX = -1;  // door/trigger: the map cell it controls (required)
    int mapY = -1;
    float angleDegrees = 0.0f;                   // player: initial view direction (0 = +x, 90 = +y)
    PickupType pickupKind = PickupType::Health;  // pickup: "kind" (required): health, ammo or key
    std::optional<int> pickupValue;              // pickup: "value"; empty = gameplay.json pickups.<kind>.value
    KeyColor key = KeyColor::None;               // pickup of kind "key": "key" (required): red, blue or gold
    KeyColor lock = KeyColor::None;              // door: "lock" (optional): red, blue or gold
    EnemyVariant variant = EnemyVariant::Guard;  // enemy: "variant" (optional): guard, brute or runner
    std::string message;                         // trigger: "message" (optional): its HUD text; empty = the default
    std::string source;                          // where it came from, for messages: "file: entities[3]" or "file:5"
};

// One entry of the optional "lights" array of a level's entity JSON file: a rectangle of map
// cells (x, y, w, h, all inside the map; checked when the level is built) and the light level
// its cells get. Zones are applied in file order, so a later zone overrides an earlier one;
// cells no zone covers keep 1.0 (schema: docs/CONTENT_DEFINITIONS.md).
struct LightZone {
    int x = 0;
    int y = 0;
    int width = 1;
    int height = 1;
    float level = 1.0f;
    std::string source;  // for messages: "file: lights[2]"
};

// A trigger's "message": at most this many characters, and no comma (the CSV export cannot
// hold one). The HUD shows it in capitals and shortens it if the screen is too narrow.
constexpr std::size_t kMaxTriggerMessageLength = 40;

// The range a light zone's "level" must lie in. The floor keeps every area playable: an enemy
// in a zone at the minimum is still clearly visible (see docs/LEVEL_AUTHORING.md).
constexpr double kMinLightZoneLevel = 0.2;
constexpr double kMaxLightZoneLevel = 1.0;

// "player", "enemy", ... (the names used in entity files).
const char* PlacementTypeName(PlacementType type);

// Loads a level's entity placements. The format follows the extension (case-insensitive):
//   .json  the canonical format: {"level": "...", "entities": [{"type": ..., "x": ..., ...}]}
//   .csv   the legacy table: type,x,y[,mapX,mapY[,kind[,value[,angle[,variant[,key[,lock[,message]]]]]]]]
//          (an empty column means "not given").
// Every problem is reported with the file name, the entity (index, and line for CSV) and the
// field. Errors (unreadable file, bad syntax, unknown type, missing or invalid required field)
// make the load fail; unknown or inapplicable fields are warnings. Returns false if this call
// added any error; outPlacements (and outLights) then hold only the entries that were valid.
// Light zones come only from JSON (its optional "lights" array); a CSV has none, so outLights
// is empty after loading a CSV.
bool LoadEntityPlacementsFromFile(const std::string& filePath, std::vector<EntityPlacement>& outPlacements,
                                  std::vector<LightZone>& outLights, std::vector<std::string>& errors,
                                  std::vector<std::string>& warnings);

// The same without light zones: a JSON file's "lights" are still checked (an invalid zone is
// an error) but not returned. For tools and tests that need the placements only.
bool LoadEntityPlacementsFromFile(const std::string& filePath, std::vector<EntityPlacement>& outPlacements,
                                  std::vector<std::string>& errors, std::vector<std::string>& warnings);

// The two parsers, from text; `sourceName` (normally the file path) starts every message.
bool ParseEntityPlacementsJson(const std::string& text, const std::string& sourceName,
                               std::vector<EntityPlacement>& outPlacements, std::vector<LightZone>& outLights,
                               std::vector<std::string>& errors, std::vector<std::string>& warnings);

bool ParseEntityPlacementsJson(const std::string& text, const std::string& sourceName,
                               std::vector<EntityPlacement>& outPlacements, std::vector<std::string>& errors,
                               std::vector<std::string>& warnings);

bool ParseEntityPlacementsCsv(const std::string& text, const std::string& sourceName,
                              std::vector<EntityPlacement>& outPlacements, std::vector<std::string>& errors,
                              std::vector<std::string>& warnings);
