#pragma once

#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 Add(const Vec2& a, const Vec2& b) {
    return {a.x + b.x, a.y + b.y};
}

inline Vec2 Subtract(const Vec2& a, const Vec2& b) {
    return {a.x - b.x, a.y - b.y};
}

inline Vec2 Scale(const Vec2& value, float scalar) {
    return {value.x * scalar, value.y * scalar};
}

inline float Length(const Vec2& value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

inline Vec2 Normalize(const Vec2& value) {
    const float len = Length(value);
    if (len <= 0.000001f) {
        return {0.0f, 0.0f};
    }

    const float invLen = 1.0f / len;
    return {value.x * invLen, value.y * invLen};
}

inline float Dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}
