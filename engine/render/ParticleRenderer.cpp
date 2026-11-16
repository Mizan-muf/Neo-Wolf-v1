#include "engine/render/ParticleRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>

#include "engine/core/MathUtils.h"
#include "engine/render/Framebuffer.h"
#include "engine/render/RenderMath.h"
#include "engine/world/Map.h"
#include "engine/world/Player.h"

namespace {
// Particle shading: distance falloff per unit, never below kParticleMinDistanceShade; then the
// light level and the fraction of the lifetime left (a particle fades out).
constexpr float kParticleFalloff = 0.17f;
constexpr float kParticleMinDistanceShade = 0.4f;
// Particles with a lifetime this short (or unset) are skipped: their fade would divide by ~0.
constexpr float kMinParticleLifetime = 0.0001f;
}  // namespace

void ParticleRenderer::Render(const Map& map, const Player& player, Framebuffer& framebuffer,
                              const std::vector<float>& wallDepthBuffer, const std::vector<Particle>& particles,
                              float fieldOfViewRadians) {
    const int screenWidth = framebuffer.Width();
    const int screenHeight = framebuffer.Height();
    if (screenWidth <= 0 || screenHeight <= 0) {
        return;
    }
    if (wallDepthBuffer.size() != static_cast<std::size_t>(screenWidth)) {
        return;
    }

    const std::optional<BillboardProjection> projection =
        MakeBillboardProjection(player.position, MakeCameraBasis(player.directionAngleRadians, fieldOfViewRadians));
    if (!projection) {
        return;
    }

    for (const Particle& p : particles) {
        if (!p.active || p.lifetimeTotal <= kMinParticleLifetime) {
            continue;
        }

        const ViewPoint view = ToViewSpace(*projection, p.position);
        if (view.depth <= kBillboardNearPlane) {
            continue;
        }

        const int screenX = ScreenColumn(view, screenWidth);
        if (screenX < 0 || screenX >= screenWidth) {
            continue;
        }
        if (view.depth >= wallDepthBuffer[static_cast<std::size_t>(screenX)]) {
            continue;
        }

        // A square of p.size world units, centred on the horizon.
        const int sizePx = std::max(1, static_cast<int>((p.size * static_cast<float>(screenHeight)) / view.depth));
        const int x0 = screenX - (sizePx / 2);
        const int y0 = (screenHeight / 2) - (sizePx / 2);
        const float life = Clamp(p.lifetimeRemaining / p.lifetimeTotal, 0.0f, 1.0f);
        const float mapLight = LightAt(map, p.position.x, p.position.y);
        const float distanceShade = DistanceShade(view.depth, kParticleFalloff, kParticleMinDistanceShade);
        framebuffer.DrawRect(x0, y0, sizePx, sizePx, ScaleRgb(p.color, mapLight * distanceShade * life));
    }
}
