#include <chrono>
#include <iomanip>
#include <iostream>

#include "engine/core/Engine.h"

int main(int /*argc*/, char* /*argv*/[]) {
    Engine engine;
    if (!engine.Init()) {
        return 1;
    }

    using clock = std::chrono::steady_clock;
    auto previous = clock::now();
    auto fpsStart = previous;
    int frameCount = 0;

    std::cout << "Main loop started.\n";

    while (engine.IsRunning()) {
        auto now = clock::now();
        const std::chrono::duration<double> frameTime = now - previous;
        previous = now;

        engine.Update(frameTime.count());
        engine.Render();

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

    engine.Shutdown();
    return 0;
}
