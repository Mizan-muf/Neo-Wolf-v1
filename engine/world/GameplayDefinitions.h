#pragma once

// Gameplay values loaded from game/defs/gameplay.json (engine/assets/GameplayConfig.cpp).
//
// The member initialisers below are the documented fallback defaults: they apply to a key
// that is missing from gameplay.json, and to code that runs without a loaded file (the C API
// before Init, unit tests). They match the shipped gameplay.json. Each key, its range and
// where it is applied are documented in docs/CONTENT_DEFINITIONS.md; keep the two in sync.

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
    int damage = 35;          // health removed from an enemy per hit
    float fireRate = 0.25f;   // seconds between two shots (a cooldown, not shots per second)
    float range = 12.0f;      // map cells; walls stop shots earlier
    int initialAmmo = 24;     // ammo at level start
    int maxAmmo = 99;         // ammo pickups never raise ammo above this
};

// One enemy archetype. gameplay.json has a single default archetype ("enemy") today;
// Roadmap Phase 6 adds named archetypes ("enemies": {"guard": {...}}) of the same shape,
// selected by an entity placement's "variant".
struct EnemyDefinition {
    int health = 50;
    int damage = 12;            // health removed from the player per attack
    float moveSpeed = 1.6f;     // map cells per second while chasing
    float alertRadius = 6.5f;   // map cells; an idle enemy starts chasing inside this distance
    float attackRadius = 1.1f;  // map cells; attacks inside this distance
    float attackRate = 1.0f;    // seconds between two attacks (a cooldown)
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
    EnemyDefinition enemy{};
    PickupDefinitions pickups{};
};

// The fallback defaults as constants, for struct initialisers elsewhere (Entity, Player,
// Engine) so the literals exist only once.
inline constexpr PlayerDefinition kDefaultPlayerDefinition{};
inline constexpr WeaponDefinition kDefaultWeaponDefinition{};
inline constexpr EnemyDefinition kDefaultEnemyDefinition{};
inline constexpr PickupDefinitions kDefaultPickupDefinitions{};
