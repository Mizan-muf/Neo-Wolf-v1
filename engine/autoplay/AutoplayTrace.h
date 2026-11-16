#pragma once

// The event log of an autoplay run (neo_wolf_autoplay --trace, docs/TESTING.md "Autoplay bot").
// Events are found by comparing the engine's state after each Gameplay frame with a snapshot
// taken before it; nothing in the engine reports them. Within one frame the lines follow the
// engine's own order: the player's shot, pickups and keys, triggers, doors, the exit switch,
// then the enemy AI (state changes, hits on the player). Entities are listed in entity order.
// Only game time, frame numbers, ids and cells appear: two runs of a build give the same text.

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "engine/world/Entity.h"

class Engine;
struct AutoplayLevelReport;

// Used by RunAutoplaySession. Appends the log to `out`; with a null `out` it records nothing.
class TraceRecorder {
public:
    explicit TraceRecorder(std::string* out);

    // Before the first frame of a level: the snapshot every later frame is compared with.
    void BeginLevel(const Engine& engine, int levelNumber);

    // After an Engine::Update that started in Gameplay; `frame` counts this level's frames.
    void AfterFrame(const Engine& engine, std::uint64_t frame);

    // When the level ended: its result line.
    void EndLevel(const Engine& engine, const AutoplayLevelReport& report);

private:
    struct Snap {
        int health = 0;
        EnemyState enemyState = EnemyState::Idle;
        float attackCooldown = 0.0f;
        bool collected = false;
        bool doorOpen = false;
        bool triggerActivated = false;
    };

    // 1. The player's shot (it comes first in a frame): health lost by an enemy.
    void TraceHitsDealt(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities);

    // 2. Pickups (key cards as "key" lines).
    void TracePickups(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities);

    // 3. Triggers.
    void TraceTriggers(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities);

    // 4. Doors; "unlocked" the first time a locked door opens.
    void TraceDoors(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities);

    // 5. The exit switch: the frame ends there (no enemy AI in that frame). True when it was used.
    bool TraceExit(const Engine& engine, std::uint64_t frame);

    // 6a. Enemy AI state changes.
    void TraceEnemyStates(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities);

    // 6b. Attacks on the player, by enemy where it can be told.
    void TraceHitsTaken(const Engine& engine, std::uint64_t frame, const std::vector<Entity>& entities, bool exited);

    // Remembers the state every entity is in now, for the next frame's comparison.
    void Snapshot(const Engine& engine);
    // The snapshot of entity `id`; null for an entity that did not exist before this frame.
    const Snap* Find(int id) const;
    // Appends one event line with the "L<level> t=<seconds> f=<frame> " prefix.
    void Line(const Engine& engine, std::uint64_t frame, const std::string& event);

    std::string* out_ = nullptr;
    int levelNumber_ = 0;
    std::map<int, Snap> snaps_;
    std::vector<int> unlockedDoors_;
    int damageTaken_ = 0;
    double levelTime_ = 0.0;
};
