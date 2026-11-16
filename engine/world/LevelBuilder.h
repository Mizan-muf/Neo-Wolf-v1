#pragma once

#include <string>
#include <vector>

#include "engine/world/Entity.h"
#include "engine/world/EntityPlacementLoader.h"
#include "engine/world/GameplayDefinitions.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"
#include "engine/world/Sprite.h"

// Everything a level's entity placements turn into.
struct LevelContents {
    EntityManager entities{};
    std::vector<SpriteEntity> sprites{};
    Player player{};
};

// Turns placements into entities, sprites and the player, using gameplay.json values:
//   player      the Player (position, view angle, speeds, radius) and an invisible Player entity
//   enemy       Enemy entity with the stats of gameplay.json "enemy", plus an enemy sprite
//   pickup      Pickup entity with the placement's kind; value from the placement or from
//               gameplay.json pickups.<kind>.value; plus a pickup sprite
//   door        Door entity bound to (mapX, mapY); that map cell becomes a closed door
//   trigger     Trigger (level exit) entity bound to (mapX, mapY); that cell becomes a 'T' tile
//   decoration  Decoration entity (no interaction) plus a decoration sprite
// The level needs exactly one player; every position and map cell must lie inside the map.
// On an error nothing is changed: `map` and `out` are only written when the function returns
// true. Errors name the placement's source (file and entity index).
bool BuildLevelFromPlacements(
    Map& map,
    const std::vector<EntityPlacement>& placements,
    const GameplayDefinitions& gameplay,
    LevelContents& out,
    std::vector<std::string>& errors);

// Keeps the sprite list in step with the entities once per frame, without rebuilding it:
// each sprite takes its entity's position and visibility (hidden while the entity is inactive
// or invisible), sprites of destroyed entities are removed, and enemies, pickups and
// decorations that have no sprite yet (e.g. spawned through the C API) get one.
void SyncSpritesWithEntities(const EntityManager& entities, std::vector<SpriteEntity>& sprites);

// "6 entities: 1 player, 1 enemy, 1 pickup, 1 door, 1 trigger, 1 decoration", for the log.
std::string DescribeLevelContents(const LevelContents& contents);
