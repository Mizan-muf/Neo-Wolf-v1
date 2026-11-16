#pragma once

// The player: position, view direction, speeds and collision radius.

#include "engine/core/Vec2.h"
#include "engine/world/GameplayDefinitions.h"

// Position and view come from the level's player placement, speeds and radius from
// gameplay.json (BuildLevelFromPlacements). The initialisers are fallbacks for code that
// creates a Player directly (C API before Init, tests).
struct Player {
    Vec2 position{2.5f, 2.5f};
    float directionAngleRadians = 0.0f;
    float moveSpeed = kDefaultPlayerDefinition.moveSpeed;
    float turnSpeedRadians = kDefaultPlayerDefinition.turnSpeed;
    float collisionRadius = kDefaultPlayerDefinition.collisionRadius;
};
