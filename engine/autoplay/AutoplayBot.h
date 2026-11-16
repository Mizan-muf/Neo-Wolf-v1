#pragma once

// Headless autoplay bot (Roadmap Phase 7, docs/TESTING.md "Autoplay bot").
//
// The bot plays the REAL engine: Engine::Update at a fixed time step, driven only through the
// input-injection seam (Engine::SetInputInjector), exactly like a player on the keyboard. It
// never moves the player, opens doors or changes any game state itself; it only reads the
// engine's state to decide which keys to hold.
//
// Strategy, re-planned whenever the world changes (a key, a door, a pickup, low health/ammo,
// no progress):
//   1. Health at or below AutoplayConfig::lowHealthFraction of the maximum: walk to the nearest
//      reachable health pickup, if there is one. Ammo at or below lowAmmo: the same for ammo.
//   2. Otherwise walk to the nearest reachable floor cell beside an exit switch 'X', face the
//      switch and press Interact.
//   3. If no exit switch can be reached yet: walk to the nearest reachable key card whose colour
//      opens a locked door and is not held yet (this follows the key-before-lock order), then
//      plan again.
//   4. Nothing reachable: the bot gives up with a reason (result "stuck").
// Paths are breadth-first searches over map cells (4-connected); closed doors are passable
// when they can be opened (unlocked, or the key is held). The bot walks to a closed door on its
// path and presses Interact. A living enemy in line of sight (HasLineOfSight, the engine's own
// sight test) and within weapon range is fought first: the bot stops, turns to it and fires
// until it is dead; if a ready shot stays blocked by a corner for 2 s (the enemy's centre is
// in sight, the shot's line is not), the bot ignores that enemy for 3 s and walks on. With no
// progress for AutoplayConfig::stuckSeconds the bot backs off and re-plans; after
// maxStuckReplans such attempts it gives up (result "stuck").
//
// Limits (docs/TESTING.md): no dodging or retreating, it fights where it stands; it does not
// plan around living enemies (they block the player, known issue C15) except by shooting them;
// it ignores enemies it cannot see; it does not look for pickups it does not need; it presses
// Interact near doors, which also toggles every other door within reach, as for a player.

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "engine/core/InputState.h"
#include "engine/core/Vec2.h"
#include "engine/core/Vec2i.h"
#include "engine/world/KeyColor.h"

class Engine;

struct AutoplayConfig {
    double fixedStepSeconds = 1.0 / 60.0;  // Engine::Update step
    double timeLimitSeconds = 300.0;       // per level, game time spent in Gameplay
    double stuckSeconds = 8.0;             // no progress this long: back off and re-plan
    int maxStuckReplans = 3;               // more stuck re-plans than this: give up ("stuck")
    float lowHealthFraction = 0.4f;        // detour to health at or below this share of the maximum
    int lowAmmo = 6;                       // detour to ammo at or below this
};

enum class AutoplayResult : std::uint8_t {
    NotPlayed = 0,  // --all: a level after one that failed
    Complete,       // the exit switch was used
    Died,           // health reached 0
    Timeout,        // AutoplayConfig::timeLimitSeconds of game time passed
    Stuck           // no route, or no progress; `reason` says why
};

// "not-played", "complete", "died", "timeout", "stuck".
const char* AutoplayResultName(AutoplayResult result);

struct AutoplayLevelReport {
    int levelNumber = 0;  // 1-based campaign index
    std::string id;
    std::string name;
    AutoplayResult result = AutoplayResult::NotPlayed;
    std::string reason;
    double gameSeconds = 0.0;  // time in Gameplay on this level
    std::uint64_t frames = 0;  // Engine::Update calls in Gameplay on this level
    int healthStart = 0;
    int ammoStart = 0;
    int healthEnd = 0;  // at the exit (complete), or when the run stopped
    int ammoEnd = 0;
    int damageTaken = 0;
    int kills = 0;
    int totalEnemies = 0;
    int pickups = 0;
    int totalPickups = 0;
    int shotsFired = 0;
    int doorsOpened = 0;
    int replans = 0;
    std::vector<std::string> keysCollected;  // colours, in the order they were picked up
    std::vector<std::string> keysUsed;       // colours of locked doors the bot opened
};

struct AutoplayRunReport {
    std::string campaignTitle;
    int campaignLevels = 0;
    bool wholeCampaign = false;      // --all
    bool campaignCompleted = false;  // --all: the credits screen was reached
    std::vector<AutoplayLevelReport> levels;
    bool carryOverChecked = false;  // --all with more than one level played
    std::vector<std::string> carryOverProblems;
    std::string error;  // the engine did not start, or a usage problem
    double fixedStepSeconds = 0.0;
    double timeLimitSeconds = 0.0;

    // Every requested level completed, the carry-over checks passed and nothing failed.
    bool Success() const;
};

// The per-frame controller. Install it with
//   engine.SetInputInjector([&bot](InputState& input) { bot.Fill(input); });
// and call Engine::Update. On the main menu it chooses New Game, on the level-complete screen
// it continues; in Gameplay it plays. Failed() becomes true when it gives up.
class AutoplayBot {
public:
    AutoplayBot(Engine& engine, const AutoplayConfig& config);

