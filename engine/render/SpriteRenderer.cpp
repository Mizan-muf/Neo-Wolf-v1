#include "engine/render/SpriteRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "engine/assets/Texture.h"
#include "engine/core/MathUtils.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/RenderMath.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

namespace {
struct SpriteRenderCandidate {
    std::size_t index = 0;
    float distanceSq = 0.0f;
};

const Texture* ResolveTexture(const SpriteTextureSet& textures, const SpriteEntity& sprite) {
    if (sprite.textureId >= 0 && sprite.textureId < kSpriteTextureCount) {
        const Texture* texture = textures.slots[static_cast<std::size_t>(sprite.textureId)];
        if (texture != nullptr) {
            return texture;
        }
    }
    return textures.missing;
}

// Sprite shading: distance falloff per unit times the light level, never below kSpriteMinShade.
constexpr float kSpriteFalloff = 0.12f;
constexpr float kSpriteMinShade = 0.35f;
// Sprite textures use magenta as the colour key for transparent pixels (alpha is ignored).
constexpr std::uint32_t kSpriteColorKey = 0x00FF00FFu;
constexpr std::uint32_t kRgbMask = 0x00FFFFFFu;

std::uint8_t ShadeChannel(std::uint8_t value, float shade, bool flash) {
    float result = static_cast<float>(value) * shade;
    if (flash) {
        result += (255.0f - result) * kSpriteHitFlashStrength;
    }
    return static_cast<std::uint8_t>(std::min(255.0f, result));
}

std::uint32_t ShadeSpritePixel(std::uint32_t argb, float shade, bool flash) {
    const ColorRGBA color = UnpackARGB32(argb);
    return PackARGB32(MakeColor(ShadeChannel(color.r, shade, flash), ShadeChannel(color.g, shade, flash),
                                ShadeChannel(color.b, shade, flash), color.a));
}

bool IsSpritePixelTransparent(std::uint32_t argb) {
    return (argb & kRgbMask) == kSpriteColorKey;
}

// Where a sprite lands on the screen: a square of `size` pixels centred on column centerX and
// on the horizon. topY is the unclamped top edge: texture rows are measured from it, so a
// sprite taller than the screen is cropped at the top and bottom, not squeezed to fit.
struct SpriteScreenRect {
    int leftX = 0;
    int size = 0;
    int topY = 0;
    int rows = 0;
    int drawStartX = 0;
    int drawEndX = 0;
    int drawStartY = 0;
    int drawEndY = 0;
};

SpriteScreenRect ComputeSpriteScreenRect(int centerX, float depth, int screenWidth, int screenHeight) {
    SpriteScreenRect rect{};
    rect.size = std::max(1, static_cast<int>(std::fabs(FocalLengthPixels(screenHeight) / depth)));
    rect.leftX = (-rect.size / 2) + centerX;
    rect.topY = (-rect.size / 2) + (screenHeight / 2);
    const int bottomY = (rect.size / 2) + (screenHeight / 2);
    rect.rows = bottomY - rect.topY + 1;
    rect.drawStartY = std::max(0, rect.topY);
    rect.drawEndY = std::min(screenHeight - 1, bottomY);
    rect.drawStartX = std::max(0, rect.leftX);
    rect.drawEndX = std::min(screenWidth - 1, (rect.size / 2) + centerX);
    return rect;
}

// Draws the sprite's columns that are in front of the walls (wallDepthBuffer).
void DrawSpriteColumns(Framebuffer& framebuffer, const std::vector<float>& wallDepthBuffer, const Texture& texture,
                       const SpriteScreenRect& rect, float depth, float shade, bool flash) {
    for (int stripe = rect.drawStartX; stripe <= rect.drawEndX; ++stripe) {
        if (depth >= wallDepthBuffer[static_cast<std::size_t>(stripe)]) {
            continue;
        }

        const float texU = static_cast<float>(stripe - rect.leftX) / static_cast<float>(rect.size);
        for (int y = rect.drawStartY; y <= rect.drawEndY; ++y) {
            const float texV = static_cast<float>(y - rect.topY) / static_cast<float>(rect.rows);
            const std::uint32_t color = texture.SampleNearest(texU, texV);
            if (!IsSpritePixelTransparent(color)) {
                framebuffer.PutPixel(stripe, y, ShadeSpritePixel(color, shade, flash));
            }
        }
    }
}
}  // namespace

std::vector<std::size_t> SpriteRenderer::BuildSortedSpriteIndices(const Player& player,
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

    std::sort(candidates.begin(), candidates.end(), [](const SpriteRenderCandidate& a, const SpriteRenderCandidate& b) {
        return a.distanceSq > b.distanceSq;
    });

    std::vector<std::size_t> result;
    result.reserve(candidates.size());
    for (const SpriteRenderCandidate& candidate : candidates) {
        result.push_back(candidate.index);
    }
    return result;
}

void SpriteRenderer::Render(const Map& map, const Player& player, Framebuffer& framebuffer,
                            const std::vector<float>& wallDepthBuffer, const std::vector<SpriteEntity>& sprites,
                            const SpriteTextureSet& textures) {
    const int screenWidth = framebuffer.Width();
    const int screenHeight = framebuffer.Height();
    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }
    if (wallDepthBuffer.size() != static_cast<std::size_t>(screenWidth)) {
        return;
    }

    const std::vector<std::size_t> sortedIndices = BuildSortedSpriteIndices(player, sprites);

    const std::optional<BillboardProjection> projection = MakeBillboardProjection(
        player.position, MakeCameraBasis(player.directionAngleRadians, screenWidth, screenHeight));
    if (!projection) {
        return;
    }

    for (const std::size_t spriteIndex : sortedIndices) {
        const SpriteEntity& sprite = sprites[spriteIndex];
        const Texture* texture = ResolveTexture(textures, sprite);
        if (!IsDrawable(texture)) {
            continue;
        }

        const ViewPoint view = ToViewSpace(*projection, sprite.position);
        if (view.depth <= kBillboardNearPlane) {
            continue;
        }

        const SpriteScreenRect rect =
            ComputeSpriteScreenRect(ScreenColumn(view, screenWidth), view.depth, screenWidth, screenHeight);
        const float mapLight = LightAt(map, sprite.position.x, sprite.position.y);
        const float shade = Clamp((1.0f / (1.0f + view.depth * kSpriteFalloff)) * mapLight, kSpriteMinShade, 1.0f);
        DrawSpriteColumns(framebuffer, wallDepthBuffer, *texture, rect, view.depth, shade, sprite.flash);
    }
}
