#include "engine/world/InteractionRules.h"

#include "engine/world/MovementRules.h"

void SyncPlayerEntity(EntityManager& entityManager, const Player& player) {
    for (Entity& entity : entityManager.Entities()) {
        if (entity.type == EntityType::Player) {
            entity.position = player.position;
            break;
        }
    }
}

void UpdateEntityInteractions(EntityManager& entityManager, Map& map, const Player& player, bool interactPressed) {
    constexpr float kPickupRadiusSq = 0.60f * 0.60f;
    constexpr float kTriggerRadiusSq = 0.80f * 0.80f;
    constexpr float kDoorInteractRadiusSq = 1.05f * 1.05f;

    for (Entity& entity : entityManager.Entities()) {
        if (!entity.active) {
            continue;
        }

        if (entity.type == EntityType::Pickup && !entity.collected) {
            if (DistanceSquared(entity.position, player.position) <= kPickupRadiusSq) {
                entity.collected = true;
                entity.active = false;
                entity.visible = false;
            }
            continue;
        }

        if (entity.type == EntityType::Trigger && !entity.triggerActivated) {
            if (DistanceSquared(entity.position, player.position) <= kTriggerRadiusSq) {
                entity.triggerActivated = true;
            }
            continue;
        }

        if (entity.type == EntityType::Door && interactPressed) {
            if (DistanceSquared(entity.position, player.position) <= kDoorInteractRadiusSq) {
                if (entity.doorOpen) {
                    if (!IsCircleOverlappingCell(player.position, player.collisionRadius, entity.mapX, entity.mapY)) {
                        entity.doorOpen = false;
                        map.SetCell(entity.mapX, entity.mapY, TileType::Door);
                    }
                } else {
                    entity.doorOpen = true;
                    map.SetCell(entity.mapX, entity.mapY, TileType::Empty);
                }
            }
        }
    }
}
