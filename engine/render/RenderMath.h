#pragma once

// Helpers shared by the wall, sprite and particle renderers: the camera set-up of the DDA
// raycaster, the billboard projection, and the shading of ARGB pixels. Inline, because most
// of them run once per screen column or once per pixel.

#include <cmath>
#include <cstdint>
#include <optional>

#include "engine/assets/Texture.h"
#include "engine/core/Color.h"
#include "engine/core/MathUtils.h"
#include "engine/core/Vec2.h"
#include "engine/world/Map.h"

// The projection ("Hor+"). The vertical field of view is fixed: at distance 1 the view spans
// kViewHalfHeight world units above and below the horizon, so a wall one unit high fills the
// screen height (tan(vertical fov / 2) = 0.5, about 53.1 degrees, unchanged since 1.0.0).
// Pixels are square, so the horizontal field of view follows from the framebuffer's shape: a
// wider framebuffer shows more at the sides, never less above and below (about 67.4 degrees
// at 4:3, 83.3 degrees at 16:9). Up to 1.0.0 the horizontal field of view was a fixed 60
// degrees, which stretched the 320x240 image 1.15 times horizontally.
inline constexpr float kViewHalfHeight = 0.5f;
// The eye is half a wall high: the horizon is the middle row.
inline constexpr float kEyeHeight = 0.5f;

// Pixels per world unit at distance 1, across and up the screen alike: the screen height.
inline float FocalLengthPixels(int screenHeight) {
    return 0.5f * static_cast<float>(screenHeight) / kViewHalfHeight;
}

// The player's view as the renderers use it: the unit forward direction and the camera plane,
// perpendicular to it, whose length spans half the screen width at distance 1
// (kViewHalfHeight times the framebuffer's width / height).
struct CameraBasis {
    Vec2 forward{};
    Vec2 plane{};
};

inline CameraBasis MakeCameraBasis(float directionAngleRadians, int screenWidth, int screenHeight) {
    const Vec2 forward{std::cos(directionAngleRadians), std::sin(directionAngleRadians)};
    const float planeScale =
        screenHeight > 0 ? kViewHalfHeight * static_cast<float>(screenWidth) / static_cast<float>(screenHeight) : 0.0f;
    return {forward, {-forward.y * planeScale, forward.x * planeScale}};
}

// A point in camera space: `x` across the view (in camera-plane units), `depth` along it.
struct ViewPoint {
    float x = 0.0f;
    float depth = 0.0f;
};

// Billboards (sprites, particles) closer than this, or behind the eye, are not drawn.
inline constexpr float kBillboardNearPlane = 0.001f;

// World-to-camera transform for billboards: the inverse of the matrix [plane forward].
struct BillboardProjection {
    Vec2 eye{};
    CameraBasis camera{};
    float invDet = 0.0f;
};

// Empty when the camera matrix cannot be inverted (a framebuffer of about 0 width).
inline std::optional<BillboardProjection> MakeBillboardProjection(const Vec2& eye, const CameraBasis& camera) {
    constexpr float kMinDeterminant = 1e-6f;
    const float det = (camera.plane.x * camera.forward.y) - (camera.forward.x * camera.plane.y);
    if (std::fabs(det) < kMinDeterminant) {
        return std::nullopt;
    }
    return BillboardProjection{eye, camera, 1.0f / det};
}

inline ViewPoint ToViewSpace(const BillboardProjection& projection, const Vec2& worldPosition) {
    const Vec2& forward = projection.camera.forward;
    const Vec2& plane = projection.camera.plane;
    const float relX = worldPosition.x - projection.eye.x;
    const float relY = worldPosition.y - projection.eye.y;
    return {projection.invDet * ((forward.y * relX) - (forward.x * relY)),
            projection.invDet * ((-plane.y * relX) + (plane.x * relY))};
}

// The screen column of a camera-space point (depth > 0); may lie outside the screen.
inline int ScreenColumn(const ViewPoint& point, int screenWidth) {
    return static_cast<int>((static_cast<float>(screenWidth) * 0.5f) * (1.0f + point.x / point.depth));
}

// Brightness falloff with distance: 1 at the eye, never below minShade.
inline float DistanceShade(float distance, float falloffPerUnit, float minShade) {
    return Clamp(1.0f / (1.0f + distance * falloffPerUnit), minShade, 1.0f);
}

// The light level (Map::GetLightLevel) of the cell that contains a world point.
inline float LightAt(const Map& map, float worldX, float worldY) {
    return map.GetLightLevel(static_cast<int>(std::floor(worldX)), static_cast<int>(std::floor(worldY)));
}

inline ColorRGBA UnpackARGB32(std::uint32_t argb) {
    return MakeColor(static_cast<std::uint8_t>((argb >> 16u) & 0xFFu), static_cast<std::uint8_t>((argb >> 8u) & 0xFFu),
                     static_cast<std::uint8_t>(argb & 0xFFu), static_cast<std::uint8_t>((argb >> 24u) & 0xFFu));
}

// Scales the colour channels by `factor` (clamped to 0-1, rounding down); alpha is kept.
inline std::uint32_t ScaleRgb(std::uint32_t argb, float factor) {
    factor = Clamp(factor, 0.0f, 1.0f);
    const ColorRGBA color = UnpackARGB32(argb);
    return PackARGB32(MakeColor(static_cast<std::uint8_t>(static_cast<float>(color.r) * factor),
                                static_cast<std::uint8_t>(static_cast<float>(color.g) * factor),
                                static_cast<std::uint8_t>(static_cast<float>(color.b) * factor), color.a));
}

// A texture with pixels to sample.
inline bool IsDrawable(const Texture* texture) {
    return texture != nullptr && texture->Width() > 0 && texture->Height() > 0;
}
