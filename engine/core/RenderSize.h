#pragma once

#include <string>

// The size of the framebuffer the game draws into (--render-size, EngineOptions::renderSize)
// and its limits. The window shows the framebuffer letterboxed and scaled; the simulation
// never depends on it (docs/ARCHITECTURE.md, "Rendering").

struct RenderSize {
    int width = 0;
    int height = 0;
};

inline constexpr RenderSize kDefaultRenderSize{640, 360};  // 16:9
inline constexpr int kMinRenderWidth = 320;
inline constexpr int kMaxRenderWidth = 1920;
inline constexpr int kMinRenderHeight = 200;
inline constexpr int kMaxRenderHeight = 1080;
// The shape limits, width:height from 5:4 to 64:27 (2560x1080, the monitors sold as 21:9;
// 21/9 itself is a little narrower). The horizontal field of view follows the shape
// (RenderMath.h), so these keep it between about 64 and 100 degrees, and keep the UI from
// being squeezed into a narrow strip.
inline constexpr RenderSize kMinRenderAspect{5, 4};
inline constexpr RenderSize kMaxRenderAspect{64, 27};

// Inside the limits, and both even so that the view's centre lies between two pixel rows and
// two pixel columns.
constexpr bool IsValidRenderSize(const RenderSize& size) {
    return size.width >= kMinRenderWidth && size.width <= kMaxRenderWidth && size.height >= kMinRenderHeight &&
           size.height <= kMaxRenderHeight && size.width % 2 == 0 && size.height % 2 == 0 &&
           size.width * kMinRenderAspect.height >= size.height * kMinRenderAspect.width &&
           size.width * kMaxRenderAspect.height <= size.height * kMaxRenderAspect.width;
}

// The initial window is the largest whole multiple of the render size that fits in this box
// (at least 1x): 1280x720 for the default 640x360, 1280x960 for 320x240. Whole multiples keep
// the nearest-neighbour scaling even. PlatformSDL shrinks it on a small desktop.
inline constexpr RenderSize kWindowBox{1280, 960};

constexpr RenderSize InitialWindowSize(const RenderSize& render) {
    const int byWidth = kWindowBox.width / render.width;
    const int byHeight = kWindowBox.height / render.height;
    const int multiple = byWidth < byHeight ? byWidth : byHeight;
    return multiple > 1 ? RenderSize{render.width * multiple, render.height * multiple} : render;
}

// The limits in words, for error messages (--render-size, Engine::Init).
inline std::string DescribeRenderSizeLimits() {
    return "an even width from " + std::to_string(kMinRenderWidth) + " to " + std::to_string(kMaxRenderWidth) +
           ", an even height from " + std::to_string(kMinRenderHeight) + " to " + std::to_string(kMaxRenderHeight) +
           ", and a width:height from 5:4 to 64:27";
}
