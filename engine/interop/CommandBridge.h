#pragma once

#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

class Engine;

enum class InteropCommandType : std::int32_t {
    None = 0,
    SetGameState,
    ResetLevel,
    LoadLevel,
    SpawnEntity,
    DestroyEntity,
    DamagePlayer,
    HealPlayer,
    AddAmmo,
    PlayerAttack,
    DamageEnemy,
    PlaySound,
    SpawnParticle,
    OpenDoor,
    CloseDoor,
    ShowMessage
};

struct InteropCommand {
    InteropCommandType type = InteropCommandType::None;
    std::int32_t intArg1 = 0;
    std::int32_t intArg2 = 0;
    float floatArg1 = 0.0f;
    float floatArg2 = 0.0f;
    float floatArg3 = 0.0f;
    char strArg[128] = {0};

    void SetString(const char* str) {
        if (str == nullptr) {
            strArg[0] = '\0';
            return;
        }
        std::strncpy(strArg, str, sizeof(strArg) - 1);
        strArg[sizeof(strArg) - 1] = '\0';
    }
};

enum class InteropEventType : std::int32_t {
    None = 0,
    StateChanged,
    PlayerDamaged,
    PlayerDied,
    PlayerAttacked,
    EnemyAlerted,
    EnemyDamaged,
    EnemyDied,
    PickupCollected,
    DoorToggled,
    TriggerActivated,
    LevelLoaded
};

struct InteropEvent {
    InteropEventType type = InteropEventType::None;
    std::int32_t intArg1 = 0;
    std::int32_t intArg2 = 0;
    float floatArg1 = 0.0f;
    float floatArg2 = 0.0f;
    float floatArg3 = 0.0f;
    char strArg[128] = {0};

    void SetString(const char* str) {
        if (str == nullptr) {
            strArg[0] = '\0';
            return;
        }
        std::strncpy(strArg, str, sizeof(strArg) - 1);
        strArg[sizeof(strArg) - 1] = '\0';
    }
};

class CommandBridge {
public:
    CommandBridge() = default;

    void EnqueueCommand(const InteropCommand& cmd);
    bool PollCommand(InteropCommand& outCmd);
    void ProcessCommands(Engine& engine);

    void PushEvent(const InteropEvent& evt);
    bool PollEvent(InteropEvent& outEvt);
    bool HasEvents() const;

    void Clear();

private:
    mutable std::mutex mutex_{};
    std::deque<InteropCommand> commands_{};
    std::deque<InteropEvent> events_{};
};
