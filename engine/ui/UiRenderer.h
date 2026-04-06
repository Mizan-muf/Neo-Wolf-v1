#pragma once

#include <cstdint>
#include <string>

class Framebuffer;

namespace UiRenderer {
void DrawText(
    Framebuffer& framebuffer,
    int x,
    int y,
    const std::string& text,
    std::uint32_t color,
    int scale = 1);

void DrawHud(
    Framebuffer& framebuffer,
    int health,
    int ammo);

void DrawCrosshair(Framebuffer& framebuffer);

void DrawDebugOverlay(
    Framebuffer& framebuffer,
    float fps,
    float playerX,
    float playerY,
    float angleDegrees,
    int cellX,
    int cellY);

void DrawPauseOverlay(Framebuffer& framebuffer);
} // namespace UiRenderer
