#include "engine/autoplay/AutoplayBot.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "engine/core/Engine.h"
#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/core/Paths.h"
#include "engine/core/Progress.h"
#include "engine/render/Raycaster.h"
#include "engine/world/EnemyAi.h"
#include "engine/world/InteractionRules.h"
#include "engine/world/Navigation.h"

namespace {
constexpr float kMinTurnDeadband = 0.016f;  // radians; one frame of turning is ~0.03 at 1.8 rad/s
constexpr float kTurnDeadband = 0.04f;
constexpr float kWalkWhileTurning = 0.35f;    // walk only within this angle (radians) of the target
constexpr float kAimMargin = 0.32f;           // cells off the enemy's centre; a shot hits within 0.45
constexpr float kDoorPressDistance = 0.85f;   // doors toggle within 1.05 cells of their centre
constexpr float kExitStandTolerance = 0.25f;  // cells from the centre of the cell beside the switch
constexpr float kExitFacing = 0.08f;          // radians
constexpr float kCorridorRadius = 0.3f;       // the player's radius (0.22) plus a margin
constexpr std::size_t kLookahead = 6;         // path cells the bot may cut across
constexpr double kInteractRepeat = 0.3;       // seconds between two Interact presses
constexpr double kUnstickSeconds = 0.6;
constexpr double kProgressStep = 0.25;  // cells closer to the goal count as progress
// An enemy whose centre is in sight but whose shot grazes a corner: after this long aimed at it
// without a clear shot, the bot ignores it for kSkipEnemySeconds and walks on (an idle enemy
// out of its own notice range would otherwise hold the bot there for good).
constexpr double kAimBlockedGiveUp = 2.0;
constexpr double kSkipEnemySeconds = 3.0;

float WrapAngle(float angle) {
    while (angle > kPi) {
        angle -= 2.0f * kPi;
    }
    while (angle < -kPi) {
        angle += 2.0f * kPi;
    }
    return angle;
}

std::string FormatCell(const Vec2i& cell) {
    return "(" + std::to_string(cell.x) + ", " + std::to_string(cell.y) + ")";
}

std::string FormatPosition(const Vec2& position) {
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "(%.2f, %.2f)", static_cast<double>(position.x),
                  static_cast<double>(position.y));
    return buffer;
}

std::string FormatSeconds(double seconds) {
    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "%.1f", seconds);
    return buffer;
}

int KeyMask(const KeyRing& keys) {
    int mask = 0;
    for (int i = 0; i < kKeyColorCount; ++i) {
        mask |= keys.Has(kKeyColors[i]) ? (1 << i) : 0;
    }
    return mask;
}

void AddUnique(std::vector<std::string>& list, const std::string& value) {
    if (std::find(list.begin(), list.end(), value) == list.end()) {
        list.push_back(value);
    }
}
}  // namespace

const char* AutoplayResultName(AutoplayResult result) {
    switch (result) {
        case AutoplayResult::NotPlayed:
            return "not-played";
        case AutoplayResult::Complete:
            return "complete";
        case AutoplayResult::Died:
            return "died";
        case AutoplayResult::Timeout:
            return "timeout";
        case AutoplayResult::Stuck:
            return "stuck";
    }
    return "unknown";
}

bool AutoplayRunReport::Success() const {
    if (!error.empty() || levels.empty() || !carryOverProblems.empty()) {
        return false;
    }
    for (const AutoplayLevelReport& level : levels) {
        if (level.result != AutoplayResult::Complete) {
            return false;
        }
    }
    return !wholeCampaign || campaignCompleted;
}

AutoplayBot::AutoplayBot(Engine& engine, const AutoplayConfig& config) : engine_(engine), config_(config) {}

// --- Input -------------------------------------------------------------------------------------

void AutoplayBot::Hold(InputAction action) {
    want_[static_cast<std::size_t>(action)] = true;
}

void AutoplayBot::Tap(InputAction action) {
    // A press is an edge: press only if the action was released in the previous frame.
    const std::size_t index = static_cast<std::size_t>(action);
    want_[index] = !wasDown_[index];
}

float AutoplayBot::TurnTowards(const Vec2& target) {
    const Vec2 to = target - engine_.player_.position;
    if (LengthSquared(to) < 1e-8f) {
        return 0.0f;
    }
    const float diff = WrapAngle(std::atan2(to.y, to.x) - engine_.player_.directionAngleRadians);
    // Aim more precisely at far targets (a shot must pass within kAimMargin of an enemy).
    const float deadband =
        std::max(kMinTurnDeadband, std::min(kTurnDeadband, 0.6f * std::asin(std::min(1.0f, kAimMargin / Length(to)))));
    if (diff > deadband) {
        Hold(InputAction::TurnRight);  // the view angle grows to the right (+y is down on the map)
    } else if (diff < -deadband) {
        Hold(InputAction::TurnLeft);
    }
    return std::fabs(diff);
}

void AutoplayBot::SteerTowards(const Vec2& target) {
    if (LengthSquared(target - engine_.player_.position) < 0.02f * 0.02f) {
        return;
    }
    if (TurnTowards(target) < kWalkWhileTurning) {
        Hold(InputAction::MoveForward);
    }
}

