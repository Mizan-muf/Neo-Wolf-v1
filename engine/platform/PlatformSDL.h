#pragma once

#include <cstddef>
#include <string>

#include "engine/core/InputState.h"

class Framebuffer;

struct PlatformConfig {
    const char* title = "Neo-Wolf";
    int windowWidth = 1024;  // initial window size in points (HiDPI scales it)
    int windowHeight = 768;
    int logicalWidth = 320;  // framebuffer size; letterboxed and scaled with nearest-neighbour
    int logicalHeight = 240;
    bool fullscreen = false;  // start in desktop fullscreen
};

class PlatformSDL {
public:
    PlatformSDL() = default;
    ~PlatformSDL();

    PlatformSDL(const PlatformSDL&) = delete;
    PlatformSDL& operator=(const PlatformSDL&) = delete;

    // Initialises SDL video only (audio is AudioSystem's job), the window and a renderer.
    bool Init(const PlatformConfig& config);
    // Reads pending events into `input`. Window-level keys (fullscreen toggle) are handled here.
    void PumpEvents(InputState& input);
    // Uploads the framebuffer and presents it: the one present per frame.
    void PresentFramebuffer(const Framebuffer& framebuffer);
    void Shutdown();

    void ToggleFullscreen();
    bool HasVSync() const { return vsync_; }
    // Refresh rate of the window's display in Hz, or 0 if unknown.
    int DisplayRefreshRate() const;
    bool IsMinimized() const;

    // Shows a modal error box (works before Init). Failure is ignored: there may be no
    // message system at all (dummy video driver, headless CI).
    static void ShowErrorMessageBox(const std::string& title, const std::string& message);

    // Writes the framebuffer (e.g. 320x240) as a 24-bit BMP (--screenshot). Works without a
    // window. On failure returns false and sets `error`.
    static bool SaveFramebufferBmp(const Framebuffer& framebuffer, const std::string& path, std::string& error);

    // Key bindings (docs/CONTROLS.md). `scancode` is an SDL_Scancode and `modifiers` an SDL
    // keymod mask; they are plain integers so tests can call these without SDL events.
    // Fills `actions` with the actions bound to the key and returns how many (0-2). Ctrl is
    // not the fire key while Cmd/Windows is held (B13).
    static std::size_t MapKeyToActions(int scancode, unsigned modifiers, InputAction (&actions)[2]);
    // F11, Alt+Enter, and on macOS Cmd+Ctrl+F.
    static bool IsFullscreenShortcut(int scancode, unsigned modifiers);

private:
    bool EnsureFramebufferTexture(int width, int height);
    bool CreateRenderer();

    struct SDL_Window* window_ = nullptr;
    struct SDL_Renderer* renderer_ = nullptr;
    struct SDL_Texture* framebufferTexture_ = nullptr;
    int framebufferTextureWidth_ = 0;
    int framebufferTextureHeight_ = 0;
    int logicalWidth_ = 320;
    int logicalHeight_ = 240;
    bool vsync_ = false;
    bool videoInitialized_ = false;
    // True while the window holds keyboard focus. Focus loss only pauses the game after the
    // window actually had focus: SDL's dummy driver (headless runs) never grants focus.
    bool hasFocus_ = false;
    // True for SDL's headless video drivers (dummy, offscreen): focus events are ignored.
    bool headlessVideo_ = false;
};
