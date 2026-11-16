#pragma once

// Screens and HUD (Roadmap Phase 5, known issue C8).
//
// The UI is laid out in UI pixels on a canvas of at least kDesignWidth x kDesignHeight; each
// UI pixel is drawn as a UiScale x UiScale block of framebuffer pixels, so the UI keeps its
// size relative to the screen at every render size: 320x240 shows it at scale 1 (as up to
// 1.0.0), the default 640x360 at scale 2 on a 320x180 canvas. Every position is derived from
// the canvas's width and height, and every screen keeps UiLayout::kMargin UI pixels away from
// the edges; test `ui_layout_bounds` renders each screen at several render sizes and checks
// the recorded draw bounds (Framebuffer::GetDrawBounds). Text uses the built-in 5x7 bitmap
// font: scale 1 for body text, scale 2-3 for titles. Long text is shortened to fit
// (FitText), never clipped. Sizes in UiLayout and TextWidth are in UI pixels; the rectangles
// and layouts the functions below return are in framebuffer pixels.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "engine/core/GameFlow.h"
#include "engine/world/KeyColor.h"

class Framebuffer;
class Map;
struct Player;
struct SpriteEntity;

namespace UiLayout {
// The smallest canvas every screen is laid out for, in UI pixels: 16:9 at the default render
// size, 4:3 (320x240) is taller.
inline constexpr int kDesignWidth = 320;
inline constexpr int kDesignHeight = 180;
// Framebuffer pixels per UI pixel: the largest whole number that leaves a canvas of at least
// kDesignWidth x kDesignHeight UI pixels, at least 1. 320x240 -> 1, 640x360 and 640x480 -> 2,
// 960x540 -> 3, 1280x720 -> 4. Whole numbers keep the bitmap font and 1-pixel lines crisp.
constexpr int UiScale(int framebufferWidth, int framebufferHeight) {
    return std::max(1, std::min(framebufferWidth / kDesignWidth, framebufferHeight / kDesignHeight));
}
inline constexpr int kMargin = 4;      // minimum distance of any UI element from the edges
inline constexpr int kGlyphWidth = 5;  // per scale unit
inline constexpr int kGlyphHeight = 7;
inline constexpr int kGlyphAdvance = 6;  // glyph width + 1 pixel spacing
inline constexpr int kTextInset = 6;     // horizontal padding between a panel's or bar's edge and its text
inline constexpr int kHudBarHeight = 14;
// The HUD bar's "HEALTH" and "AMMO" labels (x); the bar starts at the left margin at every width.
inline constexpr int kHudHealthX = kMargin + kTextInset;
inline constexpr int kHudAmmoX = kHudHealthX + 80;
// Key-card icons (Phase 6): one slot per key colour (red, blue, gold, in that order) of
// kKeyCardSlotWidth pixels in the HUD bar, between the ammo counter and the level label. A
// held key is drawn as a card of kKeyCardWidth x kKeyCardHeight in its colour with its
// initial (R, B, G) on it, so the keys differ by more than colour.
inline constexpr int kKeyCardSlots = 3;
inline constexpr int kKeyCardSlotWidth = 14;
inline constexpr int kKeyCardWidth = 12;
inline constexpr int kKeyCardHeight = 10;
inline constexpr int kKeyCardsOffset = 64;  // from the ammo label's x
}  // namespace UiLayout

namespace UiRenderer {
// Width in UI pixels of `text` at `scale` (no trailing spacing).
int TextWidth(const std::string& text, int scale);
// `text`, shortened with ".." so that it is at most maxWidth UI pixels wide at `scale`.
std::string FitText(const std::string& text, int maxWidth, int scale);

struct HudView {
    int health = 0;
    int ammo = 0;
    int levelNumber = 0;  // 1-based; 0 = not a campaign level (C API load)
    std::string levelName;
    bool showLevelBanner = false;  // large "LEVEL N / name" banner at level start
    std::string message;           // pickups, hits, checkpoints...
    int reservedLeft =
        0;           // the minimap's right edge (framebuffer x, DrawMinimap) when shown: the banner moves right of it
    KeyRing keys{};  // key cards held (icons in the bar)
};

struct UiRect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};
// Where the HUD draws the card for `color` (red, blue or gold), in framebuffer pixels.
UiRect KeyCardRect(int framebufferWidth, int framebufferHeight, KeyColor color);
// The colour of a key card (HUD, minimap).
std::uint32_t KeyCardColor(KeyColor color);
void DrawHud(Framebuffer& framebuffer, const HudView& view);
void DrawCrosshair(Framebuffer& framebuffer);
void DrawDebugOverlay(Framebuffer& framebuffer, float fps, float playerX, float playerY, float angleDegrees, int cellX,
                      int cellY);

struct MenuView {
    std::string campaignTitle;
    std::vector<std::string> items;
    int selected = 0;
};
void DrawMenuScreen(Framebuffer& framebuffer, const MenuView& view);

struct LevelSelectEntry {
    int number = 0;  // 1-based
    std::string name;
    bool unlocked = false;
};
struct LevelSelectView {
    std::string campaignTitle;
    std::vector<LevelSelectEntry> levels;
    int selected = 0;  // index into levels
};
void DrawLevelSelectScreen(Framebuffer& framebuffer, const LevelSelectView& view);

// "LEVEL 2: NAME", or "" when not a campaign level.
std::string LevelLabel(int levelNumber, const std::string& levelName);

void DrawPauseOverlay(Framebuffer& framebuffer, const std::string& levelLabel);

struct LevelCompleteView {
    int levelNumber = 0;
    std::string levelName;
    LevelResults results{};
    bool lastLevel = false;
};
void DrawLevelCompleteScreen(Framebuffer& framebuffer, const LevelCompleteView& view);
void DrawGameOverScreen(Framebuffer& framebuffer, const std::string& levelLabel);
void DrawCampaignCompleteScreen(Framebuffer& framebuffer, const std::string& campaignTitle, int levelCount);

// "M:SS" (or "H:MM:SS" from one hour).
std::string FormatDuration(double seconds);

// Minimap (known issue C9): a square panel in the top-left corner, a third of the canvas
// height but at least kMinimapMinPanel UI pixels (a third of the 320x240 canvas), so the 16:9
// canvas (320x180) shows as much of the map as 320x240. Small maps are scaled to fill it; maps
// too big for kMinimapMinCell UI pixels per cell are drawn at that size, centred on the player
// and cut at the panel edge. Exit switches are drawn as an "X" cross on a green cell, not by
// colour alone; the player is a dot with a direction tick. Key cards are drawn as a small key
// shape in their colour, locked doors as a cell in the lock's colour crossed by a dark bar
// (shape and colour, Phase 6).
inline constexpr int kMinimapMinPanel = 80;
inline constexpr int kMinimapMinCell = 3;
inline constexpr int kMinimapMaxCell = 8;
// In framebuffer pixels (the cell size too).
struct MinimapLayout {
    int panelX = 0;
    int panelY = 0;
    int panelSize = 0;
    int cellSize = 0;
    int originX = 0;  // framebuffer position of map cell (0, 0)'s top-left corner
    int originY = 0;
    bool followsPlayer = false;  // the map is bigger than the panel
};
MinimapLayout ComputeMinimapLayout(int framebufferWidth, int framebufferHeight, int mapWidth, int mapHeight,
                                   float playerX, float playerY);
// Returns the panel's right edge, a framebuffer x (for HudView::reservedLeft).
int DrawMinimap(Framebuffer& framebuffer, const Map& map, const Player& player,
                const std::vector<SpriteEntity>& sprites);
}  // namespace UiRenderer
