#include "engine/platform/PlatformSDL.h"

#include <SDL.h>

#include <iostream>

#include "engine/core/InputState.h"
#include "engine/render/Framebuffer.h"

namespace {
bool TryMapScancodeToAction(SDL_Scancode scancode, InputAction& action) {
    switch (scancode) {
    case SDL_SCANCODE_W:
        action = InputAction::MoveForward;
        return true;
    case SDL_SCANCODE_S:
        action = InputAction::MoveBackward;
        return true;
    case SDL_SCANCODE_A:
        action = InputAction::StrafeLeft;
        return true;
    case SDL_SCANCODE_D:
        action = InputAction::StrafeRight;
        return true;
    case SDL_SCANCODE_LEFT:
        action = InputAction::TurnLeft;
        return true;
    case SDL_SCANCODE_RIGHT:
        action = InputAction::TurnRight;
        return true;
    case SDL_SCANCODE_E:
        action = InputAction::Interact;
        return true;
    case SDL_SCANCODE_ESCAPE:
        action = InputAction::Pause;
        return true;
    case SDL_SCANCODE_TAB:
        action = InputAction::ToggleDebugView;
        return true;
    default:
        return false;
    }
}
} // namespace

PlatformSDL::~PlatformSDL() {
    Shutdown();
}

bool PlatformSDL::Init(const char* title, int width, int height) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return false;
    }

    window_ = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN);
    if (window_ == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (renderer_ == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
        return false;
    }

    return true;
}

void PlatformSDL::PumpEvents(InputState& input) const {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
        if (event.type == SDL_QUIT) {
            input.quitRequested = true;
            continue;
        }

        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            InputAction action = InputAction::MoveForward;
            if (TryMapScancodeToAction(event.key.keysym.scancode, action)) {
                input.SetActionState(action, event.type == SDL_KEYDOWN);
            }
        }
    }
}

void PlatformSDL::Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) const {
    SDL_SetRenderDrawColor(renderer_, r, g, b, a);
    SDL_RenderClear(renderer_);
}

void PlatformSDL::Present() const {
    SDL_RenderPresent(renderer_);
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

    if (SDL_UpdateTexture(
            framebufferTexture_,
            nullptr,
            framebuffer.Data(),
            framebuffer.PitchBytes()) != 0) {
        std::cerr << "SDL_UpdateTexture failed: " << SDL_GetError() << '\n';
        return;
    }

    SDL_RenderClear(renderer_);
    if (SDL_RenderCopy(renderer_, framebufferTexture_, nullptr, nullptr) != 0) {
        std::cerr << "SDL_RenderCopy failed: " << SDL_GetError() << '\n';
        return;
    }

    SDL_RenderPresent(renderer_);
}

bool PlatformSDL::EnsureFramebufferTexture(int width, int height) {
    if (renderer_ == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    if (framebufferTexture_ != nullptr &&
        framebufferTextureWidth_ == width &&
        framebufferTextureHeight_ == height) {
        return true;
    }

    if (framebufferTexture_ != nullptr) {
        SDL_DestroyTexture(framebufferTexture_);
        framebufferTexture_ = nullptr;
    }

    framebufferTexture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height);
    if (framebufferTexture_ == nullptr) {
        std::cerr << "SDL_CreateTexture failed: " << SDL_GetError() << '\n';
        framebufferTextureWidth_ = 0;
        framebufferTextureHeight_ = 0;
        return false;
    }

    framebufferTextureWidth_ = width;
    framebufferTextureHeight_ = height;
    return true;
}

void PlatformSDL::Shutdown() {
    if (framebufferTexture_ != nullptr) {
        SDL_DestroyTexture(framebufferTexture_);
        framebufferTexture_ = nullptr;
        framebufferTextureWidth_ = 0;
        framebufferTextureHeight_ = 0;
    }

    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    if (SDL_WasInit(0) != 0) {
        SDL_Quit();
    }
}

