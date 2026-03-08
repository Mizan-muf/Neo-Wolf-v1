#pragma once

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

class Engine {
public:
    Engine() = default;
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    bool Init();
    void Update(double deltaSeconds);
    void Render();
    void Shutdown();

    bool IsRunning() const;

private:
    void HandleEvent(const SDL_Event& event);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool running_ = false;
};