void AutoplayBot::Fill(InputState& input) {
    ++frame_;
    want_.fill(false);
    interactCooldown_ = std::max(0.0, interactCooldown_ - config_.fixedStepSeconds);

    switch (engine_.GetState()) {
        case GameState::Menu:
            if (!failed_) {
                Tap(InputAction::Select);  // the main menu opens on New Game
            }
            break;
        case GameState::LevelComplete:
            Tap(InputAction::Confirm);  // on to the next level (or the credits)
            break;
        case GameState::Gameplay:
            if (!failed_) {
                PlayFrame();
            }
            break;
        default:
            break;  // paused, game over, credits: the session stops there
    }

    for (std::size_t i = 0; i < kActionCount; ++i) {
        input.SetActionState(static_cast<InputAction>(i), want_[i]);
        wasDown_[i] = want_[i];
    }
}

// --- One frame of play -------------------------------------------------------------------------

void AutoplayBot::StartLevel() {
    ++levelsStarted_;
    levelIndex_ = engine_.currentLevelIndex_;
    failed_ = false;
    failureReason_.clear();
    keyMask_ = KeyMask(engine_.keys_);
    pickupsLeft_ = 0;
    enemyHealthTotal_ = 0;
    doors_.clear();
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (entity.type == EntityType::Pickup && entity.active && !entity.collected) {
            ++pickupsLeft_;
        } else if (IsEnemyAlive(entity)) {
            enemyHealthTotal_ += entity.health;
        } else if (entity.type == EntityType::Door) {
            doors_.push_back(DoorState{entity.id, entity.doorOpen});
        }
    }
    lowHealth_ = false;
    lowAmmo_ = false;
    replanRequested_ = true;
    goal_ = GoalKind::None;
    path_.clear();
    next_ = 0;
    interactCooldown_ = 0.0;
    bestPotential_ = std::numeric_limits<double>::max();
    sinceProgress_ = 0.0;
    stuckStrikes_ = 0;
    unstickTimer_ = 0.0;
    aimBlockedSeconds_ = 0.0;
    skippedEnemyId_ = -1;
    skipTimer_ = 0.0;
    doorsOpened_ = 0;
    replans_ = -1;  // the first plan of a level is not a re-plan
    keysCollected_.clear();
    keysUsed_.clear();
}

void AutoplayBot::PlayFrame() {
    // A new level: another campaign index, or the same one reloaded (its clock started again).
    if (engine_.currentLevelIndex_ != levelIndex_ || engine_.levelTimeSeconds_ < lastLevelTime_) {
        StartLevel();
    }
    lastLevelTime_ = engine_.levelTimeSeconds_;

    Observe();
    CheckProgress();
    if (failed_) {
        return;
    }
    if (unstickTimer_ > 0.0) {
        // Back off and sidestep (alternating sides), then follow a fresh plan.
        unstickTimer_ -= config_.fixedStepSeconds;
        Hold(InputAction::MoveBackward);
        Hold(stuckStrikes_ % 2 == 1 ? InputAction::StrafeLeft : InputAction::StrafeRight);
        return;
    }
    if (FightVisibleEnemy()) {
        return;
    }
    if (replanRequested_ || NeedsReplan()) {
        Plan();
        if (failed_) {
            return;
        }
    }
    FollowPath();
}

void AutoplayBot::Observe() {
    bool progress = false;

    const int mask = KeyMask(engine_.keys_);
    if (mask != keyMask_) {
        for (int i = 0; i < kKeyColorCount; ++i) {
            if ((mask & (1 << i)) != 0 && (keyMask_ & (1 << i)) == 0) {
                keysCollected_.push_back(KeyColorName(kKeyColors[i]));
            }
        }
        keyMask_ = mask;
        progress = true;
        replanRequested_ = true;
    }

    int pickupsLeft = 0;
    int enemyHealth = 0;
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (entity.type == EntityType::Pickup && entity.active && !entity.collected) {
            ++pickupsLeft;
        } else if (IsEnemyAlive(entity)) {
            enemyHealth += entity.health;
        } else if (entity.type == EntityType::Door) {
            for (DoorState& door : doors_) {
                if (door.id != entity.id || door.open == entity.doorOpen) {
                    continue;
                }
                door.open = entity.doorOpen;
                if (entity.doorOpen) {
                    ++doorsOpened_;
                    if (entity.doorLock != KeyColor::None) {
                        AddUnique(keysUsed_, KeyColorName(entity.doorLock));
                    }
                }
                progress = true;
                replanRequested_ = true;
            }
        }
    }
    if (pickupsLeft != pickupsLeft_) {
        pickupsLeft_ = pickupsLeft;
        progress = true;
        replanRequested_ = true;
    }
    if (enemyHealth < enemyHealthTotal_) {
        progress = true;  // hits count as progress; no new plan needed
    }
    enemyHealthTotal_ = enemyHealth;

    const bool lowHealth = engine_.playerHealth_ < engine_.playerMaxHealth_ &&
                           static_cast<float>(engine_.playerHealth_) <=
                               config_.lowHealthFraction * static_cast<float>(engine_.playerMaxHealth_);
    const bool lowAmmo = engine_.playerAmmo_ < engine_.playerMaxAmmo_ && engine_.playerAmmo_ <= config_.lowAmmo;
    if (lowHealth != lowHealth_ || lowAmmo != lowAmmo_) {
        lowHealth_ = lowHealth;
        lowAmmo_ = lowAmmo;
        replanRequested_ = true;
    }

    if (progress) {
        sinceProgress_ = 0.0;
        stuckStrikes_ = 0;
        bestPotential_ = std::numeric_limits<double>::max();
    }
}

