#pragma once

#include <cstdint>

#include "engine/core/Vec2.h"

enum class SpriteType : std::uint8_t {
    Enemy = 0,
    Pickup,
    Decoration
};

struct SpriteEntity {
    Vec2 position{};
    int textureId = 0;
    int entityId = -1;
    SpriteType type = SpriteType::Decoration;
    bool active = true;
    bool visible = true;
};
