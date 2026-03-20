#pragma once

#include <cmath>
#include <type_traits>

constexpr float kPi = 3.14159265358979323846f;

template <typename T>
constexpr T Clamp(T value, T minimum, T maximum) {
    static_assert(std::is_arithmetic<T>::value, "Clamp requires arithmetic types.");
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

constexpr float DegreesToRadians(float degrees) {
    return degrees * (kPi / 180.0f);
}

constexpr float RadiansToDegrees(float radians) {
    return radians * (180.0f / kPi);
}

inline float NormalizeAngleRadians(float radians) {
    constexpr float twoPi = 2.0f * kPi;
    float result = std::fmod(radians, twoPi);
    if (result < 0.0f) {
        result += twoPi;
    }
    return result;
}

inline float NormalizeAngleDegrees(float degrees) {
    float result = std::fmod(degrees, 360.0f);
    if (result < 0.0f) {
        result += 360.0f;
    }
    return result;
}
