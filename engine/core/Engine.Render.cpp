#include "engine/core/Engine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "engine/core/Color.h"
#include "engine/core/MathUtils.h"
#include "engine/platform/PlatformSDL.h"
#include "engine/render/ParticleRenderer.h"
#include "engine/render/Raycaster.h"
#include "engine/render/SpriteRenderer.h"
#include "engine/ui/UiRenderer.h"

namespace {
void DrawLine(
    Framebuffer& framebuffer,
    int x0,
    int y0,
    int x1,
    int y1,
    std::uint32_t color) {
    int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    while (true) {
        framebuffer.PutPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }

        const int doubledError = 2 * error;
        if (doubledError >= dy) {
            error += dy;
            x0 += sx;
        }
        if (doubledError <= dx) {
            error += dx;
            y0 += sy;
        }
    }
}

void DrawDebugTopDownMap(
    Framebuffer& framebuffer,
    const Map& map,
    const Player& player,
    const std::vector<RaycastHit>& hits,
    const std::vector<SpriteEntity>& sprites) {
    if (map.Width() <= 0 || map.Height() <= 0) {
        return;
    }

    const int targetOverlayWidth = framebuffer.Width() / 3;
    const int targetOverlayHeight = framebuffer.Height() / 3;
    const int horizontalCellSize = targetOverlayWidth / (map.Width() + 1);
    const int verticalCellSize = targetOverlayHeight / (map.Height() + 1);
    const int cellSize = std::max(4, std::min(horizontalCellSize, verticalCellSize));
    const int mapPixelWidth = map.Width() * cellSize;
    const int mapPixelHeight = map.Height() * cellSize;
    const int mapOriginX = 16;
    const int mapOriginY = 16;

    const std::uint32_t panelColor = PackARGB32(MakeColor(10, 12, 18, 220));
    framebuffer.DrawRect(
        mapOriginX - 6,
        mapOriginY - 6,
        mapPixelWidth + 12,
        mapPixelHeight + 12,
        panelColor);

    const std::uint32_t emptyColor = PackARGB32(MakeColor(34, 39, 48));
    const std::uint32_t wallColor = PackARGB32(MakeColor(205, 220, 240));
    const std::uint32_t wallAltAColor = PackARGB32(MakeColor(148, 192, 225));
    const std::uint32_t wallAltBColor = PackARGB32(MakeColor(228, 186, 121));
    const std::uint32_t doorColor = PackARGB32(MakeColor(201, 135, 45));
    const std::uint32_t triggerColor = PackARGB32(MakeColor(86, 191, 107));
    const std::uint32_t gridColor = PackARGB32(MakeColor(20, 23, 30));
    const std::uint32_t playerColor = PackARGB32(MakeColor(255, 64, 64));
    const std::uint32_t facingColor = PackARGB32(MakeColor(255, 209, 102));
    const std::uint32_t rayColor = PackARGB32(MakeColor(255, 246, 196, 180));
    const std::uint32_t enemySpriteColor = PackARGB32(MakeColor(231, 92, 92));
    const std::uint32_t pickupSpriteColor = PackARGB32(MakeColor(248, 210, 94));
    const std::uint32_t decorSpriteColor = PackARGB32(MakeColor(120, 180, 230));

    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            std::uint32_t tileColor = emptyColor;
            const TileType tile = map.GetCell(x, y);
            if (tile == TileType::Wall) {
                tileColor = wallColor;
            } else if (tile == TileType::WallAltA) {
                tileColor = wallAltAColor;
            } else if (tile == TileType::WallAltB) {
                tileColor = wallAltBColor;
            } else if (tile == TileType::Door) {
                tileColor = doorColor;
            } else if (tile == TileType::Trigger) {
                tileColor = triggerColor;
            }

            const int px = mapOriginX + x * cellSize;
            const int py = mapOriginY + y * cellSize;
            framebuffer.DrawRect(px, py, cellSize, cellSize, tileColor);

            if (cellSize > 4) {
                framebuffer.DrawRect(px, py, cellSize, 1, gridColor);
                framebuffer.DrawRect(px, py, 1, cellSize, gridColor);
            }
        }
    }

    const float playerScreenX = static_cast<float>(mapOriginX) + player.position.x * static_cast<float>(cellSize);
    const float playerScreenY = static_cast<float>(mapOriginY) + player.position.y * static_cast<float>(cellSize);

    for (std::size_t i = 0; i < hits.size(); i += 8) {
        const RaycastHit& hit = hits[i];
        if (!hit.hit) {
            continue;
        }

        const int rayEndX =
            static_cast<int>(std::round(static_cast<float>(mapOriginX) + hit.hitPosition.x * cellSize));
        const int rayEndY =
            static_cast<int>(std::round(static_cast<float>(mapOriginY) + hit.hitPosition.y * cellSize));
        DrawLine(
            framebuffer,
            static_cast<int>(std::round(playerScreenX)),
            static_cast<int>(std::round(playerScreenY)),
            rayEndX,
            rayEndY,
            rayColor);
    }

    const int playerRadiusPixels = std::max(2, static_cast<int>(std::round(player.collisionRadius * cellSize)));
    framebuffer.DrawRect(
        static_cast<int>(std::round(playerScreenX)) - playerRadiusPixels,
        static_cast<int>(std::round(playerScreenY)) - playerRadiusPixels,
        playerRadiusPixels * 2 + 1,
        playerRadiusPixels * 2 + 1,
        playerColor);

    const float facingLineLength = static_cast<float>(cellSize) * 0.7f;
    const int facingEndX = static_cast<int>(std::round(
        playerScreenX + std::cos(player.directionAngleRadians) * facingLineLength));
    const int facingEndY = static_cast<int>(std::round(
        playerScreenY + std::sin(player.directionAngleRadians) * facingLineLength));
    DrawLine(
        framebuffer,
        static_cast<int>(std::round(playerScreenX)),
        static_cast<int>(std::round(playerScreenY)),
        facingEndX,
        facingEndY,
        facingColor);

    for (const SpriteEntity& sprite : sprites) {
        if (!sprite.active || !sprite.visible) {
            continue;
        }

        const int sx = static_cast<int>(std::round(static_cast<float>(mapOriginX) + sprite.position.x * static_cast<float>(cellSize)));
        const int sy = static_cast<int>(std::round(static_cast<float>(mapOriginY) + sprite.position.y * static_cast<float>(cellSize)));
        std::uint32_t color = decorSpriteColor;
        if (sprite.type == SpriteType::Enemy) {
            color = enemySpriteColor;
        } else if (sprite.type == SpriteType::Pickup) {
            color = pickupSpriteColor;
        }

        framebuffer.DrawRect(sx - 2, sy - 2, 5, 5, color);
    }
}
} // namespace

