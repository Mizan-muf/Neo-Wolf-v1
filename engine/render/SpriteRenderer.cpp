#include "engine/render/SpriteRenderer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "engine/assets/Texture.h"
#include "engine/core/MathUtils.h"
#include "engine/render/Framebuffer.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

namespace {
struct SpriteRenderCandidate {
    std::size_t index = 0;
    float distanceSq = 0.0f;
};

const Texture* ResolveTexture(const SpriteTextureSet& textures, const SpriteEntity& sprite) {
    if (sprite.textureId == 0 && textures.enemy != nullptr) {
        return textures.enemy;
    }
    if (sprite.textureId == 1 && textures.pickup != nullptr) {
        return textures.pickup;
    }
    if (sprite.textureId == 2 && textures.decoration != nullptr) {
        return textures.decoration;
    }

    switch (sprite.type) {
    case SpriteType::Enemy:
        return (textures.enemy != nullptr) ? textures.enemy : textures.missing;
    case SpriteType::Pickup:
        return (textures.pickup != nullptr) ? textures.pickup : textures.missing;
    case SpriteType::Decoration:
    default:
        return (textures.decoration != nullptr) ? textures.decoration : textures.missing;
    }
}

bool IsSpritePixelTransparent(std::uint32_t argb) {
    // Color-key magenta for simple sprite masks.
    return (argb & 0x00FFFFFFu) == 0x00FF00FFu;
}
} // namespace

std::vector<std::size_t> SpriteRenderer::BuildSortedSpriteIndices(
    const Player& player,
    const std::vector<SpriteEntity>& sprites) {
    std::vector<SpriteRenderCandidate> candidates;
    candidates.reserve(sprites.size());
    for (std::size_t i = 0; i < sprites.size(); ++i) {
        const SpriteEntity& sprite = sprites[i];
        if (!sprite.active || !sprite.visible) {
            continue;
        }

        const float dx = sprite.position.x - player.position.x;
        const float dy = sprite.position.y - player.position.y;
        candidates.push_back({i, dx * dx + dy * dy});
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const SpriteRenderCandidate& a, const SpriteRenderCandidate& b) {
            return a.distanceSq > b.distanceSq;
        });

    std::vector<std::size_t> result;
    result.reserve(candidates.size());
    for (const SpriteRenderCandidate& candidate : candidates) {
        result.push_back(candidate.index);
    }
    return result;
}

void SpriteRenderer::Render(
    const Map& map,
    const Player& player,
    Framebuffer& framebuffer,
    const std::vector<float>& wallDepthBuffer,
    const std::vector<SpriteEntity>& sprites,
    const SpriteTextureSet& textures,
    float fieldOfViewRadians) {
    const int screenWidth = framebuffer.Width();
    const int screenHeight = framebuffer.Height();
    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }
    if (wallDepthBuffer.size() != static_cast<std::size_t>(screenWidth)) {
        return;
    }

    const std::vector<std::size_t> sortedIndices = BuildSortedSpriteIndices(player, sprites);

    const Vec2 forward{std::cos(player.directionAngleRadians), std::sin(player.directionAngleRadians)};
    const float halfFov = fieldOfViewRadians * 0.5f;
    const float planeScale = std::tan(halfFov);
    const Vec2 cameraPlane{-forward.y * planeScale, forward.x * planeScale};
    const float det = (cameraPlane.x * forward.y) - (forward.x * cameraPlane.y);
    if (std::fabs(det) < 1e-6f) {
        return;
    }
    const float invDet = 1.0f / det;

    for (std::size_t spriteIndex : sortedIndices) {
        const SpriteEntity& sprite = sprites[spriteIndex];
        const Texture* texture = ResolveTexture(textures, sprite);
        if (texture == nullptr || texture->Width() <= 0 || texture->Height() <= 0) {
            continue;
        }

        const float relX = sprite.position.x - player.position.x;
        const float relY = sprite.position.y - player.position.y;

        const float transformX = invDet * ((forward.y * relX) - (forward.x * relY));
        const float transformY = invDet * ((-cameraPlane.y * relX) + (cameraPlane.x * relY));
        if (transformY <= 0.001f) {
            continue;
        }

        const int spriteScreenX = static_cast<int>((static_cast<float>(screenWidth) * 0.5f) * (1.0f + transformX / transformY));
        const int spriteHeight = std::max(1, static_cast<int>(std::fabs(static_cast<float>(screenHeight) / transformY)));
        const int spriteWidth = spriteHeight;

        const int drawStartY = std::max(0, (-spriteHeight / 2) + (screenHeight / 2));
        const int drawEndY = std::min(screenHeight - 1, (spriteHeight / 2) + (screenHeight / 2));
        const int drawStartX = std::max(0, (-spriteWidth / 2) + spriteScreenX);
        const int drawEndX = std::min(screenWidth - 1, (spriteWidth / 2) + spriteScreenX);

        for (int stripe = drawStartX; stripe <= drawEndX; ++stripe) {
            if (transformY >= wallDepthBuffer[static_cast<std::size_t>(stripe)]) {
                continue;
            }

            const float texU = static_cast<float>(stripe - ((-spriteWidth / 2) + spriteScreenX)) / static_cast<float>(spriteWidth);
            if (texU < 0.0f || texU > 1.0f) {
                continue;
            }

            for (int y = drawStartY; y <= drawEndY; ++y) {
                const float texV = static_cast<float>(y - drawStartY) / static_cast<float>(std::max(1, drawEndY - drawStartY + 1));
                const std::uint32_t color = texture->SampleNearest(texU, texV);
                if (IsSpritePixelTransparent(color)) {
                    continue;
                }

                const int cellX = static_cast<int>(std::floor(sprite.position.x));
                const int cellY = static_cast<int>(std::floor(sprite.position.y));
                const float mapLight = map.GetLightLevel(cellX, cellY);
                const float shade = Clamp((1.0f / (1.0f + transformY * 0.12f)) * mapLight, 0.35f, 1.0f);
                const std::uint8_t a = static_cast<std::uint8_t>((color >> 24) & 0xFFu);
                const std::uint8_t r = static_cast<std::uint8_t>((color >> 16) & 0xFFu);
                const std::uint8_t g = static_cast<std::uint8_t>((color >> 8) & 0xFFu);
                const std::uint8_t b = static_cast<std::uint8_t>(color & 0xFFu);
                const std::uint32_t shaded =
                    (static_cast<std::uint32_t>(a) << 24) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(r * shade)) << 16) |
                    (static_cast<std::uint32_t>(static_cast<std::uint8_t>(g * shade)) << 8) |
                    static_cast<std::uint32_t>(static_cast<std::uint8_t>(b * shade));
                framebuffer.PutPixel(stripe, y, shaded);
            }
        }
    }
}
