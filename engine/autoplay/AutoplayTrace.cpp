#include "engine/autoplay/AutoplayTrace.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "engine/autoplay/AutoplayBot.h"
#include "engine/core/Engine.h"
#include "engine/world/EnemyAi.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/KeyColor.h"
#include "engine/world/Navigation.h"

namespace {
const char* EnemyStateName(EnemyState state) {
    switch (state) {
        case EnemyState::Idle:
            return "idle";
        case EnemyState::Alert:
            return "alert";
        case EnemyState::Chase:
            return "chase";
        case EnemyState::Attack:
            return "attack";
        case EnemyState::Dead:
            return "dead";
        case EnemyState::Search:
            return "search";
    }
    return "unknown";
}

std::string TraceCell(const Vec2i& cell) {
    return std::to_string(cell.x) + "," + std::to_string(cell.y);
}

std::string TracePosition(const Vec2& position) {
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.2f,%.2f", static_cast<double>(position.x),
                  static_cast<double>(position.y));
    return buffer.data();
}

std::string TraceEnemy(const EnemyDefinitions& types, const Entity& enemy) {
    return std::string(types.Name(enemy.enemyType)) + "#" + std::to_string(enemy.id);
}

std::string HeldKeys(const KeyRing& keys) {
    std::string held;
    for (const KeyColor color : kKeyColors) {
        if (keys.Has(color)) {
            held += (held.empty() ? "" : ",") + std::string(KeyColorName(color));
        }
    }
    return held.empty() ? "none" : held;
}

}  // namespace

TraceRecorder::TraceRecorder(std::string* out) : out_(out) {
    if (out_ != nullptr) {
        *out_ +=
            "# neo_wolf_autoplay trace 1: one event per line, \"L<level> t=<game seconds> f=<frame> "
            "<event> key=value...\" (docs/TESTING.md, Autoplay bot)\n";
    }
}

// Before the first frame of a level: the snapshot every later frame is compared with.
void TraceRecorder::BeginLevel(const Engine& engine, int levelNumber) {
    if (out_ == nullptr) {
        return;
    }
    levelNumber_ = levelNumber;
    unlockedDoors_.clear();
    int enemies = 0;
    int pickups = 0;
    for (const Entity& entity : engine.entityManager_.Entities()) {
        enemies += entity.type == EntityType::Enemy ? 1 : 0;
        pickups += entity.type == EntityType::Pickup ? 1 : 0;
    }
    const CampaignLevel* level = engine.CurrentCampaignLevel();
    Line(engine, 0,
         "level-start id=" + (level != nullptr ? level->id : std::string("?")) +
             " hp=" + std::to_string(engine.playerHealth_) + " ammo=" + std::to_string(engine.playerAmmo_) +
             " pos=" + TracePosition(engine.player_.position) + " enemies=" + std::to_string(enemies) +
             " pickups=" + std::to_string(pickups));
    Snapshot(engine);
}

// After an Engine::Update that started in Gameplay; `frame` counts this level's frames.
void TraceRecorder::AfterFrame(const Engine& engine, std::uint64_t frame) {
    if (out_ == nullptr) {
        return;
    }
    const std::vector<Entity>& entities = engine.entityManager_.Entities();
    TraceHitsDealt(engine, frame, entities);
    TracePickups(engine, frame, entities);
    TraceTriggers(engine, frame, entities);
    TraceDoors(engine, frame, entities);
    const bool exited = TraceExit(engine, frame);
    TraceEnemyStates(engine, frame, entities);
    TraceHitsTaken(engine, frame, entities, exited);
    Snapshot(engine);
}

