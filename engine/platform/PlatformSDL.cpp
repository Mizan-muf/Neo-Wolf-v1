#include "engine/platform/PlatformSDL.h"

#include <SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "engine/core/InputState.h"
#include "engine/core/Log.h"
#include "engine/render/Framebuffer.h"

namespace {
// The initial window covers at most this fraction of the usable desktop area.
constexpr double kMaxDesktopFraction = 0.9;
// The display whose usable area bounds the initial window: SDL's primary display.
constexpr int kPrimaryDisplay = 0;
// SDL_CreateRenderer index: the first render driver that supports the requested flags.
constexpr int kFirstMatchingRenderDriver = -1;
// Framebuffer pixels are 32-bit ARGB.
constexpr int kFramebufferBitsPerPixel = 32;
// Window states in which nothing is shown (App then paces frames itself, not by vsync).
constexpr Uint32 kNotVisibleWindowFlags = SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN;

struct SurfaceDeleter {
    void operator()(SDL_Surface* surface) const { SDL_FreeSurface(surface); }
};

// True while a Cmd (macOS) / Windows key is held: Ctrl is then part of a shortcut such as
// Cmd+Ctrl+F, not the fire key (known issue B13).
bool IsGuiHeld(unsigned modifiers) {
    return (modifiers & KMOD_GUI) != 0;
}

bool IsGuiKey(SDL_Scancode scancode) {
    return scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI;
}

// SDL's headless video drivers have no display and no real keyboard focus.
bool IsHeadlessVideoDriver(const char* driver) {
    const std::string driverName = driver != nullptr ? driver : "";
    return driverName == "dummy" || driverName == "offscreen";
}

std::string SdlError() {
    return SDL_GetError();
}

struct WindowSize {
    int width = 0;
    int height = 0;
};

// The configured window size, shrunk (keeping its shape) to fit the usable desktop area of
// small or scaled displays, but never smaller than the framebuffer.
WindowSize FitWindowToDesktop(const PlatformConfig& config) {
    WindowSize size{config.windowWidth, config.windowHeight};
    SDL_Rect usable{};
    if (SDL_GetDisplayUsableBounds(kPrimaryDisplay, &usable) == 0 && usable.w > 0 && usable.h > 0) {
        const double scale = std::min({1.0, kMaxDesktopFraction * usable.w / static_cast<double>(size.width),
                                       kMaxDesktopFraction * usable.h / static_cast<double>(size.height)});
        size.width = std::max(config.logicalWidth, static_cast<int>(size.width * scale));
        size.height = std::max(config.logicalHeight, static_cast<int>(size.height * scale));
    }
    return size;
}
}  // namespace

void PlatformSDL::WindowDeleter::operator()(SDL_Window* window) const {
    SDL_DestroyWindow(window);
}

void PlatformSDL::RendererDeleter::operator()(SDL_Renderer* renderer) const {
    SDL_DestroyRenderer(renderer);
}

void PlatformSDL::TextureDeleter::operator()(SDL_Texture* texture) const {
    SDL_DestroyTexture(texture);
}

