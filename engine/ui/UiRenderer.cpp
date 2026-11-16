#include "engine/ui/UiRenderer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "engine/core/Color.h"
#include "engine/render/Framebuffer.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"
#include "engine/world/Sprite.h"

namespace {
using Glyph = std::array<std::uint8_t, 7>;

Glyph GetGlyph(char c) {
    switch (c) {
        case 'A':
            return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        case 'B':
            return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
        case 'C':
            return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
        case 'D':
            return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
        case 'E':
            return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
        case 'F':
            return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
        case 'G':
            return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
        case 'H':
            return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
        case 'I':
            return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
        case 'J':
            return {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
        case 'K':
            return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
        case 'L':
            return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
        case 'M':
            return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
        case 'N':
            return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
        case 'O':
            return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        case 'P':
            return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
        case 'Q':
            return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
        case 'R':
            return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
        case 'S':
            return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
        case 'T':
            return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
        case 'U':
            return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
        case 'V':
            return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
        case 'W':
            return {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
        case 'X':
            return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
        case 'Y':
            return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
        case 'Z':
            return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
        case '0':
            return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
        case '1':
            return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
        case '2':
            return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
        case '3':
            return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
        case '4':
            return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
        case '5':
            return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
        case '6':
            return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
        case '7':
            return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
        case '8':
            return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
        case '9':
            return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
        case ':':
            return {0x00, 0x04, 0x00, 0x00, 0x04, 0x00, 0x00};
        case '.':
            return {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00};
        case ',':
            return {0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08};
        case '-':
            return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
        case '!':
            return {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04};
        case '?':
            return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04};
        case '+':
            return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
        case '(':
            return {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
        case ')':
            return {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
        case '/':
            return {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00};
        case ' ':
            return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        case '>':
            return {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08};
        case '<':
            return {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02};
        case '\'':
            return {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00};
        case '_':
            return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
        case '=':
            return {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00};
        case '*':
            return {0x00, 0x15, 0x0E, 0x1F, 0x0E, 0x15, 0x00};
        case '#':
            return {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A};
        case '%':
            return {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03};
        case '&':
            return {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D};
        default:
            return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x00, 0x08};
    }
}

void DrawGlyph(Framebuffer& framebuffer, int x, int y, const Glyph& glyph, std::uint32_t color, int scale) {
    for (int row = 0; row < UiLayout::kGlyphHeight; ++row) {
        for (int col = 0; col < UiLayout::kGlyphWidth; ++col) {
            if ((glyph[static_cast<std::size_t>(row)] & (1 << (4 - col))) == 0) {
                continue;
            }
            framebuffer.DrawRect(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

// Palette.
constexpr std::uint32_t Rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    return PackARGB32(MakeColor(r, g, b));
}
constexpr std::uint32_t kPanel = Rgb(14, 18, 28);
constexpr std::uint32_t kPanelBorder = Rgb(90, 110, 140);
constexpr std::uint32_t kHighlight = Rgb(44, 58, 92);
constexpr std::uint32_t kTitleRed = Rgb(255, 70, 60);
constexpr std::uint32_t kGold = Rgb(255, 220, 110);
constexpr std::uint32_t kText = Rgb(205, 222, 240);
constexpr std::uint32_t kTextDim = Rgb(105, 115, 130);
constexpr std::uint32_t kValue = Rgb(246, 247, 250);
constexpr std::uint32_t kWarn = Rgb(255, 90, 80);
constexpr std::uint32_t kMessage = Rgb(255, 235, 120);

using UiLayout::kMargin;
constexpr int kLineHeight = UiLayout::kGlyphHeight;  // at scale 1

// A filled panel with a 1-pixel border.
void DrawPanel(Framebuffer& framebuffer, int x, int y, int width, int height, std::uint32_t fill = kPanel,
               std::uint32_t border = kPanelBorder) {
    framebuffer.DrawRect(x, y, width, height, border);
    framebuffer.DrawRect(x + 1, y + 1, width - 2, height - 2, fill);
}

// A centred panel; returns its top-left corner.
void DrawCenteredPanel(Framebuffer& framebuffer, int width, int height, std::uint32_t fill, int& outX, int& outY) {
    width = std::min(width, framebuffer.Width() - 2 * kMargin);
    height = std::min(height, framebuffer.Height() - 2 * kMargin);
    outX = (framebuffer.Width() - width) / 2;
    outY = (framebuffer.Height() - height) / 2;
    DrawPanel(framebuffer, outX, outY, width, height, fill);
}

std::string Upper(const std::string& text) {
    std::string upper = text;
    for (char& c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return upper;
}

// Fills the part of the rectangle that lies inside the clip rectangle.
void FillClipped(Framebuffer& framebuffer, int x, int y, int width, int height, int clipX, int clipY, int clipW,
                 int clipH, std::uint32_t color) {
    const int x0 = std::max(x, clipX);
    const int y0 = std::max(y, clipY);
    const int x1 = std::min(x + width, clipX + clipW);
    const int y1 = std::min(y + height, clipY + clipH);
    if (x1 > x0 && y1 > y0) {
        framebuffer.DrawRect(x0, y0, x1 - x0, y1 - y0, color);
    }
}
}  // namespace

int UiRenderer::TextWidth(const std::string& text, int scale) {
    if (text.empty()) {
        return 0;
    }
    scale = std::max(1, scale);
    return static_cast<int>(text.size()) * UiLayout::kGlyphAdvance * scale - scale;
}

std::string UiRenderer::FitText(const std::string& text, int maxWidth, int scale) {
    if (TextWidth(text, scale) <= maxWidth) {
        return text;
    }
    std::string shortened = text;
    while (!shortened.empty() && TextWidth(shortened + "..", scale) > maxWidth) {
        shortened.pop_back();
    }
    while (!shortened.empty() && shortened.back() == ' ') {
        shortened.pop_back();
    }
    return shortened.empty() ? std::string() : shortened + "..";
}

void UiRenderer::DrawText(Framebuffer& framebuffer, int x, int y, const std::string& text, std::uint32_t color,
                          int scale) {
    scale = std::max(1, scale);
    int cursorX = x;
    for (const char c : text) {
        const char upper = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (upper != ' ') {
            DrawGlyph(framebuffer, cursorX, y, GetGlyph(upper), color, scale);
        }
        cursorX += UiLayout::kGlyphAdvance * scale;
    }
}

void UiRenderer::DrawTextCentered(Framebuffer& framebuffer, int centerX, int y, const std::string& text,
                                  std::uint32_t color, int scale) {
    DrawText(framebuffer, centerX - TextWidth(text, scale) / 2, y, text, color, scale);
}

std::string UiRenderer::LevelLabel(int levelNumber, const std::string& levelName) {
    if (levelNumber <= 0) {
        return {};
    }
    return "LEVEL " + std::to_string(levelNumber) + (levelName.empty() ? std::string() : ": " + Upper(levelName));
}

std::string UiRenderer::FormatDuration(double seconds) {
    const long total = static_cast<long>(std::max(0.0, std::floor(seconds)));
    char buffer[32] = {};
    if (total >= 3600) {
        std::snprintf(buffer, sizeof(buffer), "%ld:%02ld:%02ld", total / 3600, (total / 60) % 60, total % 60);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%ld:%02ld", total / 60, total % 60);
    }
    return buffer;
}

void UiRenderer::DrawHud(Framebuffer& framebuffer, const HudView& view) {
    const int width = framebuffer.Width();
    const int height = framebuffer.Height();

    // Bottom bar: health, ammo, (key-card slots, Phase 6), compact level label.
    const int barX = kMargin;
    const int barY = height - kMargin - UiLayout::kHudBarHeight;
    const int barW = width - 2 * kMargin;
    DrawPanel(framebuffer, barX, barY, barW, UiLayout::kHudBarHeight);
    const int textY = barY + (UiLayout::kHudBarHeight - kLineHeight) / 2;
    const int healthX = barX + 6;
    const int ammoX = healthX + 80;
    DrawText(framebuffer, healthX, textY, "HEALTH", kText);
    DrawText(framebuffer, healthX + TextWidth("HEALTH ", 1), textY, std::to_string(view.health),
             view.health <= 25 ? kWarn : kValue);
    DrawText(framebuffer, ammoX, textY, "AMMO", kText);
    DrawText(framebuffer, ammoX + TextWidth("AMMO ", 1), textY, std::to_string(view.ammo),
             view.ammo == 0 ? kWarn : kValue);
    for (const KeyColor color : kKeyColors) {
        if (!view.keys.Has(color)) {
            continue;
        }
        const UiRect card = KeyCardRect(width, height, color);
        DrawPanel(framebuffer, card.x, card.y, card.width, card.height, KeyCardColor(color), Rgb(20, 20, 24));
        const char letter[2] = {KeyColorLabel(color)[0], '\0'};
        DrawText(framebuffer, card.x + (card.width - UiLayout::kGlyphWidth) / 2,
                 card.y + (card.height - UiLayout::kGlyphHeight) / 2, letter, Rgb(16, 16, 20));
    }
    if (view.levelNumber > 0) {
        const std::string compact = "LEVEL " + std::to_string(view.levelNumber);
        DrawText(framebuffer, barX + barW - 6 - TextWidth(compact, 1), textY, compact, kGold);
    }

    // Message line above the bar.
    if (!view.message.empty()) {
        const std::string text = FitText(Upper(view.message), barW - 12, 1);
        const int boxW = TextWidth(text, 1) + 12;
        const int boxH = kLineHeight + 6;
        const int boxX = (width - boxW) / 2;
        const int boxY = barY - 4 - boxH;
        DrawPanel(framebuffer, boxX, boxY, boxW, boxH);
        DrawText(framebuffer, boxX + 6, boxY + 3, text, kMessage);
    }

    // Level banner at level start, right of the minimap when it is shown.
    if (view.showLevelBanner && view.levelNumber > 0) {
        const int left = std::max(kMargin, view.reservedLeft + 4);
        const int right = width - kMargin;
        const int areaW = right - left;
        const std::string title = "LEVEL " + std::to_string(view.levelNumber);
        const std::string name = FitText(Upper(view.levelName), areaW - 16, 1);
        const int boxW = std::min(areaW, std::max(TextWidth(title, 2), TextWidth(name, 1)) + 16);
        const int boxH = 2 * kLineHeight + kLineHeight + 14;
        const int boxX = left + (areaW - boxW) / 2;
        const int boxY = height / 5;
        DrawPanel(framebuffer, boxX, boxY, boxW, boxH);
        DrawTextCentered(framebuffer, boxX + boxW / 2, boxY + 5, title, kGold, 2);
        DrawTextCentered(framebuffer, boxX + boxW / 2, boxY + 5 + 2 * kLineHeight + 4, name, kText);
    }
}

UiRenderer::UiRect UiRenderer::KeyCardRect(int framebufferWidth, int framebufferHeight, KeyColor color) {
    (void)framebufferWidth;  // the bar starts at the left margin at every width
    const int barY = framebufferHeight - kMargin - UiLayout::kHudBarHeight;
    const int ammoX = kMargin + 6 + 80;  // as in DrawHud
    const int slot = std::max(0, KeyColorIndex(color));
    UiRect rect{};
    rect.x = ammoX + UiLayout::kKeyCardsOffset + slot * UiLayout::kKeyCardSlotWidth;
    rect.y = barY + (UiLayout::kHudBarHeight - UiLayout::kKeyCardHeight) / 2;
    rect.width = UiLayout::kKeyCardWidth;
    rect.height = UiLayout::kKeyCardHeight;
    return rect;
}

std::uint32_t UiRenderer::KeyCardColor(KeyColor color) {
    switch (color) {
        case KeyColor::Red:
            return Rgb(235, 60, 50);
        case KeyColor::Blue:
            return Rgb(70, 130, 255);
        case KeyColor::Gold:
            return Rgb(250, 200, 40);
        case KeyColor::None:
            break;
    }
    return Rgb(128, 128, 128);
}

void UiRenderer::DrawCrosshair(Framebuffer& framebuffer) {
    const int cx = framebuffer.Width() / 2;
    const int cy = framebuffer.Height() / 2;
    const std::uint32_t crosshair = Rgb(248, 240, 220);
    framebuffer.DrawRect(cx - 8, cy, 6, 1, crosshair);
    framebuffer.DrawRect(cx + 3, cy, 6, 1, crosshair);
    framebuffer.DrawRect(cx, cy - 8, 1, 6, crosshair);
    framebuffer.DrawRect(cx, cy + 3, 1, 6, crosshair);
}

void UiRenderer::DrawDebugOverlay(Framebuffer& framebuffer, float fps, float playerX, float playerY, float angleDegrees,
                                  int cellX, int cellY) {
    char lines[4][48] = {};
    std::snprintf(lines[0], sizeof(lines[0]), "FPS %.1f", static_cast<double>(fps));
    std::snprintf(lines[1], sizeof(lines[1]), "POS %.2f, %.2f", static_cast<double>(playerX),
                  static_cast<double>(playerY));
    std::snprintf(lines[2], sizeof(lines[2]), "ANG %.1f", static_cast<double>(angleDegrees));
    std::snprintf(lines[3], sizeof(lines[3]), "CELL %d, %d", cellX, cellY);
    int textW = 0;
    for (const auto& line : lines) {
        textW = std::max(textW, TextWidth(line, 1));
    }
    const int boxW = textW + 10;
    const int boxH = 4 * (kLineHeight + 3) + 7;
    const int boxX = framebuffer.Width() - kMargin - boxW;
    const int boxY = kMargin;
    DrawPanel(framebuffer, boxX, boxY, boxW, boxH);
    for (int i = 0; i < 4; ++i) {
        DrawText(framebuffer, boxX + 5, boxY + 5 + i * (kLineHeight + 3), lines[i], Rgb(196, 231, 255));
    }
}

void UiRenderer::DrawMenuScreen(Framebuffer& framebuffer, const MenuView& view) {
    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    const int cx = width / 2;
    framebuffer.Clear(Rgb(12, 14, 22));

    const int titleY = height / 15;
    DrawTextCentered(framebuffer, cx, titleY, "NEO-WOLF", kTitleRed, 3);
    const int subtitleY = titleY + 3 * kLineHeight + 6;
    DrawTextCentered(framebuffer, cx, subtitleY, FitText(Upper(view.campaignTitle), width - 2 * kMargin - 8, 1), kGold);

    // Items at scale 2. The selected one has a highlight bar and > < markers (not colour only).
    const int rowH = 2 * kLineHeight + 8;
    const int itemsY = subtitleY + kLineHeight + 16;
    const int barW = std::min(width - 2 * kMargin, 200);
    const int markerSpace = 6 + TextWidth(">", 2) + 6;  // marker plus padding on each side
    for (std::size_t i = 0; i < view.items.size(); ++i) {
        const int rowY = itemsY + static_cast<int>(i) * rowH;
        const bool selected = static_cast<int>(i) == view.selected;
        const std::string label = FitText(view.items[i], barW - 2 * markerSpace, 2);
        if (selected) {
            DrawPanel(framebuffer, cx - barW / 2, rowY - 3, barW, 2 * kLineHeight + 6, kHighlight, kGold);
            DrawText(framebuffer, cx - barW / 2 + 6, rowY, ">", kGold, 2);
            DrawText(framebuffer, cx + barW / 2 - 6 - TextWidth("<", 2), rowY, "<", kGold, 2);
        }
        DrawTextCentered(framebuffer, cx, rowY, label, selected ? kValue : kText, 2);
    }

    // Controls summary and key hints at the bottom.
    const int footerY = height - kMargin - kLineHeight - 6;
    const int controlsY = footerY - 4 * (kLineHeight + 4);
    const char* controls[] = {"MOVE: W/S   STRAFE: A/D   TURN: ARROWS, Q",
                              "FIRE: SPACE, CTRL, F   USE: E   MAP: M, TAB", "PAUSE: P, ESC   RESTART LEVEL: R"};
    for (int i = 0; i < 3; ++i) {
        DrawTextCentered(framebuffer, cx, controlsY + i * (kLineHeight + 4),
                         FitText(controls[i], width - 2 * kMargin, 1), kTextDim);
    }
    DrawTextCentered(framebuffer, cx, footerY,
                     FitText("UP/DOWN: CHOOSE   ENTER/SPACE: SELECT   ESC: QUIT", width - 2 * kMargin, 1), kText);
}

void UiRenderer::DrawLevelSelectScreen(Framebuffer& framebuffer, const LevelSelectView& view) {
    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    const int cx = width / 2;
    framebuffer.Clear(Rgb(12, 14, 22));

    const int titleY = height / 16;
    DrawTextCentered(framebuffer, cx, titleY, "LEVEL SELECT", kGold, 2);
    DrawTextCentered(framebuffer, cx, titleY + 2 * kLineHeight + 6,
                     FitText(Upper(view.campaignTitle), width - 2 * kMargin - 8, 1), kText);

    const int footerY = height - kMargin - kLineHeight - 6;
    const int listY = titleY + 2 * kLineHeight + 6 + kLineHeight + 12;
    const int rowH = kLineHeight + 8;
    const int visibleRows = std::max(1, (footerY - 8 - listY) / rowH);
    const int count = static_cast<int>(view.levels.size());
    const int firstRow = std::clamp(view.selected - visibleRows / 2, 0, std::max(0, count - visibleRows));

    const int barX = kMargin + 8;
    const int barW = width - 2 * barX;
    const int numberX = barX + 18;
    const int nameX = numberX + TextWidth("10", 1) + 10;
    const std::string lockedTag = "LOCKED";
    for (int row = 0; row < visibleRows && firstRow + row < count; ++row) {
        const int index = firstRow + row;
        const LevelSelectEntry& entry = view.levels[static_cast<std::size_t>(index)];
        const int rowY = listY + row * rowH;
        const bool selected = index == view.selected;
        if (selected) {
            DrawPanel(framebuffer, barX, rowY - 4, barW, kLineHeight + 7, kHighlight, kGold);
            DrawText(framebuffer, barX + 6, rowY, ">", kGold);
        }
        const std::uint32_t color = entry.unlocked ? (selected ? kValue : kText) : kTextDim;
        DrawText(framebuffer, numberX, rowY, std::to_string(entry.number), color);
        int nameRight = barX + barW - 6;
        if (!entry.unlocked) {
            // Locked levels are dimmed AND tagged, so the state does not depend on colour.
            nameRight -= TextWidth(lockedTag, 1) + 8;
            DrawText(framebuffer, barX + barW - 6 - TextWidth(lockedTag, 1), rowY, lockedTag, kTextDim);
        }
        DrawText(framebuffer, nameX, rowY, FitText(Upper(entry.name), nameRight - nameX, 1), color);
    }
    if (firstRow > 0) {
        DrawText(framebuffer, barX + barW - 6 - TextWidth("MORE", 1), listY - rowH + 2, "MORE", kTextDim);
    }
    if (firstRow + visibleRows < count) {
        DrawText(framebuffer, barX + barW - 6 - TextWidth("MORE", 1), listY + visibleRows * rowH - 2, "MORE", kTextDim);
    }

    DrawTextCentered(framebuffer, cx, footerY,
                     FitText("UP/DOWN: CHOOSE   ENTER: START   ESC: BACK", width - 2 * kMargin, 1), kText);
}

void UiRenderer::DrawPauseOverlay(Framebuffer& framebuffer, const std::string& levelLabel) {
    framebuffer.Darken(40);
    int x = 0;
    int y = 0;
    const int boxW = 210;
    DrawCenteredPanel(framebuffer, boxW, 96, kPanel, x, y);
    const int cx = x + boxW / 2;
    DrawTextCentered(framebuffer, cx, y + 9, "PAUSED", kGold, 2);
    DrawTextCentered(framebuffer, cx, y + 30, FitText(levelLabel, boxW - 16, 1), kText);
    DrawTextCentered(framebuffer, cx, y + 48, "ESC, P OR ENTER: RESUME", kValue);
    DrawTextCentered(framebuffer, cx, y + 60, "R: RESTART LEVEL", kValue);
    DrawTextCentered(framebuffer, cx, y + 72, "Q: QUIT TO MENU", kValue);
}

void UiRenderer::DrawLevelCompleteScreen(Framebuffer& framebuffer, const LevelCompleteView& view) {
    framebuffer.Darken(35);
    int x = 0;
    int y = 0;
    const int boxW = framebuffer.Width() - 2 * 24;
    const int boxH = 150;
    DrawCenteredPanel(framebuffer, boxW, boxH, Rgb(10, 30, 18), x, y);
    const int cx = x + boxW / 2;
    const std::string title = view.levelNumber > 0 ? "LEVEL " + std::to_string(view.levelNumber) + " COMPLETE"
                                                   : std::string("LEVEL COMPLETE");
    DrawTextCentered(framebuffer, cx, y + 9, FitText(title, boxW - 16, 2), kGold, 2);
    DrawTextCentered(framebuffer, cx, y + 30, FitText(Upper(view.levelName), boxW - 16, 1), kText);

    const LevelResults& r = view.results;
    const std::string rows[3][2] = {
        {"ENEMIES KILLED", std::to_string(r.kills) + " / " + std::to_string(r.totalEnemies)},
        {"PICKUPS", std::to_string(r.pickups) + " / " + std::to_string(r.totalPickups)},
        {"TIME", FormatDuration(r.seconds)},
    };
    const int labelX = x + 36;
    const int valueRight = x + boxW - 36;
    for (int i = 0; i < 3; ++i) {
        const int rowY = y + 50 + i * (kLineHeight + 6);
        DrawText(framebuffer, labelX, rowY, rows[i][0], kText);
        DrawText(framebuffer, valueRight - TextWidth(rows[i][1], 1), rowY, rows[i][1], kValue);
    }
    DrawTextCentered(framebuffer, cx, y + 100,
                     view.lastLevel ? "THAT WAS THE LAST LEVEL!" : "HEALTH AND AMMO CARRY OVER", kMessage);
    DrawTextCentered(framebuffer, cx, y + boxH - 20,
                     view.lastLevel ? "ENTER: CONTINUE   ESC: MENU" : "ENTER: NEXT LEVEL   ESC: MENU", kValue);
}

void UiRenderer::DrawGameOverScreen(Framebuffer& framebuffer, const std::string& levelLabel) {
    framebuffer.Darken(35);
    int x = 0;
    int y = 0;
    const int boxW = 240;
    DrawCenteredPanel(framebuffer, boxW, 108, Rgb(34, 8, 8), x, y);
    const int cx = x + boxW / 2;
    DrawTextCentered(framebuffer, cx, y + 9, "GAME OVER", kWarn, 3);
    DrawTextCentered(framebuffer, cx, y + 38, FitText(levelLabel, boxW - 16, 1), kText);
    DrawTextCentered(framebuffer, cx, y + 56, "R OR ENTER: RESTART LEVEL", kValue);
    DrawTextCentered(framebuffer, cx, y + 68, "ESC: MAIN MENU", kValue);
    DrawTextCentered(framebuffer, cx, y + 88, FitText("HEALTH AND AMMO AS AT LEVEL START", boxW - 16, 1), kTextDim);
}

void UiRenderer::DrawCampaignCompleteScreen(Framebuffer& framebuffer, const std::string& campaignTitle,
                                            int levelCount) {
    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    const int cx = width / 2;
    const int textW = width - 2 * kMargin - 8;
    framebuffer.Clear(Rgb(8, 22, 14));

    int y = height / 12;
    DrawTextCentered(framebuffer, cx, y, "CONGRATULATIONS!", kGold, 2);
    y += 2 * kLineHeight + 10;
    DrawTextCentered(framebuffer, cx, y, "CAMPAIGN COMPLETE", kValue, 2);
    y += 2 * kLineHeight + 12;
    DrawTextCentered(framebuffer, cx, y, FitText(Upper(campaignTitle), textW, 1), kText);
    y += kLineHeight + 5;
    DrawTextCentered(framebuffer, cx, y, "ALL " + std::to_string(levelCount) + " LEVELS CLEARED", kText);

    y += kLineHeight + 18;
    const char* credits[] = {"CREDITS",
                             "NEO-WOLF: A SOFTWARE RAYCASTER",
                             "ENGINE: C++17 AND SDL2",
                             "JSON: NLOHMANN/JSON (MIT)",
                             "TEXTURES: TOOLS/DEVKIT/GEN_TEXTURES.PY",
                             "THANKS FOR PLAYING!"};
    for (int i = 0; i < 6; ++i) {
        DrawTextCentered(framebuffer, cx, y, FitText(credits[i], textW, 1), i == 0 ? kGold : kTextDim);
        y += kLineHeight + 5;
    }

    DrawTextCentered(framebuffer, cx, height - kMargin - kLineHeight - 6, "ENTER: MAIN MENU", kValue);
}

UiRenderer::MinimapLayout UiRenderer::ComputeMinimapLayout(int framebufferWidth, int framebufferHeight, int mapWidth,
                                                           int mapHeight, float playerX, float playerY) {
    MinimapLayout layout{};
    layout.panelSize = std::max(16, std::min(framebufferWidth, framebufferHeight) / 3);
    layout.panelX = kMargin;
    layout.panelY = kMargin;
    const int inner = layout.panelSize - 4;  // 2-pixel frame
    if (mapWidth <= 0 || mapHeight <= 0) {
        layout.cellSize = kMinimapMinCell;
        return layout;
    }
    const int fitCell = std::min(inner / mapWidth, inner / mapHeight);
    if (fitCell >= kMinimapMinCell) {
        layout.cellSize = std::min(fitCell, kMinimapMaxCell);
        layout.originX = layout.panelX + (layout.panelSize - mapWidth * layout.cellSize) / 2;
        layout.originY = layout.panelY + (layout.panelSize - mapHeight * layout.cellSize) / 2;
        layout.followsPlayer = false;
    } else {
        layout.cellSize = kMinimapMinCell;
        layout.originX = layout.panelX + layout.panelSize / 2 -
                         static_cast<int>(std::lround(playerX * static_cast<float>(layout.cellSize)));
        layout.originY = layout.panelY + layout.panelSize / 2 -
                         static_cast<int>(std::lround(playerY * static_cast<float>(layout.cellSize)));
        layout.followsPlayer = true;
    }
    return layout;
}

int UiRenderer::DrawMinimap(Framebuffer& framebuffer, const Map& map, const Player& player,
                            const std::vector<SpriteEntity>& sprites) {
    const MinimapLayout layout = ComputeMinimapLayout(framebuffer.Width(), framebuffer.Height(), map.Width(),
                                                      map.Height(), player.position.x, player.position.y);
    const int cell = layout.cellSize;
    DrawPanel(framebuffer, layout.panelX, layout.panelY, layout.panelSize, layout.panelSize, Rgb(10, 12, 16));
    // Everything inside the frame is clipped to it.
    const int clipX = layout.panelX + 2;
    const int clipY = layout.panelY + 2;
    const int clipSize = layout.panelSize - 4;
    const auto fill = [&](int x, int y, int w, int h, std::uint32_t color) {
        FillClipped(framebuffer, x, y, w, h, clipX, clipY, clipSize, clipSize, color);
    };

    const int firstX = std::max(0, (clipX - layout.originX) / cell);
    const int lastX = std::min(map.Width() - 1, (clipX + clipSize - layout.originX) / cell);
    const int firstY = std::max(0, (clipY - layout.originY) / cell);
    const int lastY = std::min(map.Height() - 1, (clipY + clipSize - layout.originY) / cell);
    const int gap = cell >= 5 ? 1 : 0;  // a grid line between cells once they are big enough
    std::vector<std::pair<int, int>> exits;
    std::vector<std::pair<int, int>> lockedDoors;
    for (int y = firstY; y <= lastY; ++y) {
        for (int x = firstX; x <= lastX; ++x) {
            std::uint32_t color = 0;
            switch (map.GetCell(x, y)) {
                case TileType::Wall:
                    color = Rgb(136, 136, 136);
                    break;
                case TileType::WallAltA:
                    color = Rgb(170, 102, 102);
                    break;
                case TileType::WallAltB:
                    color = Rgb(102, 102, 170);
                    break;
                case TileType::Door:
                    if (map.GetDoorLock(x, y) != KeyColor::None) {
                        lockedDoors.emplace_back(x, y);
                        continue;  // drawn on top below
                    }
                    color = Rgb(204, 136, 51);
                    break;
                case TileType::Trigger:
                    color = Rgb(40, 80, 50);
                    break;
                case TileType::Exit:
                    continue;  // drawn on top below
                case TileType::Empty:
                    continue;
            }
            fill(layout.originX + x * cell, layout.originY + y * cell, cell - gap, cell - gap, color);
        }
    }

    // Exit switches: a green cell under a white "X" cross of at least 7x7 pixels.
    const auto drawExitMarker = [&](int centerX, int centerY) {
        const int arm = std::max(3, cell / 2 + 1);
        fill(centerX - cell / 2, centerY - cell / 2, cell, cell, Rgb(40, 200, 80));
        for (int d = -arm; d <= arm; ++d) {
            fill(centerX + d, centerY + d, 1, 1, Rgb(255, 255, 255));
            fill(centerX + d, centerY - d, 1, 1, Rgb(255, 255, 255));
        }
    };
    // Every exit of the map: an exit outside the panel (big maps) is pinned to the panel edge
    // in its direction, so the way to it stays visible.
    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            if (map.GetCell(x, y) == TileType::Exit) {
                exits.emplace_back(x, y);
            }
        }
    }
    const int markerArm = std::max(3, cell / 2 + 1);
    for (const auto& exitCell : exits) {
        const int centerX = layout.originX + exitCell.first * cell + cell / 2;
        const int centerY = layout.originY + exitCell.second * cell + cell / 2;
        drawExitMarker(std::clamp(centerX, clipX + markerArm, clipX + clipSize - 1 - markerArm),
                       std::clamp(centerY, clipY + markerArm, clipY + clipSize - 1 - markerArm));
    }

    // Locked doors: the lock's colour crossed by a dark bar (not by colour alone).
    for (const auto& doorCell : lockedDoors) {
        const int x0 = layout.originX + doorCell.first * cell;
        const int y0 = layout.originY + doorCell.second * cell;
        fill(x0, y0, cell - gap, cell - gap, KeyCardColor(map.GetDoorLock(doorCell.first, doorCell.second)));
        fill(x0, y0 + (cell - gap) / 2, cell - gap, 1, Rgb(16, 16, 20));
    }

    for (const SpriteEntity& sprite : sprites) {
        if (!sprite.active || !sprite.visible || sprite.type == SpriteType::Corpse) {
            continue;
        }
        const int sx = layout.originX + static_cast<int>(sprite.position.x * static_cast<float>(cell));
        const int sy = layout.originY + static_cast<int>(sprite.position.y * static_cast<float>(cell));
        if (sprite.type == SpriteType::Key) {
            // A key: a 3x3 ring with a shaft and a tooth, in the key's colour.
            const KeyColor keyColor = sprite.textureId == static_cast<int>(SpriteTextureId::KeyBlue)   ? KeyColor::Blue
                                      : sprite.textureId == static_cast<int>(SpriteTextureId::KeyGold) ? KeyColor::Gold
                                                                                                       : KeyColor::Red;
            const std::uint32_t keyRgb = KeyCardColor(keyColor);
            fill(sx - 3, sy - 1, 3, 1, keyRgb);
            fill(sx - 3, sy + 1, 3, 1, keyRgb);
            fill(sx - 3, sy, 1, 1, keyRgb);
            fill(sx - 1, sy, 4, 1, keyRgb);
            fill(sx + 2, sy + 1, 1, 1, keyRgb);
            continue;
        }
        std::uint32_t color = Rgb(80, 220, 80);  // pickup
        if (sprite.type == SpriteType::Enemy) {
            color = Rgb(255, 60, 60);
        } else if (sprite.type == SpriteType::Decoration) {
            color = Rgb(150, 150, 150);
        }
        fill(sx - 1, sy - 1, 3, 3, color);
    }

    // Player: a 3x3 dot and a short tick in the view direction.
    const int px = layout.originX + static_cast<int>(player.position.x * static_cast<float>(cell));
    const int py = layout.originY + static_cast<int>(player.position.y * static_cast<float>(cell));
    const std::uint32_t playerColor = Rgb(255, 255, 0);
    fill(px - 1, py - 1, 3, 3, playerColor);
    const float dirX = std::cos(player.directionAngleRadians);
    const float dirY = std::sin(player.directionAngleRadians);
    for (int step = 2; step <= 4; ++step) {
        fill(px + static_cast<int>(std::lround(dirX * static_cast<float>(step))),
             py + static_cast<int>(std::lround(dirY * static_cast<float>(step))), 1, 1, playerColor);
    }

    // Legend under a fitted map when there is room: the marker shape and the word EXIT.
    const int mapBottom = layout.originY + map.Height() * cell;
    if (!layout.followsPlayer && !exits.empty() && clipY + clipSize - mapBottom >= kLineHeight + 5) {
        const int legendY = clipY + clipSize - kLineHeight - 2;
        const int legendX = clipX + (clipSize - (7 + 4 + TextWidth("EXIT", 1))) / 2;
        drawExitMarker(legendX + 3, legendY + 3);
        DrawText(framebuffer, legendX + 11, legendY, "EXIT", kValue);
    }
    return layout.panelX + layout.panelSize;
}
