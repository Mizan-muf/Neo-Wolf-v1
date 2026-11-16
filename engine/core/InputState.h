#pragma once

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
    Confirm,
    ToggleDebugView,
    Back,  // Escape only: leaves the current screen (quits from the main menu)
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

    bool IsDown(InputAction action) const {
        return actions[static_cast<std::size_t>(action)].isDown;
    }

    bool WasPressed(InputAction action) const {
        return actions[static_cast<std::size_t>(action)].wasPressed;
    }

    bool WasReleased(InputAction action) const {
        return actions[static_cast<std::size_t>(action)].wasReleased;
    }
};
