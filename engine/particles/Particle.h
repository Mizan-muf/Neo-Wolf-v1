#pragma once

#include <cstdint>
#include <vector>

#include "engine/core/Vec2.h"

struct Particle {
    Vec2 position{};
    Vec2 velocity{};
    float lifetimeRemaining = 0.0f;
    float lifetimeTotal = 0.0f;
    int spriteFrame = 0;
    float size = 0.1f;
    std::uint32_t color = 0xFFFFFFFFu;
    bool active = false;
};

class ParticleManager {
public:
    void Clear();
    void Spawn(const Particle& particle);
    void SpawnHitSpark(const Vec2& worldPosition);
    void SpawnPickupSparkle(const Vec2& worldPosition);
    void SpawnAmbientDust(const Vec2& worldPosition);
    void Update(float deltaSeconds);

    const std::vector<Particle>& Particles() const;

private:
    static constexpr std::size_t kMaxParticles = 512;
    std::vector<Particle> particles_{};
};
