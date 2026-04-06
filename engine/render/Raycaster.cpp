#include "engine/render/Raycaster.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "engine/assets/Texture.h"
#include "engine/core/Color.h"
#include "engine/core/MathUtils.h"
#include "engine/render/Framebuffer.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

namespace {
float SafeInverse(float value) {
    if (std::fabs(value) < 1e-6f) {
        return std::numeric_limits<float>::infinity();
    }
    return 1.0f / value;
}

const Texture* ResolveTextureForTile(const RaycasterRenderParams& params, TileType tileType) {
    switch (tileType) {
    case TileType::Wall:
        return params.wallTexture != nullptr ? params.wallTexture : params.missingTexture;
    case TileType::Door:
        return params.doorTexture != nullptr ? params.doorTexture : params.missingTexture;
    case TileType::WallAltA:
        return params.wallAltATexture != nullptr ? params.wallAltATexture : params.missingTexture;
    case TileType::WallAltB:
        return params.wallAltBTexture != nullptr ? params.wallAltBTexture : params.missingTexture;
    default:
        return params.missingTexture;
    }
}

std::uint32_t DarkenColor(std::uint32_t argbColor, float factor) {
    factor = Clamp(factor, 0.0f, 1.0f);
    const std::uint8_t a = static_cast<std::uint8_t>((argbColor >> 24) & 0xFFu);
    const std::uint8_t r = static_cast<std::uint8_t>((argbColor >> 16) & 0xFFu);
    const std::uint8_t g = static_cast<std::uint8_t>((argbColor >> 8) & 0xFFu);
    const std::uint8_t b = static_cast<std::uint8_t>(argbColor & 0xFFu);
    return PackARGB32(MakeColor(
        static_cast<std::uint8_t>(static_cast<float>(r) * factor),
        static_cast<std::uint8_t>(static_cast<float>(g) * factor),
        static_cast<std::uint8_t>(static_cast<float>(b) * factor),
        a));
}

float SampleMapLight(const Map& map, float worldX, float worldY) {
    const int cellX = static_cast<int>(std::floor(worldX));
    const int cellY = static_cast<int>(std::floor(worldY));
    return map.GetLightLevel(cellX, cellY);
}
} // namespace

