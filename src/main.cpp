#include <SDL.h>

#include <chrono>
#include <iomanip>
#include <iostream>

int main(int /*argc*/, char* /*argv*/[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << '\n';
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Raycast Engine - Phase 1",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        720,
        SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << '\n';
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    bool running = true;
    using clock = std::chrono::steady_clock;
    auto previous = clock::now();
    auto fpsStart = previous;
    int frameCount = 0;

    std::cout << "Main loop started.\n";

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }

        auto now = clock::now();
        const std::chrono::duration<double> frameTime = now - previous;
        previous = now;

        SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);

        ++frameCount;
        const std::chrono::duration<double> fpsWindow = now - fpsStart;
        if (fpsWindow.count() >= 1.0) {
            const double fps = static_cast<double>(frameCount) / fpsWindow.count();
            std::cout << "FPS: " << std::fixed << std::setprecision(2) << fps
                      << " | Frame Time: " << frameTime.count() * 1000.0 << " ms\n";
            frameCount = 0;
            fpsStart = now;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
