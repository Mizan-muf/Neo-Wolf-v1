#pragma once

// Billboard sprites: what the renderer draws for enemies, pickups and decorations.

#include <array>
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

// Sprite slots. The first kSpriteRoleCount slots are the sprites the engine itself needs: its
// pickups, key cards and decoration (sprites.cfg keys kSpriteRoleKeys). The game's enemy types
// add their own slots after them, named by gameplay.json (SpriteSlotNames in
// engine/assets/GameplayConfig.h); the engine knows no enemy sprite by name.
enum class SpriteRole : int { Health = 0, Ammo, Decoration, KeyRed, KeyBlue, KeyGold, Count };

inline constexpr int kSpriteRoleCount = static_cast<int>(SpriteRole::Count);

// sprites.cfg keys of the role slots, in SpriteRole order (all required).
inline constexpr std::array<const char*, kSpriteRoleCount> kSpriteRoleKeys = {"health",  "ammo",     "decoration",
                                                                              "key_red", "key_blue", "key_gold"};

constexpr int SpriteRoleSlot(SpriteRole role) {
    return static_cast<int>(role);
}

struct SpriteEntity {
    Vec2 position{};
    int textureId = 0;  // a sprite slot (see SpriteRole); an unknown slot draws the missing texture
    int entityId = -1;
    SpriteType type = SpriteType::Decoration;
    bool active = true;
    bool visible = true;
    bool flash = false;  // drawn brightened: the enemy was just hit
};
