#pragma once

#include <vector>

#include "engine/world/Sprite.h"

class Framebuffer;
class Texture;
class Map;
struct Player;

struct SpriteTextureSet {
    const Texture* enemy = nullptr;
    const Texture* pickup = nullptr;
    const Texture* decoration = nullptr;
    const Texture* missing = nullptr;
};

class SpriteRenderer {
public:
    static std::vector<std::size_t> BuildSortedSpriteIndices(
        const Player& player,
        const std::vector<SpriteEntity>& sprites);

    static void Render(
        const Map& map,
        const Player& player,
        Framebuffer& framebuffer,
        const std::vector<float>& wallDepthBuffer,
        const std::vector<SpriteEntity>& sprites,
        const SpriteTextureSet& textures,
        float fieldOfViewRadians);
};