void TraceRecorder::EndLevel(const Engine& engine, const AutoplayLevelReport& report) {
    if (out_ == nullptr) {
        return;
    }
    std::string line = std::string("level-end result=") + AutoplayResultName(report.result) +
                       " hp=" + std::to_string(report.healthEnd) + " ammo=" + std::to_string(report.ammoEnd) +
                       " damage=" + std::to_string(report.damageTaken) + " kills=" + std::to_string(report.kills) +
                       "/" + std::to_string(report.totalEnemies) + " pickups=" + std::to_string(report.pickups) + "/" +
                       std::to_string(report.totalPickups) + " shots=" + std::to_string(report.shotsFired);
    if (!report.reason.empty()) {
        line += " reason=\"" + report.reason + "\"";
    }
    Line(engine, report.frames, line);
}

// 1. The player's shot (it comes first in a frame): health lost by an enemy.
void TraceRecorder::TraceHitsDealt(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities) {
    for (const Entity& entity : entities) {
        const Snap* was = Find(entity.id);
        if (entity.type != EntityType::Enemy || was == nullptr || entity.health >= was->health) {
            continue;
        }
        Line(engine, frame,
             "hit-dealt target=" + TraceEnemy(engine.gameplay_.enemies, entity) +
                 " cell=" + TraceCell(CellOf(entity.position)) +
                 " damage=" + std::to_string(was->health - entity.health) + " hp=" + std::to_string(entity.health) +
                 "/" + std::to_string(entity.maxHealth) + (entity.enemyState == EnemyState::Dead ? " killed" : ""));
    }
}

// 2. Pickups (key cards as "key" lines).
void TraceRecorder::TracePickups(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities) {
    for (const Entity& entity : entities) {
        const Snap* was = Find(entity.id);
        if (entity.type != EntityType::Pickup || was == nullptr || !entity.collected || was->collected) {
            continue;
        }
        const std::string cell = TraceCell(CellOf(entity.position));
        if (entity.pickupType == PickupType::Key) {
            Line(engine, frame,
                 std::string("key colour=") + KeyColorName(entity.keyColor) + " cell=" + cell +
                     " held=" + HeldKeys(engine.keys_));
        } else {
            Line(engine, frame,
                 std::string("pickup kind=") + (entity.pickupType == PickupType::Health ? "health" : "ammo") +
                     " value=" + std::to_string(entity.pickupValue) + " cell=" + cell +
                     " hp=" + std::to_string(engine.playerHealth_) + " ammo=" + std::to_string(engine.playerAmmo_));
        }
    }
}

// 3. Triggers.
void TraceRecorder::TraceTriggers(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities) {
    for (const Entity& entity : entities) {
        const Snap* was = Find(entity.id);
        if (entity.type == EntityType::Trigger && was != nullptr && entity.triggerActivated && !was->triggerActivated) {
            Line(engine, frame,
                 "trigger id=" + std::to_string(entity.id) + " cell=" + TraceCell(Vec2i{entity.mapX, entity.mapY}) +
                     " pos=" + TracePosition(engine.player_.position));
        }
    }
}

// 4. Doors; "unlocked" the first time a locked door opens.
void TraceRecorder::TraceDoors(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities) {
    for (const Entity& entity : entities) {
        const Snap* was = Find(entity.id);
        if (entity.type != EntityType::Door || was == nullptr || entity.doorOpen == was->doorOpen) {
            continue;
        }
        std::string line = std::string(entity.doorOpen ? "door-open" : "door-close") +
                           " cell=" + TraceCell(Vec2i{entity.mapX, entity.mapY}) +
                           " lock=" + (entity.doorLock == KeyColor::None ? "none" : KeyColorName(entity.doorLock));
        if (entity.doorOpen && entity.doorLock != KeyColor::None &&
            std::find(unlockedDoors_.begin(), unlockedDoors_.end(), entity.id) == unlockedDoors_.end()) {
            unlockedDoors_.push_back(entity.id);
            line += " unlocked";
        }
        Line(engine, frame, line);
    }
}

