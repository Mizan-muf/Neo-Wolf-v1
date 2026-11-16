#pragma once

// The command and event queues behind the C API (docs/C_API.md). Commands are queued from any
// thread and run at the start of the next Engine::Update; the engine pushes events that a caller
// polls. Both queues share one mutex. InteropCommand and InteropEvent are part of the C API:
// their layout and enum values must not change (enums are append only, never renumbered).

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>

class Engine;

// Capacity of InteropCommand::strArg and InteropEvent::strArg, the terminating NUL included.
inline constexpr std::size_t kInteropStringCapacity = 128;

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
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): C API struct layout
    char strArg[kInteropStringCapacity] = {0};

    // Copies `str` (null: empty), truncated to fit.
    void SetString(const char* str) { CopyBoundedString(strArg, sizeof(strArg), str); }
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
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-c-arrays,modernize-avoid-c-arrays): C API struct layout
    char strArg[kInteropStringCapacity] = {0};

    // Copies `str` (null: empty), truncated to fit.
    void SetString(const char* str) { CopyBoundedString(strArg, sizeof(strArg), str); }
};

class CommandBridge {
public:
    CommandBridge() = default;

    void EnqueueCommand(const InteropCommand& cmd);
    bool PollCommand(InteropCommand& outCmd);
    // Runs every queued command on `engine`, in the order queued (Engine::Update calls it).
    void ProcessCommands(Engine& engine);

    void PushEvent(const InteropEvent& evt);
    bool PollEvent(InteropEvent& outEvt);
    bool HasEvents() const;

    void Clear();

private:
    mutable std::mutex mutex_;
    std::deque<InteropCommand> commands_;
    std::deque<InteropEvent> events_;
};
