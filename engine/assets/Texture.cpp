#include "engine/assets/Texture.h"

#include <algorithm>
#include <cmath>

#include "engine/core/MathUtils.h"

Texture::Texture(int width, int height) {
    Resize(width, height);
}

bool Texture::Resize(int width, int height) {
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

int Texture::Width() const {
    return width_;
}

int Texture::Height() const {
    return height_;
}

std::uint32_t* Texture::Pixels() {
    return pixels_.empty() ? nullptr : pixels_.data();
}

const std::uint32_t* Texture::Pixels() const {
    return pixels_.empty() ? nullptr : pixels_.data();
}

std::uint32_t Texture::GetPixel(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_ || pixels_.empty()) {
        return 0u;
    }

    return pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)];
}

void Texture::SetPixel(int x, int y, std::uint32_t color) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_ || pixels_.empty()) {
        return;
    }

    pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = color;
}

std::uint32_t Texture::SampleNearest(float u, float v) const {
    if (width_ <= 0 || height_ <= 0 || pixels_.empty()) {
        return 0u;
    }

    const float wrappedU = u - std::floor(u);
    const float wrappedV = v - std::floor(v);
    const int x = Clamp(static_cast<int>(wrappedU * static_cast<float>(width_)), 0, width_ - 1);
    const int y = Clamp(static_cast<int>(wrappedV * static_cast<float>(height_)), 0, height_ - 1);
    return GetPixel(x, y);
}
