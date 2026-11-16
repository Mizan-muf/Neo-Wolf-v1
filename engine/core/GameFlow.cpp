#include "engine/core/GameFlow.h"

#include <algorithm>

const char* MenuItemLabel(MenuItem item) {
    switch (item) {
    case MenuItem::NewGame:
        return "NEW GAME";
    case MenuItem::Continue:
        return "CONTINUE";
    case MenuItem::LevelSelect:
        return "LEVEL SELECT";
    case MenuItem::Quit:
        return "QUIT";
    }
    return "?";
}

void GameFlow::Configure(const GameFlowConfig& config) {
    config_ = config;
    config_.levelCount = std::max(1, config.levelCount);
    highestUnlocked_ = std::clamp(highestUnlocked_, 1, config_.levelCount);
    currentLevel_ = -1;
    entrySnapshot_ = config_.initial;
    carried_ = config_.initial;
    lastResults_ = {};
    GoToMenu();
}

void GameFlow::SetProgress(bool hasProgress, int highestUnlocked) {
    hasProgress_ = hasProgress;
    highestUnlocked_ = hasProgress ? std::clamp(highestUnlocked, 1, config_.levelCount) : 1;
    menuCursor_ = 0;
}

void GameFlow::SetMenuPage(MenuPage page, int cursor) {
    state_ = GameState::Menu;
    page_ = page;
    if (page == MenuPage::Main) {
        menuCursor_ = std::clamp(cursor, 0, static_cast<int>(MenuItems().size()) - 1);
    } else {
        levelCursor_ = std::clamp(cursor, 0, config_.levelCount - 1);
    }
}

std::vector<MenuItem> GameFlow::MenuItems() const {
    std::vector<MenuItem> items{MenuItem::NewGame};
    if (hasProgress_) {
        items.push_back(MenuItem::Continue);
    }
    items.push_back(MenuItem::LevelSelect);
    items.push_back(MenuItem::Quit);
    return items;
}

bool GameFlow::IsLevelUnlocked(int levelIndex) const {
    return levelIndex >= 0 && levelIndex < config_.levelCount && levelIndex + 1 <= highestUnlocked_;
}

PlayerStats GameFlow::ClampCarried(const PlayerStats& stats) const {
    PlayerStats clamped{};
    clamped.health = std::clamp(stats.health, 1, std::max(1, config_.maximum.health));
    clamped.ammo = std::clamp(stats.ammo, 0, std::max(0, config_.maximum.ammo));
    return clamped;
}

FlowRequest GameFlow::StartLevelDirect(int levelIndex) {
    std::vector<FlowRequest> requests;
    StartLevel(std::clamp(levelIndex, 0, config_.levelCount - 1), config_.initial, FlowRequestType::LoadLevel, requests);
    return requests.front();
}

void GameFlow::ForceState(GameState state) {
    if (state == GameState::Victory) {
        state = GameState::LevelComplete;  // deprecated alias (C API compatibility)
    }
    if (state == GameState::Menu) {
        GoToMenu();
        return;
    }
    state_ = state;
}

void GameFlow::GoToMenu() {
    state_ = GameState::Menu;
    page_ = MenuPage::Main;
    menuCursor_ = 0;
}

void GameFlow::StartLevel(int levelIndex, const PlayerStats& stats, FlowRequestType type,
                          std::vector<FlowRequest>& requests) {
    currentLevel_ = levelIndex;
    entrySnapshot_ = stats;  // what a restart after death goes back to
    state_ = GameState::Gameplay;
    FlowRequest request{};
    request.type = type;
    request.levelIndex = levelIndex;
    request.stats = stats;
    requests.push_back(request);
}