RaycastHit Raycaster::CastRay(
    const Map& map,
    const Vec2& rayOrigin,
    const Vec2& rayDirection,
    float playerDirectionAngleRadians) {
    RaycastHit result{};

    if (map.Width() <= 0 || map.Height() <= 0) {
        return result;
    }

    const float rayLength = std::sqrt(rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y);
    if (rayLength < 1e-6f) {
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
    result.hitPosition = {
        rayOrigin.x + rayDirection.x * distanceRayParameter,
        rayOrigin.y + rayDirection.y * distanceRayParameter};

    result.distanceRaw = distanceRayParameter * rayLength;
    const float rayAngle = std::atan2(rayDirection.y, rayDirection.x);
    const float correctedScale = std::cos(NormalizeAngleRadians(rayAngle - playerDirectionAngleRadians));
    result.distanceCorrected = std::max(0.0001f, result.distanceRaw * correctedScale);

    return result;
}

void Raycaster::RenderWalls(
    const Map& map,
    const Player& player,
    Framebuffer& framebuffer,
    const RaycasterRenderParams& params,
    std::vector<RaycastHit>& outHits) {
    outHits.clear();

    const int width = framebuffer.Width();
    const int height = framebuffer.Height();
    if (width <= 0 || height <= 0) {
        return;
    }

    const int horizon = height / 2;
    outHits.resize(static_cast<std::size_t>(width));
    const float halfFov = params.fieldOfViewRadians * 0.5f;
    const float planeScale = std::tan(halfFov);

    const Vec2 forward{std::cos(player.directionAngleRadians), std::sin(player.directionAngleRadians)};
    const Vec2 cameraPlane{-forward.y * planeScale, forward.x * planeScale};
    const Vec2 rayDirLeft{forward.x - cameraPlane.x, forward.y - cameraPlane.y};
    const Vec2 rayDirRight{forward.x + cameraPlane.x, forward.y + cameraPlane.y};

    const Texture* floorTexture =
        (params.floorTexture != nullptr && params.floorTexture->Width() > 0 && params.floorTexture->Height() > 0)
            ? params.floorTexture
            : params.missingTexture;
    const Texture* ceilingTexture =
        (params.ceilingTexture != nullptr && params.ceilingTexture->Width() > 0 &&
         params.ceilingTexture->Height() > 0)
            ? params.ceilingTexture
            : params.missingTexture;

    const float cameraZ = 0.5f * static_cast<float>(height);
    for (int y = horizon + 1; y < height; ++y) {
        const int mirroredY = height - y - 1;
        const float p = static_cast<float>(y - horizon);
        if (p <= 0.0001f) {
            continue;
        }

        const float rowDistance = cameraZ / p;
        const float stepX = rowDistance * (rayDirRight.x - rayDirLeft.x) / static_cast<float>(width);
        const float stepY = rowDistance * (rayDirRight.y - rayDirLeft.y) / static_cast<float>(width);

        float worldX = player.position.x + rowDistance * rayDirLeft.x;
        float worldY = player.position.y + rowDistance * rayDirLeft.y;
        const float distanceShade = Clamp(1.0f / (1.0f + rowDistance * 0.13f), 0.28f, 1.0f);

        for (int x = 0; x < width; ++x) {
            const float mapLight = SampleMapLight(map, worldX, worldY);
            const float light = Clamp(distanceShade * mapLight, 0.20f, 1.0f);
            if (floorTexture != nullptr) {
                std::uint32_t floorColor = floorTexture->SampleNearest(worldX, worldY);
                framebuffer.PutPixel(x, y, DarkenColor(floorColor, light));
            }
            if (ceilingTexture != nullptr && mirroredY >= 0 && mirroredY < height) {
                std::uint32_t ceilingColor = ceilingTexture->SampleNearest(worldX, worldY);
                framebuffer.PutPixel(x, mirroredY, DarkenColor(ceilingColor, light));
            }

            worldX += stepX;
            worldY += stepY;
        }
    }

    for (int x = 0; x < width; ++x) {
        const float cameraX = (2.0f * static_cast<float>(x) / static_cast<float>(width)) - 1.0f;
        const Vec2 rayDirection{
            forward.x + cameraPlane.x * cameraX,
            forward.y + cameraPlane.y * cameraX};

        RaycastHit hit = CastRay(map, player.position, rayDirection, player.directionAngleRadians);
        outHits[static_cast<std::size_t>(x)] = hit;
        if (!hit.hit) {
            continue;
        }

        const int projectedWallHeight =
            std::max(1, static_cast<int>(std::round(static_cast<float>(height) / hit.distanceCorrected)));
        const int yStart = (height - projectedWallHeight) / 2;
        const int yEnd = yStart + projectedWallHeight - 1;

        const Texture* texture = ResolveTextureForTile(params, hit.tileType);
        if (texture == nullptr || texture->Width() <= 0 || texture->Height() <= 0) {
            continue;
        }

        float wallU = (hit.side == RayHitSide::Vertical) ? hit.hitPosition.y : hit.hitPosition.x;
        wallU = wallU - std::floor(wallU);
        if ((hit.side == RayHitSide::Vertical && rayDirection.x > 0.0f) ||
            (hit.side == RayHitSide::Horizontal && rayDirection.y < 0.0f)) {
            wallU = 1.0f - wallU;
        }

        const float sideShade = (hit.side == RayHitSide::Vertical) ? 1.0f : 0.72f;
        const float distanceShade = Clamp(1.0f / (1.0f + hit.distanceCorrected * 0.11f), 0.35f, 1.0f);
        const float mapLight = map.GetLightLevel(hit.mapX, hit.mapY);
        const float wallShade = Clamp(sideShade * distanceShade * mapLight, 0.18f, 1.0f);
        for (int y = yStart; y <= yEnd; ++y) {
            const float wallV =
                (projectedWallHeight <= 1) ? 0.0f : static_cast<float>(y - yStart) / static_cast<float>(projectedWallHeight - 1);
            std::uint32_t sampled = texture->SampleNearest(wallU, wallV);
            sampled = DarkenColor(sampled, wallShade);
            framebuffer.PutPixel(x, y, sampled);
        }
    }
}
