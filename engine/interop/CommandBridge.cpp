#include "engine/interop/CommandBridge.h"

#include <algorithm>
#include <string>
#include <utility>

#include "engine/core/Engine.h"
#include "engine/world/EnemyAi.h"

namespace {
constexpr const char* kDerivedEntitiesDir = "game/defs/";
constexpr const char* kEntitiesSuffix = ".entities.json";

// The LoadLevel command's strArg: "<map>;<entities>", "<map>,<entities>", or only "<map>", for
// which the entity file is derived from the map's file name (docs/C_API.md, LoadLevel).
std::pair<std::string, std::string> SplitLevelPaths(const std::string& paths) {
    auto sep = paths.find(';');
    if (sep == std::string::npos) {
        sep = paths.find(',');
    }
    if (sep != std::string::npos) {
        return {paths.substr(0, sep), paths.substr(sep + 1)};
    }
    std::string base = paths;
    const auto dot = base.rfind('.');
    if (dot != std::string::npos) {
        base = base.substr(0, dot);
    }
    const auto slash = base.rfind('/');
    if (slash != std::string::npos) {
        return {paths, kDerivedEntitiesDir + base.substr(slash + 1) + kEntitiesSuffix};
    }
    return {paths, base + kEntitiesSuffix};
}

void LoadLevelCommand(Engine& engine, const InteropCommand& cmd) {
    const auto [mapPath, entitiesPath] = SplitLevelPaths(cmd.strArg);
    engine.LoadLevel(mapPath, entitiesPath);
}

void DamagePlayerCommand(Engine& engine, const InteropCommand& cmd) {
    engine.playerHealth_ = std::max(0, engine.playerHealth_ - cmd.intArg1);
    if (engine.playerHealth_ <= 0) {
        engine.SetState(GameState::GameOver);
    }
}

void DamageEnemyCommand(Engine& engine, const InteropCommand& cmd) {
    Entity* enemy = engine.entityManager_.GetEntityById(cmd.intArg1);
    if (enemy == nullptr || !IsEnemyAlive(*enemy)) {
        return;
    }
    enemy->health -= cmd.intArg2;
    enemy->hitFlashTimer = kEnemyHitFlashSeconds;
    if (enemy->health <= 0) {
        enemy->health = 0;
        enemy->enemyState = EnemyState::Dead;  // stays visible as a corpse
        enemy->active = false;
        enemy->path.clear();
    }
}

// OpenDoor / CloseDoor: sets the map cell (intArg1, intArg2); negative cells are ignored.
void SetDoorCellCommand(Engine& engine, const InteropCommand& cmd, TileType tile) {
    if (cmd.intArg1 >= 0 && cmd.intArg2 >= 0) {
        engine.map_.SetCell(cmd.intArg1, cmd.intArg2, tile);
    }
}

void ShowMessageCommand(Engine& engine, const InteropCommand& cmd) {
    if (cmd.floatArg1 > 0.0f) {
        engine.ShowMessage(cmd.strArg, cmd.floatArg1);
    } else {
        engine.ShowMessage(cmd.strArg);  // the default duration
    }
}

void RunCommand(Engine& engine, const InteropCommand& cmd) {
    switch (cmd.type) {
        case InteropCommandType::SetGameState:
            // Menu .. CampaignComplete; Victory (4, deprecated) selects LevelComplete.
            if (IsValidGameStateValue(cmd.intArg1)) {
                engine.SetState(static_cast<GameState>(cmd.intArg1));
            }
            break;
        case InteropCommandType::ResetLevel:
            engine.ResetLevel();
            break;
        case InteropCommandType::LoadLevel:
            LoadLevelCommand(engine, cmd);
            break;
        case InteropCommandType::SpawnEntity:
            engine.SpawnEntity(static_cast<EntityType>(cmd.intArg1), {cmd.floatArg1, cmd.floatArg2});
            break;
        case InteropCommandType::DestroyEntity:
            engine.entityManager_.DestroyEntity(cmd.intArg1);
            break;
        case InteropCommandType::DamagePlayer:
            DamagePlayerCommand(engine, cmd);
            break;
        case InteropCommandType::HealPlayer:
            engine.playerHealth_ = std::min(engine.playerMaxHealth_, engine.playerHealth_ + cmd.intArg1);
            break;
        case InteropCommandType::AddAmmo:
            engine.playerAmmo_ = std::min(engine.playerMaxAmmo_, engine.playerAmmo_ + cmd.intArg1);
            break;
        case InteropCommandType::PlayerAttack:
            engine.PlayerAttack();
            break;
        case InteropCommandType::DamageEnemy:
            DamageEnemyCommand(engine, cmd);
            break;
        case InteropCommandType::PlaySound:
            engine.audio_.PlayEffect(cmd.strArg);
            break;
        case InteropCommandType::SpawnParticle:
            engine.particleManager_.SpawnHitSpark({cmd.floatArg1, cmd.floatArg2});
            break;
        case InteropCommandType::OpenDoor:
            SetDoorCellCommand(engine, cmd, TileType::Empty);
            break;
        case InteropCommandType::CloseDoor:
            SetDoorCellCommand(engine, cmd, TileType::Door);
            break;
        case InteropCommandType::ShowMessage:
            ShowMessageCommand(engine, cmd);
            break;
        default:
            break;  // None, or a value from a newer caller
    }
}
}  // namespace

void CommandBridge::EnqueueCommand(const InteropCommand& cmd) {
    const std::scoped_lock lock(mutex_);
    commands_.push_back(cmd);
}

bool CommandBridge::PollCommand(InteropCommand& outCmd) {
    const std::scoped_lock lock(mutex_);
    if (commands_.empty()) {
        return false;
    }
    outCmd = commands_.front();
    commands_.pop_front();
    return true;
}

void CommandBridge::PushEvent(const InteropEvent& evt) {
    const std::scoped_lock lock(mutex_);
    events_.push_back(evt);
}

bool CommandBridge::PollEvent(InteropEvent& outEvt) {
    const std::scoped_lock lock(mutex_);
    if (events_.empty()) {
        return false;
    }
    outEvt = events_.front();
    events_.pop_front();
    return true;
}

bool CommandBridge::HasEvents() const {
    const std::scoped_lock lock(mutex_);
    return !events_.empty();
}

void CommandBridge::Clear() {
    const std::scoped_lock lock(mutex_);
    commands_.clear();
    events_.clear();
}

void CommandBridge::ProcessCommands(Engine& engine) {
    // Run the commands outside the lock: a command may make the engine push events.
    std::deque<InteropCommand> localCommands;
    {
        const std::scoped_lock lock(mutex_);
        localCommands.swap(commands_);
    }
    for (const InteropCommand& cmd : localCommands) {
        RunCommand(engine, cmd);
    }
}