void AutoplayBot::CheckProgress() {
    // "Potential": how far the bot still has to go along its current plan.
    double potential = std::numeric_limits<double>::max();
    const Vec2 position = engine_.player_.position;
    if (next_ < path_.size()) {
        potential = static_cast<double>(path_.size() - next_) + Length(CellCenter(path_[next_]) - position);
    } else if (goal_ != GoalKind::None && goalCell_.x >= 0) {
        potential = Length(CellCenter(goalCell_) - position);
    }
    if (potential < bestPotential_ - kProgressStep) {
        if (bestPotential_ != std::numeric_limits<double>::max()) {
            sinceProgress_ = 0.0;
            stuckStrikes_ = 0;
        }
        bestPotential_ = potential;
    }

    sinceProgress_ += config_.fixedStepSeconds;
    if (sinceProgress_ < config_.stuckSeconds) {
        return;
    }
    ++stuckStrikes_;
    sinceProgress_ = 0.0;
    if (stuckStrikes_ > config_.maxStuckReplans) {
        std::string goal = "nothing";
        switch (goal_) {
            case GoalKind::Exit:
                goal = "the exit switch at " + FormatCell(exitCell_);
                break;
            case GoalKind::Key:
                goal = "a key card in cell " + FormatCell(goalCell_);
                break;
            case GoalKind::Health:
                goal = "a health pickup in cell " + FormatCell(goalCell_);
                break;
            case GoalKind::Ammo:
                goal = "an ammo pickup in cell " + FormatCell(goalCell_);
                break;
            case GoalKind::None:
                break;
        }
        Fail("no progress towards " + goal + " for " +
             FormatSeconds(config_.stuckSeconds * static_cast<double>(stuckStrikes_)) + " s (" +
             std::to_string(config_.maxStuckReplans) + " re-plans tried); stopped at " + FormatPosition(position));
        return;
    }
    unstickTimer_ = kUnstickSeconds;
    replanRequested_ = true;
    bestPotential_ = std::numeric_limits<double>::max();
}

void AutoplayBot::Fail(const std::string& reason) {
    failed_ = true;
    failureReason_ = reason;
    LogWarning("Autoplay: giving up on level " + std::to_string(engine_.currentLevelIndex_ + 1) + ": " + reason);
}

// --- Fighting ----------------------------------------------------------------------------------

bool AutoplayBot::FightVisibleEnemy() {
    if (engine_.playerAmmo_ <= 0) {
        return false;  // nothing to fight with: keep walking (towards ammo when it is low)
    }
    if (skipTimer_ > 0.0) {
        skipTimer_ -= config_.fixedStepSeconds;
        if (skipTimer_ <= 0.0) {
            skippedEnemyId_ = -1;
        }
    }
    const Player& player = engine_.player_;
    const Entity* target = nullptr;
    float targetDistance = engine_.gameplay_.weapon.range;
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (!IsEnemyAlive(entity) || entity.id == skippedEnemyId_) {
            continue;
        }
        const float distance = Length(entity.position - player.position);
        if (distance < 1e-3f || distance >= targetDistance) {
            continue;
        }
        if (HasLineOfSight(engine_.map_, player.position, entity.position)) {
            target = &entity;
            targetDistance = distance;
        }
    }
    if (target == nullptr) {
        aimBlockedSeconds_ = 0.0;
        return false;
    }

    const float remaining = TurnTowards(target->position);
    const float tolerance = std::asin(std::min(1.0f, kAimMargin / targetDistance));
    if (remaining <= tolerance && engine_.playerAttackCooldown_ <= 0.0f) {
        // The shot stops at the first wall along the view direction: fire only when it can
        // reach the enemy (it may stand just behind a corner).
        const float angle = player.directionAngleRadians;
        const Vec2 direction{std::cos(angle), std::sin(angle)};
        const RaycastHit wall = Raycaster::CastRay(engine_.map_, player.position, direction, angle);
        const float along = Dot(target->position - player.position, direction);
        if (!wall.hit || wall.distanceRaw > along) {
            Tap(InputAction::Attack);
            aimBlockedSeconds_ = 0.0;
        } else {
            aimBlockedSeconds_ += config_.fixedStepSeconds;
            if (aimBlockedSeconds_ >= kAimBlockedGiveUp) {
                skippedEnemyId_ = target->id;
                skipTimer_ = kSkipEnemySeconds;
                aimBlockedSeconds_ = 0.0;
                return false;
            }
        }
    }
    return true;
}

// --- Planning ----------------------------------------------------------------------------------

bool AutoplayBot::IsPassable(int x, int y) const {
    const Map& map = engine_.map_;
    if (!map.IsInsideMap(x, y)) {
        return false;
    }
    const TileType tile = map.GetCell(x, y);
    if (tile == TileType::Empty || tile == TileType::Trigger) {
        return true;
    }
    if (tile != TileType::Door) {
        return false;
    }
    // A closed door: passable when a door entity controls the cell and it can be opened.
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (entity.type == EntityType::Door && entity.mapX == x && entity.mapY == y) {
            return entity.doorLock == KeyColor::None || engine_.keys_.Has(entity.doorLock);
        }
    }
    return false;  // a 'D' without a door entity never opens
}