// This table is the source of truth for the controls (docs/CONTROLS.md).
std::size_t PlatformSDL::MapKeyToActions(int scancode, unsigned modifiers,
                                         std::array<InputAction, kMaxActionsPerKey>& actions) {
    switch (static_cast<SDL_Scancode>(scancode)) {
        case SDL_SCANCODE_W:
        case SDL_SCANCODE_UP:
            actions[0] = InputAction::MoveForward;  // also "menu up"
            return 1;
        case SDL_SCANCODE_S:
        case SDL_SCANCODE_DOWN:
            actions[0] = InputAction::MoveBackward;  // also "menu down"
            return 1;
        case SDL_SCANCODE_A:
            actions[0] = InputAction::StrafeLeft;
            return 1;
        case SDL_SCANCODE_D:
            actions[0] = InputAction::StrafeRight;
            return 1;
        case SDL_SCANCODE_LEFT:
            actions[0] = InputAction::TurnLeft;
            return 1;
        case SDL_SCANCODE_Q:
            actions[0] = InputAction::TurnLeft;
            actions[1] = InputAction::QuitToMenu;
            return 2;
        case SDL_SCANCODE_RIGHT:
            actions[0] = InputAction::TurnRight;
            return 1;
        case SDL_SCANCODE_E:
            actions[0] = InputAction::Interact;
            return 1;
        case SDL_SCANCODE_SPACE:
            actions[0] = InputAction::Attack;
            actions[1] = InputAction::Select;
            return 2;
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
            if (IsGuiHeld(modifiers)) {
                return 0;  // Cmd+Ctrl(+F): a window shortcut, not a shot
            }
            actions[0] = InputAction::Attack;
            return 1;
        case SDL_SCANCODE_F:
            actions[0] = InputAction::Attack;
            return 1;
        case SDL_SCANCODE_ESCAPE:
            actions[0] = InputAction::Pause;
            actions[1] = InputAction::Back;
            return 2;
        case SDL_SCANCODE_P:
            actions[0] = InputAction::Pause;
            return 1;
        case SDL_SCANCODE_R:
            actions[0] = InputAction::Restart;
            return 1;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER:
            actions[0] = InputAction::Confirm;
            actions[1] = InputAction::Select;
            return 2;
        case SDL_SCANCODE_TAB:
        case SDL_SCANCODE_M:
            actions[0] = InputAction::ToggleMinimap;
            return 1;
        case SDL_SCANCODE_F3:
            actions[0] = InputAction::ToggleDebugView;
            return 1;
        default:
            return 0;
    }
}

// F11 and Alt+Enter everywhere; Cmd+Ctrl+F (the standard shortcut) on macOS.
bool PlatformSDL::IsFullscreenShortcut(int scancode, unsigned modifiers) {
    const auto code = static_cast<SDL_Scancode>(scancode);
    if (code == SDL_SCANCODE_F11) {
        return true;
    }
    if ((code == SDL_SCANCODE_RETURN || code == SDL_SCANCODE_KP_ENTER) && (modifiers & KMOD_ALT) != 0) {
        return true;
    }
#if defined(__APPLE__)
    if (code == SDL_SCANCODE_F && IsGuiHeld(modifiers) && (modifiers & KMOD_CTRL) != 0) {
        return true;
    }
#endif
    return false;
}

PlatformSDL::~PlatformSDL() {
    Shutdown();
}

bool PlatformSDL::Init(const PlatformConfig& config) {
    logicalWidth_ = config.logicalWidth;
    logicalHeight_ = config.logicalHeight;

    // Hints must be set before the subsystem / texture they affect is created.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");  // nearest-neighbour scaling
    // Windows: be DPI aware and scale window coordinates like macOS points (SDL 2.24+).
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        LogError("SDL video initialisation failed: " + SdlError());
        return false;
    }
    videoInitialized_ = true;

    const char* videoDriver = SDL_GetCurrentVideoDriver();
    LogInfo(std::string("SDL video driver: ") + (videoDriver != nullptr ? videoDriver : "(none)"));
    // Focus events from a headless driver must not pause the game (otherwise --screenshot of
    // gameplay always shows the pause screen).
    headlessVideo_ = IsHeadlessVideoDriver(videoDriver);

    if (!OpenWindow(config) || !CreateRenderer()) {
        Shutdown();
        return false;
    }
    return true;
}

