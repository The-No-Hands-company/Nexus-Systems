#pragma once

#include <nexus/utility/physics/particle_emitter.h>
#include <functional>
#include <vector>

namespace nexus::utility::physics {
using namespace nexus::utility::math;

/**
 * @brief Particle updater with custom behaviors.
 */
class ParticleUpdater {
public:
    using UpdateFunc = std::function<void(ParticleEmitter::Particle&, float)>;

    /**
     * @brief Applies gravity to particles.
     */
    static UpdateFunc gravityUpdater(const Vector3& gravity) {
        return [gravity](ParticleEmitter::Particle& p, float dt) {
            p.acceleration += gravity;
        };
    }

    /**
     * @brief Applies drag/air resistance.
     */
    static UpdateFunc dragUpdater(float dragCoefficient) {
        return [dragCoefficient](ParticleEmitter::Particle& p, float dt) {
            p.velocity *= (1.0f - dragCoefficient * dt);
        };
    }

    /**
     * @brief Size over lifetime.
     */
    static UpdateFunc sizeOverLifetime(float startSize, float endSize) {
        return [startSize, endSize](ParticleEmitter::Particle& p, float dt) {
            float t = p.age / p.lifetime;
            p.size = startSize + (endSize - startSize) * t;
        };
    }

    /**
     * @brief Turbulence/noise.
     */
    static UpdateFunc turbulence(float strength) {
        return [strength](ParticleEmitter::Particle& p, float dt) {
            // Simple noise (would use Perlin in production)
            float noise = std::sin(p.position.x * 0.1f + p.age) * 
                         std::cos(p.position.z * 0.1f + p.age);
            p.velocity.x += noise * strength * dt;
            p.velocity.z += noise * strength * dt;
        };
    }

    /**
     * @brief Vortex force.
     */
    static UpdateFunc vortex(const Vector3& center, float strength) {
        return [center, strength](ParticleEmitter::Particle& p, float dt) {
            Vector3 toCenter = center - p.position;
            float distance = toCenter.length();
            if (distance > 0.001f) {
                Vector3 tangent = Vector3Utils::cross(toCenter, Vector3::Up()).normalized();
                p.velocity += tangent * (strength / distance) * dt;
            }
        };
    }

    /**
     * @brief Attractor force.
     */
    static UpdateFunc attractor(const Vector3& point, float strength) {
        return [point, strength](ParticleEmitter::Particle& p, float dt) {
            Vector3 toPoint = point - p.position;
            float distance = toPoint.length();
            if (distance > 0.001f) {
                p.velocity += toPoint.normalized() * (strength / (distance * distance)) * dt;
            }
        };
    }

    /**
     * @brief Collision with plane.
     */
    static UpdateFunc planeCollision(const Vector3& planePoint, const Vector3& planeNormal, 
                                    float restitution = 0.5f) {
        return [planePoint, planeNormal, restitution](ParticleEmitter::Particle& p, float dt) {
            float dist = Vector3Utils::dot(p.position - planePoint, planeNormal);
            if (dist < 0) {
                p.position -= planeNormal * dist;
                Vector3 velNormal = planeNormal * Vector3Utils::dot(p.velocity, planeNormal);
                p.velocity -= velNormal * (1.0f + restitution);
            }
        };
    }

    /**
     * @brief Applies multiple updaters.
     */
    static void applyUpdaters(ParticleEmitter::Particle& particle, float deltaTime,
                             const std::vector<UpdateFunc>& updaters) {
        for (const auto& updater : updaters) {
            updater(particle, deltaTime);
        }
    }
};

} // namespace nexus::utility::physics
