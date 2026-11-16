#pragma once

// Campaign game flow: menus, level progression, death/restart, pause and the end screens.
// SDL-free and engine-free, so every transition is unit-tested (docs/GAMEPLAY.md has the
// state diagram).
//
// Each frame the engine passes the input snapshot plus what the simulation reported (level
// complete, player died, focus lost). GameFlow updates its state and returns requests for
// the engine to carry out: load a level with given health/ammo, restart the current level,
// save progress, quit. It never loads anything itself.
//
// The test level (campaign.json "testLevel", the Test Chamber) sits outside the campaign: it is
// started from its own menu item or at boot, its exit switch restarts it, and it never unlocks a
// level or asks for progress to be saved.

#include <cstdint>
#include <vector>

#include "engine/core/GameState.h"
#include "engine/core/InputState.h"

// FlowRequest::levelIndex and GameFlow::CurrentLevel() for the test level (not a campaign index).
inline constexpr int kTestLevelIndex = -2;

struct PlayerStats {
    int health = 0;
    int ammo = 0;
};

// Shown on the level-complete screen.
struct LevelResults {
    int kills = 0;
    int totalEnemies = 0;
    int pickups = 0;
    int totalPickups = 0;
    double seconds = 0.0;
    int score = 0;        // sum of the killed enemies' gameplay.json "score" (logged; not shown yet)
    int shotsFired = 0;   // shots the player fired on this level (Phase 7 autoplay report)
    int damageTaken = 0;  // health the player lost to enemy attacks on this level
};

// What the simulation reported this frame.
struct FlowEvents {
    bool levelCompleted = false;  // the exit switch was used
    bool playerDied = false;
    bool focusLost = false;  // the window lost focus (pauses gameplay)
    PlayerStats stats{};     // health and ammo when the level was completed
    LevelResults results{};
};

enum class FlowRequestType : std::uint8_t {
    LoadLevel,     // load levelIndex (or the test level) and start it with `stats`
    RestartLevel,  // reload levelIndex (or the test level) with `stats` (the level-entry snapshot)
    SaveProgress,  // write progress.json with highestUnlocked
    Quit
};

struct FlowRequest {
    FlowRequestType type = FlowRequestType::Quit;
    int levelIndex = -1;      // 0-based campaign index, or kTestLevelIndex (LoadLevel, RestartLevel)
    PlayerStats stats{};      // starting health and ammo (LoadLevel, RestartLevel)
    int highestUnlocked = 0;  // 1-based (SaveProgress)
};

enum class MenuPage : std::uint8_t { Main, LevelSelect };
enum class MenuItem : std::uint8_t { NewGame, Continue, LevelSelect, TestLevel, Quit };

const char* MenuItemLabel(MenuItem item);

struct GameFlowConfig {
    int levelCount = 1;
    PlayerStats initial{};      // gameplay.json initial values: New Game, Continue, Level Select
    PlayerStats maximum{};      // gameplay.json maxima: carried-over stats are clamped to these
    bool hasTestLevel = false;  // campaign.json has a "testLevel": the main menu offers it
};

class GameFlow {
public:
    // Resets to the main menu with no current level.
    void Configure(const GameFlowConfig& config);
    // Saved progress from progress.json (highestUnlocked is 1-based; clamped to the campaign).
    void SetProgress(bool hasProgress, int highestUnlocked);

    // One frame. Input edges (WasPressed) drive the menus and screens.
    std::vector<FlowRequest> Update(const InputState& input, const FlowEvents& events);

    // Starts a level directly with the initial stats, ignoring unlocks (--level N, previews).
    FlowRequest StartLevelDirect(int levelIndex);
    // Starts the test level with the initial stats (campaign.json "bootIntoTestLevel").
    FlowRequest StartTestLevel();
    // Sets the state from outside (C API, command bridge, previews). The deprecated Victory
    // value is stored as LevelComplete; Menu returns to the main page.
    void ForceState(GameState state);
    // A level was loaded by path (C API, command bridge) in place of the test level: restarts
    // reload that level and its exit ends it as for any level loaded by path. (After a campaign
    // level the flow keeps its campaign position, as documented in docs/C_API.md.)
    void LeaveTestLevel() {
        if (InTestLevel()) {
            currentLevel_ = -1;
        }
    }
    // A LoadLevel or RestartLevel request failed, so the engine still has the level from before
    // it: the flow goes back to that level (its index and restart stats) and to the main menu,
    // so that the two agree on which level the exit switch completes (and whether it is the
    // test level, which saves nothing).
    void CancelLevelStart();
    // Previews and tests: the results shown on the level-complete screen.
    void SetLastResults(const LevelResults& results) { lastResults_ = results; }
    void SetMenuPage(MenuPage page, int cursor);

    GameState State() const { return state_; }
    MenuPage Page() const { return page_; }
    std::vector<MenuItem> MenuItems() const;
    int MenuCursor() const { return menuCursor_; }
    int LevelCursor() const { return levelCursor_; }
    bool HasProgress() const { return hasProgress_; }
    int HighestUnlocked() const { return highestUnlocked_; }  // 1-based
    bool IsLevelUnlocked(int levelIndex) const;
    int LevelCount() const { return config_.levelCount; }
    int CurrentLevel() const { return currentLevel_; }  // 0-based, -1 before the first level, or kTestLevelIndex
    bool InTestLevel() const { return currentLevel_ == kTestLevelIndex; }
    bool HasTestLevel() const { return config_.hasTestLevel; }
    bool IsLastLevel() const { return currentLevel_ == config_.levelCount - 1; }
    const PlayerStats& EntrySnapshot() const { return entrySnapshot_; }
    const PlayerStats& CarriedStats() const { return carried_; }
    const LevelResults& LastResults() const { return lastResults_; }

    // Clamps health to [1, maximum.health] and ammo to [0, maximum.ammo].
    PlayerStats ClampCarried(const PlayerStats& stats) const;

private:
    void UpdateMainMenu(const InputState& input, std::vector<FlowRequest>& requests);
    void UpdateLevelSelect(const InputState& input, std::vector<FlowRequest>& requests);
    void UpdateGameplay(const InputState& input, const FlowEvents& events, std::vector<FlowRequest>& requests);
    void UpdateLevelComplete(const InputState& input, std::vector<FlowRequest>& requests);
    void StartLevel(int levelIndex, const PlayerStats& stats, FlowRequestType type, std::vector<FlowRequest>& requests);
    void GoToMenu();

    GameFlowConfig config_{};
    GameState state_ = GameState::Menu;
    MenuPage page_ = MenuPage::Main;
    int menuCursor_ = 0;
    int levelCursor_ = 0;
    bool hasProgress_ = false;
    int highestUnlocked_ = 1;
    int currentLevel_ = -1;
    int previousLevel_ = -1;  // currentLevel_ before the last StartLevel (CancelLevelStart)
    PlayerStats entrySnapshot_{};
    PlayerStats previousSnapshot_{};
    PlayerStats carried_{};
    LevelResults lastResults_{};
};
