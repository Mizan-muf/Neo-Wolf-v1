#include "engine/ui/UiRenderer.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <string>

#include "engine/core/Color.h"
#include "engine/render/Framebuffer.h"

namespace {
using Glyph = std::array<std::uint8_t, 7>;

Glyph GetGlyph(char c) {
    switch (c) {
    case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
    case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
    case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
    case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
    case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
    case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
    case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
    case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
    case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
    case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
    case ':': return {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00};
    case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00};
    case ',': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08};
    case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    default: return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x00, 0x08};
    }
}

void DrawGlyph(
    Framebuffer& framebuffer,
    int x,
    int y,
    const Glyph& glyph,
    std::uint32_t color,
    int scale) {
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 5; ++col) {
            if ((glyph[row] & (1 << (4 - col))) == 0) {
                continue;
            }
            framebuffer.DrawRect(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}
} // namespace

void UiRenderer::DrawText(
    Framebuffer& framebuffer,
    int x,
    int y,
    const std::string& text,
    std::uint32_t color,
    int scale) {
    if (scale < 1) {
        scale = 1;
    }

    int cursorX = x;
    for (char c : text) {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        DrawGlyph(framebuffer, cursorX, y, GetGlyph(upper), color, scale);
        cursorX += 6 * scale;
    }
}

void UiRenderer::DrawHud(
    Framebuffer& framebuffer,
    int health,
    int ammo) {
    const std::uint32_t panel = PackARGB32(MakeColor(8, 12, 20, 235));
    const std::uint32_t label = PackARGB32(MakeColor(173, 202, 236));
    const std::uint32_t value = PackARGB32(MakeColor(246, 247, 250));

    framebuffer.DrawRect(14, framebuffer.Height() - 54, 230, 40, panel);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "HEALTH: %d", health);
    DrawText(framebuffer, 22, framebuffer.Height() - 46, buffer, label, 2);
    std::snprintf(buffer, sizeof(buffer), "AMMO: %d", ammo);
    DrawText(framebuffer, 22, framebuffer.Height() - 28, buffer, value, 2);
}

void UiRenderer::DrawCrosshair(Framebuffer& framebuffer) {
    const int cx = framebuffer.Width() / 2;
    const int cy = framebuffer.Height() / 2;
    const std::uint32_t crosshair = PackARGB32(MakeColor(248, 240, 220));
    framebuffer.DrawRect(cx - 8, cy, 6, 1, crosshair);
    framebuffer.DrawRect(cx + 3, cy, 6, 1, crosshair);
    framebuffer.DrawRect(cx, cy - 8, 1, 6, crosshair);
    framebuffer.DrawRect(cx, cy + 3, 1, 6, crosshair);
}

void UiRenderer::DrawDebugOverlay(
    Framebuffer& framebuffer,
    float fps,
    float playerX,
    float playerY,
    float angleDegrees,
    int cellX,
    int cellY) {
    const std::uint32_t panel = PackARGB32(MakeColor(8, 12, 20, 210));
    const std::uint32_t text = PackARGB32(MakeColor(196, 231, 255));

    framebuffer.DrawRect(framebuffer.Width() - 292, 14, 278, 92, panel);

    char line[128];
    std::snprintf(line, sizeof(line), "FPS: %.1f", fps);
    DrawText(framebuffer, framebuffer.Width() - 282, 22, line, text, 2);
    std::snprintf(line, sizeof(line), "POS: %.2f, %.2f", playerX, playerY);
    DrawText(framebuffer, framebuffer.Width() - 282, 38, line, text, 2);
    std::snprintf(line, sizeof(line), "ANG: %.1f", angleDegrees);
    DrawText(framebuffer, framebuffer.Width() - 282, 54, line, text, 2);
    std::snprintf(line, sizeof(line), "CELL: %d, %d", cellX, cellY);
    DrawText(framebuffer, framebuffer.Width() - 282, 70, line, text, 2);
}

void UiRenderer::DrawPauseOverlay(Framebuffer& framebuffer) {
    const int boxW = 260;
    const int boxH = 56;
    const int x = (framebuffer.Width() - boxW) / 2;
    const int y = (framebuffer.Height() - boxH) / 2;
    const std::uint32_t panel = PackARGB32(MakeColor(18, 20, 26, 230));
    const std::uint32_t text = PackARGB32(MakeColor(255, 231, 161));
    framebuffer.DrawRect(x, y, boxW, boxH, panel);
    DrawText(framebuffer, x + 32, y + 18, "PAUSED  PRESS ESC", text, 2);
}
