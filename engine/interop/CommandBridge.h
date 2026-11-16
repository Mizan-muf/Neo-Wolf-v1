#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

class Engine;

// Copies a NUL-terminated string into a fixed-size buffer, truncating if needed.
// The result is always NUL-terminated. Portable replacement for strncpy, which
// MSVC flags as unsafe (C4996).
inline void CopyBoundedString(char* dest, std::size_t destSize, const char* src) {
    if (dest == nullptr || destSize == 0) {
        return;
    }
    std::size_t len = 0;
    if (src != nullptr) {
        while (len + 1 < destSize && src[len] != '\0') {
            ++len;
        }
        std::memcpy(dest, src, len);
    }
    dest[len] = '\0';
}

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
        CopyBoundedString(strArg, sizeof(strArg), str);
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
    LevelLoaded,
    // Appended in Phase 5: the exit switch was used. intArg1 = campaign level number
    // (1-based, 0 for a level loaded by path), intArg2 = health, floatArg1 = seconds played.
    LevelCompleted
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
        CopyBoundedString(strArg, sizeof(strArg), str);
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
