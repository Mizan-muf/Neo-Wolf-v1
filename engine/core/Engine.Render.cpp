#include "engine/core/Engine.h"

#include <cmath>
#include <vector>

#include "engine/core/Color.h"
#include "engine/core/Log.h"
#include "engine/platform/PlatformSDL.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/ParticleRenderer.h"
#include "engine/render/Raycaster.h"
#include "engine/render/SpriteRenderer.h"
#include "engine/ui/UiRenderer.h"

namespace {
constexpr float kFieldOfViewRadians = 1.04719755f; // 60 degrees

void RenderCeilingAndFloor(
    Framebuffer& framebuffer,
    const Texture* ceilingTexture,
    const Texture* floorTexture) {
    const int halfHeight = framebuffer.Height() / 2;
    const int width = framebuffer.Width();

    for (int y = 0; y < halfHeight; ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint32_t pixel = 0xFF2A2A2Au;
            if (ceilingTexture != nullptr && ceilingTexture->Width() > 0 && ceilingTexture->Height() > 0) {
                const int tx = x % ceilingTexture->Width();
                const int ty = y % ceilingTexture->Height();
                pixel = ceilingTexture->GetPixel(tx, ty);
            }
            framebuffer.PutPixel(x, y, pixel);
        }
    }

    for (int y = halfHeight; y < framebuffer.Height(); ++y) {
        for (int x = 0; x < width; ++x) {
            std::uint32_t pixel = 0xFF3F3F3Fu;
            if (floorTexture != nullptr && floorTexture->Width() > 0 && floorTexture->Height() > 0) {
                const int tx = x % floorTexture->Width();
                const int ty = y % floorTexture->Height();
                pixel = floorTexture->GetPixel(tx, ty);
            }
            framebuffer.PutPixel(x, y, pixel);
        }
    }
}

void RenderTopDownMap(
    Framebuffer& framebuffer,
    const Map& map,
    const Player& player,
    const std::vector<RaycastHit>& /*hits*/,
    const std::vector<SpriteEntity>& sprites) {
    constexpr int kCellSize = 4;
    constexpr int kOffsetX = 8;
    constexpr int kOffsetY = 8;

    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            const TileType tile = map.GetCell(x, y);
            std::uint32_t color = 0x88202020u;
            if (tile == TileType::Wall) {
                color = 0xDD888888u;
            } else if (tile == TileType::WallAltA) {
                color = 0xDDAA6666u;
            } else if (tile == TileType::WallAltB) {
                color = 0xDD6666AAu;
            } else if (tile == TileType::Door) {
                color = 0xDDCC8833u;
            } else if (tile == TileType::Trigger) {
                color = 0xDD44CC44u;
            }
            framebuffer.DrawRect(kOffsetX + x * kCellSize, kOffsetY + y * kCellSize, kCellSize - 1, kCellSize - 1, color);
        }
    }

    // Sprites on minimap
    for (const SpriteEntity& spr : sprites) {
        if (!spr.active) continue;
        const int sx = kOffsetX + static_cast<int>(spr.position.x * kCellSize);
        const int sy = kOffsetY + static_cast<int>(spr.position.y * kCellSize);
        const std::uint32_t col = (spr.type == SpriteType::Enemy) ? 0xFFFF3333u : 0xFF33FF33u;
        framebuffer.DrawRect(sx - 1, sy - 1, 3, 3, col);
    }

    // Player position
    const int px = kOffsetX + static_cast<int>(player.position.x * kCellSize);
    const int py = kOffsetY + static_cast<int>(player.position.y * kCellSize);
    framebuffer.DrawRect(px - 1, py - 1, 3, 3, 0xFFFFFF00u);
}
} // namespace

void Engine::Render() {
    if (!running_ || platform_ == nullptr) {
        return;
    }

    if (state_ == GameState::Menu) {
        UiRenderer::DrawMenuScreen(framebuffer_);
        platform_->PresentFramebuffer(framebuffer_);  // the frame's only present
        return;
    }

    // Gameplay, Paused, GameOver, Victory screens share world backdrop
    RenderCeilingAndFloor(framebuffer_, ceilingTexture_, floorTexture_);

    RaycasterRenderParams rayParams{};
    rayParams.fieldOfViewRadians = kFieldOfViewRadians;
    rayParams.wallTexture = wallTexture_;
    rayParams.wallAltATexture = wallAltATexture_;
    rayParams.wallAltBTexture = wallAltBTexture_;
    rayParams.doorTexture = doorTexture_;
    rayParams.floorTexture = floorTexture_;
    rayParams.ceilingTexture = ceilingTexture_;
    rayParams.missingTexture = &missingTexture_;

    Raycaster::RenderWalls(map_, player_, framebuffer_, rayParams, latestRayHits_);

    latestWallDepthBuffer_.resize(latestRayHits_.size());
    for (std::size_t i = 0; i < latestRayHits_.size(); ++i) {
        latestWallDepthBuffer_[i] = latestRayHits_[i].distanceCorrected;
    }

    SpriteTextureSet spriteTextures{};
    spriteTextures.enemy = enemySpriteTexture_;
    spriteTextures.pickup = pickupSpriteTexture_;
    spriteTextures.decoration = decorationSpriteTexture_;
    spriteTextures.missing = &missingTexture_;

    SpriteRenderer::Render(
        map_,
        player_,
        framebuffer_,
        latestWallDepthBuffer_,
        sprites_,
        spriteTextures,
        kFieldOfViewRadians);

    ParticleRenderer::Render(
        map_,
        player_,
        framebuffer_,
        latestWallDepthBuffer_,
        particleManager_.Particles(),
        kFieldOfViewRadians);

    if (debugTopDownEnabled_) {
        RenderTopDownMap(framebuffer_, map_, player_, latestRayHits_, sprites_);
    }

    UiRenderer::DrawCrosshair(framebuffer_);
    UiRenderer::DrawHud(framebuffer_, playerHealth_, playerAmmo_, hudMessage_);

    if (state_ == GameState::Paused) {
        UiRenderer::DrawPauseOverlay(framebuffer_);
    } else if (state_ == GameState::GameOver) {
        UiRenderer::DrawGameOverScreen(framebuffer_);
    } else if (state_ == GameState::Victory) {
        UiRenderer::DrawVictoryScreen(framebuffer_);
    }

    if (debugTopDownEnabled_) {
        const float fps = time_.deltaSeconds > 0.0 ? static_cast<float>(1.0 / time_.deltaSeconds) : 0.0f;
        const int cellX = static_cast<int>(std::floor(player_.position.x));
        const int cellY = static_cast<int>(std::floor(player_.position.y));
        UiRenderer::DrawDebugOverlay(
            framebuffer_,
            fps,
            player_.position.x,
            player_.position.y,
            player_.directionAngleRadians * 57.2957795f,
            cellX,
            cellY);
    }

    platform_->PresentFramebuffer(framebuffer_);  // the frame's only present
}
