#pragma once

#include "engine/world/Entity.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

void SyncPlayerEntity(EntityManager& entityManager, const Player& player);
void UpdateEntityInteractions(EntityManager& entityManager, Map& map, const Player& player, bool interactPressed);
