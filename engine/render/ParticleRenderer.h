#pragma once

// Particles drawn as small shaded squares, projected like sprites and clipped against the
// wall depth buffer.

#include <vector>

#include "engine/particles/Particle.h"

class Framebuffer;
class Map;
struct Player;

namespace ParticleRenderer {
void Render(const Map& map, const Player& player, Framebuffer& framebuffer, const std::vector<float>& wallDepthBuffer,
            const std::vector<Particle>& particles);
}  // namespace ParticleRenderer
