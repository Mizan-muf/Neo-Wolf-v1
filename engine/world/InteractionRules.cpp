#include "engine/world/InteractionRules.h"

#include <algorithm>
#include <cmath>

#include "engine/render/Raycaster.h"
#include "engine/world/MovementRules.h"

void SyncPlayerEntity(EntityManager& entityManager, const Player& player) {
    for (Entity& entity : entityManager.Entities()) {
        if (entity.type == EntityType::Player) {
            entity.position = player.position;
            break;
        }
    }
}

void UpdateEntityInteractions(
    EntityManager& entityManager,
    Map& map,
    const Player& player,
    int& playerHealth,
    int& playerAmmo,
    int maxHealth,
    int maxAmmo,
    bool interactPressed,
    GameplayEvents& outEvents) {
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

                outEvents.pickupCollected = true;
                outEvents.pickupType = entity.pickupType;
                outEvents.pickupValue = entity.pickupValue;

                if (entity.pickupType == PickupType::Health) {
                    playerHealth = std::min(maxHealth, playerHealth + entity.pickupValue);
                } else if (entity.pickupType == PickupType::Ammo) {
                    playerAmmo = std::min(maxAmmo, playerAmmo + entity.pickupValue);
                }
            }
            continue;
        }

        if (entity.type == EntityType::Trigger && !entity.triggerActivated) {
            if (DistanceSquared(entity.position, player.position) <= kTriggerRadiusSq) {
                entity.triggerActivated = true;
                outEvents.triggerActivated = true;
                outEvents.triggerId = entity.id;
            }
            continue;
        }

        if (entity.type == EntityType::Door && interactPressed) {
            if (DistanceSquared(entity.position, player.position) <= kDoorInteractRadiusSq) {
                if (entity.doorOpen) {
                    if (!IsCircleOverlappingCell(player.position, player.collisionRadius, entity.mapX, entity.mapY)) {
                        entity.doorOpen = false;
                        map.SetCell(entity.mapX, entity.mapY, TileType::Door);
                        outEvents.doorToggled = true;
                        outEvents.doorOpened = false;
                        outEvents.doorMapX = entity.mapX;
                        outEvents.doorMapY = entity.mapY;
                    }
                } else {
                    entity.doorOpen = true;
                    map.SetCell(entity.mapX, entity.mapY, TileType::Empty);
                    outEvents.doorToggled = true;
                    outEvents.doorOpened = true;
                    outEvents.doorMapX = entity.mapX;
                    outEvents.doorMapY = entity.mapY;
                }
            }
        }
    }
}

void UpdateEnemyAi(
    EntityManager& entityManager,
    const Map& map,
    const Player& player,
    int& playerHealth,
    float deltaSeconds,
    GameplayEvents& outEvents) {
    for (Entity& entity : entityManager.Entities()) {
        if (!entity.active || entity.type != EntityType::Enemy || entity.enemyState == EnemyState::Dead) {
            continue;
        }

        entity.attackCooldown = std::max(0.0f, entity.attackCooldown - deltaSeconds);

        const Vec2 toPlayer = player.position - entity.position;
        const float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;
        const float dist = std::sqrt(distSq);

        if (entity.enemyState == EnemyState::Idle) {
            if (dist <= entity.alertRadius) {
                entity.enemyState = EnemyState::Chase;
                outEvents.enemyAlerted = true;
                outEvents.alertedEnemyId = entity.id;
            }
        }

        if (entity.enemyState == EnemyState::Alert || entity.enemyState == EnemyState::Chase) {
            if (dist > entity.attackRadius && dist > 1e-4f) {
                const Vec2 dir{toPlayer.x / dist, toPlayer.y / dist};
                const Vec2 move = dir * (entity.moveSpeed * deltaSeconds);
                entity.position = MoveWithSliding(map, entity.position, move, 0.25f);
            }

            // Check attack range
            const Vec2 updatedToPlayer = player.position - entity.position;
            const float updatedDist = std::sqrt(updatedToPlayer.x * updatedToPlayer.x + updatedToPlayer.y * updatedToPlayer.y);
            if (updatedDist <= entity.attackRadius) {
                if (entity.attackCooldown <= 0.0f) {
                    playerHealth = std::max(0, playerHealth - entity.damage);
                    entity.attackCooldown = entity.attackRate;
                    outEvents.playerDamaged = true;
                    outEvents.playerDamageAmount = entity.damage;
                    if (playerHealth <= 0) {
                        outEvents.playerDied = true;
                    }
                }
            }
        }
    }
}

bool PerformPlayerAttack(
    EntityManager& entityManager,
    const Map& map,
    const Player& player,
    int& playerAmmo,
    int damage,
    float maxRange,
    GameplayEvents& outEvents) {
    if (playerAmmo <= 0) {
        return false;
    }

    playerAmmo -= 1;
    outEvents.weaponFired = true;

    const Vec2 playerDir{
        std::cos(player.directionAngleRadians),
        std::sin(player.directionAngleRadians)
    };

    const RaycastHit wallHit = Raycaster::CastRay(map, player.position, playerDir, player.directionAngleRadians);
    float maxDistance = (wallHit.hit && wallHit.distanceRaw > 0.0f) ? wallHit.distanceRaw : maxRange;
    if (maxDistance > maxRange) {
        maxDistance = maxRange;
    }

    Entity* closestEnemy = nullptr;
    float closestDist = maxDistance;
    constexpr float kEnemyHitRadiusSq = 0.45f * 0.45f;

    for (Entity& entity : entityManager.Entities()) {
        if (!entity.active || entity.type != EntityType::Enemy || entity.enemyState == EnemyState::Dead) {
            continue;
        }

        const Vec2 toEnemy = entity.position - player.position;
        const float proj = toEnemy.x * playerDir.x + toEnemy.y * playerDir.y;
        if (proj <= 0.0f || proj >= closestDist) {
            continue;
        }

        const Vec2 closestPoint = player.position + playerDir * proj;
        const float perpDistSq = DistanceSquared(entity.position, closestPoint);
        if (perpDistSq <= kEnemyHitRadiusSq) {
            closestEnemy = &entity;
            closestDist = proj;
        }
    }

    if (closestEnemy != nullptr) {
        closestEnemy->health = std::max(0, closestEnemy->health - damage);
        closestEnemy->enemyState = EnemyState::Chase; // alert on shot
        outEvents.attackHit = true;
        outEvents.enemyDamaged = true;
        outEvents.damagedEnemyId = closestEnemy->id;
        outEvents.enemyDamageAmount = damage;
        outEvents.hitEnemyId = closestEnemy->id;
        outEvents.hitPosition = player.position + playerDir * closestDist;

        if (closestEnemy->health <= 0) {
            closestEnemy->enemyState = EnemyState::Dead;
            closestEnemy->active = false;
            closestEnemy->visible = false;
            outEvents.enemyDied = true;
            outEvents.deadEnemyId = closestEnemy->id;
        }
        return true;
    }

    outEvents.hitPosition = player.position + playerDir * maxDistance;
    return true;
}
