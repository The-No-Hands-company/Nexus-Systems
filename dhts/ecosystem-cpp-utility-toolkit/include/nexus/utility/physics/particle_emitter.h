#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/graphics/color_utils.h>
#include <vector>
#include <random>

namespace nexus::utility::physics {

/**
 * @brief Particle emitter for particle systems.
 */
class ParticleEmitter {
public:
    using Vector3 = nexus::utility::math::Vector3;
    using Color = nexus::utility::graphics::Color;
    using ColorUtils = nexus::utility::graphics::ColorUtils;
    struct Particle {
        Vector3 position;
        Vector3 velocity;
        Vector3 acceleration;
        Color color;
        float lifetime;
        float age;
        float size;
        bool active;
        
        Particle() : lifetime(1.0f), age(0), size(1.0f), active(false) {}
    };

    struct EmitterConfig {
        Vector3 position;
        Vector3 direction{0, 1, 0};
        float emissionRate = 10.0f;  // particles per second
        float particleLifetime = 2.0f;
        float particleSize = 1.0f;
        float spread = 0.2f;  // cone angle
        float speed = 5.0f;
        float speedVariation = 1.0f;
        Color startColor = Color(1, 1, 1, 1);
        Color endColor = Color(1, 1, 1, 0);
        Vector3 gravity{0, -9.8f, 0};
        size_t maxParticles = 1000;
    };

    explicit ParticleEmitter(const EmitterConfig& config)
        : config_(config), accumulator_(0), rng_(std::random_device{}()) {
        particles_.resize(config.maxParticles);
    }

    /**
     * @brief Updates particle system.
     */
    void update(float deltaTime) {
        // Emit new particles
        accumulator_ += deltaTime;
        float emitInterval = 1.0f / config_.emissionRate;
        
        while (accumulator_ >= emitInterval) {
            emitParticle();
            accumulator_ -= emitInterval;
        }
        
        // Update existing particles
        for (auto& particle : particles_) {
            if (!particle.active) continue;
            
            particle.age += deltaTime;
            if (particle.age >= particle.lifetime) {
                particle.active = false;
                continue;
            }
            
            // Physics
            particle.velocity += particle.acceleration * deltaTime;
            particle.position += particle.velocity * deltaTime;
            
            // Color interpolation
            float t = particle.age / particle.lifetime;
            particle.color = ColorUtils::lerp(config_.startColor, config_.endColor, t);
        }
    }

    /**
     * @brief Gets active particles.
     */
    std::vector<Particle> getActiveParticles() const {
        std::vector<Particle> active;
        for (const auto& p : particles_) {
            if (p.active) active.push_back(p);
        }
        return active;
    }

    /**
     * @brief Gets particle count.
     */
    size_t getActiveCount() const {
        size_t count = 0;
        for (const auto& p : particles_) {
            if (p.active) count++;
        }
        return count;
    }

    /**
     * @brief Clears all particles.
     */
    void clear() {
        for (auto& p : particles_) {
            p.active = false;
        }
    }

    /**
     * @brief Sets emitter position.
     */
    void setPosition(const Vector3& pos) { config_.position = pos; }

    /**
     * @brief Gets configuration.
     */
    const EmitterConfig& getConfig() const { return config_; }
    EmitterConfig& getConfig() { return config_; }

private:
    void emitParticle() {
        // Find inactive particle
        for (auto& particle : particles_) {
            if (!particle.active) {
                particle.active = true;
                particle.position = config_.position;
                particle.age = 0;
                particle.lifetime = config_.particleLifetime;
                particle.size = config_.particleSize;
                particle.color = config_.startColor;
                
                // Random velocity within cone
                std::uniform_real_distribution<float> spreadDist(-config_.spread, config_.spread);
                std::uniform_real_distribution<float> speedDist(
                    config_.speed - config_.speedVariation,
                    config_.speed + config_.speedVariation
                );
                
                Vector3 randomDir = config_.direction;
                randomDir.x += spreadDist(rng_);
                randomDir.y += spreadDist(rng_);
                randomDir.z += spreadDist(rng_);
                randomDir = randomDir.normalized();
                
                particle.velocity = randomDir * speedDist(rng_);
                particle.acceleration = config_.gravity;
                
                break;
            }
        }
    }

    EmitterConfig config_;
    std::vector<Particle> particles_;
    float accumulator_;
    std::mt19937 rng_;
};

} // namespace nexus::utility::physics
