#include "engine/interop/EngineInterop.h"

#include <cmath>

#include "engine/core/Engine.h"
#include "engine/core/GameState.h"

namespace {
Engine* ToEngine(void* handle) {
    return static_cast<Engine*>(handle);
}

const Engine* ToEngineConst(const void* handle) {
    return static_cast<const Engine*>(handle);
}
} // namespace

void* nw_engine_create() {
    return new Engine();
}

void nw_engine_destroy(void* handle) {
    delete ToEngine(handle);
}

int nw_engine_init(void* handle) {
    Engine* engine = ToEngine(handle);
    if (engine == nullptr) return 0;
    return engine->Init() ? 1 : 0;
}

void nw_engine_update(void* handle, double deltaSeconds) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr) {
        engine->Update(deltaSeconds);
    }
}

void nw_engine_render(void* handle) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr) {
        engine->Render();
    }
}

void nw_engine_shutdown(void* handle) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr) {
        engine->Shutdown();
    }
}

int nw_engine_is_running(const void* handle) {
    const Engine* engine = ToEngineConst(handle);
    return (engine != nullptr && engine->IsRunning()) ? 1 : 0;
}

int nw_engine_get_state(const void* handle) {
    const Engine* engine = ToEngineConst(handle);
    if (engine == nullptr) return 0;
    return static_cast<int>(engine->GetState());
}

void nw_engine_set_state(void* handle, int state) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr) {
        engine->SetState(static_cast<GameState>(state));
    }
}

void nw_engine_reset_level(void* handle) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr) {
        engine->ResetLevel();
    }
}

int nw_engine_load_level(void* handle, const char* mapPath, const char* entitiesPath) {
    Engine* engine = ToEngine(handle);
    if (engine == nullptr || mapPath == nullptr || entitiesPath == nullptr) {
        return 0;
    }
    return engine->LoadLevel(mapPath, entitiesPath) ? 1 : 0;
}

void nw_engine_get_player_pos(const void* handle, float* outX, float* outY, float* outAngle) {
    const Engine* engine = ToEngineConst(handle);
    if (engine == nullptr) return;
    if (outX) *outX = engine->player_.position.x;
    if (outY) *outY = engine->player_.position.y;
    if (outAngle) *outAngle = engine->player_.directionAngleRadians;
}

void nw_engine_set_player_pos(void* handle, float x, float y, float angle) {
    Engine* engine = ToEngine(handle);
    if (engine == nullptr) return;
    engine->player_.position = {x, y};
    engine->player_.directionAngleRadians = angle;
}

void nw_engine_get_player_stats(const void* handle, int* outHealth, int* outAmmo) {
    const Engine* engine = ToEngineConst(handle);
    if (engine == nullptr) return;
    if (outHealth) *outHealth = engine->playerHealth_;
    if (outAmmo) *outAmmo = engine->playerAmmo_;
}

void nw_engine_set_player_stats(void* handle, int health, int ammo) {
    Engine* engine = ToEngine(handle);
    if (engine == nullptr) return;
    engine->playerHealth_ = health;
    engine->playerAmmo_ = ammo;
}

void nw_engine_player_attack(void* handle) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr) {
        engine->PlayerAttack();
    }
}

int nw_engine_get_entity_count(const void* handle) {
    const Engine* engine = ToEngineConst(handle);
    if (engine == nullptr) return 0;
    return static_cast<int>(engine->entityManager_.Entities().size());
}

int nw_engine_get_entity(
    const void* handle,
    int index,
    int* outId,
    int* outType,
    float* outX,
    float* outY,
    int* outActive) {
    const Engine* engine = ToEngineConst(handle);
    if (engine == nullptr) return 0;
    const auto& entities = engine->entityManager_.Entities();
    if (index < 0 || index >= static_cast<int>(entities.size())) {
        return 0;
    }
    const Entity& ent = entities[static_cast<std::size_t>(index)];
    if (outId) *outId = ent.id;
    if (outType) *outType = static_cast<int>(ent.type);
    if (outX) *outX = ent.position.x;
    if (outY) *outY = ent.position.y;
    if (outActive) *outActive = ent.active ? 1 : 0;
    return 1;
}

int nw_engine_spawn_entity(void* handle, int type, float x, float y) {
    Engine* engine = ToEngine(handle);
    if (engine == nullptr) return -1;
    Entity& ent = engine->entityManager_.CreateEntity(static_cast<EntityType>(type), {x, y});
    return ent.id;
}

int nw_engine_destroy_entity(void* handle, int id) {
    Engine* engine = ToEngine(handle);
    if (engine == nullptr) return 0;
    return engine->entityManager_.DestroyEntity(id) ? 1 : 0;
}

void nw_engine_play_sound(void* handle, const char* soundId) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr && soundId != nullptr) {
        engine->audio_.PlayEffect(soundId);
    }
}

void nw_engine_spawn_particle(
    void* handle,
    float x,
    float y,
    float vx,
    float vy,
    float life,
    std::uint32_t color) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr) {
        engine->particleManager_.SpawnHitSpark({x, y});
    }
}

void nw_engine_show_message(void* handle, const char* message, float duration) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr && message != nullptr) {
        engine->ShowMessage(message, duration);
    }
}

int nw_engine_send_command(void* handle, const InteropCommand* cmd) {
    Engine* engine = ToEngine(handle);
    if (engine == nullptr || cmd == nullptr) return 0;
    engine->bridge_.EnqueueCommand(*cmd);
    return 1;
}

int nw_engine_poll_event(void* handle, InteropEvent* outEvt) {
    Engine* engine = ToEngine(handle);
    if (engine == nullptr || outEvt == nullptr) return 0;
    return engine->bridge_.PollEvent(*outEvt) ? 1 : 0;
}

int nw_engine_has_pending_events(const void* handle) {
    const Engine* engine = ToEngineConst(handle);
    if (engine == nullptr) return 0;
    return engine->bridge_.HasEvents() ? 1 : 0;
}

void nw_engine_clear_bridge(void* handle) {
    Engine* engine = ToEngine(handle);
    if (engine != nullptr) {
        engine->bridge_.Clear();
    }
}