void AutoplayBot::SearchFromPlayer() {
    const Map& map = engine_.map_;
    const std::size_t cells = static_cast<std::size_t>(map.Width()) * static_cast<std::size_t>(map.Height());
    distance_.assign(cells, -1);
    parent_.assign(cells, -1);
    const Vec2i start = CellOf(engine_.player_.position);
    if (!map.IsInsideMap(start.x, start.y)) {
        return;
    }
    const auto index = [&map](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(map.Width()) + static_cast<std::size_t>(x);
    };
    std::vector<Vec2i> queue{start};
    distance_[index(start.x, start.y)] = 0;
    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const Vec2i cell = queue[head];
        for (int d = 0; d < 4; ++d) {
            const int nx = cell.x + kDx[d];
            const int ny = cell.y + kDy[d];
            if (!IsPassable(nx, ny) || distance_[index(nx, ny)] >= 0) {
                continue;
            }
            distance_[index(nx, ny)] = distance_[index(cell.x, cell.y)] + 1;
            parent_[index(nx, ny)] = static_cast<int>(index(cell.x, cell.y));
            queue.push_back(Vec2i{nx, ny});
        }
    }
}

bool AutoplayBot::IsReached(const Vec2i& cell) const {
    return DistanceTo(cell) >= 0;
}

int AutoplayBot::DistanceTo(const Vec2i& cell) const {
    const Map& map = engine_.map_;
    if (!map.IsInsideMap(cell.x, cell.y) || distance_.empty()) {
        return -1;
    }
    return distance_[static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(map.Width()) +
                     static_cast<std::size_t>(cell.x)];
}

std::vector<Vec2i> AutoplayBot::PathTo(const Vec2i& cell) const {
    std::vector<Vec2i> path;
    if (!IsReached(cell)) {
        return path;
    }
    const int width = engine_.map_.Width();
    int at = cell.y * width + cell.x;
    while (parent_[static_cast<std::size_t>(at)] >= 0) {
        path.push_back(Vec2i{at % width, at / width});
        at = parent_[static_cast<std::size_t>(at)];
    }
    std::reverse(path.begin(), path.end());
    return path;  // excludes the start cell; empty when already there
}

std::string AutoplayBot::DescribeBlockage() const {
    const Map& map = engine_.map_;
    int exits = 0;
    std::vector<std::string> deadDoors;
    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            if (map.GetCell(x, y) == TileType::Exit) {
                ++exits;
            } else if (map.GetCell(x, y) == TileType::Door) {
                bool hasEntity = false;
                for (const Entity& entity : engine_.entityManager_.Entities()) {
                    hasEntity = hasEntity || (entity.type == EntityType::Door && entity.mapX == x && entity.mapY == y);
                }
                if (!hasEntity) {
                    deadDoors.push_back(FormatCell(Vec2i{x, y}));
                }
            }
        }
    }
    const Vec2i start = CellOf(engine_.player_.position);
    if (exits == 0) {
        return "no route to an exit switch: the map has none ('X')";
    }
    std::string reason = "no route to an exit switch: none of the " + std::to_string(exits) +
                         " exit switch(es) has a reachable floor cell beside it from cell " + FormatCell(start);
    std::vector<std::string> locked;
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (entity.type == EntityType::Door && !entity.doorOpen && entity.doorLock != KeyColor::None &&
            !engine_.keys_.Has(entity.doorLock)) {
            locked.push_back(std::string(KeyColorName(entity.doorLock)) + " door at " +
                             FormatCell(Vec2i{entity.mapX, entity.mapY}));
        }
    }
    if (!locked.empty()) {
        reason += "; locked without a reachable key:";
        for (const std::string& door : locked) {
            reason += " " + door;
        }
    }
    if (!deadDoors.empty()) {
        reason += "; 'D' cells without a door entity (never open):";
        for (const std::string& door : deadDoors) {
            reason += " " + door;
        }
    }
    return reason;
}

bool AutoplayBot::NeedsReplan() const {
    if (goal_ == GoalKind::None) {
        return true;
    }
    // The next step became impassable (e.g. a door closed and the key is not held).
    return next_ < path_.size() && !IsPassable(path_[next_].x, path_[next_].y);
}

void AutoplayBot::Plan() {
    replanRequested_ = false;
    ++replans_;
    SearchFromPlayer();
    const Map& map = engine_.map_;

    struct Candidate {
        int distance = -1;
        Vec2i cell{-1, -1};
        Vec2i exit{-1, -1};
        int entityId = -1;
    };
    const auto consider = [](Candidate& best, int distance, const Vec2i& cell, const Vec2i& exit, int entityId) {
        if (distance >= 0 && (best.distance < 0 || distance < best.distance)) {
            best = Candidate{distance, cell, exit, entityId};
        }
    };

    Candidate health;
    Candidate ammo;
    Candidate key;
    Candidate exitCandidate;
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (entity.type != EntityType::Pickup || !entity.active || entity.collected) {
            continue;
        }
        const Vec2i cell = CellOf(entity.position);
        const int distance = DistanceTo(cell);
        if (entity.pickupType == PickupType::Health && lowHealth_) {
            consider(health, distance, cell, Vec2i{-1, -1}, entity.id);
        } else if (entity.pickupType == PickupType::Ammo && lowAmmo_) {
            consider(ammo, distance, cell, Vec2i{-1, -1}, entity.id);
        } else if (entity.pickupType == PickupType::Key && !engine_.keys_.Has(entity.keyColor)) {
            // Only keys that open a door of this level.
            bool opensSomething = false;
            for (const Entity& door : engine_.entityManager_.Entities()) {
                opensSomething = opensSomething || (door.type == EntityType::Door && door.doorLock == entity.keyColor);
            }
            if (opensSomething) {
                consider(key, distance, cell, Vec2i{-1, -1}, entity.id);
            }
        }
    }
    constexpr int kDx[4] = {1, -1, 0, 0};
    constexpr int kDy[4] = {0, 0, 1, -1};
    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            if (map.GetCell(x, y) != TileType::Exit) {
                continue;
            }
            for (int d = 0; d < 4; ++d) {
                const Vec2i beside{x + kDx[d], y + kDy[d]};
                consider(exitCandidate, DistanceTo(beside), beside, Vec2i{x, y}, -1);
            }
        }
    }

    Candidate chosen;
    if (health.distance >= 0) {
        goal_ = GoalKind::Health;
        chosen = health;
    } else if (ammo.distance >= 0) {
        goal_ = GoalKind::Ammo;
        chosen = ammo;
    } else if (exitCandidate.distance >= 0) {
        goal_ = GoalKind::Exit;
        chosen = exitCandidate;
    } else if (key.distance >= 0) {
        goal_ = GoalKind::Key;
        chosen = key;
    } else {
        goal_ = GoalKind::None;
        path_.clear();
        next_ = 0;
        Fail(DescribeBlockage());
        return;
    }
    goalCell_ = chosen.cell;
    exitCell_ = chosen.exit;
    goalEntityId_ = chosen.entityId;
    path_ = PathTo(goalCell_);
    next_ = 0;
    bestPotential_ = std::numeric_limits<double>::max();
}

