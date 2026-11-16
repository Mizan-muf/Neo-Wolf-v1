#pragma once

// Developer tool: `neo_wolf --preview <screen>` shows one screen with fixed sample data
// (docs/TESTING.md "Screen previews"). Combined with --frames and --screenshot it produces
// the reference images in docs/images/screens/.

#include <cstdint>
#include <string>

enum class PreviewScreen : std::uint8_t {
    None = 0,
    Menu,
    LevelSelect,
    Hud,
    Minimap,
    Pause,
    LevelComplete,
    GameOver,
    CampaignComplete
};

inline const char* PreviewScreenName(PreviewScreen screen) {
    switch (screen) {
    case PreviewScreen::None:
        return "none";
    case PreviewScreen::Menu:
        return "menu";
    case PreviewScreen::LevelSelect:
        return "levelselect";
    case PreviewScreen::Hud:
        return "hud";
    case PreviewScreen::Minimap:
        return "minimap";
    case PreviewScreen::Pause:
        return "pause";
    case PreviewScreen::LevelComplete:
        return "levelcomplete";
    case PreviewScreen::GameOver:
        return "gameover";
    case PreviewScreen::CampaignComplete:
        return "campaigncomplete";
    }
    return "none";
}

// Every preview id, for help texts and tests.
inline constexpr PreviewScreen kPreviewScreens[] = {
    PreviewScreen::Menu,  PreviewScreen::LevelSelect,   PreviewScreen::Hud,      PreviewScreen::Minimap,
    PreviewScreen::Pause, PreviewScreen::LevelComplete, PreviewScreen::GameOver, PreviewScreen::CampaignComplete};

inline bool ParsePreviewScreen(const std::string& name, PreviewScreen& out) {
    for (const PreviewScreen screen : kPreviewScreens) {
        if (name == PreviewScreenName(screen)) {
            out = screen;
            return true;
        }
    }
    return false;
}
