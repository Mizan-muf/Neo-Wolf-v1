#pragma once

// Input as game actions (move, turn, fire, menu keys), independent of the keys that produce
// them; the platform layer and the input injector fill it once per frame.

#include <array>
#include <cstddef>
#include <cstdint>

enum class InputAction : std::uint8_t {
    MoveForward = 0,
    MoveBackward,
    StrafeLeft,
    StrafeRight,
    TurnLeft,
    TurnRight,
    Interact,
    Attack,
    Pause,
    Restart,
    Confirm,          // Enter: confirms on result screens (level complete, game over, credits)
    ToggleDebugView,  // F3: FPS / position overlay
    Back,             // Escape only: leaves the current screen (quits from the main menu)
    // Appended in Phase 5 (docs/CONTROLS.md):
    Select,         // Enter or Space: activates the highlighted menu item
    ToggleMinimap,  // M or Tab
    QuitToMenu,     // Q (also TurnLeft): leaves the pause screen for the main menu
    Count
};

struct ActionButtonState {
    bool isDown = false;
    bool wasPressed = false;
    bool wasReleased = false;
};

struct InputState {
    bool quitRequested = false;
    bool focusLost = false;  // the window lost keyboard focus this frame
    std::array<ActionButtonState, static_cast<std::size_t>(InputAction::Count)> actions{};

    void BeginFrame() {
        focusLost = false;
        for (ActionButtonState& state : actions) {
            state.wasPressed = false;
            state.wasReleased = false;
        }
    }

    void SetActionState(InputAction action, bool isDownNow) {
        ActionButtonState& state = actions[static_cast<std::size_t>(action)];
        if (state.isDown == isDownNow) {
            return;
        }

        state.isDown = isDownNow;
        if (isDownNow) {
            state.wasPressed = true;
        } else {
            state.wasReleased = true;
        }
    }

    // Forgets a press made this frame and releases the action without reporting a release
    // (used when a key turns out to be part of a window shortcut, known issue B13).
    void CancelAction(InputAction action) {
        ActionButtonState& state = actions[static_cast<std::size_t>(action)];
        state.isDown = false;
        state.wasPressed = false;
        state.wasReleased = false;
    }

    bool IsDown(InputAction action) const { return actions[static_cast<std::size_t>(action)].isDown; }

    bool WasPressed(InputAction action) const { return actions[static_cast<std::size_t>(action)].wasPressed; }

    bool WasReleased(InputAction action) const { return actions[static_cast<std::size_t>(action)].wasReleased; }
};
