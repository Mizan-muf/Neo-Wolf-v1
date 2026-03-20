#pragma once

#include <cstdint>
#include <vector>

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
    void DrawVerticalLine(int x, int yStart, int yEnd, std::uint32_t color);
    void DrawRect(int x, int y, int width, int height, std::uint32_t color);

    static constexpr std::uint32_t PackRGBA(
        std::uint8_t r,
        std::uint8_t g,
        std::uint8_t b,
        std::uint8_t a = 255) {
        return (static_cast<std::uint32_t>(a) << 24) |
               (static_cast<std::uint32_t>(r) << 16) |
               (static_cast<std::uint32_t>(g) << 8) |
               static_cast<std::uint32_t>(b);
    }

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint32_t> pixels_{};
};
