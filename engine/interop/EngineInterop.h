#pragma once

#include <cstdint>

#include "engine/interop/CommandBridge.h"

#ifdef __cplusplus
extern "C" {
#endif

// Engine lifecycle
void* nw_engine_create();
void nw_engine_destroy(void* handle);
int nw_engine_init(void* handle);
void nw_engine_update(void* handle, double deltaSeconds);
void nw_engine_render(void* handle);
void nw_engine_shutdown(void* handle);
int nw_engine_is_running(const void* handle);

// State management
int nw_engine_get_state(const void* handle);
void nw_engine_set_state(void* handle, int state);
void nw_engine_reset_level(void* handle);
int nw_engine_load_level(void* handle, const char* mapPath, const char* entitiesPath);

// Player state
void nw_engine_get_player_pos(const void* handle, float* outX, float* outY, float* outAngle);
void nw_engine_set_player_pos(void* handle, float x, float y, float angle);
void nw_engine_get_player_stats(const void* handle, int* outHealth, int* outAmmo);
void nw_engine_set_player_stats(void* handle, int health, int ammo);
void nw_engine_player_attack(void* handle);

// Entity management
int nw_engine_get_entity_count(const void* handle);
int nw_engine_get_entity(
    const void* handle,
    int index,
    int* outId,
    int* outType,
    float* outX,
    float* outY,
    int* outActive);
int nw_engine_spawn_entity(void* handle, int type, float x, float y);
int nw_engine_destroy_entity(void* handle, int id);

// Audio & FX
void nw_engine_play_sound(void* handle, const char* soundId);
void nw_engine_spawn_particle(
    void* handle,
    float x,
    float y,
    float vx,
    float vy,
    float life,
    std::uint32_t color);
void nw_engine_show_message(void* handle, const char* message, float duration);

// Interop Command Bridge
int nw_engine_send_command(void* handle, const InteropCommand* cmd);
int nw_engine_poll_event(void* handle, InteropEvent* outEvt);
int nw_engine_has_pending_events(const void* handle);
void nw_engine_clear_bridge(void* handle);

#ifdef __cplusplus
}
#endif
