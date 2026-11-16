#include "engine/ui/UiRenderer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <optional>
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
        // Bit 4 of a glyph row is the leftmost column.
        const unsigned rowBits = glyph[static_cast<std::size_t>(row)];
        for (int col = 0; col < UiLayout::kGlyphWidth; ++col) {
            const unsigned columnBit = 1u << static_cast<unsigned>(UiLayout::kGlyphWidth - 1 - col);
            if ((rowBits & columnBit) == 0u) {
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
constexpr std::uint32_t kDarkInk = Rgb(16, 16, 20);  // letters on key cards, bars on locked doors
constexpr std::uint32_t kKeyCardBorder = Rgb(20, 20, 24);
constexpr std::uint32_t kKeyRed = Rgb(235, 60, 50);
constexpr std::uint32_t kKeyBlue = Rgb(70, 130, 255);
constexpr std::uint32_t kKeyGold = Rgb(250, 200, 40);
constexpr std::uint32_t kKeyUnknown = Rgb(128, 128, 128);
constexpr std::uint32_t kCrosshair = Rgb(248, 240, 220);
constexpr std::uint32_t kDebugText = Rgb(196, 231, 255);
// Full-screen backgrounds and the fills of the result panels.
constexpr std::uint32_t kMenuBackground = Rgb(12, 14, 22);
constexpr std::uint32_t kCampaignCompleteBackground = Rgb(8, 22, 14);
constexpr std::uint32_t kLevelCompleteFill = Rgb(10, 30, 18);
constexpr std::uint32_t kGameOverFill = Rgb(34, 8, 8);
// Minimap: panel, cells by tile, exit marker, sprites by type, player.
constexpr std::uint32_t kMinimapPanel = Rgb(10, 12, 16);
constexpr std::uint32_t kMinimapWall = Rgb(136, 136, 136);
constexpr std::uint32_t kMinimapWallAltA = Rgb(170, 102, 102);
constexpr std::uint32_t kMinimapWallAltB = Rgb(102, 102, 170);
constexpr std::uint32_t kMinimapDoor = Rgb(204, 136, 51);
constexpr std::uint32_t kMinimapTrigger = Rgb(40, 80, 50);
constexpr std::uint32_t kMinimapExit = Rgb(40, 200, 80);
constexpr std::uint32_t kMinimapExitCross = Rgb(255, 255, 255);
constexpr std::uint32_t kMinimapPickup = Rgb(80, 220, 80);
constexpr std::uint32_t kMinimapEnemy = Rgb(255, 60, 60);
constexpr std::uint32_t kMinimapDecoration = Rgb(150, 150, 150);
constexpr std::uint32_t kMinimapPlayer = Rgb(255, 255, 0);

using UiLayout::kMargin;
using UiLayout::kTextInset;
constexpr int kLineHeight = UiLayout::kGlyphHeight;  // at scale 1
// The overlay panels (pause, level complete, game over): title row from the panel top, and
// the padding on each side of fitted text.
constexpr int kOverlayTitleY = 9;
constexpr int kOverlayTextInset = 8;
// How much of the world's brightness stays visible behind the overlays (Framebuffer::Darken).
constexpr int kPauseDimPercent = 40;
constexpr int kResultDimPercent = 35;
// Health at or below this is shown in the warning colour.
constexpr int kLowHealth = 25;
// The minimap's frame: a 1-pixel border and a 1-pixel gap; the map is drawn inside it.
constexpr int kMinimapFrame = 2;
// Cells at least this big get a 1-pixel grid line between them.
constexpr int kMinimapGridMinCell = 5;
// The exit marker's "X" has arms of at least this many pixels (at least 7x7 pixels in all).
constexpr int kExitMarkerMinArm = 3;

// A filled panel with a 1-pixel border.
void DrawPanel(Framebuffer& framebuffer, int x, int y, int width, int height, std::uint32_t fill = kPanel,
               std::uint32_t border = kPanelBorder) {
    framebuffer.DrawRect(x, y, width, height, border);
    framebuffer.DrawRect(x + 1, y + 1, width - 2, height - 2, fill);
}

struct Point {
    int x = 0;
    int y = 0;
};

// A centred panel, shrunk to fit inside the margins; returns its top-left corner.
Point DrawCenteredPanel(Framebuffer& framebuffer, int width, int height, std::uint32_t fill) {
    width = std::min(width, framebuffer.Width() - 2 * kMargin);
    height = std::min(height, framebuffer.Height() - 2 * kMargin);
    const Point topLeft{(framebuffer.Width() - width) / 2, (framebuffer.Height() - height) / 2};
    DrawPanel(framebuffer, topLeft.x, topLeft.y, width, height, fill);
    return topLeft;
}

std::string Upper(const std::string& text) {
    std::string upper = text;
    for (char& c : upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return upper;
}

// "LEVEL 3".
std::string LevelNumberText(int levelNumber) {
    return "LEVEL " + std::to_string(levelNumber);
}

// The y of the key-hint line at the bottom of the full-screen pages.
int FooterY(int framebufferHeight) {
    return framebufferHeight - kMargin - kLineHeight - 6;
}

// Scale-1 text that ends at `right` (exclusive of spacing).
void DrawTextRightAligned(Framebuffer& framebuffer, int right, int y, const std::string& text, std::uint32_t color) {
    UiRenderer::DrawText(framebuffer, right - UiRenderer::TextWidth(text, 1), y, text, color);
}

// The minimap panel's inner square: everything drawn on the map is clipped to it.
struct MinimapCanvas {
    Framebuffer* framebuffer = nullptr;  // borrowed
    int clipX = 0;
    int clipY = 0;
    int clipSize = 0;

    // Fills the part of the rectangle that lies inside the clip square.
    void Fill(int x, int y, int width, int height, std::uint32_t color) const {
        const int x0 = std::max(x, clipX);
        const int y0 = std::max(y, clipY);
        const int x1 = std::min(x + width, clipX + clipSize);
        const int y1 = std::min(y + height, clipY + clipSize);
        if (x1 > x0 && y1 > y0) {
            framebuffer->DrawRect(x0, y0, x1 - x0, y1 - y0, color);
        }
    }
};

// The range of map cells that can show inside the clip square.
struct CellRange {
    int firstX = 0;
    int lastX = -1;
    int firstY = 0;
    int lastY = -1;
};

CellRange VisibleCells(const MinimapCanvas& canvas, const UiRenderer::MinimapLayout& layout, const Map& map) {
    const int cell = layout.cellSize;
    return {std::max(0, (canvas.clipX - layout.originX) / cell),
            std::min(map.Width() - 1, (canvas.clipX + canvas.clipSize - layout.originX) / cell),
            std::max(0, (canvas.clipY - layout.originY) / cell),
            std::min(map.Height() - 1, (canvas.clipY + canvas.clipSize - layout.originY) / cell)};
}

bool IsLockedDoor(const Map& map, int x, int y) {
    return map.GetCell(x, y) == TileType::Door && map.GetDoorLock(x, y) != KeyColor::None;
}

// The colour of a plain cell; empty for cells that are not drawn or drawn on top later
// (exits, locked doors).
std::optional<std::uint32_t> MinimapCellColor(const Map& map, int x, int y) {
    switch (map.GetCell(x, y)) {
        case TileType::Wall:
            return kMinimapWall;
        case TileType::WallAltA:
            return kMinimapWallAltA;
        case TileType::WallAltB:
            return kMinimapWallAltB;
        case TileType::Door:
            if (IsLockedDoor(map, x, y)) {
                return std::nullopt;
            }
            return kMinimapDoor;
        case TileType::Trigger:
            return kMinimapTrigger;
        case TileType::Exit:
        case TileType::Empty:
            return std::nullopt;
    }
    return std::nullopt;
}

void DrawMinimapCells(const MinimapCanvas& canvas, const UiRenderer::MinimapLayout& layout, const Map& map,
                      const CellRange& cells, int gap) {
    const int cell = layout.cellSize;
    for (int y = cells.firstY; y <= cells.lastY; ++y) {
        for (int x = cells.firstX; x <= cells.lastX; ++x) {
            if (const std::optional<std::uint32_t> color = MinimapCellColor(map, x, y)) {
                canvas.Fill(layout.originX + x * cell, layout.originY + y * cell, cell - gap, cell - gap, *color);
            }
        }
    }
}

int ExitMarkerArm(int cell) {
    return std::max(kExitMarkerMinArm, cell / 2 + 1);
}

// An exit switch: a green cell under a white "X" cross.
void DrawExitMarker(const MinimapCanvas& canvas, int cell, int centerX, int centerY) {
    const int arm = ExitMarkerArm(cell);
    canvas.Fill(centerX - cell / 2, centerY - cell / 2, cell, cell, kMinimapExit);
    for (int d = -arm; d <= arm; ++d) {
        canvas.Fill(centerX + d, centerY + d, 1, 1, kMinimapExitCross);
        canvas.Fill(centerX + d, centerY - d, 1, 1, kMinimapExitCross);
    }
}

// Every exit of the map: an exit outside the panel (big maps) is pinned to the panel edge in
// its direction, so the way to it stays visible. Returns whether the map has an exit.
bool DrawMinimapExits(const MinimapCanvas& canvas, const UiRenderer::MinimapLayout& layout, const Map& map) {
    const int cell = layout.cellSize;
    const int arm = ExitMarkerArm(cell);
    bool anyExit = false;
    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            if (map.GetCell(x, y) != TileType::Exit) {
                continue;
            }
            anyExit = true;
            const int centerX = layout.originX + x * cell + cell / 2;
            const int centerY = layout.originY + y * cell + cell / 2;
            DrawExitMarker(canvas, cell,
                           std::clamp(centerX, canvas.clipX + arm, canvas.clipX + canvas.clipSize - 1 - arm),
                           std::clamp(centerY, canvas.clipY + arm, canvas.clipY + canvas.clipSize - 1 - arm));
        }
    }
    return anyExit;
}

// Locked doors: the lock's colour crossed by a dark bar (not by colour alone).
void DrawMinimapLockedDoors(const MinimapCanvas& canvas, const UiRenderer::MinimapLayout& layout, const Map& map,
                            const CellRange& cells, int gap) {
    const int cell = layout.cellSize;
    for (int y = cells.firstY; y <= cells.lastY; ++y) {
        for (int x = cells.firstX; x <= cells.lastX; ++x) {
            if (!IsLockedDoor(map, x, y)) {
                continue;
            }
            const int x0 = layout.originX + x * cell;
            const int y0 = layout.originY + y * cell;
            canvas.Fill(x0, y0, cell - gap, cell - gap, UiRenderer::KeyCardColor(map.GetDoorLock(x, y)));
            canvas.Fill(x0, y0 + (cell - gap) / 2, cell - gap, 1, kDarkInk);
        }
    }
}

KeyColor KeyColorOfSprite(const SpriteEntity& sprite) {
    if (sprite.textureId == static_cast<int>(SpriteTextureId::KeyBlue)) {
        return KeyColor::Blue;
    }
    if (sprite.textureId == static_cast<int>(SpriteTextureId::KeyGold)) {
        return KeyColor::Gold;
    }
    return KeyColor::Red;
}

// A key: a 3x3 ring with a shaft and a tooth, in the key's colour, ring centred left of (x, y).
void DrawMinimapKey(const MinimapCanvas& canvas, int x, int y, std::uint32_t color) {
    canvas.Fill(x - 3, y - 1, 3, 1, color);
    canvas.Fill(x - 3, y + 1, 3, 1, color);
    canvas.Fill(x - 3, y, 1, 1, color);
    canvas.Fill(x - 1, y, 4, 1, color);
    canvas.Fill(x + 2, y + 1, 1, 1, color);
}

std::uint32_t MinimapSpriteColor(SpriteType type) {
    if (type == SpriteType::Enemy) {
        return kMinimapEnemy;
    }
    if (type == SpriteType::Decoration) {
        return kMinimapDecoration;
    }
    return kMinimapPickup;
}

// Living sprites: keys as a key shape, everything else as a 3x3 dot coloured by type.
void DrawMinimapSprites(const MinimapCanvas& canvas, const UiRenderer::MinimapLayout& layout,
                        const std::vector<SpriteEntity>& sprites) {
    const auto cell = static_cast<float>(layout.cellSize);
    for (const SpriteEntity& sprite : sprites) {
        if (!sprite.active || !sprite.visible || sprite.type == SpriteType::Corpse) {
            continue;
        }
        const int sx = layout.originX + static_cast<int>(sprite.position.x * cell);
        const int sy = layout.originY + static_cast<int>(sprite.position.y * cell);
        if (sprite.type == SpriteType::Key) {
            DrawMinimapKey(canvas, sx, sy, UiRenderer::KeyCardColor(KeyColorOfSprite(sprite)));
        } else {
            canvas.Fill(sx - 1, sy - 1, 3, 3, MinimapSpriteColor(sprite.type));
        }
    }
}

// The player: a 3x3 dot and a short tick (2 to 4 pixels out) in the view direction.
void DrawMinimapPlayer(const MinimapCanvas& canvas, const UiRenderer::MinimapLayout& layout, const Player& player) {
    const auto cell = static_cast<float>(layout.cellSize);
    const int px = layout.originX + static_cast<int>(player.position.x * cell);
    const int py = layout.originY + static_cast<int>(player.position.y * cell);
    canvas.Fill(px - 1, py - 1, 3, 3, kMinimapPlayer);
    const float dirX = std::cos(player.directionAngleRadians);
    const float dirY = std::sin(player.directionAngleRadians);
    for (int step = 2; step <= 4; ++step) {
        canvas.Fill(px + static_cast<int>(std::lround(dirX * static_cast<float>(step))),
                    py + static_cast<int>(std::lround(dirY * static_cast<float>(step))), 1, 1, kMinimapPlayer);
    }
}

// Legend under a fitted map when there is room: an exit marker and the word EXIT, laid out
// for the marker's minimum size.
void DrawMinimapLegend(const MinimapCanvas& canvas, const UiRenderer::MinimapLayout& layout, const Map& map) {
    const int mapBottom = layout.originY + map.Height() * layout.cellSize;
    if (canvas.clipY + canvas.clipSize - mapBottom < kLineHeight + 5) {
        return;
    }
    constexpr int kMarkerSize = 2 * kExitMarkerMinArm + 1;
    constexpr int kGap = 4;
    const int legendY = canvas.clipY + canvas.clipSize - kLineHeight - 2;
    const int legendX = canvas.clipX + (canvas.clipSize - (kMarkerSize + kGap + UiRenderer::TextWidth("EXIT", 1))) / 2;
    DrawExitMarker(canvas, layout.cellSize, legendX + kExitMarkerMinArm, legendY + kExitMarkerMinArm);
    UiRenderer::DrawText(*canvas.framebuffer, legendX + kMarkerSize + kGap, legendY, "EXIT", kValue);
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
    constexpr long kSecondsPerMinute = 60;
    constexpr long kSecondsPerHour = 3600;
    const auto total = static_cast<long>(std::max(0.0, std::floor(seconds)));
    std::array<char, 32> buffer{};
    if (total >= kSecondsPerHour) {
        std::snprintf(buffer.data(), buffer.size(), "%ld:%02ld:%02ld", total / kSecondsPerHour,
                      (total / kSecondsPerMinute) % kSecondsPerMinute, total % kSecondsPerMinute);
    } else {
        std::snprintf(buffer.data(), buffer.size(), "%ld:%02ld", total / kSecondsPerMinute, total % kSecondsPerMinute);
    }
    return buffer.data();
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
    DrawText(framebuffer, UiLayout::kHudHealthX, textY, "HEALTH", kText);
    DrawText(framebuffer, UiLayout::kHudHealthX + TextWidth("HEALTH ", 1), textY, std::to_string(view.health),
             view.health <= kLowHealth ? kWarn : kValue);
    DrawText(framebuffer, UiLayout::kHudAmmoX, textY, "AMMO", kText);
    DrawText(framebuffer, UiLayout::kHudAmmoX + TextWidth("AMMO ", 1), textY, std::to_string(view.ammo),
             view.ammo == 0 ? kWarn : kValue);
    for (const KeyColor color : kKeyColors) {
        if (!view.keys.Has(color)) {
            continue;
        }
        const UiRect card = KeyCardRect(width, height, color);
        DrawPanel(framebuffer, card.x, card.y, card.width, card.height, KeyCardColor(color), kKeyCardBorder);
        DrawText(framebuffer, card.x + (card.width - UiLayout::kGlyphWidth) / 2,
                 card.y + (card.height - UiLayout::kGlyphHeight) / 2, std::string(1, KeyColorLabel(color)[0]),
                 kDarkInk);
    }
    if (view.levelNumber > 0) {
        DrawTextRightAligned(framebuffer, barX + barW - kTextInset, textY, LevelNumberText(view.levelNumber), kGold);
    }

    // Message line above the bar.
    if (!view.message.empty()) {
        const std::string text = FitText(Upper(view.message), barW - 2 * kTextInset, 1);
        const int boxW = TextWidth(text, 1) + 2 * kTextInset;
        const int boxH = kLineHeight + 6;
        const int boxX = (width - boxW) / 2;
        const int boxY = barY - 4 - boxH;
        DrawPanel(framebuffer, boxX, boxY, boxW, boxH);
        DrawText(framebuffer, boxX + kTextInset, boxY + 3, text, kMessage);
    }

    // Level banner at level start, right of the minimap when it is shown.
    if (view.showLevelBanner && view.levelNumber > 0) {
        const int left = std::max(kMargin, view.reservedLeft + 4);
        const int right = width - kMargin;
        const int areaW = right - left;
        const std::string title = LevelNumberText(view.levelNumber);
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
    const int slot = std::max(0, KeyColorIndex(color));
    UiRect rect{};
    rect.x = UiLayout::kHudAmmoX + UiLayout::kKeyCardsOffset + slot * UiLayout::kKeyCardSlotWidth;
    rect.y = barY + (UiLayout::kHudBarHeight - UiLayout::kKeyCardHeight) / 2;
    rect.width = UiLayout::kKeyCardWidth;
    rect.height = UiLayout::kKeyCardHeight;
    return rect;
}

std::uint32_t UiRenderer::KeyCardColor(KeyColor color) {
    switch (color) {
        case KeyColor::Red:
            return kKeyRed;
        case KeyColor::Blue:
            return kKeyBlue;
        case KeyColor::Gold:
            return kKeyGold;
        case KeyColor::None:
            break;
    }
    return kKeyUnknown;
}

void UiRenderer::DrawCrosshair(Framebuffer& framebuffer) {
    // Four 6-pixel arms with a 2-pixel gap around the centre pixel.
    const int cx = framebuffer.Width() / 2;
    const int cy = framebuffer.Height() / 2;
    framebuffer.DrawRect(cx - 8, cy, 6, 1, kCrosshair);
    framebuffer.DrawRect(cx + 3, cy, 6, 1, kCrosshair);
    framebuffer.DrawRect(cx, cy - 8, 1, 6, kCrosshair);
    framebuffer.DrawRect(cx, cy + 3, 1, 6, kCrosshair);
}

void UiRenderer::DrawDebugOverlay(Framebuffer& framebuffer, float fps, float playerX, float playerY, float angleDegrees,
                                  int cellX, int cellY) {
    std::array<std::array<char, 48>, 4> lines{};
    std::snprintf(lines[0].data(), lines[0].size(), "FPS %.1f", static_cast<double>(fps));
    std::snprintf(lines[1].data(), lines[1].size(), "POS %.2f, %.2f", static_cast<double>(playerX),
                  static_cast<double>(playerY));
    std::snprintf(lines[2].data(), lines[2].size(), "ANG %.1f", static_cast<double>(angleDegrees));
    std::snprintf(lines[3].data(), lines[3].size(), "CELL %d, %d", cellX, cellY);
    constexpr int kPadding = 5;
    constexpr int kRowH = kLineHeight + 3;
    int textW = 0;
    for (const auto& line : lines) {
        textW = std::max(textW, TextWidth(line.data(), 1));
    }
    const int boxW = textW + 2 * kPadding;
    const int boxH = static_cast<int>(lines.size()) * kRowH + 7;
    const int boxX = framebuffer.Width() - kMargin - boxW;
    const int boxY = kMargin;
    DrawPanel(framebuffer, boxX, boxY, boxW, boxH);
    for (std::size_t i = 0; i < lines.size(); ++i) {
        DrawText(framebuffer, boxX + kPadding, boxY + kPadding + static_cast<int>(i) * kRowH, lines[i].data(),
                 kDebugText);
    }
}

void UiRenderer::DrawMenuScreen(Framebuffer& framebuffer, const MenuView& view) {
    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    const int cx = width / 2;
    framebuffer.Clear(kMenuBackground);

    const int titleY = height / 15;
    DrawTextCentered(framebuffer, cx, titleY, "NEO-WOLF", kTitleRed, 3);
    const int subtitleY = titleY + 3 * kLineHeight + 6;
    DrawTextCentered(framebuffer, cx, subtitleY, FitText(Upper(view.campaignTitle), width - 2 * kMargin - 8, 1), kGold);

    // Items at scale 2. The selected one has a highlight bar and > < markers (not colour only).
    const int rowH = 2 * kLineHeight + 8;
    const int itemsY = subtitleY + kLineHeight + 16;
    const int barW = std::min(width - 2 * kMargin, 200);
    const int markerSpace = kTextInset + TextWidth(">", 2) + kTextInset;  // marker plus padding on each side
    for (std::size_t i = 0; i < view.items.size(); ++i) {
        const int rowY = itemsY + static_cast<int>(i) * rowH;
        const bool selected = static_cast<int>(i) == view.selected;
        const std::string label = FitText(view.items[i], barW - 2 * markerSpace, 2);
        if (selected) {
            DrawPanel(framebuffer, cx - barW / 2, rowY - 3, barW, 2 * kLineHeight + 6, kHighlight, kGold);
            DrawText(framebuffer, cx - barW / 2 + kTextInset, rowY, ">", kGold, 2);
            DrawText(framebuffer, cx + barW / 2 - kTextInset - TextWidth("<", 2), rowY, "<", kGold, 2);
        }
        DrawTextCentered(framebuffer, cx, rowY, label, selected ? kValue : kText, 2);
    }

    // Controls summary and key hints at the bottom.
    const int footerY = FooterY(height);
    const std::array<const char*, 3> controls = {"MOVE: W/S   STRAFE: A/D   TURN: ARROWS, Q",
                                                 "FIRE: SPACE, CTRL, F   USE: E   MAP: M, TAB",
                                                 "PAUSE: P, ESC   RESTART LEVEL: R"};
    const int controlsRowH = kLineHeight + 4;
    int controlsY = footerY - 4 * controlsRowH;  // one empty row above the footer
    for (const char* line : controls) {
        DrawTextCentered(framebuffer, cx, controlsY, FitText(line, width - 2 * kMargin, 1), kTextDim);
        controlsY += controlsRowH;
    }
    DrawTextCentered(framebuffer, cx, footerY,
                     FitText("UP/DOWN: CHOOSE   ENTER/SPACE: SELECT   ESC: QUIT", width - 2 * kMargin, 1), kText);
}

namespace {
std::uint32_t LevelEntryColor(const UiRenderer::LevelSelectEntry& entry, bool selected) {
    if (!entry.unlocked) {
        return kTextDim;
    }
    return selected ? kValue : kText;
}
}  // namespace

void UiRenderer::DrawLevelSelectScreen(Framebuffer& framebuffer, const LevelSelectView& view) {
    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    const int cx = width / 2;
    framebuffer.Clear(kMenuBackground);

    const int titleY = height / 16;
    DrawTextCentered(framebuffer, cx, titleY, "LEVEL SELECT", kGold, 2);
    DrawTextCentered(framebuffer, cx, titleY + 2 * kLineHeight + 6,
                     FitText(Upper(view.campaignTitle), width - 2 * kMargin - 8, 1), kText);

    const int footerY = FooterY(height);
    const int listY = titleY + 2 * kLineHeight + 6 + kLineHeight + 12;
    const int rowH = kLineHeight + 8;
    const int visibleRows = std::max(1, (footerY - 8 - listY) / rowH);
    const int count = static_cast<int>(view.levels.size());
    const int firstRow = std::clamp(view.selected - visibleRows / 2, 0, std::max(0, count - visibleRows));

    const int barX = kMargin + 8;
    const int barW = width - 2 * barX;
    const int textRight = barX + barW - kTextInset;
    const int numberX = barX + 18;
    const int nameX = numberX + TextWidth("10", 1) + 10;  // room for two-digit level numbers
    const std::string lockedTag = "LOCKED";
    for (int row = 0; row < visibleRows && firstRow + row < count; ++row) {
        const int index = firstRow + row;
        const LevelSelectEntry& entry = view.levels[static_cast<std::size_t>(index)];
        const int rowY = listY + row * rowH;
        const bool selected = index == view.selected;
        if (selected) {
            DrawPanel(framebuffer, barX, rowY - 4, barW, kLineHeight + 7, kHighlight, kGold);
            DrawText(framebuffer, barX + kTextInset, rowY, ">", kGold);
        }
        const std::uint32_t color = LevelEntryColor(entry, selected);
        DrawText(framebuffer, numberX, rowY, std::to_string(entry.number), color);
        int nameRight = textRight;
        if (!entry.unlocked) {
            // Locked levels are dimmed AND tagged, so the state does not depend on colour.
            nameRight -= TextWidth(lockedTag, 1) + 8;
            DrawTextRightAligned(framebuffer, textRight, rowY, lockedTag, kTextDim);
        }
        DrawText(framebuffer, nameX, rowY, FitText(Upper(entry.name), nameRight - nameX, 1), color);
    }
    // "MORE" above or below the list when it scrolls.
    if (firstRow > 0) {
        DrawTextRightAligned(framebuffer, textRight, listY - rowH + 2, "MORE", kTextDim);
    }
    if (firstRow + visibleRows < count) {
        DrawTextRightAligned(framebuffer, textRight, listY + visibleRows * rowH - 2, "MORE", kTextDim);
    }

    DrawTextCentered(framebuffer, cx, footerY,
                     FitText("UP/DOWN: CHOOSE   ENTER: START   ESC: BACK", width - 2 * kMargin, 1), kText);
}

// The overlay panels below lay out their rows with one-off offsets from the panel top; each is
// used once, next to the text it places, so they stay literals (docs/CODING_STANDARDS.md, 5).
void UiRenderer::DrawPauseOverlay(Framebuffer& framebuffer, const std::string& levelLabel) {
    framebuffer.Darken(kPauseDimPercent);
    const int boxW = 210;
    const Point panel = DrawCenteredPanel(framebuffer, boxW, 96, kPanel);
    const int cx = panel.x + boxW / 2;
    const int y = panel.y;
    DrawTextCentered(framebuffer, cx, y + kOverlayTitleY, "PAUSED", kGold, 2);
    DrawTextCentered(framebuffer, cx, y + 30, FitText(levelLabel, boxW - 2 * kOverlayTextInset, 1), kText);
    DrawTextCentered(framebuffer, cx, y + 48, "ESC, P OR ENTER: RESUME", kValue);
    DrawTextCentered(framebuffer, cx, y + 60, "R: RESTART LEVEL", kValue);
    DrawTextCentered(framebuffer, cx, y + 72, "Q: QUIT TO MENU", kValue);
}

void UiRenderer::DrawLevelCompleteScreen(Framebuffer& framebuffer, const LevelCompleteView& view) {
    framebuffer.Darken(kResultDimPercent);
    const int boxW = framebuffer.Width() - 2 * 24;
    const int boxH = 150;
    const Point panel = DrawCenteredPanel(framebuffer, boxW, boxH, kLevelCompleteFill);
    const int cx = panel.x + boxW / 2;
    const int y = panel.y;
    const int textW = boxW - 2 * kOverlayTextInset;
    const std::string title =
        view.levelNumber > 0 ? LevelNumberText(view.levelNumber) + " COMPLETE" : std::string("LEVEL COMPLETE");
    DrawTextCentered(framebuffer, cx, y + kOverlayTitleY, FitText(title, textW, 2), kGold, 2);
    DrawTextCentered(framebuffer, cx, y + 30, FitText(Upper(view.levelName), textW, 1), kText);

    struct ResultRow {
        const char* label;
        std::string value;
    };
    const LevelResults& r = view.results;
    const std::array<ResultRow, 3> rows = {{
        {"ENEMIES KILLED", std::to_string(r.kills) + " / " + std::to_string(r.totalEnemies)},
        {"PICKUPS", std::to_string(r.pickups) + " / " + std::to_string(r.totalPickups)},
        {"TIME", FormatDuration(r.seconds)},
    }};
    const int labelX = panel.x + 36;
    const int valueRight = panel.x + boxW - 36;
    int rowY = y + 50;
    for (const ResultRow& row : rows) {
        DrawText(framebuffer, labelX, rowY, row.label, kText);
        DrawText(framebuffer, valueRight - TextWidth(row.value, 1), rowY, row.value, kValue);
        rowY += kLineHeight + 6;
    }
    DrawTextCentered(framebuffer, cx, y + 100,
                     view.lastLevel ? "THAT WAS THE LAST LEVEL!" : "HEALTH AND AMMO CARRY OVER", kMessage);
    DrawTextCentered(framebuffer, cx, y + boxH - 20,
                     view.lastLevel ? "ENTER: CONTINUE   ESC: MENU" : "ENTER: NEXT LEVEL   ESC: MENU", kValue);
}

void UiRenderer::DrawGameOverScreen(Framebuffer& framebuffer, const std::string& levelLabel) {
    framebuffer.Darken(kResultDimPercent);
    const int boxW = 240;
    const Point panel = DrawCenteredPanel(framebuffer, boxW, 108, kGameOverFill);
    const int cx = panel.x + boxW / 2;
    const int y = panel.y;
    const int textW = boxW - 2 * kOverlayTextInset;
    DrawTextCentered(framebuffer, cx, y + kOverlayTitleY, "GAME OVER", kWarn, 3);
    DrawTextCentered(framebuffer, cx, y + 38, FitText(levelLabel, textW, 1), kText);
    DrawTextCentered(framebuffer, cx, y + 56, "R OR ENTER: RESTART LEVEL", kValue);
    DrawTextCentered(framebuffer, cx, y + 68, "ESC: MAIN MENU", kValue);
    DrawTextCentered(framebuffer, cx, y + 88, FitText("HEALTH AND AMMO AS AT LEVEL START", textW, 1), kTextDim);
}

void UiRenderer::DrawCampaignCompleteScreen(Framebuffer& framebuffer, const std::string& campaignTitle,
                                            int levelCount) {
    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    const int cx = width / 2;
    const int textW = width - 2 * kMargin - 8;
    framebuffer.Clear(kCampaignCompleteBackground);

    int y = height / 12;
    DrawTextCentered(framebuffer, cx, y, "CONGRATULATIONS!", kGold, 2);
    y += 2 * kLineHeight + 10;
    DrawTextCentered(framebuffer, cx, y, "CAMPAIGN COMPLETE", kValue, 2);
    y += 2 * kLineHeight + 12;
    DrawTextCentered(framebuffer, cx, y, FitText(Upper(campaignTitle), textW, 1), kText);
    y += kLineHeight + 5;
    DrawTextCentered(framebuffer, cx, y, "ALL " + std::to_string(levelCount) + " LEVELS CLEARED", kText);

    y += kLineHeight + 18;
    // The first line is the heading.
    const std::array<const char*, 6> credits = {"CREDITS",
                                                "NEO-WOLF: A SOFTWARE RAYCASTER",
                                                "ENGINE: C++17 AND SDL2",
                                                "JSON: NLOHMANN/JSON (MIT)",
                                                "TEXTURES: TOOLS/DEVKIT/GEN_TEXTURES.PY",
                                                "THANKS FOR PLAYING!"};
    for (std::size_t i = 0; i < credits.size(); ++i) {
        DrawTextCentered(framebuffer, cx, y, FitText(credits[i], textW, 1), i == 0 ? kGold : kTextDim);
        y += kLineHeight + 5;
    }

    DrawTextCentered(framebuffer, cx, FooterY(height), "ENTER: MAIN MENU", kValue);
}

UiRenderer::MinimapLayout UiRenderer::ComputeMinimapLayout(int framebufferWidth, int framebufferHeight, int mapWidth,
                                                           int mapHeight, float playerX, float playerY) {
    constexpr int kMinPanelSize = 16;
    MinimapLayout layout{};
    layout.panelSize = std::max(kMinPanelSize, std::min(framebufferWidth, framebufferHeight) / 3);
    layout.panelX = kMargin;
    layout.panelY = kMargin;
    const int inner = layout.panelSize - 2 * kMinimapFrame;
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
    DrawPanel(framebuffer, layout.panelX, layout.panelY, layout.panelSize, layout.panelSize, kMinimapPanel);
    const MinimapCanvas canvas{&framebuffer, layout.panelX + kMinimapFrame, layout.panelY + kMinimapFrame,
                               layout.panelSize - 2 * kMinimapFrame};

    const CellRange cells = VisibleCells(canvas, layout, map);
    const int gap = layout.cellSize >= kMinimapGridMinCell ? 1 : 0;
    DrawMinimapCells(canvas, layout, map, cells, gap);
    const bool anyExit = DrawMinimapExits(canvas, layout, map);
    DrawMinimapLockedDoors(canvas, layout, map, cells, gap);
    DrawMinimapSprites(canvas, layout, sprites);
    DrawMinimapPlayer(canvas, layout, player);
    if (!layout.followsPlayer && anyExit) {
        DrawMinimapLegend(canvas, layout, map);
    }
    return layout.panelX + layout.panelSize;
}
