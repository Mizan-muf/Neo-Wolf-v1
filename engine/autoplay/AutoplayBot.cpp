#include "engine/autoplay/AutoplayBot.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "engine/autoplay/AutoplayTrace.h"
#include "engine/core/Engine.h"
#include "engine/core/Log.h"
#include "engine/core/MathUtils.h"
#include "engine/core/Paths.h"
#include "engine/core/Progress.h"
#include "engine/render/Raycaster.h"
#include "engine/world/EnemyAi.h"
#include "engine/world/Navigation.h"

namespace {
constexpr float kMinTurnDeadband = 0.016f;  // radians; one frame of turning is ~0.03 at 1.8 rad/s
constexpr float kTurnDeadband = 0.04f;
constexpr float kDeadbandShareOfAimTolerance = 0.6f;  // the deadband is this share of the aim tolerance
constexpr float kArrivedDistance = 0.02f;             // cells; SteerTowards does nothing closer than this
constexpr float kWalkWhileTurning = 0.35f;            // walk only within this angle (radians) of the target
constexpr float kAimMargin = 0.32f;                   // cells off the enemy's centre; a shot hits within 0.45
constexpr float kDoorPressDistance = 0.85f;           // doors toggle within 1.05 cells of their centre
constexpr float kExitStandTolerance = 0.25f;          // cells from the centre of the cell beside the switch
constexpr float kExitFacing = 0.08f;                  // radians
constexpr float kCorridorRadius = 0.3f;               // the player's radius (0.22) plus a margin
constexpr std::size_t kLookahead = 6;                 // path cells the bot may cut across
constexpr double kInteractRepeat = 0.3;               // seconds between two Interact presses
constexpr double kUnstickSeconds = 0.6;
constexpr double kProgressStep = 0.25;  // cells closer to the goal count as progress
// An enemy whose centre is in sight but whose shot grazes a corner: after this long aimed at it
// without a clear shot, the bot ignores it for kSkipEnemySeconds and walks on (an idle enemy
// out of its own notice range would otherwise hold the bot there for good).
constexpr double kAimBlockedGiveUp = 2.0;
constexpr double kSkipEnemySeconds = 3.0;
// RunAutoplaySession's frame limit: the time limit of every level plus this much for menus and screens.
constexpr double kFrameLimitMarginSeconds = 10.0;
// The 4-connected neighbours of a cell; the order decides between equally short paths.
constexpr std::array<Vec2i, 4> kNeighbourSteps{{{1, 0}, {-1, 0}, {0, 1}, {0, -1}}};

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
    std::array<char, 64> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "(%.2f, %.2f)", static_cast<double>(position.x),
                  static_cast<double>(position.y));
    return buffer.data();
}

std::string FormatSeconds(double seconds) {
    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.1f", seconds);
    return buffer.data();
}

// The bit of kKeyColors[index] in a key mask.
unsigned KeyBit(int index) {
    return 1U << static_cast<unsigned>(index);
}

unsigned KeyMask(const KeyRing& keys) {
    unsigned mask = 0;
    for (int i = 0; i < kKeyColorCount; ++i) {
        mask |= keys.Has(kKeyColors[i]) ? KeyBit(i) : 0U;
    }
    return mask;
}

// Appends `title` and the items, each after a space; nothing for no items.
void AppendList(std::string& text, const char* title, const std::vector<std::string>& items) {
    if (items.empty()) {
        return;
    }
    text += title;
    for (const std::string& item : items) {
        text += " " + item;
    }
}

bool IsAvailablePickup(const Entity& entity) {
    return entity.type == EntityType::Pickup && entity.active && !entity.collected;
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
    const auto index = static_cast<std::size_t>(action);
    want_[index] = !wasDown_[index];
}

void AutoplayBot::PressInteract() {
    if (interactCooldown_ <= 0.0) {
        Tap(InputAction::Interact);
        interactCooldown_ = kInteractRepeat;
    }
}

float AutoplayBot::TurnTowards(const Vec2& target) {
    const Vec2 to = target - engine_.player_.position;
    if (LengthSquared(to) < 1e-8f) {
        return 0.0f;
    }
    const float diff = WrapAngle(std::atan2(to.y, to.x) - engine_.player_.directionAngleRadians);
    // Aim more precisely at far targets (a shot must pass within kAimMargin of an enemy).
    const float deadband = std::max(
        kMinTurnDeadband,
        std::min(kTurnDeadband, kDeadbandShareOfAimTolerance * std::asin(std::min(1.0f, kAimMargin / Length(to)))));
    if (diff > deadband) {
        Hold(InputAction::TurnRight);  // the view angle grows to the right (+y is down on the map)
    } else if (diff < -deadband) {
        Hold(InputAction::TurnLeft);
    }
    return std::fabs(diff);
}

