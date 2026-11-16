#pragma once

// Small maths helpers shared by the engine modules (angles, clamping).

#include <cmath>
#include <type_traits>

constexpr float kPi = 3.14159265358979323846f;

template <typename T>
constexpr T Clamp(T value, T minimum, T maximum) {
    static_assert(std::is_arithmetic_v<T>, "Clamp requires arithmetic types.");
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

constexpr float DegreesToRadians(float degrees) {
    return degrees * (kPi / 180.0f);
}

inline float NormalizeAngleRadians(float radians) {
    constexpr float twoPi = 2.0f * kPi;
    float result = std::fmod(radians, twoPi);
    if (result < 0.0f) {
        result += twoPi;
    }
    return result;
}

inline float NormalizeAngle(float radians) {
    return NormalizeAngleRadians(radians);
}