// --- Walking -----------------------------------------------------------------------------------

void AutoplayBot::FollowPath() {
    const Map& map = engine_.map_;
    const Vec2 position = engine_.player_.position;
    const Vec2i cell = CellOf(position);

    // Advance to the furthest path cell the player stands in (the bot may cut corners).
    for (std::size_t i = next_; i < path_.size(); ++i) {
        if (path_[i] == cell) {
            next_ = i + 1;
        }
    }
    if (next_ < path_.size()) {
        const Vec2i ahead = path_[next_];
        if (std::abs(ahead.x - cell.x) + std::abs(ahead.y - cell.y) > 1) {
            replanRequested_ = true;  // pushed off the path (a fight, a door, an unstick move)
            return;
        }
        if (map.GetCell(ahead.x, ahead.y) == TileType::Door) {
            // Walk up to the closed door, then press Interact (once per kInteractRepeat).
            const Vec2 doorCenter = CellCenter(ahead);
            if (Length(doorCenter - position) > kDoorPressDistance) {
                SteerTowards(doorCenter);
                return;
            }
            TurnTowards(doorCenter);
            if (interactCooldown_ <= 0.0) {
                Tap(InputAction::Interact);
                interactCooldown_ = kInteractRepeat;
            }
            return;
        }
        // Look ahead: steer for the furthest cell that a body can reach in a straight line.
        Vec2 target = CellCenter(ahead);
        for (std::size_t j = next_ + 1; j < path_.size() && j < next_ + kLookahead; ++j) {
            if (map.GetCell(path_[j].x, path_[j].y) == TileType::Door ||
                !HasClearCorridor(map, position, CellCenter(path_[j]), kCorridorRadius)) {
                break;
            }
            target = CellCenter(path_[j]);
        }
        SteerTowards(target);
        return;
    }

    // At the goal cell.
    if (goal_ == GoalKind::Exit) {
        const Vec2 standAt = CellCenter(goalCell_);
        if (Length(standAt - position) > kExitStandTolerance) {
            SteerTowards(standAt);
            return;
        }
        if (TurnTowards(CellCenter(exitCell_)) < kExitFacing && interactCooldown_ <= 0.0) {
            Tap(InputAction::Interact);
            interactCooldown_ = kInteractRepeat;
        }
        return;
    }
    const Entity* pickup = engine_.entityManager_.GetEntityById(goalEntityId_);
    if (pickup == nullptr || !pickup->active || pickup->collected) {
        replanRequested_ = true;
        return;
    }
    SteerTowards(pickup->position);  // collected within 0.6 cells of its position
}

// --- Sessions ----------------------------------------------------------------------------------

namespace {
// --- Event trace (--trace) ---------------------------------------------------------------------
// Events are found by comparing the engine's state after each Gameplay frame with a snapshot
// taken before it; nothing in the engine reports them. Within one frame the lines follow the
// engine's own order: the player's shot, pickups and keys, triggers, doors, the exit switch,
// then the enemy AI (state changes, hits on the player). Entities are listed in entity order.
// Only game time, frame numbers, ids and cells appear: two runs of a build give the same text.

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
    char buffer[64] = {};
    std::snprintf(buffer, sizeof(buffer), "%.2f,%.2f", static_cast<double>(position.x),
                  static_cast<double>(position.y));
    return buffer;
}

std::string TraceEnemy(const Entity& enemy) {
    return std::string(EnemyVariantName(enemy.variant)) + "#" + std::to_string(enemy.id);
}

std::string HeldKeys(const KeyRing& keys) {
    std::string held;
    for (int i = 0; i < kKeyColorCount; ++i) {
        if (keys.Has(kKeyColors[i])) {
            held += (held.empty() ? "" : ",") + std::string(KeyColorName(kKeyColors[i]));
        }
    }
    return held.empty() ? "none" : held;
}

