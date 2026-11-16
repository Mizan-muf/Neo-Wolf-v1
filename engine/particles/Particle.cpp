#include "engine/particles/Particle.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "engine/core/MathUtils.h"

namespace {
// Velocity is multiplied by this once per Update call (per frame, not per second).
constexpr float kVelocityDampingPerUpdate = 0.98f;

// A ring of particles flying outwards from one point. Particle i of `count` leaves at angle
// i / count of a full turn with speed baseSpeed + (i % speedVariants) * speedStep, plus `drift`.
struct RingBurst {
    int count = 0;
    float baseSpeed = 0.0f;  // world units per second
    int speedVariants = 1;
    float speedStep = 0.0f;   // world units per second
    Vec2 drift{};             // world units per second, added to every particle's velocity
    float lifetime = 0.0f;    // seconds
    float size = 0.0f;        // world units
    std::uint32_t color = 0;  // ARGB
};

constexpr RingBurst kHitSpark{12, 1.8f, 3, 0.25f, {0.0f, 0.0f}, 0.22f, 0.06f, 0xFFFFC266u};         // warm spark
constexpr RingBurst kPickupSparkle{10, 0.8f, 2, 0.35f, {0.0f, -0.22f}, 0.55f, 0.08f, 0xFFFFF29Eu};  // bright

// Ambient dust: one slow, long-lived particle.
constexpr Vec2 kDustVelocity{0.0f, -0.07f};
constexpr float kDustLifetime = 2.2f;
constexpr float kDustSize = 0.045f;
constexpr std::uint32_t kDustColor = 0x99C2CCD8u;  // subtle dusty blue-gray, partly transparent

Vec2 DirectionFromIndex(int i, int count) {
    const float angle = (static_cast<float>(i) / static_cast<float>(count)) * (2.0f * kPi);
    return {std::cos(angle), std::sin(angle)};
}

Particle MakeParticle(const Vec2& position, const Vec2& velocity, float lifetime, float size, std::uint32_t color) {
    Particle p{};
    p.position = position;
    p.velocity = velocity;
    p.lifetimeRemaining = lifetime;
    p.lifetimeTotal = lifetime;
    p.size = size;
    p.color = color;
    p.active = true;
    return p;
}

// Spawns burst.count particles evenly spread around worldPosition.
void SpawnRing(ParticleManager& manager, const Vec2& worldPosition, const RingBurst& burst) {
    for (int i = 0; i < burst.count; ++i) {
        const Vec2 dir = DirectionFromIndex(i, burst.count);
        const float speed = burst.baseSpeed + (static_cast<float>(i % burst.speedVariants) * burst.speedStep);
        const Vec2 velocity = Add(Scale(dir, speed), burst.drift);
        manager.Spawn(MakeParticle(worldPosition, velocity, burst.lifetime, burst.size, burst.color));
    }
}
}  // namespace

void ParticleManager::Clear() {
    particles_.clear();
}

void ParticleManager::Spawn(const Particle& particle) {
    if (particles_.size() >= kMaxParticles) {
        const auto it = std::find_if(particles_.begin(), particles_.end(), [](const Particle& p) { return !p.active; });
        if (it != particles_.end()) {
            *it = particle;
            return;
        }
        particles_.erase(particles_.begin());
    }

    particles_.push_back(particle);
}

void ParticleManager::SpawnHitSpark(const Vec2& worldPosition) {
    SpawnRing(*this, worldPosition, kHitSpark);
}

void ParticleManager::SpawnPickupSparkle(const Vec2& worldPosition) {
    SpawnRing(*this, worldPosition, kPickupSparkle);
}

void ParticleManager::SpawnAmbientDust(const Vec2& worldPosition) {
    Spawn(MakeParticle(worldPosition, kDustVelocity, kDustLifetime, kDustSize, kDustColor));
}

void ParticleManager::Update(float deltaSeconds) {
    if (deltaSeconds <= 0.0f) {
        return;
    }

    for (Particle& p : particles_) {
        if (!p.active) {
            continue;
        }

        p.lifetimeRemaining -= deltaSeconds;
        if (p.lifetimeRemaining <= 0.0f) {
            p.active = false;
            continue;
        }

        p.position = Add(p.position, Scale(p.velocity, deltaSeconds));
        p.velocity = Scale(p.velocity, kVelocityDampingPerUpdate);
    }
}

const std::vector<Particle>& ParticleManager::Particles() const {
    return particles_;
}
