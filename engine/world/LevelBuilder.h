#pragma once

// Building a level from its entity placements, and keeping sprites in step with the entities.

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
    std::vector<SpriteEntity> sprites;
    Player player{};
};

// Turns placements into entities, sprites and the player, using gameplay.json values:
//   player      the Player (position, view angle, speeds, radius) and an invisible Player entity
//   enemy       Enemy entity with the stats of its archetype (gameplay.json "enemies.<variant>",
//               guard when the placement has no variant), plus that archetype's sprite
//   pickup      Pickup entity with the placement's kind; value from the placement or from
//               gameplay.json pickups.<kind>.value (keys: the key colour); plus its sprite
//   door        Door entity bound to (mapX, mapY), with the placement's lock; that map cell
//               becomes a closed door (Map::SetDoorLock records the lock for drawing)
//   trigger     Trigger (level exit) entity bound to (mapX, mapY); that cell becomes a 'T' tile
//   decoration  Decoration entity (no interaction) plus a decoration sprite
// Light zones (the entity file's "lights") set Map::SetLightLevel for their cells, in order, so
// a later zone overrides an earlier one; cells no zone covers keep the map's level (1.0 for a
// freshly loaded map). Light only changes the picture: enemy sight, hearing and the autoplay
// bot ignore it.
// The level needs exactly one player; every position, map cell and light zone must lie inside
// the map. On an error nothing is changed: `map` and `out` are only written when the function
// returns true. Errors name the placement's or zone's source (file and index).
bool BuildLevelFromPlacements(Map& map, const std::vector<EntityPlacement>& placements,
                              const std::vector<LightZone>& lights, const GameplayDefinitions& gameplay,
                              LevelContents& out, std::vector<std::string>& errors);

// The same without light zones.
bool BuildLevelFromPlacements(Map& map, const std::vector<EntityPlacement>& placements,
                              const GameplayDefinitions& gameplay, LevelContents& out,
                              std::vector<std::string>& errors);

// Copies an archetype's stats onto an enemy entity (health, damage, speeds, radii, rates).
void ApplyEnemyArchetype(Entity& enemy, EnemyVariant variant, const EnemyDefinitions& enemies);

// Keeps the sprite list in step with the entities once per frame, without rebuilding it:
// each sprite takes its entity's position, visibility (hidden while the entity is inactive or
// invisible, except corpses), texture (dead enemies switch to their corpse) and hit flash;
// sprites of destroyed entities are removed, and enemies, pickups and
// decorations that have no sprite yet (e.g. spawned through the C API) get one.
void SyncSpritesWithEntities(const EntityManager& entities, std::vector<SpriteEntity>& sprites);

// "6 entities: 1 player, 1 enemy, 1 pickup, 1 door, 1 trigger, 1 decoration", for the log.
std::string DescribeLevelContents(const LevelContents& contents);
