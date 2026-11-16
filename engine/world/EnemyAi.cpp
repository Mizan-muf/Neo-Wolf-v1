#include "engine/world/EnemyAi.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "engine/world/MovementRules.h"
#include "engine/world/Navigation.h"

namespace {
constexpr float kWaypointReached = 0.12f;  // map cells from a cell centre

void StartChase(Entity& enemy, GameplayEvents& events) {
    enemy.enemyState = EnemyState::Chase;
    enemy.lostTimer = 0.0f;
    enemy.repathTimer = 0.0f;
    enemy.stuckTimer = 0.0f;
    events.enemyAlerted = true;
    events.alertedEnemyId = enemy.id;
}

// Idle -> Alert: remembers where the player is (or the shot came from) and waits its reaction
// time; with no reaction time it chases at once.
void Notice(Entity& enemy, const Vec2& where, GameplayEvents& events) {
    enemy.lastKnownPlayerPosition = where;
    enemy.enemyState = EnemyState::Alert;
    enemy.stateTimer = enemy.reactionTime;
    if (enemy.stateTimer <= 0.0f) {
        StartChase(enemy, events);
    }
}

// Moves `enemy` towards `target`, at most moveSpeed * dt, stopping `stopDistance` short of it.
// Goes straight when a body of kEnemyRadius fits along the line; otherwise follows a grid path
// (recomputed at most every kEnemyRepathInterval seconds), steering for cell centres.
void MoveTowards(Entity& enemy, const Map& map, const Vec2& target, float stopDistance, float dt) {
    float step = enemy.moveSpeed * dt;
    if (step <= 0.0f) {
        return;
    }

    Vec2 waypoint = target;
    float stopAt = stopDistance;
    if (HasClearCorridor(map, enemy.position, target, kEnemyRadius)) {
        enemy.path.clear();
        enemy.pathGoal = Vec2i{-1, -1};
    } else {
        stopAt = 0.0f;  // cell centres are walked to exactly
        const Vec2i goal = CellOf(target);
        const bool stale = enemy.path.empty() || enemy.pathGoal != goal;
        if (stale && enemy.repathTimer <= 0.0f) {
            FindPath(map, CellOf(enemy.position), goal, enemy.path);
            enemy.pathGoal = goal;
            enemy.repathTimer = kEnemyRepathInterval;
            enemy.stuckTimer = 0.0f;
        }
        // Drop reached waypoints; skip one ahead when the next-but-one is in plain view.
        while (!enemy.path.empty() && Length(CellCenter(enemy.path.front()) - enemy.position) < kWaypointReached) {
            enemy.path.erase(enemy.path.begin());
        }
        if (enemy.path.size() >= 2 && HasClearCorridor(map, enemy.position, CellCenter(enemy.path[1]), kEnemyRadius)) {
            enemy.path.erase(enemy.path.begin());
        }
        if (enemy.path.empty()) {
            return;  // no way there (e.g. a closed door): wait; Chase turns into Search
        }
        waypoint = CellCenter(enemy.path.front());
    }

    const Vec2 to = waypoint - enemy.position;
    const float distance = Length(to);
    step = std::min(step, distance - stopAt);
    if (step <= 1e-4f) {
        return;
    }
    const Vec2 before = enemy.position;
    enemy.position = MoveWithSliding(map, enemy.position, Vec2{to.x / distance * step, to.y / distance * step}, kEnemyRadius);

    // Wedged (e.g. a door closed on the path): forget the path and search again.
    if (Length(enemy.position - before) < step * 0.2f) {
        enemy.stuckTimer += dt;
        if (enemy.stuckTimer >= kEnemyStuckSeconds) {
            enemy.path.clear();
            enemy.repathTimer = 0.0f;
            enemy.stuckTimer = 0.0f;
        }
    } else {
        enemy.stuckTimer = 0.0f;
    }
}

void Attack(Entity& enemy, int& playerHealth, GameplayEvents& events) {
    if (enemy.attackCooldown > 0.0f) {
        return;
    }
    playerHealth = std::max(0, playerHealth - enemy.damage);
    enemy.attackCooldown = enemy.attackRate;
    events.playerDamaged = true;
    events.playerDamageAmount = enemy.damage;
    if (playerHealth <= 0) {
        events.playerDied = true;
    }
}

bool Hears(const Entity& enemy, const Map& map, const std::vector<int>& soundDistances, float radius) {
    if (soundDistances.empty()) {
        return false;
    }
    const Vec2i cell = CellOf(enemy.position);
    if (!map.IsInsideMap(cell.x, cell.y)) {
        return false;
    }
    const int distance = soundDistances[static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(map.Width()) +
                                        static_cast<std::size_t>(cell.x)];
    return distance >= 0 && static_cast<float>(distance) <= radius;
}

void UpdateEnemy(Entity& enemy, const Map& map, const Player& player, int& playerHealth, float dt,
                 const NoiseEvent* noise, const std::vector<int>& soundDistances, GameplayEvents& events) {
    enemy.attackCooldown = std::max(0.0f, enemy.attackCooldown - dt);
    enemy.hitFlashTimer = std::max(0.0f, enemy.hitFlashTimer - dt);
    enemy.repathTimer = std::max(0.0f, enemy.repathTimer - dt);

    const float distance = Length(player.position - enemy.position);
    const bool heard = noise != nullptr && noise->active && Hears(enemy, map, soundDistances, noise->radius);

    if (enemy.enemyState == EnemyState::Idle) {
        if (distance <= enemy.alertRadius && HasLineOfSight(map, enemy.position, player.position)) {
            Notice(enemy, player.position, events);
        } else if (heard) {
            Notice(enemy, noise->position, events);
        }
        return;
    }

    // Alerted states see further than an idle enemy notices.
    const float sightRange = std::max(enemy.alertRadius * kEnemyChaseSightFactor, enemy.attackRadius);
    const bool sees = distance <= sightRange && HasLineOfSight(map, enemy.position, player.position);
    if (sees) {
        enemy.lastKnownPlayerPosition = player.position;
        enemy.lostTimer = 0.0f;
    } else {
        enemy.lostTimer += dt;
        if (heard) {
            enemy.lastKnownPlayerPosition = noise->position;
        }
    }

    switch (enemy.enemyState) {
    case EnemyState::Alert:
        enemy.stateTimer -= dt;
        if (enemy.stateTimer <= 0.0f) {
            StartChase(enemy, events);
        }
        return;
    case EnemyState::Search:
        if (sees || heard) {
            StartChase(enemy, events);
            break;  // continue as Chase below
        }
        enemy.stateTimer -= dt;
        if (enemy.stateTimer <= 0.0f) {
            enemy.enemyState = EnemyState::Idle;
            enemy.path.clear();
            return;
        }
        MoveTowards(enemy, map, enemy.lastKnownPlayerPosition, 0.0f, dt);
        return;
    case EnemyState::Attack:
        if (sees && distance <= enemy.attackRadius + kEnemyAttackHysteresis) {
            Attack(enemy, playerHealth, events);
            return;
        }
        enemy.enemyState = EnemyState::Chase;
        break;
    default:
        break;
    }

    // Chase.
    if (sees && distance <= enemy.attackRadius) {
        enemy.enemyState = EnemyState::Attack;
        enemy.path.clear();
        Attack(enemy, playerHealth, events);
        return;
    }
    if (enemy.lostTimer >= kEnemyLoseTargetSeconds) {
        enemy.enemyState = EnemyState::Search;
        enemy.stateTimer = kEnemySearchSeconds;
    }
    // Stop short of the player's body; walk all the way to a last known position.
    const float stopDistance = sees ? std::min(enemy.attackRadius * 0.9f, player.collisionRadius + kEnemyRadius) : 0.0f;
    MoveTowards(enemy, map, sees ? player.position : enemy.lastKnownPlayerPosition, stopDistance, dt);
}
} // namespace