bool PlatformSDL::OpenWindow(const PlatformConfig& config) {
    const WindowSize size = FitWindowToDesktop(config);
    Uint32 windowFlags = SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE;
    if (config.fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    window_.reset(SDL_CreateWindow(config.title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, size.width,
                                   size.height, windowFlags));
    if (window_ == nullptr) {
        LogError("SDL_CreateWindow failed: " + SdlError());
        return false;
    }
    hasFocus_ = (SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_INPUT_FOCUS) != 0;
    SDL_SetWindowMinimumSize(window_.get(), config.logicalWidth, config.logicalHeight);
    return true;
}

bool PlatformSDL::CreateRenderer() {
    renderer_.reset(SDL_CreateRenderer(window_.get(), kFirstMatchingRenderDriver,
                                       SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
    if (renderer_ == nullptr) {
        // No GPU renderer: dummy/offscreen video driver, VMs, remote sessions, missing drivers.
        LogWarning("No accelerated renderer (" + SdlError() + "); falling back to the software renderer.");
        renderer_.reset(SDL_CreateRenderer(window_.get(), kFirstMatchingRenderDriver, SDL_RENDERER_SOFTWARE));
    }
    if (renderer_ == nullptr) {
        LogError("SDL_CreateRenderer failed (software fallback too): " + SdlError());
        return false;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer_.get(), &info) == 0) {
        // The software driver always advertises PRESENTVSYNC (a capability, not the state),
        // and we created it without vsync, so only trust the flag for accelerated renderers.
        const bool accelerated = (info.flags & SDL_RENDERER_ACCELERATED) != 0;
        vsync_ = accelerated && (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
        int outputWidth = 0;
        int outputHeight = 0;
        SDL_GetRendererOutputSize(renderer_.get(), &outputWidth, &outputHeight);
        LogInfo(std::string("Renderer: ") + (info.name != nullptr ? info.name : "?") +
                (accelerated ? " (accelerated)" : " (not accelerated)") + ", vsync " + (vsync_ ? "on" : "off") +
                ", output " + std::to_string(outputWidth) + "x" + std::to_string(outputHeight) + " pixels");
    }

    // Letterboxed, aspect-correct scaling of the 320x240 framebuffer at any window size.
    if (SDL_RenderSetLogicalSize(renderer_.get(), logicalWidth_, logicalHeight_) != 0) {
        LogWarning("SDL_RenderSetLogicalSize failed: " + SdlError());
    }
    return true;
}

void PlatformSDL::PumpEvents(InputState& input) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        switch (event.type) {
            case SDL_QUIT:  // window close button, Cmd+Q, Alt+F4, system shutdown
                input.quitRequested = true;
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    hasFocus_ = true;
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    if (hasFocus_ && !headlessVideo_) {
                        input.focusLost = true;
                    }
                    hasFocus_ = false;
                }
                break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
                HandleKeyEvent(event.key, input);
                break;
            default:
                break;
        }
    }
}

void PlatformSDL::HandleKeyEvent(const SDL_KeyboardEvent& key, InputState& input) {
    const bool down = key.type == SDL_KEYDOWN;
    const unsigned modifiers = key.keysym.mod;
    if (down && IsFullscreenShortcut(key.keysym.scancode, modifiers)) {
        if (key.repeat == 0) {
            ToggleFullscreen();
        }
        // B13: a Ctrl pressed as part of the chord must not fire in this frame.
        input.CancelAction(InputAction::Attack);
        return;
    }
    if (down && IsGuiHeld(modifiers) && IsGuiKey(key.keysym.scancode)) {
        // Cmd pressed while Ctrl is already down: the start of a shortcut, not a shot.
        input.CancelAction(InputAction::Attack);
    }
    std::array<InputAction, kMaxActionsPerKey> actions{};
    const std::size_t count = MapKeyToActions(key.keysym.scancode, modifiers, actions);
    for (std::size_t i = 0; i < count; ++i) {
        input.SetActionState(actions[i], down);
    }
}

void PlatformSDL::ToggleFullscreen() {
    if (window_ == nullptr) {
        return;
    }
    const bool isFullscreen = (SDL_GetWindowFlags(window_.get()) & SDL_WINDOW_FULLSCREEN) != 0;
    if (SDL_SetWindowFullscreen(window_.get(), isFullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
        LogWarning("Could not change fullscreen mode: " + SdlError());
        return;
    }
    LogInfo(isFullscreen ? "Windowed mode." : "Fullscreen (desktop) mode.");
}

int PlatformSDL::DisplayRefreshRate() const {
    if (window_ == nullptr) {
        return 0;
    }
    const int displayIndex = SDL_GetWindowDisplayIndex(window_.get());
    SDL_DisplayMode mode{};
    if (displayIndex < 0 || SDL_GetDesktopDisplayMode(displayIndex, &mode) != 0) {
        return 0;
    }
    return mode.refresh_rate;
}

bool PlatformSDL::IsMinimized() const {
    return window_ != nullptr && (SDL_GetWindowFlags(window_.get()) & kNotVisibleWindowFlags) != 0;
}

void PlatformSDL::ShowErrorMessageBox(const std::string& title, const std::string& message) {
    if (SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, title.c_str(), message.c_str(), nullptr) != 0) {
        LogInfo("No error dialog shown (" + SdlError() + ").");
    }
}

