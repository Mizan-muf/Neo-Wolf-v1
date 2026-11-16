#include "engine/render/Raycaster.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

#include "engine/assets/Texture.h"
#include "engine/core/MathUtils.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/RenderMath.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

namespace {
// Ray directions shorter than this are treated as zero (no hit, infinite step distance).
constexpr float kMinRayComponent = 1e-6f;
// The corrected wall distance never drops below this, so the projected height stays finite.
constexpr float kMinWallDistance = 0.0001f;

// Shading (brightness factors, 0-1). Floor and ceiling: distance falloff per unit and its
// floor, then the light level, never below kFloorMinLight.
constexpr float kFloorFalloff = 0.13f;
constexpr float kFloorMinDistanceShade = 0.28f;
constexpr float kFloorMinLight = 0.20f;
// Walls: north and south faces (horizontal hits) are darker than east and west faces.
constexpr float kHorizontalFaceShade = 0.72f;
constexpr float kWallFalloff = 0.11f;
constexpr float kWallMinDistanceShade = 0.35f;
constexpr float kWallMinLight = 0.18f;

float SafeInverse(float value) {
    if (std::fabs(value) < kMinRayComponent) {
        return std::numeric_limits<float>::infinity();
    }
    return 1.0f / value;
}

const Texture* OrMissing(const Texture* texture, const RaycasterRenderParams& params) {
    return texture != nullptr ? texture : params.missingTexture;
}

const Texture* ResolveTextureForTile(const RaycasterRenderParams& params, TileType tileType) {
    switch (tileType) {
        case TileType::Wall:
            return OrMissing(params.wallTexture, params);
        case TileType::Door:
            return OrMissing(params.doorTexture, params);
        case TileType::WallAltA:
            return OrMissing(params.wallAltATexture, params);
        case TileType::WallAltB:
            return OrMissing(params.wallAltBTexture, params);
        case TileType::Exit:
            return OrMissing(params.exitTexture, params);
        default:
            return params.missingTexture;
    }
}

// The tile's texture; a locked door uses the texture of its lock colour when there is one.
const Texture* ResolveWallTexture(const Map& map, const RaycasterRenderParams& params, const RaycastHit& hit) {
    if (hit.tileType == TileType::Door) {
        const int lock = KeyColorIndex(map.GetDoorLock(hit.mapX, hit.mapY));
        if (lock >= 0 && params.lockedDoorTextures[static_cast<std::size_t>(lock)] != nullptr) {
            return params.lockedDoorTextures[static_cast<std::size_t>(lock)];
        }
    }
    return ResolveTextureForTile(params, hit.tileType);
}

// Floor and ceiling: for every row below the horizon, the floor point under each column by
// perspective projection; the mirrored row above the horizon samples the ceiling there.
void RenderFloorAndCeiling(const Map& map, const Vec2& eye, const CameraBasis& camera, Framebuffer& framebuffer,
                           const RaycasterRenderParams& params) {
    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    const int horizon = height / 2;
    const Vec2 rayDirLeft{camera.forward.x - camera.plane.x, camera.forward.y - camera.plane.y};
    const Vec2 rayDirRight{camera.forward.x + camera.plane.x, camera.forward.y + camera.plane.y};
    const Texture* floorTexture = IsDrawable(params.floorTexture) ? params.floorTexture : params.missingTexture;
    const Texture* ceilingTexture = IsDrawable(params.ceilingTexture) ? params.ceilingTexture : params.missingTexture;

    const float cameraZ = 0.5f * static_cast<float>(height);
    for (int y = horizon + 1; y < height; ++y) {
        const int mirroredY = height - y - 1;
        const auto rowsBelowHorizon = static_cast<float>(y - horizon);
        const float rowDistance = cameraZ / rowsBelowHorizon;
        const float stepX = rowDistance * (rayDirRight.x - rayDirLeft.x) / static_cast<float>(width);
        const float stepY = rowDistance * (rayDirRight.y - rayDirLeft.y) / static_cast<float>(width);

        float worldX = eye.x + rowDistance * rayDirLeft.x;
        float worldY = eye.y + rowDistance * rayDirLeft.y;
        const float distanceShade = DistanceShade(rowDistance, kFloorFalloff, kFloorMinDistanceShade);

        for (int x = 0; x < width; ++x) {
            const float light = Clamp(distanceShade * LightAt(map, worldX, worldY), kFloorMinLight, 1.0f);
            if (floorTexture != nullptr) {
                framebuffer.PutPixel(x, y, ScaleRgb(floorTexture->SampleNearest(worldX, worldY), light));
            }
            if (ceilingTexture != nullptr) {
                framebuffer.PutPixel(x, mirroredY, ScaleRgb(ceilingTexture->SampleNearest(worldX, worldY), light));
            }

            worldX += stepX;
            worldY += stepY;
        }
    }
}

// The texture column (0-1) of a wall hit. It must grow from the viewer's left to right. Map y
// grows downwards, so looking +x the viewer's left is the smaller y (u = frac(y)) and looking
// -x it is the larger y (flip); looking -y the left is the smaller x (u = frac(x)) and looking
// +y the larger x (flip). Before Phase 5 the flips were the other way round and every wall was
// mirrored, which showed on the exit switch's "EXIT" label.
float WallTextureU(const RaycastHit& hit, const Vec2& rayDirection) {
    float wallU = (hit.side == RayHitSide::Vertical) ? hit.hitPosition.y : hit.hitPosition.x;
    wallU = wallU - std::floor(wallU);
    if ((hit.side == RayHitSide::Vertical && rayDirection.x < 0.0f) ||
        (hit.side == RayHitSide::Horizontal && rayDirection.y > 0.0f)) {
        wallU = 1.0f - wallU;
    }
    return wallU;
}

// A wall face is lit like the cell in front of it, the one the ray crossed last (light zones
// cover floor cells; a wall between a lit and a dark room is lit on one side and dark on the
// other).
float WallShade(const Map& map, const RaycastHit& hit, const Vec2& rayDirection) {
    const float sideShade = (hit.side == RayHitSide::Vertical) ? 1.0f : kHorizontalFaceShade;
    const float distanceShade = DistanceShade(hit.distanceCorrected, kWallFalloff, kWallMinDistanceShade);
    const int faceX = hit.side == RayHitSide::Vertical ? hit.mapX + (rayDirection.x < 0.0f ? 1 : -1) : hit.mapX;
    const int faceY = hit.side == RayHitSide::Horizontal ? hit.mapY + (rayDirection.y < 0.0f ? 1 : -1) : hit.mapY;
    const float mapLight = map.GetLightLevel(faceX, faceY);
    return Clamp(sideShade * distanceShade * mapLight, kWallMinLight, 1.0f);
}

// One textured wall column, centred on the horizon; its height is screen height / distance.
void DrawWallColumn(Framebuffer& framebuffer, int x, float distance, const Texture& texture, float wallU, float shade) {
    const int height = framebuffer.Height();
    const int projectedWallHeight = std::max(1, static_cast<int>(std::round(static_cast<float>(height) / distance)));
    const int yStart = (height - projectedWallHeight) / 2;
    const int yEnd = yStart + projectedWallHeight - 1;
    for (int y = yStart; y <= yEnd; ++y) {
        const float wallV = (projectedWallHeight <= 1)
                                ? 0.0f
                                : static_cast<float>(y - yStart) / static_cast<float>(projectedWallHeight - 1);
        framebuffer.PutPixel(x, y, ScaleRgb(texture.SampleNearest(wallU, wallV), shade));
    }
}
}  // namespace