bool IsEnemyAlive(const Entity& entity) {
    return entity.type == EntityType::Enemy && entity.active && entity.enemyState != EnemyState::Dead;
}

bool IsEnemyCorpse(const Entity& entity) {
    return entity.type == EntityType::Enemy && entity.enemyState == EnemyState::Dead;
}

void UpdateEnemyAi(
    EntityManager& entityManager,
    const Map& map,
    const Player& player,
    int& playerHealth,
    float deltaSeconds,
    GameplayEvents& outEvents,
    const NoiseEvent* noise) {
    std::vector<int> soundDistances;
    if (noise != nullptr && noise->active && noise->radius >= 0.0f) {
        soundDistances = ComputeSoundDistances(map, CellOf(noise->position), static_cast<int>(std::ceil(noise->radius)));
    }
    for (Entity& entity : entityManager.Entities()) {
        if (IsEnemyAlive(entity)) {
            UpdateEnemy(entity, map, player, playerHealth, deltaSeconds, noise, soundDistances, outEvents);
        }
    }
    SeparateEnemies(entityManager, map);
    SeparateEnemiesFromPlayer(entityManager, map, player);
}

bool AlertEnemyByDamage(Entity& enemy, const Vec2& playerPosition) {
    enemy.hitFlashTimer = kEnemyHitFlashSeconds;
    enemy.lastKnownPlayerPosition = playerPosition;
    enemy.lostTimer = 0.0f;
    const bool wasUnaware = enemy.enemyState == EnemyState::Idle || enemy.enemyState == EnemyState::Alert ||
                            enemy.enemyState == EnemyState::Search;
    if (wasUnaware) {
        enemy.enemyState = EnemyState::Chase;
        enemy.repathTimer = 0.0f;
    }
    return enemy.enemyState == EnemyState::Chase && wasUnaware;
}