    void Fill(InputState& input);

    bool Failed() const { return failed_; }
    const std::string& FailureReason() const { return failureReason_; }

    // Statistics of the level being played (reset when a new level starts).
    int DoorsOpened() const { return doorsOpened_; }
    int Replans() const { return replans_; }
    const std::vector<std::string>& KeysCollected() const { return keysCollected_; }
    const std::vector<std::string>& KeysUsed() const { return keysUsed_; }
    // Counts levels started since construction (a reload of the same level counts too).
    int LevelsStarted() const { return levelsStarted_; }

private:
    enum class GoalKind : std::uint8_t { None, Exit, Key, Health, Ammo };
    static constexpr std::size_t kActionCount = static_cast<std::size_t>(InputAction::Count);

    struct DoorState {
        int id = -1;
        bool open = false;
    };

    void StartLevel();
    void PlayFrame();
    void Observe();
    bool NeedsReplan() const;
    void Plan();
    bool FightVisibleEnemy();
    void FollowPath();
    void CheckProgress();
    void Fail(const std::string& reason);

    // Breadth-first search over cells from the player's cell; fills distance_ and parent_.
    void SearchFromPlayer();
    bool IsPassable(int x, int y) const;
    bool IsReached(const Vec2i& cell) const;
    int DistanceTo(const Vec2i& cell) const;
    std::vector<Vec2i> PathTo(const Vec2i& cell) const;
    std::string DescribeBlockage() const;

    // Input helpers: hold an action this frame, or tap it (a press edge, then a release).
    void Hold(InputAction action);
    void Tap(InputAction action);
    // Turns towards `target`; returns the remaining absolute angle (radians).
    float TurnTowards(const Vec2& target);
    // Turns and walks towards `target` (walks only when roughly facing it).
    void SteerTowards(const Vec2& target);

    Engine& engine_;
    AutoplayConfig config_;
    std::uint64_t frame_ = 0;
    std::array<bool, kActionCount> want_{};
    std::array<bool, kActionCount> wasDown_{};

    // Level tracking.
    int levelsStarted_ = 0;
    int levelIndex_ = -2;
    double lastLevelTime_ = -1.0;
    bool failed_ = false;
    std::string failureReason_;

    // World snapshot used to detect changes.
    int keyMask_ = 0;
    int pickupsLeft_ = 0;
    int enemyHealthTotal_ = 0;
    std::vector<DoorState> doors_;
    bool lowHealth_ = false;
    bool lowAmmo_ = false;

    // The plan.
    bool replanRequested_ = true;
    GoalKind goal_ = GoalKind::None;
    Vec2i goalCell_{-1, -1};
    Vec2i exitCell_{-1, -1};
    int goalEntityId_ = -1;
    std::vector<Vec2i> path_;
    std::size_t next_ = 0;
    std::vector<int> distance_;
    std::vector<int> parent_;

    // Doors and the exit switch: no repeated presses while the last one takes effect.
    double interactCooldown_ = 0.0;

    // Progress and stuck handling.
    double bestPotential_ = 0.0;
    double sinceProgress_ = 0.0;
    int stuckStrikes_ = 0;
    double unstickTimer_ = 0.0;

    // An enemy in sight that cannot be shot (the shot grazes a corner): skipped for a while.
    double aimBlockedSeconds_ = 0.0;
    int skippedEnemyId_ = -1;
    double skipTimer_ = 0.0;

    // Statistics for the current level.
    int doorsOpened_ = 0;
    int replans_ = 0;
    std::vector<std::string> keysCollected_;
    std::vector<std::string> keysUsed_;
};

// Runs the bot on an engine that was initialised by the caller (headless). With
// `wholeCampaign` the engine must be on the main menu: the bot starts New Game and plays every
// level in order with health and ammo carried over, and the report checks the carry-over.
// Otherwise it plays the level the engine is in (started with --level) once.
// With `trace`, the event log of the run (--trace, docs/TESTING.md "Autoplay bot") is appended
// to it: one line per event (level start/end, enemy state changes, hits taken and dealt,
// pickups, keys, doors, triggers, the exit), found by comparing the engine's state before and
// after every frame. Deterministic: game time and frame numbers only, a fixed order.
AutoplayRunReport RunAutoplaySession(Engine& engine, const AutoplayConfig& config, bool wholeCampaign,
                                     std::string* trace = nullptr);

struct AutoplayRunOptions {
    std::string dataDir;            // --data; empty = the automatic content-root search
    std::filesystem::path prefDir;  // progress.json goes here (must be set)
    int level = 0;                  // 1-based campaign level; 0 = the whole campaign (--all)
    AutoplayConfig config{};
    std::string* trace = nullptr;  // --trace: receives the event log (see RunAutoplaySession)
};

// Starts a headless engine (EngineOptions::headless, no error dialogs) and runs the session.
AutoplayRunReport RunAutoplay(const AutoplayRunOptions& options);
