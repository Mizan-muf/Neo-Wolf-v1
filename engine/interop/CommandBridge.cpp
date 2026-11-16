#include "engine/interop/CommandBridge.h"

#include <algorithm>

#include "engine/core/Engine.h"
#include "engine/core/Log.h"
#include "engine/world/EnemyAi.h"

void CommandBridge::EnqueueCommand(const InteropCommand& cmd) {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back(cmd);
}

bool CommandBridge::PollCommand(InteropCommand& outCmd) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (commands_.empty()) {
        return false;
    }
    outCmd = commands_.front();
    commands_.pop_front();
    return true;
}

void CommandBridge::PushEvent(const InteropEvent& evt) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back(evt);
}

bool CommandBridge::PollEvent(InteropEvent& outEvt) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (events_.empty()) {
        return false;
    }
    outEvt = events_.front();
    events_.pop_front();
    return true;
}

bool CommandBridge::HasEvents() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return !events_.empty();
}

void CommandBridge::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.clear();
    events_.clear();
}

void CommandBridge::ProcessCommands(Engine& engine) {
    std::deque<InteropCommand> localCommands;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        localCommands.swap(commands_);
    }

    for (const InteropCommand& cmd : localCommands) {
        switch (cmd.type) {
        case InteropCommandType::SetGameState: {
            // Menu .. CampaignComplete; Victory (4, deprecated) selects LevelComplete.
            if (IsValidGameStateValue(cmd.intArg1)) {
                engine.SetState(static_cast<GameState>(cmd.intArg1));
            }
            break;
        }
        case InteropCommandType::ResetLevel:
            engine.ResetLevel();
            break;
        case InteropCommandType::LoadLevel: {
            std::string str = cmd.strArg;
            std::string mapPath = str;
            std::string entitiesPath = "";
            auto sep = str.find(';');
            if (sep == std::string::npos) {
                sep = str.find(',');
            }
            if (sep != std::string::npos) {
                mapPath = str.substr(0, sep);
                entitiesPath = str.substr(sep + 1);
            } else {
                std::string base = mapPath;
                auto dot = base.rfind('.');
                if (dot != std::string::npos) {
                    base = base.substr(0, dot);
                }
                auto slash = base.rfind('/');
                if (slash != std::string::npos) {
                    std::string levelName = base.substr(slash + 1);
                    entitiesPath = "game/defs/" + levelName + ".entities.json";
                } else {
                    entitiesPath = base + ".entities.json";
                }
            }
            engine.LoadLevel(mapPath, entitiesPath);
            break;
        }
        case InteropCommandType::SpawnEntity:
            engine.entityManager_.CreateEntity(static_cast<EntityType>(cmd.intArg1), {cmd.floatArg1, cmd.floatArg2});
            break;
        case InteropCommandType::DestroyEntity:
            engine.entityManager_.DestroyEntity(cmd.intArg1);
            break;
        case InteropCommandType::DamagePlayer:
            engine.playerHealth_ = std::max(0, engine.playerHealth_ - cmd.intArg1);
            if (engine.playerHealth_ <= 0) {
                engine.SetState(GameState::GameOver);
            }
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
        case InteropCommandType::DamageEnemy: {
            Entity* enemy = engine.entityManager_.GetEntityById(cmd.intArg1);
            if (enemy != nullptr && IsEnemyAlive(*enemy)) {
                enemy->health -= cmd.intArg2;
                enemy->hitFlashTimer = kEnemyHitFlashSeconds;
                if (enemy->health <= 0) {
                    enemy->health = 0;
                    enemy->enemyState = EnemyState::Dead;  // stays visible as a corpse
                    enemy->active = false;
                    enemy->path.clear();
                }
            }
            break;
        }
        case InteropCommandType::PlaySound:
            engine.audio_.PlayEffect(cmd.strArg);
            break;
        case InteropCommandType::SpawnParticle:
            engine.particleManager_.SpawnHitSpark({cmd.floatArg1, cmd.floatArg2});
            break;
        case InteropCommandType::OpenDoor:
            if (cmd.intArg1 >= 0 && cmd.intArg2 >= 0) {
                engine.map_.SetCell(cmd.intArg1, cmd.intArg2, TileType::Empty);
            }
            break;
        case InteropCommandType::CloseDoor:
            if (cmd.intArg1 >= 0 && cmd.intArg2 >= 0) {
                engine.map_.SetCell(cmd.intArg1, cmd.intArg2, TileType::Door);
            }
            break;
        case InteropCommandType::ShowMessage:
            engine.ShowMessage(cmd.strArg, cmd.floatArg1 > 0.0f ? cmd.floatArg1 : 2.0f);
            break;
        default:
            break;
        }
    }
}
