#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <functional>
#include <cmath>
#include <vector>

namespace nexus::utility::physics {

/**
 * @brief Force field for physics simulations.
 */
class ForceField {
public:
    using Vector3 = nexus::utility::math::Vector3;
    using Vector3Utils = nexus::utility::math::Vector3Utils;
    using ForceFunc = std::function<Vector3(const Vector3&)>;

    /**
     * @brief Constant force (e.g., gravity).
     */
    static ForceFunc constantForce(const Vector3& force) {
        return [force](const Vector3&) { return force; };
    }

    /**
     * @brief Radial force (attraction/repulsion from point).
     */
    static ForceFunc radialForce(const Vector3& center, float strength, bool attract = true) {
        return [center, strength, attract](const Vector3& pos) {
            Vector3 delta = center - pos;
            float distSq = delta.lengthSquared();
            if (distSq < 0.001f) return Vector3::Zero();
            
            float force = strength / distSq;
            Vector3 direction = delta.normalized();
            return direction * (attract ? force : -force);
        };
    }

    /**
     * @brief Directional force (wind).
     */
    static ForceFunc directionalForce(const Vector3& direction, float strength, 
                                     float turbulence = 0.0f) {
        return [direction, strength, turbulence](const Vector3& pos) {
            Vector3 force = direction.normalized() * strength;
            
            if (turbulence > 0) {
                // Simple turbulence
                float noise = std::sin(pos.x * 0.1f) * std::cos(pos.z * 0.1f);
                force += Vector3(noise, noise * 0.5f, noise) * turbulence;
            }
            
            return force;
        };
    }

    /**
     * @brief Vortex force.
     */
    static ForceFunc vortexForce(const Vector3& center, const Vector3& axis, float strength) {
        return [center, axis, strength](const Vector3& pos) {
            Vector3 toPos = pos - center;
            Vector3 tangent = Vector3Utils::cross(axis.normalized(), toPos);
            float distance = toPos.length();
            
            if (distance < 0.001f) return Vector3::Zero();
            
            return tangent.normalized() * (strength / distance);
        };
    }

    /**
     * @brief Spherical force field (force inside sphere).
     */
    static ForceFunc sphericalField(const Vector3& center, float radius, 
                                   const Vector3& innerForce) {
        return [center, radius, innerForce](const Vector3& pos) {
            float dist = Vector3Utils::distance(pos, center);
            if (dist < radius) {
                float t = 1.0f - (dist / radius);
                return innerForce * t;
            }
            return Vector3::Zero();
        };
    }

    /**
     * @brief Drag force (proportional to velocity).
     */
    static ForceFunc dragForce(float coefficient) {
        return [coefficient](const Vector3& velocity) {
            return velocity * -coefficient;
        };
    }

    /**
     * @brief Spring force to point.
     */
    static ForceFunc springForce(const Vector3& anchor, float stiffness, float damping = 0) {
        return [anchor, stiffness, damping](const Vector3& pos) {
            Vector3 displacement = anchor - pos;
            return displacement * stiffness;
        };
    }

    /**
     * @brief Combines multiple force fields.
     */
    static Vector3 combinedForce(const Vector3& position, 
                                const std::vector<ForceFunc>& fields) {
        Vector3 totalForce = Vector3::Zero();
        for (const auto& field : fields) {
            totalForce += field(position);
        }
        return totalForce;
    }

    /**
     * @brief Applies force field to velocity.
     */
    static void applyForce(Vector3& velocity, const Vector3& position, 
                          const ForceFunc& field, float mass, float deltaTime) {
        Vector3 force = field(position);
        Vector3 acceleration = force / mass;
        velocity += acceleration * deltaTime;
    }
};

} // namespace nexus::utility::physics
