#include "engine/core/Engine.h"

#include "engine/core/Color.h"
#include "engine/platform/PlatformSDL.h"

void Engine::Render() {
    if (!running_ || platform_ == nullptr) {
        return;
    }

    framebuffer_.Clear(PackARGB32(MakeColor(20, 20, 24)));

    const int rectWidth = framebuffer_.Width() / 4;
    const int rectHeight = framebuffer_.Height() / 4;
    const int rectX = (framebuffer_.Width() - rectWidth) / 2;
    const int rectY = (framebuffer_.Height() - rectHeight) / 2;
    framebuffer_.DrawRect(rectX, rectY, rectWidth, rectHeight, PackARGB32(MakeColor(200, 60, 40)));

    if (framebuffer_.Width() > 0 && framebuffer_.Height() > 0) {
        const int lineX = static_cast<int>(time_.frameIndex % static_cast<std::uint64_t>(framebuffer_.Width()));
        framebuffer_.DrawVerticalLine(lineX, 0, framebuffer_.Height() - 1, PackARGB32(MakeColor(120, 220, 255)));
    }

    platform_->PresentFramebuffer(framebuffer_);
}
