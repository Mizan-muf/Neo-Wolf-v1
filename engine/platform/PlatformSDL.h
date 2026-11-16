#pragma once

// The SDL window, renderer and events: turns keyboard and window events into InputState
// actions, handles the fullscreen and quit shortcuts, and presents the framebuffer. Only
// PlatformSDL.cpp includes SDL; this header forward-declares the SDL types it holds.

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "engine/core/InputState.h"

class Framebuffer;
struct SDL_KeyboardEvent;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

// The caller (Engine::Init) sets every field; the sizes have no defaults of their own so that
// the window and framebuffer sizes are defined in one place.
struct PlatformConfig {
    const char* title = "Neo-Wolf";
    int windowWidth = 0;  // initial window size in points (HiDPI scales it)
    int windowHeight = 0;
    int logicalWidth = 0;  // framebuffer size; letterboxed and scaled with nearest-neighbour
    int logicalHeight = 0;
    bool fullscreen = false;  // start in desktop fullscreen
};

class PlatformSDL {
public:
    // Most keys trigger one action; Space, Enter, Escape and Q trigger two.
    static constexpr std::size_t kMaxActionsPerKey = 2;

    PlatformSDL() = default;
    ~PlatformSDL();

    PlatformSDL(const PlatformSDL&) = delete;
    PlatformSDL& operator=(const PlatformSDL&) = delete;
    PlatformSDL(PlatformSDL&&) = delete;
    PlatformSDL& operator=(PlatformSDL&&) = delete;

    // Initialises SDL video only (audio is AudioSystem's job), the window and a renderer.
    bool Init(const PlatformConfig& config);
    // Reads pending events into `input`. Window-level keys (fullscreen toggle) are handled here.
    void PumpEvents(InputState& input);
    // Uploads the framebuffer and presents it: the one present per frame.
    void PresentFramebuffer(const Framebuffer& framebuffer);
    void Shutdown();

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
    static std::size_t MapKeyToActions(int scancode, unsigned modifiers,
                                       std::array<InputAction, kMaxActionsPerKey>& actions);
    // F11, Alt+Enter, and on macOS Cmd+Ctrl+F.
    static bool IsFullscreenShortcut(int scancode, unsigned modifiers);

private:
    // Release the SDL objects this class owns (defined in PlatformSDL.cpp, which includes SDL).
    struct WindowDeleter {
        void operator()(SDL_Window* window) const;
    };
    struct RendererDeleter {
        void operator()(SDL_Renderer* renderer) const;
    };
    struct TextureDeleter {
        void operator()(SDL_Texture* texture) const;
    };

    bool OpenWindow(const PlatformConfig& config);
    bool CreateRenderer();
    bool EnsureFramebufferTexture(int width, int height);
    void HandleKeyEvent(const SDL_KeyboardEvent& key, InputState& input);
    void ToggleFullscreen();

    // Declared in creation order, so the implicit destruction order is texture, renderer,
    // window. Shutdown releases them in the same order before it quits the video subsystem.
    std::unique_ptr<SDL_Window, WindowDeleter> window_;
    std::unique_ptr<SDL_Renderer, RendererDeleter> renderer_;
    std::unique_ptr<SDL_Texture, TextureDeleter> framebufferTexture_;
    int framebufferTextureWidth_ = 0;
    int framebufferTextureHeight_ = 0;
    int logicalWidth_ = 0;  // set by Init from PlatformConfig
    int logicalHeight_ = 0;
    bool vsync_ = false;
    bool videoInitialized_ = false;
    // True while the window holds keyboard focus. Focus loss only pauses the game after the
    // window actually had focus: SDL's dummy driver (headless runs) never grants focus.
    bool hasFocus_ = false;
    // True for SDL's headless video drivers (dummy, offscreen): focus events are ignored.
    bool headlessVideo_ = false;
};
