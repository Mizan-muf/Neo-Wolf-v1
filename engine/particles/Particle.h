#pragma once

// Particles: a pool of short-lived coloured points in world space (drawn by ParticleRenderer as
// small squares) and the preset effects that spawn them (hit spark, pickup sparkle, ambient dust).

#include <cstddef>
#include <cstdint>
#include <vector>

#include "engine/core/Vec2.h"

struct Particle {
    Vec2 position{};                    // world (map) units
    Vec2 velocity{};                    // world units per second
    float lifetimeRemaining = 0.0f;     // seconds
    float lifetimeTotal = 0.0f;         // seconds; the renderer fades by lifetimeRemaining / lifetimeTotal
    float size = 0.1f;                  // world units
    std::uint32_t color = 0xFFFFFFFFu;  // ARGB
    bool active = false;
};

class ParticleManager {
public:
    void Clear();
    // Adds a particle. When the pool is full it reuses an expired slot, or else replaces the
    // oldest particle.
    void Spawn(const Particle& particle);
    void SpawnHitSpark(const Vec2& worldPosition);
    void SpawnPickupSparkle(const Vec2& worldPosition);
    void SpawnAmbientDust(const Vec2& worldPosition);
    void Update(float deltaSeconds);

    const std::vector<Particle>& Particles() const;

private:
    static constexpr std::size_t kMaxParticles = 512;
    std::vector<Particle> particles_;
};
