#pragma once

// Billboard sprites (enemies, corpses, pickups, keys, decorations): sorted far to near and
// clipped per column against the wall depth buffer; magenta pixels are transparent. A sprite
// is a square one world unit across, projected like the walls (RenderMath.h), so it keeps its
// size relative to them at every framebuffer size.

#include <array>
#include <cstddef>
#include <vector>

#include "engine/world/Sprite.h"

class Framebuffer;
class Texture;
class Map;
struct Player;

// One texture per sprite slot (SpriteRole, then the game's enemy sprites); a null or unknown
// slot is drawn with `missing`.
struct SpriteTextureSet {
    std::vector<const Texture*> slots;
    const Texture* missing = nullptr;

    void Set(int slot, const Texture* texture) {
        if (slot < 0) {
            return;
        }
        if (static_cast<std::size_t>(slot) >= slots.size()) {
            slots.resize(static_cast<std::size_t>(slot) + 1, nullptr);
        }
        slots[static_cast<std::size_t>(slot)] = texture;
    }
    void Set(SpriteRole role, const Texture* texture) { Set(SpriteRoleSlot(role), texture); }
};

// A sprite of an enemy that was just hit is drawn this much closer to white (0-1).
inline constexpr float kSpriteHitFlashStrength = 0.65f;

class SpriteRenderer {
public:
    static std::vector<std::size_t> BuildSortedSpriteIndices(const Player& player,
                                                             const std::vector<SpriteEntity>& sprites);

    static void Render(const Map& map, const Player& player, Framebuffer& framebuffer,
                       const std::vector<float>& wallDepthBuffer, const std::vector<SpriteEntity>& sprites,
                       const SpriteTextureSet& textures);
};