class TraceRecorder {
public:
    explicit TraceRecorder(std::string* out) : out_(out) {
        if (out_ != nullptr) {
            *out_ +=
                "# neo_wolf_autoplay trace 1: one event per line, \"L<level> t=<game seconds> f=<frame> "
                "<event> key=value...\" (docs/TESTING.md, Autoplay bot)\n";
        }
    }

    // Before the first frame of a level: the snapshot every later frame is compared with.
    void BeginLevel(const Engine& engine, int levelNumber) {
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
    void AfterFrame(const Engine& engine, std::uint64_t frame) {
        if (out_ == nullptr) {
            return;
        }
        const std::vector<Entity>& entities = engine.entityManager_.Entities();

        // 1. The player's shot (it comes first in a frame): health lost by an enemy.
        for (const Entity& entity : entities) {
            const Snap* was = Find(entity.id);
            if (entity.type != EntityType::Enemy || was == nullptr || entity.health >= was->health) {
                continue;
            }
            Line(engine, frame,
                 "hit-dealt target=" + TraceEnemy(entity) + " cell=" + TraceCell(CellOf(entity.position)) +
                     " damage=" + std::to_string(was->health - entity.health) + " hp=" + std::to_string(entity.health) +
                     "/" + std::to_string(entity.maxHealth) + (entity.enemyState == EnemyState::Dead ? " killed" : ""));
        }

        // 2. Pickups (key cards as "key" lines), 3. triggers, 4. doors.
        for (const Entity& entity : entities) {
            const Snap* was = Find(entity.id);
            if (was == nullptr) {
                continue;
            }
            const std::string cell = TraceCell(entity.type == EntityType::Door || entity.type == EntityType::Trigger
                                                   ? Vec2i{entity.mapX, entity.mapY}
                                                   : CellOf(entity.position));
            if (entity.type == EntityType::Pickup && entity.collected && !was->collected) {
                if (entity.pickupType == PickupType::Key) {
                    Line(engine, frame,
                         std::string("key colour=") + KeyColorName(entity.keyColor) + " cell=" + cell +
                             " held=" + HeldKeys(engine.keys_));
                } else {
                    Line(engine, frame,
                         std::string("pickup kind=") + (entity.pickupType == PickupType::Health ? "health" : "ammo") +
                             " value=" + std::to_string(entity.pickupValue) + " cell=" + cell + " hp=" +
                             std::to_string(engine.playerHealth_) + " ammo=" + std::to_string(engine.playerAmmo_));
                }
            }
        }
        for (const Entity& entity : entities) {
            const Snap* was = Find(entity.id);
            if (entity.type == EntityType::Trigger && was != nullptr && entity.triggerActivated &&
                !was->triggerActivated) {
                Line(engine, frame,
                     "trigger id=" + std::to_string(entity.id) + " cell=" + TraceCell(Vec2i{entity.mapX, entity.mapY}) +
                         " pos=" + TracePosition(engine.player_.position));
            }
        }
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

        // 5. The exit switch: the frame ends there (no enemy AI in that frame).
        const bool exited = engine.GetState() == GameState::LevelComplete;
        if (exited) {
            const ExitSwitchHit exitHit = FindExitSwitchInReach(engine.map_, engine.player_);
            Line(engine, frame,
                 "exit cell=" + (exitHit.found ? TraceCell(Vec2i{exitHit.mapX, exitHit.mapY}) : "?") +
                     " pos=" + TracePosition(engine.player_.position) + " hp=" + std::to_string(engine.playerHealth_) +
                     " ammo=" + std::to_string(engine.playerAmmo_));
        }

        // 6. Enemy AI: state changes (one line per frame and enemy: old -> new; a change and its
        // reverse within one frame are not seen), then attacks on the player.
        for (const Entity& entity : entities) {
            const Snap* was = Find(entity.id);
            if (entity.type == EntityType::Enemy && was != nullptr && entity.enemyState != was->enemyState) {
                Line(engine, frame,
                     "enemy " + TraceEnemy(entity) + " cell=" + TraceCell(CellOf(entity.position)) + " " +
                         EnemyStateName(was->enemyState) + "->" + EnemyStateName(entity.enemyState));
            }
        }
        // An enemy attacked this frame when the AI ran (the level clock moved and the exit was
        // not used) and it is in Attack with its cooldown just set to attackRate: the cooldown
        // counts down in every AI update and is only set back by an attack.
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
                 "hit-taken by=" + TraceEnemy(entity) + " from=" + TraceCell(CellOf(entity.position)) +
                     " damage=" + std::to_string(entity.damage) + " hp=" + std::to_string(after) +
                     " pos=" + TracePosition(engine.player_.position));
        }
        if (lost > attributed) {
            Line(engine, frame,
                 "hit-taken by=unknown damage=" + std::to_string(lost - attributed) +
                     " hp=" + std::to_string(engine.playerHealth_) + " pos=" + TracePosition(engine.player_.position));
        }

        Snapshot(engine);
    }

    void EndLevel(const Engine& engine, const AutoplayLevelReport& report) {
        if (out_ == nullptr) {
            return;
        }
        std::string line = std::string("level-end result=") + AutoplayResultName(report.result) +
                           " hp=" + std::to_string(report.healthEnd) + " ammo=" + std::to_string(report.ammoEnd) +
                           " damage=" + std::to_string(report.damageTaken) + " kills=" + std::to_string(report.kills) +
                           "/" + std::to_string(report.totalEnemies) + " pickups=" + std::to_string(report.pickups) +
                           "/" + std::to_string(report.totalPickups) + " shots=" + std::to_string(report.shotsFired);
        if (!report.reason.empty()) {
            line += " reason=\"" + report.reason + "\"";
        }
        Line(engine, report.frames, line);
    }

