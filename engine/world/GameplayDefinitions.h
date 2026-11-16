#pragma once

// Gameplay values loaded from game/defs/gameplay.json (engine/assets/GameplayConfig.cpp).
//
// The member initialisers below are the documented fallback defaults: they apply to a key
// that is missing from gameplay.json, and to code that runs without a loaded file (the C API
// before Init, unit tests). They match the shipped gameplay.json. Each key, its range and
// where it is applied are documented in docs/CONTENT_DEFINITIONS.md; keep the two in sync.

#include <cstdint>

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

// Enemy archetypes (Roadmap Phase 6). An enemy placement's "variant" selects one; a placement
// without a variant is a guard. The values are appended to the C API's numbering, never
// reordered.
enum class EnemyVariant : std::uint8_t { Guard = 0, Brute, Runner };

inline constexpr int kEnemyVariantCount = 3;
inline constexpr EnemyVariant kEnemyVariants[kEnemyVariantCount] = {EnemyVariant::Guard, EnemyVariant::Brute,
                                                                    EnemyVariant::Runner};

// "guard", "brute", "runner" (the names used in gameplay.json and entity files).
inline const char* EnemyVariantName(EnemyVariant variant) {
    switch (variant) {
        case EnemyVariant::Guard:
            return "guard";
        case EnemyVariant::Brute:
            return "brute";
        case EnemyVariant::Runner:
            return "runner";
    }
    return "guard";
}

// One enemy archetype: gameplay.json "enemies.<variant>".
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

struct EnemyDefinitions {
    EnemyDefinition guard{};
    EnemyDefinition brute{140, 25, 1.0f, 5.5f, 1.3f, 1.6f, 0.8f, 300};  // tanky, slow, hits hard
    EnemyDefinition runner{25, 6, 2.8f, 8.0f, 1.0f, 0.6f, 0.2f, 150};   // fast, fragile, low damage

    const EnemyDefinition& For(EnemyVariant variant) const {
        switch (variant) {
            case EnemyVariant::Brute:
                return brute;
            case EnemyVariant::Runner:
                return runner;
            case EnemyVariant::Guard:
                break;
        }
        return guard;
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
    EnemyDefinitions enemies{};
    PickupDefinitions pickups{};
};

// The fallback defaults as constants, for struct initialisers elsewhere (Entity, Player,
// Engine) so the literals exist only once.
inline constexpr PlayerDefinition kDefaultPlayerDefinition{};
inline constexpr WeaponDefinition kDefaultWeaponDefinition{};
inline constexpr EnemyDefinition kDefaultEnemyDefinition{};  // the guard
inline constexpr EnemyDefinitions kDefaultEnemyDefinitions{};
inline constexpr PickupDefinitions kDefaultPickupDefinitions{};
