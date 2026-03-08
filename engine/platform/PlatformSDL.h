#pragma once

#include <cstdint>

struct InputState;

class PlatformSDL {
public:
    PlatformSDL() = default;
    ~PlatformSDL();

    PlatformSDL(const PlatformSDL&) = delete;
    PlatformSDL& operator=(const PlatformSDL&) = delete;

    bool Init(const char* title, int width, int height);
    void PumpEvents(InputState& input) const;
    void Clear(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) const;
    void Present() const;
    void Shutdown();

private:
    struct SDL_Window* window_ = nullptr;
    struct SDL_Renderer* renderer_ = nullptr;
};