RaycastHit Raycaster::CastRay(const Map& map, const Vec2& rayOrigin, const Vec2& rayDirection,
                              float playerDirectionAngleRadians) {
    RaycastHit result{};

    if (map.Width() <= 0 || map.Height() <= 0) {
        return result;
    }

    const float rayLength = std::sqrt(rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y);
    if (rayLength < kMinRayComponent) {
        return result;
    }

    int mapX = static_cast<int>(std::floor(rayOrigin.x));
    int mapY = static_cast<int>(std::floor(rayOrigin.y));

    const float deltaDistX = std::fabs(SafeInverse(rayDirection.x));
    const float deltaDistY = std::fabs(SafeInverse(rayDirection.y));

    int stepX = 0;
    int stepY = 0;
    float sideDistX = 0.0f;
    float sideDistY = 0.0f;

    if (rayDirection.x < 0.0f) {
        stepX = -1;
        sideDistX = (rayOrigin.x - static_cast<float>(mapX)) * deltaDistX;
    } else {
        stepX = 1;
        sideDistX = (static_cast<float>(mapX + 1) - rayOrigin.x) * deltaDistX;
    }

    if (rayDirection.y < 0.0f) {
        stepY = -1;
        sideDistY = (rayOrigin.y - static_cast<float>(mapY)) * deltaDistY;
    } else {
        stepY = 1;
        sideDistY = (static_cast<float>(mapY + 1) - rayOrigin.y) * deltaDistY;
    }

    RayHitSide side = RayHitSide::Vertical;
    constexpr int kMaxSteps = 1024;
    for (int i = 0; i < kMaxSteps; ++i) {
        if (sideDistX < sideDistY) {
            sideDistX += deltaDistX;
            mapX += stepX;
            side = RayHitSide::Vertical;
        } else {
            sideDistY += deltaDistY;
            mapY += stepY;
            side = RayHitSide::Horizontal;
        }

        if (!map.IsInsideMap(mapX, mapY)) {
            return result;
        }

        if (map.IsWall(mapX, mapY)) {
            result.hit = true;
            result.mapX = mapX;
            result.mapY = mapY;
            result.tileType = map.GetCell(mapX, mapY);
            result.side = side;
            break;
        }
    }

    if (!result.hit) {
        return result;
    }

    float distanceRayParameter = 0.0f;
    if (result.side == RayHitSide::Vertical) {
        distanceRayParameter =
            (static_cast<float>(result.mapX) - rayOrigin.x + (1.0f - static_cast<float>(stepX)) * 0.5f) /
            rayDirection.x;
    } else {
        distanceRayParameter =
            (static_cast<float>(result.mapY) - rayOrigin.y + (1.0f - static_cast<float>(stepY)) * 0.5f) /
            rayDirection.y;
    }

    distanceRayParameter = std::max(distanceRayParameter, 0.0f);
    result.hitPosition = {rayOrigin.x + rayDirection.x * distanceRayParameter,
                          rayOrigin.y + rayDirection.y * distanceRayParameter};

    result.distanceRaw = distanceRayParameter * rayLength;
    const float rayAngle = std::atan2(rayDirection.y, rayDirection.x);
    const float correctedScale = std::cos(NormalizeAngleRadians(rayAngle - playerDirectionAngleRadians));
    result.distanceCorrected = std::max(kMinWallDistance, result.distanceRaw * correctedScale);

    return result;
}

