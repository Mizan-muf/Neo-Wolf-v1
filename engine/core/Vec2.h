#pragma once

// 2D float vector for world positions and directions (map units, one tile = 1.0).

#include <cmath>

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 Add(const Vec2& a, const Vec2& b) {
    return {a.x + b.x, a.y + b.y};
}

inline Vec2 Scale(const Vec2& value, float scalar) {
    return {value.x * scalar, value.y * scalar};
}

inline Vec2 operator+(const Vec2& a, const Vec2& b) {
    return {a.x + b.x, a.y + b.y};
}

inline Vec2 operator-(const Vec2& a, const Vec2& b) {
    return {a.x - b.x, a.y - b.y};
}

inline Vec2 operator*(const Vec2& a, float scalar) {
    return {a.x * scalar, a.y * scalar};
}

inline Vec2 operator*(float scalar, const Vec2& a) {
    return {a.x * scalar, a.y * scalar};
}

inline float Length(const Vec2& value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

inline float LengthSquared(const Vec2& value) {
    return value.x * value.x + value.y * value.y;
}

inline float Dot(const Vec2& a, const Vec2& b) {
    return a.x * b.x + a.y * b.y;
}
