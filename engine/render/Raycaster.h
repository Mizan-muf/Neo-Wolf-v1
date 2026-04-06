#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/Vec2.h"
#include "engine/world/Map.h"

class Framebuffer;
class Texture;
struct Player;

enum class RayHitSide : std::uint8_t {
    Vertical = 0,
    Horizontal
};

struct RaycastHit {
    bool hit = false;
    float distanceRaw = 0.0f;
    float distanceCorrected = 0.0f;
    Vec2 hitPosition{};
    int mapX = -1;
    int mapY = -1;
    TileType tileType = TileType::Empty;
    RayHitSide side = RayHitSide::Vertical;
};

struct RaycasterRenderParams {
    float fieldOfViewRadians = 1.04719755f; // 60 degrees
    const Texture* wallTexture = nullptr;
    const Texture* wallAltATexture = nullptr;
    const Texture* wallAltBTexture = nullptr;
    const Texture* doorTexture = nullptr;
    const Texture* floorTexture = nullptr;
    const Texture* ceilingTexture = nullptr;
    const Texture* missingTexture = nullptr;
};

class Raycaster {
public:
    static RaycastHit CastRay(
        const Map& map,
        const Vec2& rayOrigin,
        const Vec2& rayDirection,
        float playerDirectionAngleRadians);

    static void RenderWalls(
        const Map& map,
        const Player& player,
        Framebuffer& framebuffer,
        const RaycasterRenderParams& params,
        std::vector<RaycastHit>& outHits);
};
