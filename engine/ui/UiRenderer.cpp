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
    case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
    case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
    case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
    case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
    case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
    case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
    case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
    case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
    case 'J': return {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
    case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
    case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
    case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
    case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
    case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
    case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
    case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
    case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
    case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
    case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
    case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
    case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
    case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
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
    case '!': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
    case '?': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
    case '+': return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
    case '(': return {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
    case ')': return {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
    case '/': return {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};
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
    int ammo,
    const std::string& message) {
    const std::uint32_t panel = PackARGB32(MakeColor(8, 12, 20, 235));
    const std::uint32_t label = PackARGB32(MakeColor(173, 202, 236));
    const std::uint32_t value = PackARGB32(MakeColor(246, 247, 250));
    const std::uint32_t msgColor = PackARGB32(MakeColor(255, 235, 120));

    framebuffer.DrawRect(14, framebuffer.Height() - 54, 230, 40, panel);

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "HEALTH: %d", health);
    DrawText(framebuffer, 22, framebuffer.Height() - 46, buffer, label, 2);
    std::snprintf(buffer, sizeof(buffer), "AMMO: %d", ammo);
    DrawText(framebuffer, 22, framebuffer.Height() - 28, buffer, value, 2);

    if (!message.empty()) {
        const int msgW = static_cast<int>(message.size()) * 12 + 20;
        const int msgX = (framebuffer.Width() - msgW) / 2;
        framebuffer.DrawRect(msgX, framebuffer.Height() - 54, msgW, 30, panel);
        DrawText(framebuffer, msgX + 10, framebuffer.Height() - 46, message, msgColor, 2);
    }
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
    const int boxW = 340;
    const int boxH = 90;
    const int x = (framebuffer.Width() - boxW) / 2;
    const int y = (framebuffer.Height() - boxH) / 2;
    const std::uint32_t panel = PackARGB32(MakeColor(18, 20, 26, 235));
    const std::uint32_t gold = PackARGB32(MakeColor(255, 231, 161));
    const std::uint32_t sub = PackARGB32(MakeColor(200, 215, 230));

    framebuffer.DrawRect(x, y, boxW, boxH, panel);
    DrawText(framebuffer, x + 105, y + 16, "PAUSED", gold, 3);
    DrawText(framebuffer, x + 24, y + 50, "ESC: RESUME  R: RESTART", sub, 2);
    DrawText(framebuffer, x + 50, y + 68, "ENTER: MAIN MENU", sub, 2);
}

void UiRenderer::DrawMenuScreen(Framebuffer& framebuffer) {
    framebuffer.Clear(PackARGB32(MakeColor(12, 14, 22, 255)));

    const int cx = framebuffer.Width() / 2;
    const std::uint32_t titleColor = PackARGB32(MakeColor(255, 60, 60));
    const std::uint32_t subColor = PackARGB32(MakeColor(255, 215, 0));
    const std::uint32_t textColor = PackARGB32(MakeColor(200, 220, 240));
    const std::uint32_t panel = PackARGB32(MakeColor(20, 25, 38, 230));

    const int panelW = 440;
    const int panelH = 260;
    const int px = (framebuffer.Width() - panelW) / 2;
    const int py = (framebuffer.Height() - panelH) / 2;
    framebuffer.DrawRect(px, py, panelW, panelH, panel);

    DrawText(framebuffer, cx - 120, py + 20, "NEO-WOLF", titleColor, 4);
    DrawText(framebuffer, cx - 145, py + 65, "PRESS ENTER OR SPACE TO START", subColor, 2);

    DrawText(framebuffer, px + 30, py + 105, "CONTROLS:", titleColor, 2);
    DrawText(framebuffer, px + 30, py + 130, "W/S: MOVE FORWARD / BACK", textColor, 2);
    DrawText(framebuffer, px + 30, py + 150, "A/D: STRAFE LEFT / RIGHT", textColor, 2);
    DrawText(framebuffer, px + 30, py + 170, "LEFT/RIGHT OR Q: TURN", textColor, 2);
    DrawText(framebuffer, px + 30, py + 190, "SPACE / CTRL / F: FIRE WEAPON", textColor, 2);
    DrawText(framebuffer, px + 30, py + 210, "E: INTERACT (DOORS / SWITCHES)", textColor, 2);
    DrawText(framebuffer, px + 30, py + 230, "P / ESC: PAUSE | R: RESTART", textColor, 2);
}

void UiRenderer::DrawGameOverScreen(Framebuffer& framebuffer) {
    const int boxW = 380;
    const int boxH = 120;
    const int x = (framebuffer.Width() - boxW) / 2;
    const int y = (framebuffer.Height() - boxH) / 2;
    const std::uint32_t panel = PackARGB32(MakeColor(30, 8, 8, 240));
    const std::uint32_t red = PackARGB32(MakeColor(255, 50, 50));
    const std::uint32_t text = PackARGB32(MakeColor(230, 200, 200));

    framebuffer.DrawRect(x, y, boxW, boxH, panel);
    DrawText(framebuffer, x + 85, y + 20, "GAME OVER", red, 3);
    DrawText(framebuffer, x + 50, y + 60, "YOU SUCCUMBED TO THE DUNGEON.", text, 2);
    DrawText(framebuffer, x + 35, y + 85, "PRESS R TO RESTART OR ENTER FOR MENU", text, 2);
}

void UiRenderer::DrawVictoryScreen(Framebuffer& framebuffer) {
    const int boxW = 400;
    const int boxH = 120;
    const int x = (framebuffer.Width() - boxW) / 2;
    const int y = (framebuffer.Height() - boxH) / 2;
    const std::uint32_t panel = PackARGB32(MakeColor(10, 30, 15, 240));
    const std::uint32_t gold = PackARGB32(MakeColor(255, 220, 60));
    const std::uint32_t text = PackARGB32(MakeColor(200, 245, 210));

    framebuffer.DrawRect(x, y, boxW, boxH, panel);
    DrawText(framebuffer, x + 115, y + 20, "VICTORY!", gold, 3);
    DrawText(framebuffer, x + 65, y + 60, "LEVEL OBJECTIVE CLEARED!", text, 2);
    DrawText(framebuffer, x + 45, y + 85, "PRESS R TO REPLAY OR ENTER FOR MENU", text, 2);
}