bool PlatformSDL::SaveFramebufferBmp(const Framebuffer& framebuffer, const std::string& path, std::string& error) {
    if (framebuffer.Data() == nullptr || framebuffer.Width() <= 0 || framebuffer.Height() <= 0) {
        error = "the framebuffer is empty";
        return false;
    }
    // A surface that wraps the framebuffer's pixels (no copy). RGB888 = XRGB: the alpha byte
    // of the ARGB pixels is ignored, so the image is opaque.
    const std::unique_ptr<SDL_Surface, SurfaceDeleter> surface(SDL_CreateRGBSurfaceWithFormatFrom(
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): SDL takes void*; SDL_SaveBMP only reads it
        const_cast<std::uint32_t*>(framebuffer.Data()), framebuffer.Width(), framebuffer.Height(),
        kFramebufferBitsPerPixel, framebuffer.PitchBytes(), SDL_PIXELFORMAT_RGB888));
    if (surface == nullptr) {
        error = "SDL_CreateRGBSurfaceWithFormatFrom failed: " + SdlError();
        return false;
    }
    if (SDL_SaveBMP(surface.get(), path.c_str()) != 0) {
        error = "SDL_SaveBMP failed: " + SdlError();
        return false;
    }
    return true;
}

void PlatformSDL::PresentFramebuffer(const Framebuffer& framebuffer) {
    if (renderer_ == nullptr) {
        return;
    }

    if (framebuffer.Data() == nullptr || framebuffer.Width() <= 0 || framebuffer.Height() <= 0) {
        return;
    }

    if (!EnsureFramebufferTexture(framebuffer.Width(), framebuffer.Height())) {
        return;
    }

    if (SDL_UpdateTexture(framebufferTexture_.get(), nullptr, framebuffer.Data(), framebuffer.PitchBytes()) != 0) {
        LogError("SDL_UpdateTexture failed: " + SdlError());
        return;
    }

    // Black letterbox bars outside the logical viewport.
    SDL_SetRenderDrawColor(renderer_.get(), 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer_.get());
    if (SDL_RenderCopy(renderer_.get(), framebufferTexture_.get(), nullptr, nullptr) != 0) {
        LogError("SDL_RenderCopy failed: " + SdlError());
        return;
    }

    SDL_RenderPresent(renderer_.get());
}

bool PlatformSDL::EnsureFramebufferTexture(int width, int height) {
    if (renderer_ == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    if (framebufferTexture_ != nullptr && framebufferTextureWidth_ == width && framebufferTextureHeight_ == height) {
        return true;
    }

    framebufferTexture_.reset();  // release the old texture before creating its replacement
    framebufferTexture_.reset(
        SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height));
    if (framebufferTexture_ == nullptr) {
        LogError("SDL_CreateTexture failed: " + SdlError());
        framebufferTextureWidth_ = 0;
        framebufferTextureHeight_ = 0;
        return false;
    }

    framebufferTextureWidth_ = width;
    framebufferTextureHeight_ = height;
    return true;
}

void PlatformSDL::Shutdown() {
    // Texture, renderer, window: each is released before the object it was created from.
    framebufferTexture_.reset();
    framebufferTextureWidth_ = 0;
    framebufferTextureHeight_ = 0;
    renderer_.reset();
    window_.reset();

    if (videoInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        videoInitialized_ = false;
        // The audio subsystem is shut down first (Engine::Shutdown), so this is the last user.
        if (SDL_WasInit(SDL_INIT_EVERYTHING) == 0) {
            SDL_Quit();
        }
    }
    vsync_ = false;
}