private:
    struct Snap {
        int health = 0;
        EnemyState enemyState = EnemyState::Idle;
        float attackCooldown = 0.0f;
        bool collected = false;
        bool doorOpen = false;
        bool triggerActivated = false;
    };

    void Snapshot(const Engine& engine) {
        snaps_.clear();
        for (const Entity& entity : engine.entityManager_.Entities()) {
            snaps_[entity.id] = Snap{entity.health,    entity.enemyState, entity.attackCooldown,
                                     entity.collected, entity.doorOpen,   entity.triggerActivated};
        }
        damageTaken_ = engine.levelDamageTaken_;
        levelTime_ = engine.levelTimeSeconds_;
    }

    const Snap* Find(int id) const {
        const auto it = snaps_.find(id);
        return it == snaps_.end() ? nullptr : &it->second;
    }

    void Line(const Engine& engine, std::uint64_t frame, const std::string& event) {
        char prefix[64] = {};
        std::snprintf(prefix, sizeof(prefix), "L%d t=%.3f f=%llu ", levelNumber_, engine.levelTimeSeconds_,
                      static_cast<unsigned long long>(frame));
        *out_ += prefix + event + "\n";
    }

    std::string* out_ = nullptr;
    int levelNumber_ = 0;
    std::map<int, Snap> snaps_;
    std::vector<int> unlockedDoors_;
    int damageTaken_ = 0;
    double levelTime_ = 0.0;
};

struct LevelTracker {
    AutoplayLevelReport* report = nullptr;
    int levelIndex = -1;
    bool ended = false;
};

void FillLevelIdentity(const Engine& engine, int levelIndex, AutoplayLevelReport& report) {
    report.levelNumber = levelIndex + 1;
    if (levelIndex >= 0 && levelIndex < static_cast<int>(engine.campaign_.levels.size())) {
        const CampaignLevel& level = engine.campaign_.levels[static_cast<std::size_t>(levelIndex)];
        report.id = level.id;
        report.name = level.name;
    }
}

void FinishLevel(const Engine& engine, const AutoplayBot& bot, AutoplayLevelReport& report, AutoplayResult result,
                 const std::string& reason, const LevelResults& results) {
    report.result = result;
    report.reason = reason;
    report.gameSeconds = results.seconds;
    report.healthEnd = engine.playerHealth_;
    report.ammoEnd = engine.playerAmmo_;
    report.damageTaken = results.damageTaken;
    report.kills = results.kills;
    report.totalEnemies = results.totalEnemies;
    report.pickups = results.pickups;
    report.totalPickups = results.totalPickups;
    report.shotsFired = results.shotsFired;
    report.doorsOpened = bot.DoorsOpened();
    report.replans = std::max(0, bot.Replans());
    report.keysCollected = bot.KeysCollected();
    report.keysUsed = bot.KeysUsed();
    LogInfo("Autoplay: level " + std::to_string(report.levelNumber) + " (" + report.id +
            "): " + AutoplayResultName(result) + (reason.empty() ? std::string() : " - " + reason));
}
}  // namespace

