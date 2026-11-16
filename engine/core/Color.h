#pragma once

// RGBA colours and their packing into the framebuffer's ARGB pixels.

#include <cstdint>

struct ColorRGBA {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

constexpr std::uint32_t PackARGB32(const ColorRGBA& color) {
    return (static_cast<std::uint32_t>(color.a) << 24U) | (static_cast<std::uint32_t>(color.r) << 16U) |
           (static_cast<std::uint32_t>(color.g) << 8U) | static_cast<std::uint32_t>(color.b);
}

constexpr ColorRGBA MakeColor(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255) {
    return {r, g, b, a};
}
