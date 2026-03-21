#include "engine/core/Engine.h"

#include <algorithm>
#include <cmath>

#include "engine/core/Color.h"
#include "engine/platform/PlatformSDL.h"

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
} // namespace

void Engine::Render() {
    if (!running_ || platform_ == nullptr) {
        return;
    }

    framebuffer_.Clear(PackARGB32(MakeColor(14, 16, 22)));

    if (map_.Width() > 0 && map_.Height() > 0) {
        const int horizontalCellSize = framebuffer_.Width() / (map_.Width() + 2);
        const int verticalCellSize = framebuffer_.Height() / (map_.Height() + 2);
        const int cellSize = std::max(4, std::min(horizontalCellSize, verticalCellSize));

        const int mapPixelWidth = map_.Width() * cellSize;
        const int mapPixelHeight = map_.Height() * cellSize;
        const int mapOriginX = (framebuffer_.Width() - mapPixelWidth) / 2;
        const int mapOriginY = (framebuffer_.Height() - mapPixelHeight) / 2;

        const std::uint32_t emptyColor = PackARGB32(MakeColor(34, 39, 48));
        const std::uint32_t wallColor = PackARGB32(MakeColor(205, 220, 240));
        const std::uint32_t doorColor = PackARGB32(MakeColor(201, 135, 45));
        const std::uint32_t triggerColor = PackARGB32(MakeColor(86, 191, 107));
        const std::uint32_t gridColor = PackARGB32(MakeColor(20, 23, 30));
        const std::uint32_t playerColor = PackARGB32(MakeColor(255, 64, 64));
        const std::uint32_t facingColor = PackARGB32(MakeColor(255, 209, 102));

        for (int y = 0; y < map_.Height(); ++y) {
            for (int x = 0; x < map_.Width(); ++x) {
                std::uint32_t tileColor = emptyColor;
                const TileType tile = map_.GetCell(x, y);
                if (tile == TileType::Wall) {
                    tileColor = wallColor;
                } else if (tile == TileType::Door) {
                    tileColor = doorColor;
                } else if (tile == TileType::Trigger) {
                    tileColor = triggerColor;
                }

                const int px = mapOriginX + x * cellSize;
                const int py = mapOriginY + y * cellSize;
                framebuffer_.DrawRect(px, py, cellSize, cellSize, tileColor);

                if (cellSize > 4) {
                    framebuffer_.DrawRect(px, py, cellSize, 1, gridColor);
                    framebuffer_.DrawRect(px, py, 1, cellSize, gridColor);
                }
            }
        }

        const float playerScreenX = static_cast<float>(mapOriginX) + player_.position.x * static_cast<float>(cellSize);
        const float playerScreenY = static_cast<float>(mapOriginY) + player_.position.y * static_cast<float>(cellSize);
        const int playerRadiusPixels = std::max(2, static_cast<int>(std::round(player_.collisionRadius * cellSize)));
        framebuffer_.DrawRect(
            static_cast<int>(std::round(playerScreenX)) - playerRadiusPixels,
            static_cast<int>(std::round(playerScreenY)) - playerRadiusPixels,
            playerRadiusPixels * 2 + 1,
            playerRadiusPixels * 2 + 1,
            playerColor);

        const float facingLineLength = static_cast<float>(cellSize) * 0.7f;
        const int facingEndX = static_cast<int>(std::round(
            playerScreenX + std::cos(player_.directionAngleRadians) * facingLineLength));
        const int facingEndY = static_cast<int>(std::round(
            playerScreenY + std::sin(player_.directionAngleRadians) * facingLineLength));
        DrawLine(
            framebuffer_,
            static_cast<int>(std::round(playerScreenX)),
            static_cast<int>(std::round(playerScreenY)),
            facingEndX,
            facingEndY,
            facingColor);
    }

    platform_->PresentFramebuffer(framebuffer_);
}
