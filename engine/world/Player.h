#pragma once

#include "engine/core/Vec2.h"

struct Player {
    Vec2 position{2.5f, 2.5f};
    float directionAngleRadians = 0.0f;
    float moveSpeed = 3.0f;
    float turnSpeedRadians = 1.8f;
    float collisionRadius = 0.22f;
};