// 5. The exit switch: the frame ends there (no enemy AI in that frame). True when it was used.
bool TraceRecorder::TraceExit(const Engine& engine, std::uint64_t frame) {
    if (engine.GetState() != GameState::LevelComplete) {
        return false;
    }
    const ExitSwitchHit exitHit = FindExitSwitchInReach(engine.map_, engine.player_);
    Line(engine, frame,
         "exit cell=" + (exitHit.found ? TraceCell(Vec2i{exitHit.mapX, exitHit.mapY}) : "?") +
             " pos=" + TracePosition(engine.player_.position) + " hp=" + std::to_string(engine.playerHealth_) +
             " ammo=" + std::to_string(engine.playerAmmo_));
    return true;
}

// 6a. Enemy AI state changes: one line per frame and enemy, old -> new (a change and its
// reverse within one frame are not seen).
void TraceRecorder::TraceEnemyStates(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities) {
    for (const Entity& entity : entities) {
        const Snap* was = Find(entity.id);
        if (entity.type == EntityType::Enemy && was != nullptr && entity.enemyState != was->enemyState) {
            Line(engine, frame,
                 "enemy " + TraceEnemy(engine.gameplay_.enemies, entity) +
                     " cell=" + TraceCell(CellOf(entity.position)) + " " + EnemyStateName(was->enemyState) + "->" +
                     EnemyStateName(entity.enemyState));
        }
    }
}

// 6b. Attacks on the player. An enemy attacked this frame when the AI ran (the level clock
// moved and the exit was not used) and it is in Attack with its cooldown just set to
// attackRate: the cooldown counts down in every AI update and is only set back by an attack.
// Damage not explained that way is reported as "by=unknown".
void TraceRecorder::TraceHitsTaken(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities,
                                   bool exited) {
    const bool aiRan = !exited && engine.levelTimeSeconds_ != levelTime_;
    const int lost = std::max(0, engine.levelDamageTaken_ - damageTaken_);
    int health = engine.playerHealth_ + lost;  // before the attacks of this frame
    int attributed = 0;
    for (const Entity& entity : entities) {
        if (!aiRan || !IsEnemyAlive(entity) || entity.enemyState != EnemyState::Attack ||
            entity.attackCooldown != entity.attackRate) {
            continue;
        }
        const int after = std::max(0, health - entity.damage);
        attributed += health - after;
        health = after;
        Line(engine, frame,
             "hit-taken by=" + TraceEnemy(engine.gameplay_.enemies, entity) +
                 " from=" + TraceCell(CellOf(entity.position)) + " damage=" + std::to_string(entity.damage) +
                 " hp=" + std::to_string(after) + " pos=" + TracePosition(engine.player_.position));
    }
    if (lost > attributed) {
        Line(engine, frame,
             "hit-taken by=unknown damage=" + std::to_string(lost - attributed) +
                 " hp=" + std::to_string(engine.playerHealth_) + " pos=" + TracePosition(engine.player_.position));
    }
}

void TraceRecorder::Snapshot(const Engine& engine) {
    snaps_.clear();
    for (const Entity& entity : engine.entityManager_.Entities()) {
        snaps_[entity.id] = Snap{entity.health,    entity.enemyState, entity.attackCooldown,
                                 entity.collected, entity.doorOpen,   entity.triggerActivated};
    }
    damageTaken_ = engine.levelDamageTaken_;
    levelTime_ = engine.levelTimeSeconds_;
}

const TraceRecorder::Snap* TraceRecorder::Find(int id) const {
    const auto it = snaps_.find(id);
    return it == snaps_.end() ? nullptr : &it->second;
}

void TraceRecorder::Line(const Engine& engine, std::uint64_t frame, const std::string& event) {
    std::array<char, 64> prefix{};
    std::snprintf(prefix.data(), prefix.size(), "L%d t=%.3f f=%llu ", levelNumber_, engine.levelTimeSeconds_,
                  static_cast<unsigned long long>(frame));
    *out_ += prefix.data() + event + "\n";
}
