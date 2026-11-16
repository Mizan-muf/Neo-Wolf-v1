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

// One texture per SpriteTextureId slot (sprites.cfg); a null slot is drawn with `missing`.
struct SpriteTextureSet {
    std::array<const Texture*, kSpriteTextureCount> slots{};
    const Texture* missing = nullptr;

    void Set(SpriteTextureId id, const Texture* texture) { slots[static_cast<std::size_t>(id)] = texture; }
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
