#include "engine/render/Framebuffer.h"

#include <algorithm>
#include <cstddef>
#include <utility>

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

void Framebuffer::DrawVerticalLine(int x, int yStart, int yEnd, std::uint32_t color) {
    if (x < 0 || x >= width_ || height_ <= 0) {
        return;
    }

    if (yStart > yEnd) {
        std::swap(yStart, yEnd);
    }

    yStart = std::max(0, yStart);
    yEnd = std::min(height_ - 1, yEnd);
    for (int y = yStart; y <= yEnd; ++y) {
        PutPixel(x, y, color);
    }
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
    const int xEnd = std::min(width_ - 1, x + width - 1);
    const int yEnd = std::min(height_ - 1, y + height - 1);

    for (int py = yStart; py <= yEnd; ++py) {
        const std::size_t rowOffset = static_cast<std::size_t>(py) * static_cast<std::size_t>(width_);
        for (int px = xStart; px <= xEnd; ++px) {
            pixels_[rowOffset + static_cast<std::size_t>(px)] = color;
        }
    }
}

void Framebuffer::Darken(int keepPercent) {
    const std::uint32_t keep = static_cast<std::uint32_t>(std::clamp(keepPercent, 0, 100));
    for (std::uint32_t& pixel : pixels_) {
        const std::uint32_t r = ((pixel >> 16) & 0xFFu) * keep / 100u;
        const std::uint32_t g = ((pixel >> 8) & 0xFFu) * keep / 100u;
        const std::uint32_t b = (pixel & 0xFFu) * keep / 100u;
        pixel = (pixel & 0xFF000000u) | (r << 16) | (g << 8) | b;
    }
}