AutoplayRunReport RunAutoplaySession(Engine& engine, const AutoplayConfig& config, bool wholeCampaign,
                                     std::string* trace) {
    AutoplayRunReport report{};
    report.campaignTitle = engine.campaign_.title;
    report.campaignLevels = static_cast<int>(engine.campaign_.levels.size());
    report.wholeCampaign = wholeCampaign;
    report.fixedStepSeconds = config.fixedStepSeconds;
    report.timeLimitSeconds = config.timeLimitSeconds;
    if (!engine.IsRunning()) {
        report.error = "the engine is not running";
        return report;
    }
    if (wholeCampaign && engine.GetState() != GameState::Menu) {
        report.error = "a whole-campaign run must start on the main menu";
        return report;
    }

    AutoplayBot bot(engine, config);
    engine.SetInputInjector([&bot](InputState& input) { bot.Fill(input); });
    TraceRecorder recorder(trace);

    std::vector<PlayerStats> entered;
    std::vector<PlayerStats> completed;
    LevelTracker level{};
    const int levelsToPlay = wholeCampaign ? report.campaignLevels : 1;
    // A safety net in frames on top of the per-level time limit (menus and screens take a few).
    const double framesPerLevel = (config.timeLimitSeconds + 10.0) / config.fixedStepSeconds;
    const auto maxFrames = static_cast<std::uint64_t>(framesPerLevel * static_cast<double>(std::max(1, levelsToPlay)));
    bool stop = false;
    std::uint64_t frame = 0;
    for (; frame < maxFrames && !stop && engine.IsRunning(); ++frame) {
        const GameState before = engine.GetState();
        if (before == GameState::Gameplay && (level.report == nullptr || level.ended)) {
            report.levels.emplace_back();
            level = LevelTracker{&report.levels.back(), engine.currentLevelIndex_, false};
            FillLevelIdentity(engine, level.levelIndex, *level.report);
            level.report->healthStart = engine.playerHealth_;
            level.report->ammoStart = engine.playerAmmo_;
            entered.push_back(engine.CurrentStats());
            recorder.BeginLevel(engine, level.report->levelNumber);
        }

        engine.Update(config.fixedStepSeconds);

        if (level.report == nullptr || level.ended) {
            if (wholeCampaign && engine.GetState() == GameState::CampaignComplete) {
                report.campaignCompleted = true;
                stop = true;
            }
            continue;
        }
        if (before == GameState::Gameplay) {
            ++level.report->frames;
            recorder.AfterFrame(engine, level.report->frames);
        }
        const GameState after = engine.GetState();
        const std::string where = " at " + FormatPosition(engine.player_.position);
        if (after == GameState::LevelComplete) {
            level.ended = true;
            completed.push_back(engine.CurrentStats());
            FinishLevel(engine, bot, *level.report, AutoplayResult::Complete, "", engine.flow_.LastResults());
        } else if (after == GameState::GameOver) {
            level.ended = true;
            FinishLevel(engine, bot, *level.report, AutoplayResult::Died,
                        "health reached 0" + where + " after " + FormatSeconds(engine.levelTimeSeconds_) + " s",
                        engine.CurrentLevelResults());
        } else if (bot.Failed()) {
            level.ended = true;
            FinishLevel(engine, bot, *level.report, AutoplayResult::Stuck, bot.FailureReason(),
                        engine.CurrentLevelResults());
        } else if (engine.levelTimeSeconds_ >= config.timeLimitSeconds) {
            level.ended = true;
            FinishLevel(engine, bot, *level.report, AutoplayResult::Timeout,
                        "time limit of " + FormatSeconds(config.timeLimitSeconds) + " s reached" + where,
                        engine.CurrentLevelResults());
        } else if (after != GameState::Gameplay) {
            level.ended = true;
            FinishLevel(engine, bot, *level.report, AutoplayResult::Stuck,
                        std::string("the game left play unexpectedly (state ") + GameStateToString(after) + ")",
                        engine.CurrentLevelResults());
        }
        if (level.ended) {
            recorder.EndLevel(engine, *level.report);
            const bool completedLevel = level.report->result == AutoplayResult::Complete;
            stop = !wholeCampaign || !completedLevel;
        }
    }
    if (level.report != nullptr && !level.ended) {
        FinishLevel(engine, bot, *level.report, AutoplayResult::Timeout,
                    "the frame limit of the run was reached (" + std::to_string(frame) + " frames)",
                    engine.CurrentLevelResults());
        recorder.EndLevel(engine, *level.report);
    }
    engine.SetInputInjector(nullptr);

    if (wholeCampaign) {
        // Levels after a failure were not played.
        for (int index = static_cast<int>(report.levels.size()); index < report.campaignLevels; ++index) {
            AutoplayLevelReport notPlayed{};
            FillLevelIdentity(engine, index, notPlayed);
            notPlayed.reason = "an earlier level was not completed";
            report.levels.push_back(notPlayed);
        }

        // Carry-over, checked independently of GameFlow: level 1 starts with the gameplay.json
        // initial values; every later level with what the previous one ended with, clamped to
        // the maxima (health at least 1).
        report.carryOverChecked = !entered.empty();
        const GameplayDefinitions& gameplay = engine.gameplay_;
        for (std::size_t i = 0; i < entered.size(); ++i) {
            PlayerStats expected{gameplay.player.initialHealth, gameplay.weapon.initialAmmo};
            if (i > 0 && i - 1 < completed.size()) {
                expected.health = std::clamp(completed[i - 1].health, 1, gameplay.player.maxHealth);
                expected.ammo = std::clamp(completed[i - 1].ammo, 0, gameplay.weapon.maxAmmo);
            }
            if (entered[i].health != expected.health || entered[i].ammo != expected.ammo) {
                report.carryOverProblems.push_back(
                    "level " + std::to_string(i + 1) + " started with health " + std::to_string(entered[i].health) +
                    ", ammo " + std::to_string(entered[i].ammo) + "; expected health " +
                    std::to_string(expected.health) + ", ammo " + std::to_string(expected.ammo));
            }
        }
        // Progress: completing every level unlocks (and saves) up to the last one.
        if (report.campaignCompleted && !engine.prefDir_.empty()) {
            ProgressData saved{};
            std::string message;
            const std::string path = PathToUtf8(engine.prefDir_ / kProgressFileName);
            if (LoadProgressFile(path, saved, message) != ProgressLoadResult::Loaded) {
                report.carryOverProblems.push_back("progress was not saved: " + message);
            } else if (saved.highestUnlocked != report.campaignLevels) {
                report.carryOverProblems.push_back("saved progress unlocks level " +
                                                   std::to_string(saved.highestUnlocked) + ", expected " +
                                                   std::to_string(report.campaignLevels));
            }
        }
    }
    return report;
}

AutoplayRunReport RunAutoplay(const AutoplayRunOptions& options) {
    EngineOptions engineOptions{};
    engineOptions.dataDir = options.dataDir;
    engineOptions.prefDir = options.prefDir;
    engineOptions.startLevel = options.level;
    engineOptions.headless = true;
    engineOptions.errorDialogs = false;

    Engine engine;
    if (!engine.Init(engineOptions)) {
        AutoplayRunReport report{};
        report.wholeCampaign = options.level == 0;
        report.fixedStepSeconds = options.config.fixedStepSeconds;
        report.timeLimitSeconds = options.config.timeLimitSeconds;
        report.error = "the engine did not start (see the messages above)";
        return report;
    }
    AutoplayRunReport report = RunAutoplaySession(engine, options.config, options.level == 0, options.trace);
    engine.Shutdown();
    return report;
}
