#pragma once

#include <cstdint>

enum class GameState : std::uint8_t {
    Menu = 0,
    Gameplay,
    Paused,
    GameOver,
    Victory
};

inline const char* GameStateToString(GameState state) {
    switch (state) {
    case GameState::Menu:
        return "Menu";
    case GameState::Gameplay:
        return "Gameplay";
    case GameState::Paused:
        return "Paused";
    case GameState::GameOver:
        return "GameOver";
    case GameState::Victory:
        return "Victory";
    default:
        return "Unknown";
    }
}
