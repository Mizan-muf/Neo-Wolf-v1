#pragma once

// Gameplay values loaded from game/defs/gameplay.json (engine/assets/GameplayConfig.cpp).
//
// The member initialisers below are the documented fallback defaults: they apply to a key
// that is missing from gameplay.json, and to code that runs without a loaded file (the C API
// before Init, unit tests). They match the shipped gameplay.json. Each key, its range and
// where it is applied are documented in docs/CONTENT_DEFINITIONS.md; keep the two in sync.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Upper limit for every health, damage, ammo and pickup value in the content files.
inline constexpr int kMaxGameplayStat = 10000;

struct PlayerDefinition {
    int initialHealth = 100;        // health at level start
    int maxHealth = 100;            // health pickups never raise health above this
    float moveSpeed = 3.0f;         // map cells per second
    float turnSpeed = 1.8f;         // radians per second
    float collisionRadius = 0.22f;  // map cells; must stay below 0.5 to fit through a 1-cell gap
};

struct WeaponDefinition {
    int damage = 35;         // health removed from an enemy per hit
    float fireRate = 0.25f;  // seconds between two shots (a cooldown, not shots per second)
    float range = 12.0f;     // map cells; walls stop shots earlier
    int initialAmmo = 24;    // ammo at level start
    int maxAmmo = 99;        // ammo pickups never raise ammo above this
    // Enemies hear a shot when the path from the shooter to them through open cells (closed
    // doors and walls block sound) is at most this many cells long.
    float noiseRadius = 10.0f;
};

// One enemy type's stats: gameplay.json "enemies.<id>". The defaults apply to keys missing
// from a type's section, and to enemies created without a type (the C API before Init, tests).
struct EnemyDefinition {
    int health = 50;
    int damage = 12;            // health removed from the player per attack
    float moveSpeed = 1.6f;     // map cells per second while chasing
    float alertRadius = 6.5f;   // map cells; an idle enemy that sees the player this close is alerted
    float attackRadius = 1.1f;  // map cells; attacks inside this distance (with line of sight)
    float attackRate = 1.0f;    // seconds between two attacks (a cooldown)
    float reactionTime = 0.5f;  // seconds between noticing the player and chasing (Alert state)
    int score = 100;            // points for a kill (logged at level end)
};

// An enemy type, defined by the game: gameplay.json "enemies.<id>" (Roadmap 1.5.0, engine/game
// boundary). The engine knows no type by name; an enemy placement's "variant" selects one by id.
struct EnemyType {
    std::string id;            // the key under "enemies" (lower case, digits and '_')
    EnemyDefinition stats{};   // the type's values
    std::string sprite;        // sprites.cfg key of the living enemy ("sprite", default: the id)
    std::string corpseSprite;  // sprites.cfg key of its corpse ("corpseSprite", default: <id>_dead)
    int spriteSlot = -1;       // sprite slots (SpriteSlotNames): set when gameplay.json is parsed
    int corpseSpriteSlot = -1;
};

// Every enemy type of the game, sorted by id (the order gameplay.json is read in).
struct EnemyDefinitions {
    std::vector<EnemyType> types;
    // The type of an enemy placement without "variant" and of enemies spawned through the C API:
    // gameplay.json "defaultEnemy", else the only type when there is exactly one, else -1 (such a
    // placement is an error).
    int defaultType = -1;

    // The index of the type with this id, or -1.
    int Find(std::string_view id) const {
        for (std::size_t i = 0; i < types.size(); ++i) {
            if (types[i].id == id) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    // The type at `index`, or nullptr when there is none (an enemy without a type).
    const EnemyType* At(int index) const {
        return index >= 0 && static_cast<std::size_t>(index) < types.size() ? &types[static_cast<std::size_t>(index)]
                                                                            : nullptr;
    }
    // The type's id, or "enemy" for an enemy without a type (logs and the autoplay trace).
    std::string_view Name(int index) const {
        const EnemyType* type = At(index);
        return type != nullptr ? std::string_view(type->id) : std::string_view("enemy");
    }
};

struct PickupDefinition {
    int value = 0;  // health or ammo added on collection (capped by the player/weapon maximum)
};

struct PickupDefinitions {
    PickupDefinition health{25};
    PickupDefinition ammo{12};
};

struct GameplayDefinitions {
    PlayerDefinition player{};
    WeaponDefinition weapon{};
    EnemyDefinitions enemies{};  // none until gameplay.json is loaded
    PickupDefinitions pickups{};
};

// The fallback defaults as constants, for struct initialisers elsewhere (Entity, Player,
// Engine) so the literals exist only once.
inline constexpr PlayerDefinition kDefaultPlayerDefinition{};
inline constexpr WeaponDefinition kDefaultWeaponDefinition{};
inline constexpr EnemyDefinition kDefaultEnemyDefinition{};
inline constexpr PickupDefinitions kDefaultPickupDefinitions{};
