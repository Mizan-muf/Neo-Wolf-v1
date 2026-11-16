#pragma once

// DDA raycaster: CastRay walks one ray through the map grid to the first solid cell (also
// used for line of sight and shots); RenderWalls draws the textured floor, ceiling and walls
// and returns the per-column hits, whose distances are the depth buffer for sprites.

#include <array>
#include <cstdint>
#include <vector>

#include "engine/core/Vec2.h"
#include "engine/world/Map.h"

class Framebuffer;
class Texture;
struct Player;

enum class RayHitSide : std::uint8_t { Vertical = 0, Horizontal };

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

// The textures RenderWalls draws with. The projection (field of view) comes from the
// framebuffer's size (RenderMath.h).
struct RaycasterRenderParams {
    const Texture* wallTexture = nullptr;
    const Texture* wallAltATexture = nullptr;
    const Texture* wallAltBTexture = nullptr;
    const Texture* doorTexture = nullptr;
    // Closed doors with a lock (Map::GetDoorLock), indexed by KeyColorIndex: red, blue, gold.
    // A null entry falls back to doorTexture.
    std::array<const Texture*, kKeyColorCount> lockedDoorTextures{};
    const Texture* exitTexture = nullptr;
    const Texture* floorTexture = nullptr;
    const Texture* ceilingTexture = nullptr;
    const Texture* missingTexture = nullptr;
};

class Raycaster {
public:
    static RaycastHit CastRay(const Map& map, const Vec2& rayOrigin, const Vec2& rayDirection,
                              float playerDirectionAngleRadians);

    static void RenderWalls(const Map& map, const Player& player, Framebuffer& framebuffer,
                            const RaycasterRenderParams& params, std::vector<RaycastHit>& outHits);
};
