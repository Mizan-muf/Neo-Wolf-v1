#pragma once

// The software framebuffer every renderer and the UI draw into: width x height ARGB pixels
// (0xAARRGGBB, row by row from the top left), uploaded to the window by the platform layer.

#include <cstdint>
#include <vector>

#include "engine/core/Color.h"

// The union of every DrawRect request since the last ResetDrawBounds(), and how many requests
// reached outside the framebuffer (and were clipped). The UI layout tests use it to prove
// that every screen fits inside 320x240 (known issue C8).
struct DrawBounds {
    bool any = false;
    int minX = 0;
    int minY = 0;
    int maxX = -1;  // inclusive
    int maxY = -1;  // inclusive
    int clippedRects = 0;
};

class Framebuffer {
public:
    Framebuffer() = default;
    Framebuffer(int width, int height);

    bool Resize(int width, int height);

    int Width() const;
    int Height() const;
    int PitchBytes() const;

    const std::uint32_t* Data() const;
    std::uint32_t* Data();

    void Clear(std::uint32_t color);
    void PutPixel(int x, int y, std::uint32_t color);
    void DrawRect(int x, int y, int width, int height, std::uint32_t color);
    // Scales every pixel's colour by keepPercent/100 (0-100): dims the world behind overlays.
    void Darken(int keepPercent);

    void ResetDrawBounds() { drawBounds_ = DrawBounds{}; }
    const DrawBounds& GetDrawBounds() const { return drawBounds_; }

    static constexpr std::uint32_t PackRGBA(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
        return PackARGB32(MakeColor(r, g, b, a));
    }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint32_t> pixels_;
    DrawBounds drawBounds_{};
};
