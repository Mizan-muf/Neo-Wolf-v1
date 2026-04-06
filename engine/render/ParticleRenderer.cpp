#include "engine/render/ParticleRenderer.h"

#include <cmath>

#include "engine/core/MathUtils.h"
#include "engine/render/Framebuffer.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

namespace {
std::uint32_t ShadeParticle(std::uint32_t color, float factor) {
    factor = Clamp(factor, 0.0f, 1.0f);
    const std::uint8_t a = static_cast<std::uint8_t>((color >> 24) & 0xFFu);
    const std::uint8_t r = static_cast<std::uint8_t>((color >> 16) & 0xFFu);
    const std::uint8_t g = static_cast<std::uint8_t>((color >> 8) & 0xFFu);
    const std::uint8_t b = static_cast<std::uint8_t>(color & 0xFFu);
    return (static_cast<std::uint32_t>(a) << 24) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(r * factor)) << 16) |
           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(g * factor)) << 8) |
           static_cast<std::uint32_t>(static_cast<std::uint8_t>(b * factor));
}
} // namespace

void ParticleRenderer::Render(
    const Map& map,
    const Player& player,
    Framebuffer& framebuffer,
    const std::vector<float>& wallDepthBuffer,
    const std::vector<Particle>& particles,
    float fieldOfViewRadians) {
    const int screenWidth = framebuffer.Width();
    const int screenHeight = framebuffer.Height();
    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }
    if (wallDepthBuffer.size() != static_cast<std::size_t>(screenWidth)) {
        return;
    }

    const Vec2 forward{std::cos(player.directionAngleRadians), std::sin(player.directionAngleRadians)};
    const float halfFov = fieldOfViewRadians * 0.5f;
    const float planeScale = std::tan(halfFov);
    const Vec2 cameraPlane{-forward.y * planeScale, forward.x * planeScale};
    const float det = (cameraPlane.x * forward.y) - (forward.x * cameraPlane.y);
    if (std::fabs(det) < 1e-6f) {
        return;
    }
    const float invDet = 1.0f / det;

    for (const Particle& p : particles) {
        if (!p.active || p.lifetimeTotal <= 0.0001f) {
            continue;
        }

        const float relX = p.position.x - player.position.x;
        const float relY = p.position.y - player.position.y;
        const float transformX = invDet * ((forward.y * relX) - (forward.x * relY));
        const float transformY = invDet * ((-cameraPlane.y * relX) + (cameraPlane.x * relY));
        if (transformY <= 0.001f) {
            continue;
        }

        const int screenX = static_cast<int>((static_cast<float>(screenWidth) * 0.5f) * (1.0f + transformX / transformY));
        const int sizePx = std::max(1, static_cast<int>((p.size * static_cast<float>(screenHeight)) / transformY));
        const int x0 = screenX - (sizePx / 2);
        const int y0 = (screenHeight / 2) - (sizePx / 2);

        if (screenX < 0 || screenX >= screenWidth) {
            continue;
        }
        if (transformY >= wallDepthBuffer[static_cast<std::size_t>(screenX)]) {
            continue;
        }

        const float life = Clamp(p.lifetimeRemaining / p.lifetimeTotal, 0.0f, 1.0f);
        const int cellX = static_cast<int>(std::floor(p.position.x));
        const int cellY = static_cast<int>(std::floor(p.position.y));
        const float mapLight = map.GetLightLevel(cellX, cellY);
        const float distanceShade = Clamp(1.0f / (1.0f + transformY * 0.17f), 0.4f, 1.0f);
        const std::uint32_t shaded = ShadeParticle(p.color, mapLight * distanceShade * life);
        framebuffer.DrawRect(x0, y0, sizePx, sizePx, shaded);
    }
}
