#pragma once

#include <cstdint>

#include "engine/core/Vec2.h"

// What a sprite stands for (minimap symbols). Values are appended only.
enum class SpriteType : std::uint8_t {
    Enemy = 0,
    Pickup,
    Decoration,
    Key,    // a key card pickup (Phase 6)
    Corpse  // a dead enemy (Phase 6)
};

// Sprite texture slots: one per sprites.cfg key (docs/CONTENT_DEFINITIONS.md "sprites.cfg").
// 0-2 are the pre-Phase 6 slots (enemy = guard, pickup = health, decoration).
enum class SpriteTextureId : int {
    Guard = 0,
    Health = 1,
    Decoration = 2,
    Brute,
    Runner,
    GuardDead,
    BruteDead,
    RunnerDead,
    Ammo,
    KeyRed,
    KeyBlue,
    KeyGold,
    Count
};

inline constexpr int kSpriteTextureCount = static_cast<int>(SpriteTextureId::Count);

struct SpriteEntity {
    Vec2 position{};
    int textureId = 0;  // a SpriteTextureId
    int entityId = -1;
    SpriteType type = SpriteType::Decoration;
    bool active = true;
    bool visible = true;
    bool flash = false;  // drawn brightened: the enemy was just hit
};
