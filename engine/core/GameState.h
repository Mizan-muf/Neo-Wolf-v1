#pragma once

// The game's top-level states (menus, play, pause, end screens).

#include <cstdint>

// The values are part of the C API (nw_engine_get_state / nw_engine_set_state, the
// StateChanged event and the SetGameState command): append only, never renumber.
enum class GameState : std::uint8_t {
    Menu = 0,
    Gameplay,
    Paused,
    GameOver,
    // Deprecated since Phase 5 (value kept for the C API): the game never enters this state.
    // Setting it through the C API or the command bridge selects LevelComplete instead.
    Victory,
    LevelComplete,    // a level's exit switch was used; Enter continues
    CampaignComplete  // the last level was completed; credits screen
};

// The highest valid value, for range checks on integers from the C API.
inline constexpr GameState kLastGameState = GameState::CampaignComplete;

inline bool IsValidGameStateValue(int value) {
    return value >= static_cast<int>(GameState::Menu) && value <= static_cast<int>(kLastGameState);
}

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
            return "Victory";  // deprecated alias of LevelComplete
        case GameState::LevelComplete:
            return "LevelComplete";
        case GameState::CampaignComplete:
            return "CampaignComplete";
        default:
            return "Unknown";
    }
}
