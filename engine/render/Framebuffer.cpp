#include "engine/render/Framebuffer.h"

#include <algorithm>
#include <cstddef>

#include "engine/render/RenderMath.h"

Framebuffer::Framebuffer(int width, int height) {
    Resize(width, height);
}

bool Framebuffer::Resize(int width, int height) {
    if (width <= 0 || height <= 0) {
        width_ = 0;
        height_ = 0;
        pixels_.clear();
        return false;
    }

    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), 0u);
    return true;
}

int Framebuffer::Width() const {
    return width_;
}

int Framebuffer::Height() const {
    return height_;
}

int Framebuffer::PitchBytes() const {
    return width_ * static_cast<int>(sizeof(std::uint32_t));
}

const std::uint32_t* Framebuffer::Data() const {
    return pixels_.empty() ? nullptr : pixels_.data();
}

std::uint32_t* Framebuffer::Data() {
    return pixels_.empty() ? nullptr : pixels_.data();
}

void Framebuffer::Clear(std::uint32_t color) {
    std::fill(pixels_.begin(), pixels_.end(), color);
}

void Framebuffer::PutPixel(int x, int y, std::uint32_t color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return;
    }

    pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = color;
}

void Framebuffer::DrawRect(int x, int y, int width, int height, std::uint32_t color) {
    if (width <= 0 || height <= 0) {
        return;
    }

    const int right = x + width - 1;
    const int bottom = y + height - 1;
    if (!drawBounds_.any) {
        drawBounds_ = DrawBounds{true, x, y, right, bottom, 0};
    } else {
        drawBounds_.minX = std::min(drawBounds_.minX, x);
        drawBounds_.minY = std::min(drawBounds_.minY, y);
        drawBounds_.maxX = std::max(drawBounds_.maxX, right);
        drawBounds_.maxY = std::max(drawBounds_.maxY, bottom);
    }
    if (x < 0 || y < 0 || right >= width_ || bottom >= height_) {
        ++drawBounds_.clippedRects;
    }

    const int xStart = std::max(0, x);
    const int yStart = std::max(0, y);
    const int xEnd = std::min(width_ - 1, right);
    const int yEnd = std::min(height_ - 1, bottom);

    for (int py = yStart; py <= yEnd; ++py) {
        const std::size_t rowOffset = static_cast<std::size_t>(py) * static_cast<std::size_t>(width_);
        for (int px = xStart; px <= xEnd; ++px) {
            pixels_[rowOffset + static_cast<std::size_t>(px)] = color;
        }
    }
}

void Framebuffer::Darken(int keepPercent) {
    const auto keep = static_cast<std::uint32_t>(std::clamp(keepPercent, 0, 100));
    const auto scale = [keep](std::uint8_t channel) { return static_cast<std::uint8_t>(channel * keep / 100u); };
    for (std::uint32_t& pixel : pixels_) {
        const ColorRGBA color = UnpackARGB32(pixel);
        pixel = PackARGB32(MakeColor(scale(color.r), scale(color.g), scale(color.b), color.a));
    }
}
