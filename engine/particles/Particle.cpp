#include "engine/particles/Particle.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;

Vec2 DirectionFromIndex(int i, int count) {
    const float angle = (static_cast<float>(i) / static_cast<float>(count)) * (2.0f * kPi);
    return {std::cos(angle), std::sin(angle)};
}
} // namespace

void ParticleManager::Clear() {
    particles_.clear();
}

void ParticleManager::Spawn(const Particle& particle) {
    if (particles_.size() >= kMaxParticles) {
        auto it = std::find_if(particles_.begin(), particles_.end(), [](const Particle& p) { return !p.active; });
        if (it != particles_.end()) {
            *it = particle;
            return;
        }
        particles_.erase(particles_.begin());
    }

    particles_.push_back(particle);
}

void ParticleManager::SpawnHitSpark(const Vec2& worldPosition) {
    constexpr int count = 12;
    for (int i = 0; i < count; ++i) {
        const Vec2 dir = DirectionFromIndex(i, count);
        Particle p{};
        p.position = worldPosition;
        p.velocity = Scale(dir, 1.8f + (static_cast<float>(i % 3) * 0.25f));
        p.lifetimeRemaining = 0.22f;
        p.lifetimeTotal = 0.22f;
        p.spriteFrame = 0;
        p.size = 0.06f;
        p.color = 0xFFFFC266u; // warm spark
        p.active = true;
        Spawn(p);
    }
}

void ParticleManager::SpawnPickupSparkle(const Vec2& worldPosition) {
    constexpr int count = 10;
    for (int i = 0; i < count; ++i) {
        const Vec2 dir = DirectionFromIndex(i, count);
        Particle p{};
        p.position = worldPosition;
        p.velocity = Add(Scale(dir, 0.8f + static_cast<float>(i % 2) * 0.35f), Vec2{0.0f, -0.22f});
        p.lifetimeRemaining = 0.55f;
        p.lifetimeTotal = 0.55f;
        p.spriteFrame = 1;
        p.size = 0.08f;
        p.color = 0xFFFFF29Eu; // bright pickup sparkle
        p.active = true;
        Spawn(p);
    }
}

void ParticleManager::SpawnAmbientDust(const Vec2& worldPosition) {
    Particle p{};
    p.position = worldPosition;
    p.velocity = {0.0f, -0.07f};
    p.lifetimeRemaining = 2.2f;
    p.lifetimeTotal = 2.2f;
    p.spriteFrame = 2;
    p.size = 0.045f;
    p.color = 0x99C2CCD8u; // subtle dusty blue-gray
    p.active = true;
    Spawn(p);
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
        p.velocity = Scale(p.velocity, 0.98f);
    }
}

const std::vector<Particle>& ParticleManager::Particles() const {
    return particles_;
}
