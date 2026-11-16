#include "engine/world/InteractionRules.h"

#include <algorithm>
#include <cmath>

#include "engine/render/Raycaster.h"
#include "engine/world/EnemyAi.h"
#include "engine/world/MovementRules.h"

ExitSwitchHit FindExitSwitchInReach(const Map& map, const Player& player) {
    // Centre ray first, then pairs further out: the closest-to-centre hit wins.
    constexpr float kOffsets[] = {0.0f, -0.5f, 0.5f, -1.0f, 1.0f};
    ExitSwitchHit best{};
    for (const float offset : kOffsets) {
        const float angle = player.directionAngleRadians + offset * kExitFacingHalfAngle;
        const Vec2 direction{std::cos(angle), std::sin(angle)};
        const RaycastHit hit = Raycaster::CastRay(map, player.position, direction, angle);
        if (!hit.hit || hit.tileType != TileType::Exit || hit.distanceRaw > kExitReach) {
            continue;
        }
        best.found = true;
        best.mapX = hit.mapX;
        best.mapY = hit.mapY;
        best.distance = hit.distanceRaw;
        break;
    }
    return best;
}

void SyncPlayerEntity(EntityManager& entityManager, const Player& player) {
    for (Entity& entity : entityManager.Entities()) {
        if (entity.type == EntityType::Player) {
            entity.position = player.position;
            break;
        }
    }
}

bool IsDoorwayOccupied(const EntityManager& entityManager, const Player& player, int cellX, int cellY) {
    if (IsCircleOverlappingCell(player.position, player.collisionRadius, cellX, cellY)) {
        return true;
    }
    for (const Entity& entity : entityManager.Entities()) {
        if (IsEnemyAlive(entity) && IsCircleOverlappingCell(entity.position, kEnemyRadius, cellX, cellY)) {
            return true;
        }
    }
    return false;
}

void UpdateEntityInteractions(EntityManager& entityManager, Map& map, const Player& player, int& playerHealth,
                              int& playerAmmo, int maxHealth, int maxAmmo, bool interactPressed, KeyRing& keys,
                              GameplayEvents& outEvents) {
    constexpr float kPickupRadiusSq = 0.60f * 0.60f;
    constexpr float kTriggerRadiusSq = 0.80f * 0.80f;
    constexpr float kDoorInteractRadiusSq = 1.05f * 1.05f;

    // Interact uses the exit switch the player faces in preference to a door within reach (a
    // door only needs the player nearby, so one next to the exit would otherwise swallow the
    // press). A press that is not used on the exit goes to the doors.
    if (interactPressed) {
        const ExitSwitchHit exitHit = FindExitSwitchInReach(map, player);
        if (exitHit.found) {
            outEvents.exitActivated = true;
            outEvents.exitMapX = exitHit.mapX;
            outEvents.exitMapY = exitHit.mapY;
        }
    }
    const bool doorPress = interactPressed && !outEvents.exitActivated;

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
                } else if (entity.pickupType == PickupType::Key) {
                    keys.Add(entity.keyColor);
                    outEvents.pickupKey = entity.keyColor;
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

        if (entity.type == EntityType::Door && doorPress) {
            if (DistanceSquared(entity.position, player.position) > kDoorInteractRadiusSq) {
                continue;
            }
            outEvents.doorMapX = entity.mapX;
            outEvents.doorMapY = entity.mapY;
            if (entity.doorOpen) {
                if (IsDoorwayOccupied(entityManager, player, entity.mapX, entity.mapY)) {
                    outEvents.doorBlocked = true;
                    continue;
                }
                entity.doorOpen = false;
                map.SetCell(entity.mapX, entity.mapY, TileType::Door);
                outEvents.doorToggled = true;
                outEvents.doorOpened = false;
            } else {
                if (entity.doorLock != KeyColor::None && !keys.Has(entity.doorLock)) {
                    outEvents.doorNeedsKey = entity.doorLock;
                    continue;
                }
                entity.doorOpen = true;
                map.SetCell(entity.mapX, entity.mapY, TileType::Empty);
                outEvents.doorToggled = true;
                outEvents.doorOpened = true;
            }
        }
    }
}

void UpdateEntityInteractions(EntityManager& entityManager, Map& map, const Player& player, int& playerHealth,
                              int& playerAmmo, int maxHealth, int maxAmmo, bool interactPressed,
                              GameplayEvents& outEvents) {
    KeyRing noKeys{};
    UpdateEntityInteractions(entityManager, map, player, playerHealth, playerAmmo, maxHealth, maxAmmo, interactPressed,
                             noKeys, outEvents);
}

bool PerformPlayerAttack(EntityManager& entityManager, const Map& map, const Player& player, int& playerAmmo,
                         int damage, float maxRange, GameplayEvents& outEvents) {
    if (playerAmmo <= 0) {
        return false;
    }

    playerAmmo -= 1;
    outEvents.weaponFired = true;

    const Vec2 playerDir{std::cos(player.directionAngleRadians), std::sin(player.directionAngleRadians)};

    const RaycastHit wallHit = Raycaster::CastRay(map, player.position, playerDir, player.directionAngleRadians);
    float maxDistance = (wallHit.hit && wallHit.distanceRaw > 0.0f) ? wallHit.distanceRaw : maxRange;
    if (maxDistance > maxRange) {
        maxDistance = maxRange;
    }

    Entity* closestEnemy = nullptr;
    float closestDist = maxDistance;
    constexpr float kEnemyHitRadiusSq = 0.45f * 0.45f;

    for (Entity& entity : entityManager.Entities()) {
        if (!IsEnemyAlive(entity)) {
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
        if (AlertEnemyByDamage(*closestEnemy, player.position)) {
            outEvents.enemyAlerted = true;
            outEvents.alertedEnemyId = closestEnemy->id;
        }
        outEvents.attackHit = true;
        outEvents.enemyDamaged = true;
        outEvents.damagedEnemyId = closestEnemy->id;
        outEvents.enemyDamageAmount = damage;
        outEvents.hitEnemyId = closestEnemy->id;
        outEvents.hitPosition = player.position + playerDir * closestDist;

        if (closestEnemy->health <= 0) {
            // Dead: inactive (no AI, no collisions, cannot be shot) but still visible, drawn
            // with the corpse sprite (SyncSpritesWithEntities).
            closestEnemy->enemyState = EnemyState::Dead;
            closestEnemy->active = false;
            closestEnemy->path.clear();
            outEvents.enemyDied = true;
            outEvents.deadEnemyId = closestEnemy->id;
        }
        return true;
    }

    outEvents.hitPosition = player.position + playerDir * maxDistance;
    return true;
}