std::vector<FlowRequest> GameFlow::Update(const InputState& input, const FlowEvents& events) {
    std::vector<FlowRequest> requests;
    switch (state_) {
    case GameState::Menu:
        if (page_ == MenuPage::Main) {
            UpdateMainMenu(input, requests);
        } else {
            UpdateLevelSelect(input, requests);
        }
        break;
    case GameState::Gameplay:
        UpdateGameplay(input, events, requests);
        break;
    case GameState::Paused:
        if (input.WasPressed(InputAction::QuitToMenu)) {
            GoToMenu();
        } else if (input.WasPressed(InputAction::Restart)) {
            StartLevel(currentLevel_, entrySnapshot_, FlowRequestType::RestartLevel, requests);
        } else if (input.WasPressed(InputAction::Pause) || input.WasPressed(InputAction::Confirm)) {
            state_ = GameState::Gameplay;
        }
        break;
    case GameState::GameOver:
        if (input.WasPressed(InputAction::Restart) || input.WasPressed(InputAction::Confirm)) {
            StartLevel(currentLevel_, entrySnapshot_, FlowRequestType::RestartLevel, requests);
        } else if (input.WasPressed(InputAction::Back)) {
            GoToMenu();
        }
        break;
    case GameState::LevelComplete:
    case GameState::Victory:  // never stored (ForceState maps it); handled like LevelComplete
        if (input.WasPressed(InputAction::Confirm)) {
            if (IsLastLevel() || currentLevel_ < 0) {
                state_ = GameState::CampaignComplete;
            } else {
                StartLevel(currentLevel_ + 1, carried_, FlowRequestType::LoadLevel, requests);
            }
        } else if (input.WasPressed(InputAction::Back)) {
            GoToMenu();
        }
        break;
    case GameState::CampaignComplete:
        if (input.WasPressed(InputAction::Confirm) || input.WasPressed(InputAction::Back)) {
            GoToMenu();
        }
        break;
    }
    return requests;
}

void GameFlow::UpdateMainMenu(const InputState& input, std::vector<FlowRequest>& requests) {
    const std::vector<MenuItem> items = MenuItems();
    const int count = static_cast<int>(items.size());
    menuCursor_ = std::clamp(menuCursor_, 0, count - 1);
    if (input.WasPressed(InputAction::MoveForward)) {
        menuCursor_ = (menuCursor_ + count - 1) % count;
    }
    if (input.WasPressed(InputAction::MoveBackward)) {
        menuCursor_ = (menuCursor_ + 1) % count;
    }
    if (input.WasPressed(InputAction::Back)) {
        FlowRequest quit{};
        quit.type = FlowRequestType::Quit;
        requests.push_back(quit);
        return;
    }
    if (!input.WasPressed(InputAction::Select)) {
        return;
    }
    switch (items[static_cast<std::size_t>(menuCursor_)]) {
    case MenuItem::NewGame:
        StartLevel(0, config_.initial, FlowRequestType::LoadLevel, requests);
        break;
    case MenuItem::Continue:
        StartLevel(highestUnlocked_ - 1, config_.initial, FlowRequestType::LoadLevel, requests);
        break;
    case MenuItem::LevelSelect:
        page_ = MenuPage::LevelSelect;
        levelCursor_ = highestUnlocked_ - 1;
        break;
    case MenuItem::Quit: {
        FlowRequest quit{};
        quit.type = FlowRequestType::Quit;
        requests.push_back(quit);
        break;
    }
    }
}

void GameFlow::UpdateLevelSelect(const InputState& input, std::vector<FlowRequest>& requests) {
    const int count = config_.levelCount;
    if (input.WasPressed(InputAction::MoveForward)) {
        levelCursor_ = (levelCursor_ + count - 1) % count;
    }
    if (input.WasPressed(InputAction::MoveBackward)) {
        levelCursor_ = (levelCursor_ + 1) % count;
    }
    if (input.WasPressed(InputAction::Back)) {
        page_ = MenuPage::Main;
        return;
    }
    if (input.WasPressed(InputAction::Select) && IsLevelUnlocked(levelCursor_)) {
        StartLevel(levelCursor_, config_.initial, FlowRequestType::LoadLevel, requests);
    }
}

void GameFlow::UpdateGameplay(const InputState& input, const FlowEvents& events, std::vector<FlowRequest>& requests) {
    if (events.levelCompleted) {
        lastResults_ = events.results;
        carried_ = ClampCarried(events.stats);
        const int unlocked = std::min(config_.levelCount, currentLevel_ + 2);
        highestUnlocked_ = std::max(highestUnlocked_, unlocked);
        hasProgress_ = true;
        state_ = GameState::LevelComplete;
        FlowRequest save{};
        save.type = FlowRequestType::SaveProgress;
        save.highestUnlocked = highestUnlocked_;
        requests.push_back(save);
        return;
    }
    if (events.playerDied) {
        state_ = GameState::GameOver;
        return;
    }
    if (events.focusLost || input.WasPressed(InputAction::Pause)) {
        state_ = GameState::Paused;
        return;
    }
    if (input.WasPressed(InputAction::Restart)) {
        StartLevel(currentLevel_, entrySnapshot_, FlowRequestType::RestartLevel, requests);
    }
}