void AutoplayBot::SteerTowards(const Vec2& target) {
    if (LengthSquared(target - engine_.player_.position) < kArrivedDistance * kArrivedDistance) {
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
        if (IsAvailablePickup(entity)) {
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
    const bool keysChanged = ObserveKeys();
    const bool worldChanged = ObserveEntities();
    ObserveSupplies();
    if (keysChanged || worldChanged) {
        sinceProgress_ = 0.0;
        stuckStrikes_ = 0;
        bestPotential_ = std::numeric_limits<double>::max();
    }
}

bool AutoplayBot::ObserveKeys() {
    const unsigned mask = KeyMask(engine_.keys_);
    if (mask == keyMask_) {
        return false;
    }
    for (int i = 0; i < kKeyColorCount; ++i) {
        if ((mask & KeyBit(i)) != 0 && (keyMask_ & KeyBit(i)) == 0) {
            keysCollected_.emplace_back(KeyColorName(kKeyColors[i]));
        }
    }
    keyMask_ = mask;
    replanRequested_ = true;
    return true;
}

bool AutoplayBot::ObserveEntities() {
    bool progress = false;
    int pickupsLeft = 0;
    int enemyHealth = 0;
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (IsAvailablePickup(entity)) {
            ++pickupsLeft;
        } else if (IsEnemyAlive(entity)) {
            enemyHealth += entity.health;
        } else if (entity.type == EntityType::Door) {
            progress = ObserveDoor(entity) || progress;
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
    return progress;
}

bool AutoplayBot::ObserveDoor(const Entity& door) {
    bool changed = false;
    for (DoorState& known : doors_) {
        if (known.id != door.id || known.open == door.doorOpen) {
            continue;
        }
        known.open = door.doorOpen;
        if (door.doorOpen) {
            ++doorsOpened_;
            if (door.doorLock != KeyColor::None) {
                AddUnique(keysUsed_, KeyColorName(door.doorLock));
            }
        }
        changed = true;
        replanRequested_ = true;
    }
    return changed;
}

// Low health or ammo changes the goal (a detour to a pickup), so either change asks for a new plan.
void AutoplayBot::ObserveSupplies() {
    const bool lowHealth = engine_.playerHealth_ < engine_.playerMaxHealth_ &&
                           static_cast<float>(engine_.playerHealth_) <=
                               config_.lowHealthFraction * static_cast<float>(engine_.playerMaxHealth_);
    const bool lowAmmo = engine_.playerAmmo_ < engine_.playerMaxAmmo_ && engine_.playerAmmo_ <= config_.lowAmmo;
    if (lowHealth != lowHealth_ || lowAmmo != lowAmmo_) {
        lowHealth_ = lowHealth;
        lowAmmo_ = lowAmmo;
        replanRequested_ = true;
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
    const Entity* door = DoorEntityAt(x, y);
    return door != nullptr && (door->doorLock == KeyColor::None || engine_.keys_.Has(door->doorLock));
}

const Entity* AutoplayBot::DoorEntityAt(int x, int y) const {
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (entity.type == EntityType::Door && entity.mapX == x && entity.mapY == y) {
            return &entity;
        }
    }
    return nullptr;
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
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const Vec2i cell = queue[head];
        for (const Vec2i& step : kNeighbourSteps) {
            const int nx = cell.x + step.x;
            const int ny = cell.y + step.y;
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
            } else if (map.GetCell(x, y) == TileType::Door && DoorEntityAt(x, y) == nullptr) {
                deadDoors.push_back(FormatCell(Vec2i{x, y}));
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
    AppendList(reason, "; locked without a reachable key:", locked);
    AppendList(reason, "; 'D' cells without a door entity (never open):", deadDoors);
    return reason;
}

bool AutoplayBot::NeedsReplan() const {
    if (goal_ == GoalKind::None) {
        return true;
    }
    // The next step became impassable (e.g. a door closed and the key is not held).
    return next_ < path_.size() && !IsPassable(path_[next_].x, path_[next_].y);
}

struct AutoplayBot::Candidate {
    int distance = -1;  // path length in cells; -1 = none found
    Vec2i cell{-1, -1};
    Vec2i exit{-1, -1};  // the exit switch beside `cell` (exit goals only)
    int entityId = -1;   // the pickup (pickup goals only)

    // Keeps the nearer of this candidate and a reachable new one (the first of equals).
    void Consider(int newDistance, const Vec2i& newCell, const Vec2i& newExit, int newEntityId) {
        if (newDistance >= 0 && (distance < 0 || newDistance < distance)) {
            *this = Candidate{newDistance, newCell, newExit, newEntityId};
        }
    }
};

void AutoplayBot::Plan() {
    replanRequested_ = false;
    ++replans_;
    SearchFromPlayer();

    Candidate health;
    Candidate ammo;
    Candidate key;
    FindPickupGoals(health, ammo, key);
    const Candidate exitCandidate = FindExitGoal();

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

// The nearest reachable pickups worth a walk: health and ammo only when low, key cards only
// when not held and they open a door of this level.
void AutoplayBot::FindPickupGoals(Candidate& health, Candidate& ammo, Candidate& key) const {
    for (const Entity& entity : engine_.entityManager_.Entities()) {
        if (!IsAvailablePickup(entity)) {
            continue;
        }
        const Vec2i cell = CellOf(entity.position);
        const int distance = DistanceTo(cell);
        if (entity.pickupType == PickupType::Health && lowHealth_) {
            health.Consider(distance, cell, Vec2i{-1, -1}, entity.id);
        } else if (entity.pickupType == PickupType::Ammo && lowAmmo_) {
            ammo.Consider(distance, cell, Vec2i{-1, -1}, entity.id);
        } else if (entity.pickupType == PickupType::Key && !engine_.keys_.Has(entity.keyColor) &&
                   KeyOpensADoor(entity.keyColor)) {
            key.Consider(distance, cell, Vec2i{-1, -1}, entity.id);
        }
    }
}

bool AutoplayBot::KeyOpensADoor(KeyColor color) const {
    const std::vector<Entity>& entities = engine_.entityManager_.Entities();
    return std::any_of(entities.begin(), entities.end(),
                       [color](const Entity& door) { return door.type == EntityType::Door && door.doorLock == color; });
}

// The nearest reachable floor cell beside an exit switch.
AutoplayBot::Candidate AutoplayBot::FindExitGoal() const {
    const Map& map = engine_.map_;
    Candidate best;
    for (int y = 0; y < map.Height(); ++y) {
        for (int x = 0; x < map.Width(); ++x) {
            if (map.GetCell(x, y) != TileType::Exit) {
                continue;
            }
            for (const Vec2i& step : kNeighbourSteps) {
                const Vec2i beside{x + step.x, y + step.y};
                best.Consider(DistanceTo(beside), beside, Vec2i{x, y}, -1);
            }
        }
    }
    return best;
}

// --- Walking -----------------------------------------------------------------------------------

void AutoplayBot::FollowPath() {
    const Vec2 position = engine_.player_.position;
    const Vec2i cell = CellOf(position);

    // Advance to the furthest path cell the player stands in (the bot may cut corners).
    for (std::size_t i = next_; i < path_.size(); ++i) {
        if (path_[i] == cell) {
            next_ = i + 1;
        }
    }
    if (next_ >= path_.size()) {
        ActAtGoal(position);
        return;
    }
    const Vec2i ahead = path_[next_];
    if (std::abs(ahead.x - cell.x) + std::abs(ahead.y - cell.y) > 1) {
        replanRequested_ = true;  // pushed off the path (a fight, a door, an unstick move)
        return;
    }
    if (engine_.map_.GetCell(ahead.x, ahead.y) == TileType::Door) {
        OpenDoorAhead(ahead);
        return;
    }
    SteerTowards(LookaheadTarget(position));
}

// Walks up to the closed door, then presses Interact (once per kInteractRepeat).
void AutoplayBot::OpenDoorAhead(const Vec2i& doorCell) {
    const Vec2 doorCenter = CellCenter(doorCell);
    if (Length(doorCenter - engine_.player_.position) > kDoorPressDistance) {
        SteerTowards(doorCenter);
        return;
    }
    TurnTowards(doorCenter);
    PressInteract();
}

// Look ahead: the furthest of the next path cells that a body can reach in a straight line.
Vec2 AutoplayBot::LookaheadTarget(const Vec2& position) const {
    const Map& map = engine_.map_;
    Vec2 target = CellCenter(path_[next_]);
    for (std::size_t j = next_ + 1; j < path_.size() && j < next_ + kLookahead; ++j) {
        if (map.GetCell(path_[j].x, path_[j].y) == TileType::Door ||
            !HasClearCorridor(map, position, CellCenter(path_[j]), kCorridorRadius)) {
            break;
        }
        target = CellCenter(path_[j]);
    }
    return target;
}

// At the goal cell: use the exit switch, or walk onto the pickup.
void AutoplayBot::ActAtGoal(const Vec2& position) {
    if (goal_ == GoalKind::Exit) {
        const Vec2 standAt = CellCenter(goalCell_);
        if (Length(standAt - position) > kExitStandTolerance) {
            SteerTowards(standAt);
            return;
        }
        if (TurnTowards(CellCenter(exitCell_)) < kExitFacing) {
            PressInteract();
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

// A report with the run's settings and nothing played yet.
AutoplayRunReport NewRunReport(const AutoplayConfig& config, bool wholeCampaign) {
    AutoplayRunReport report{};
    report.wholeCampaign = wholeCampaign;
    report.fixedStepSeconds = config.fixedStepSeconds;
    report.timeLimitSeconds = config.timeLimitSeconds;
    return report;
}

// Why a session cannot run on this engine, or "" when it can.
std::string_view SessionStartProblem(const Engine& engine, bool wholeCampaign) {
    if (!engine.IsRunning()) {
        return "the engine is not running";
    }
    if (wholeCampaign && engine.GetState() != GameState::Menu) {
        return "a whole-campaign run must start on the main menu";
    }
    return "";
}

// A safety net in frames on top of the per-level time limit (menus and screens take a few).
std::uint64_t FrameLimit(const AutoplayConfig& config, int levelsToPlay) {
    const double framesPerLevel = (config.timeLimitSeconds + kFrameLimitMarginSeconds) / config.fixedStepSeconds;
    return static_cast<std::uint64_t>(framesPerLevel * static_cast<double>(std::max(1, levelsToPlay)));
}

// The first Gameplay frame of a level: a new level report, with the stats the level starts with.
LevelTracker BeginLevelReport(const Engine& engine, AutoplayRunReport& report, std::vector<PlayerStats>& entered,
                              TraceRecorder& recorder) {
    report.levels.emplace_back();
    const LevelTracker level{&report.levels.back(), engine.currentLevelIndex_, false};
    FillLevelIdentity(engine, level.levelIndex, *level.report);
    level.report->healthStart = engine.playerHealth_;
    level.report->ammoStart = engine.playerAmmo_;
    entered.push_back(engine.CurrentStats());
    recorder.BeginLevel(engine, level.report->levelNumber);
    return level;
}

// After a frame of the level being played: if the level ended (the exit was used, the player
// died, the bot gave up, the time ran out or the game left play), fills `report` and returns true.
bool EndLevelIfOver(const Engine& engine, const AutoplayBot& bot, const AutoplayConfig& config,
                    AutoplayLevelReport& report, std::vector<PlayerStats>& completed) {
    const GameState state = engine.GetState();
    if (state == GameState::LevelComplete) {
        completed.push_back(engine.CurrentStats());
        FinishLevel(engine, bot, report, AutoplayResult::Complete, "", engine.flow_.LastResults());
        return true;
    }
    if (state == GameState::GameOver) {
        FinishLevel(engine, bot, report, AutoplayResult::Died,
                    "health reached 0 at " + FormatPosition(engine.player_.position) + " after " +
                        FormatSeconds(engine.levelTimeSeconds_) + " s",
                    engine.CurrentLevelResults());
        return true;
    }
    if (bot.Failed()) {
        FinishLevel(engine, bot, report, AutoplayResult::Stuck, bot.FailureReason(), engine.CurrentLevelResults());
        return true;
    }
    if (engine.levelTimeSeconds_ >= config.timeLimitSeconds) {
        FinishLevel(engine, bot, report, AutoplayResult::Timeout,
                    "time limit of " + FormatSeconds(config.timeLimitSeconds) + " s reached at " +
                        FormatPosition(engine.player_.position),
                    engine.CurrentLevelResults());
        return true;
    }
    if (state != GameState::Gameplay) {
        FinishLevel(engine, bot, report, AutoplayResult::Stuck,
                    std::string("the game left play unexpectedly (state ") + GameStateToString(state) + ")",
                    engine.CurrentLevelResults());
        return true;
    }
    return false;
}

// --all: the levels after a failure were not played.
void AddNotPlayedLevels(const Engine& engine, AutoplayRunReport& report) {
    for (int index = static_cast<int>(report.levels.size()); index < report.campaignLevels; ++index) {
        AutoplayLevelReport notPlayed{};
        FillLevelIdentity(engine, index, notPlayed);
        notPlayed.reason = "an earlier level was not completed";
        report.levels.push_back(notPlayed);
    }
}

// Carry-over, checked independently of GameFlow: level 1 starts with the gameplay.json initial
// values; every later level with what the previous one ended with, clamped to the maxima
// (health at least 1).
void CheckCarryOver(const GameplayDefinitions& gameplay, const std::vector<PlayerStats>& entered,
                    const std::vector<PlayerStats>& completed, std::vector<std::string>& problems) {
    for (std::size_t i = 0; i < entered.size(); ++i) {
        PlayerStats expected{gameplay.player.initialHealth, gameplay.weapon.initialAmmo};
        if (i > 0 && i - 1 < completed.size()) {
            expected.health = std::clamp(completed[i - 1].health, 1, gameplay.player.maxHealth);
            expected.ammo = std::clamp(completed[i - 1].ammo, 0, gameplay.weapon.maxAmmo);
        }
        if (entered[i].health != expected.health || entered[i].ammo != expected.ammo) {
            problems.push_back("level " + std::to_string(i + 1) + " started with health " +
                               std::to_string(entered[i].health) + ", ammo " + std::to_string(entered[i].ammo) +
                               "; expected health " + std::to_string(expected.health) + ", ammo " +
                               std::to_string(expected.ammo));
        }
    }
}

// Progress: completing every level unlocks (and saves) up to the last one.
void CheckSavedProgress(const Engine& engine, AutoplayRunReport& report) {
    ProgressData saved{};
    std::string message;
    const std::string path = PathToUtf8(engine.prefDir_ / kProgressFileName);
    if (LoadProgressFile(path, saved, message) != ProgressLoadResult::Loaded) {
        report.carryOverProblems.push_back("progress was not saved: " + message);
    } else if (saved.highestUnlocked != report.campaignLevels) {
        report.carryOverProblems.push_back("saved progress unlocks level " + std::to_string(saved.highestUnlocked) +
                                           ", expected " + std::to_string(report.campaignLevels));
    }
}

// --all, after the last frame: the levels not played, the carry-over and the saved progress.
void FinishCampaignReport(const Engine& engine, const std::vector<PlayerStats>& entered,
                          const std::vector<PlayerStats>& completed, AutoplayRunReport& report) {
    AddNotPlayedLevels(engine, report);
    report.carryOverChecked = !entered.empty();
    CheckCarryOver(engine.gameplay_, entered, completed, report.carryOverProblems);
    if (report.campaignCompleted && !engine.prefDir_.empty()) {
        CheckSavedProgress(engine, report);
    }
}
}  // namespace

AutoplayRunReport RunAutoplaySession(Engine& engine, const AutoplayConfig& config, bool wholeCampaign,
                                     std::string* trace) {
    AutoplayRunReport report = NewRunReport(config, wholeCampaign);
    report.campaignTitle = engine.campaign_.title;
    report.campaignLevels = static_cast<int>(engine.campaign_.levels.size());
    report.error = SessionStartProblem(engine, wholeCampaign);
    if (!report.error.empty()) {
        return report;
    }

    AutoplayBot bot(engine, config);
    engine.SetInputInjector([&bot](InputState& input) { bot.Fill(input); });
    TraceRecorder recorder(trace);

    std::vector<PlayerStats> entered;
    std::vector<PlayerStats> completed;
    LevelTracker level{};
    const std::uint64_t maxFrames = FrameLimit(config, wholeCampaign ? report.campaignLevels : 1);
    bool stop = false;
    std::uint64_t frame = 0;
    for (; frame < maxFrames && !stop && engine.IsRunning(); ++frame) {
        const GameState before = engine.GetState();
        if (before == GameState::Gameplay && (level.report == nullptr || level.ended)) {
            level = BeginLevelReport(engine, report, entered, recorder);
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
        level.ended = EndLevelIfOver(engine, bot, config, *level.report, completed);
        if (level.ended) {
            recorder.EndLevel(engine, *level.report);
            stop = !wholeCampaign || level.report->result != AutoplayResult::Complete;
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
        FinishCampaignReport(engine, entered, completed, report);
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
        AutoplayRunReport report = NewRunReport(options.config, options.level == 0);
        report.error = "the engine did not start (see the messages above)";
        return report;
    }
    AutoplayRunReport report = RunAutoplaySession(engine, options.config, options.level == 0, options.trace);
    engine.Shutdown();
    return report;
}