void Engine::Render() {
    if (!running_ || platform_ == nullptr) {
        return;
    }

    framebuffer_.Clear(PackARGB32(MakeColor(14, 16, 22)));
    latestRayHits_.clear();
    latestWallDepthBuffer_.clear();

    if (map_.Width() > 0 && map_.Height() > 0) {
        RaycasterRenderParams params{};
        params.fieldOfViewRadians = DegreesToRadians(60.0f);
        params.wallTexture = wallTexture_;
        params.wallAltATexture = wallAltATexture_;
        params.wallAltBTexture = wallAltBTexture_;
        params.doorTexture = doorTexture_;
        params.floorTexture = floorTexture_;
        params.ceilingTexture = ceilingTexture_;
        params.missingTexture = &missingTexture_;
        Raycaster::RenderWalls(map_, player_, framebuffer_, params, latestRayHits_);

        latestWallDepthBuffer_.resize(latestRayHits_.size());
        for (std::size_t i = 0; i < latestRayHits_.size(); ++i) {
            latestWallDepthBuffer_[i] = latestRayHits_[i].hit
                ? latestRayHits_[i].distanceCorrected
                : 1000000.0f;
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
            params.fieldOfViewRadians);

        ParticleRenderer::Render(
            map_,
            player_,
            framebuffer_,
            latestWallDepthBuffer_,
            particleManager_.Particles(),
            params.fieldOfViewRadians);

        if (debugTopDownEnabled_) {
            DrawDebugTopDownMap(framebuffer_, map_, player_, latestRayHits_, sprites_);
        }
    }

    const float fps = (time_.deltaSeconds > 0.00001) ? static_cast<float>(1.0 / time_.deltaSeconds) : 0.0f;
    const float angleDegrees = RadiansToDegrees(player_.directionAngleRadians);
    const int cellX = static_cast<int>(std::floor(player_.position.x));
    const int cellY = static_cast<int>(std::floor(player_.position.y));

    UiRenderer::DrawHud(framebuffer_, playerHealth_, playerAmmo_);
    UiRenderer::DrawCrosshair(framebuffer_);
    UiRenderer::DrawDebugOverlay(
        framebuffer_,
        fps,
        player_.position.x,
        player_.position.y,
        angleDegrees,
        cellX,
        cellY);

    if (paused_) {
        UiRenderer::DrawPauseOverlay(framebuffer_);
    }

    platform_->PresentFramebuffer(framebuffer_);
}
