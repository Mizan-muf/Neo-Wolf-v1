#pragma once

#include <cstdint>
#include <vector>

class Texture {
public:
    Texture() = default;
    Texture(int width, int height);

    bool Resize(int width, int height);
    int Width() const;
    int Height() const;

    std::uint32_t* Pixels();
    const std::uint32_t* Pixels() const;

    std::uint32_t GetPixel(int x, int y) const;
    void SetPixel(int x, int y, std::uint32_t color);
    std::uint32_t SampleNearest(float u, float v) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<std::uint32_t> pixels_{};
};