void Raycaster::RenderWalls(const Map& map, const Player& player, Framebuffer& framebuffer,
                            const RaycasterRenderParams& params, std::vector<RaycastHit>& outHits) {
    outHits.clear();

    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    if (width <= 0 || height <= 0) {
        return;
    }

    outHits.resize(static_cast<std::size_t>(width));
    const CameraBasis camera = MakeCameraBasis(player.directionAngleRadians, params.fieldOfViewRadians);
    RenderFloorAndCeiling(map, player.position, camera, framebuffer, params);

    for (int x = 0; x < width; ++x) {
        const float cameraX = (2.0f * static_cast<float>(x) / static_cast<float>(width)) - 1.0f;
        const Vec2 rayDirection{camera.forward.x + camera.plane.x * cameraX,
                                camera.forward.y + camera.plane.y * cameraX};

        const RaycastHit hit = CastRay(map, player.position, rayDirection, player.directionAngleRadians);
        outHits[static_cast<std::size_t>(x)] = hit;
        if (!hit.hit) {
            continue;
        }

        const Texture* texture = ResolveWallTexture(map, params, hit);
        if (!IsDrawable(texture)) {
            continue;
        }
        DrawWallColumn(framebuffer, x, hit.distanceCorrected, *texture, WallTextureU(hit, rayDirection),
                       WallShade(map, hit, rayDirection));
    }
}