void SeparateEnemies(EntityManager& entityManager, const Map& map) {
    std::vector<Entity*> alive;
    for (Entity& entity : entityManager.Entities()) {
        if (IsEnemyAlive(entity)) {
            alive.push_back(&entity);
        }
    }
    constexpr float kMinDistance = 2.0f * kEnemyRadius;
    for (std::size_t i = 0; i < alive.size(); ++i) {
        for (std::size_t j = i + 1; j < alive.size(); ++j) {
            Entity& a = *alive[i];
            Entity& b = *alive[j];
            const Vec2 delta = b.position - a.position;
            const float distance = Length(delta);
            if (distance >= kMinDistance) {
                continue;
            }
            if (distance < 1e-4f) {
                // Exactly on top of each other: split along a fixed axis (deterministic).
                const float push = kMinDistance * 0.5f;
                a.position = MoveWithSliding(map, a.position, Vec2{-push, 0.0f}, kEnemyRadius);
                b.position = MoveWithSliding(map, b.position, Vec2{push, 0.0f}, kEnemyRadius);
                continue;
            }
            const float push = (kMinDistance - distance) * 0.5f;
            const Vec2 offset{delta.x / distance * push, delta.y / distance * push};
            a.position = MoveWithSliding(map, a.position, Vec2{-offset.x, -offset.y}, kEnemyRadius);
            b.position = MoveWithSliding(map, b.position, offset, kEnemyRadius);
        }
    }
}

void SeparateEnemiesFromPlayer(EntityManager& entityManager, const Map& map, const Player& player) {
    const float minDistance = player.collisionRadius + kEnemyRadius;
    for (Entity& enemy : entityManager.Entities()) {
        if (!IsEnemyAlive(enemy)) {
            continue;
        }
        const Vec2 delta = enemy.position - player.position;
        const float distance = Length(delta);
        if (distance >= minDistance) {
            continue;
        }
        // Exactly on the player: push along +x (deterministic), like SeparateEnemies.
        const Vec2 direction = distance < 1e-4f ? Vec2{1.0f, 0.0f} : Vec2{delta.x / distance, delta.y / distance};
        enemy.position = MoveWithSliding(map, enemy.position, direction * (minDistance - distance), kEnemyRadius);
    }
}

std::vector<BodyObstacle> LivingEnemyBodies(const EntityManager& entityManager) {
    std::vector<BodyObstacle> bodies;
    for (const Entity& entity : entityManager.Entities()) {
        if (IsEnemyAlive(entity)) {
            bodies.push_back(BodyObstacle{entity.position, kEnemyRadius});
        }
    }
    return bodies;
}
