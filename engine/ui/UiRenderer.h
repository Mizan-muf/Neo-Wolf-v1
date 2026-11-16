#pragma once

// Screens and HUD for the 320x240 framebuffer (Roadmap Phase 5, known issue C8).
//
// Every position is derived from the framebuffer's width and height, and every screen keeps
// UiLayout::kMargin pixels away from the edges; test `ui_layout_bounds` renders each screen
// and checks the recorded draw bounds (Framebuffer::GetDrawBounds). Text uses the built-in
// 5x7 bitmap font: scale 1 for body text, scale 2-3 for titles. Long text is shortened to
// fit (FitText), never clipped.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/core/GameFlow.h"

class Framebuffer;
class Map;
struct Player;
struct SpriteEntity;

namespace UiLayout {
inline constexpr int kMargin = 4;        // minimum distance of any UI element from the edges
inline constexpr int kGlyphWidth = 5;    // per scale unit
inline constexpr int kGlyphHeight = 7;
inline constexpr int kGlyphAdvance = 6;  // glyph width + 1 pixel spacing
inline constexpr int kHudBarHeight = 14;
// Phase 6 key-card icons: this many slots of kKeyCardSlotWidth pixels are kept free in the
// HUD bar, between the ammo counter and the level label.
inline constexpr int kKeyCardSlots = 3;
inline constexpr int kKeyCardSlotWidth = 14;
} // namespace UiLayout

namespace UiRenderer {
// Width in pixels of `text` at `scale` (no trailing spacing).
int TextWidth(const std::string& text, int scale);
// `text`, shortened with ".." so that it is at most maxWidth pixels wide at `scale`.
std::string FitText(const std::string& text, int maxWidth, int scale);

void DrawText(Framebuffer& framebuffer, int x, int y, const std::string& text, std::uint32_t color, int scale = 1);
void DrawTextCentered(Framebuffer& framebuffer, int centerX, int y, const std::string& text, std::uint32_t color,
                      int scale = 1);

struct HudView {
    int health = 0;
    int ammo = 0;
    int levelNumber = 0;           // 1-based; 0 = not a campaign level (C API load)
    std::string levelName;
    bool showLevelBanner = false;  // large "LEVEL N / name" banner at level start
    std::string message;           // pickups, hits, checkpoints...
    int reservedLeft = 0;          // the minimap's right edge when it is shown: the banner moves right of it
};
void DrawHud(Framebuffer& framebuffer, const HudView& view);
void DrawCrosshair(Framebuffer& framebuffer);
void DrawDebugOverlay(Framebuffer& framebuffer, float fps, float playerX, float playerY, float angleDegrees,
                      int cellX, int cellY);

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

// Minimap (known issue C9): a square panel of about a third of the screen height in the
// top-left corner. Small maps are scaled to fill it; maps too big for kMinimapMinCell pixels
// per cell are drawn at that size, centred on the player and cut at the panel edge. Exit
// switches are drawn as an "X" cross on a green cell, not by colour alone; the player is a
// dot with a direction tick.
inline constexpr int kMinimapMinCell = 3;
inline constexpr int kMinimapMaxCell = 8;
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
// Returns the panel's right edge (for HudView::reservedLeft).
int DrawMinimap(Framebuffer& framebuffer, const Map& map, const Player& player,
                const std::vector<SpriteEntity>& sprites);
} // namespace UiRenderer
