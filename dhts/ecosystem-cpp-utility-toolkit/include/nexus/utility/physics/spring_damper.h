#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <cmath>

namespace nexus::utility::physics {

/**
 * @brief Spring-damper system for physics simulations.
 */
class SpringDamper {
public:
    using Vector3 = nexus::utility::math::Vector3;
    /**
     * @brief Calculates spring force (Hooke's law).
     */
    static Vector3 springForce(const Vector3& position, const Vector3& target,
                              float stiffness, float restLength = 0.0f) {
        Vector3 delta = target - position;
        float distance = delta.length();
        float displacement = distance - restLength;
        
        if (distance < 0.00001f) {
            return Vector3::Zero();
        }
        
        return delta.normalized() * (stiffness * displacement);
    }

    /**
     * @brief Calculates damping force.
     */
    static Vector3 dampingForce(const Vector3& velocity, float damping) {
        return velocity * -damping;
    }

    /**
     * @brief Calculates combined spring-damper force.
     */
    static Vector3 springDamperForce(const Vector3& position, const Vector3& velocity,
                                    const Vector3& target, const Vector3& targetVelocity,
                                    float stiffness, float damping, float restLength = 0.0f) {
        Vector3 spring = springForce(position, target, stiffness, restLength);
        Vector3 damp = dampingForce(velocity - targetVelocity, damping);
        return spring + damp;
    }

    /**
     * @brief Critical damping coefficient.
     */
    static float criticalDamping(float mass, float stiffness) {
        return 2.0f * std::sqrt(mass * stiffness);
    }

    /**
     * @brief Smooth damp (Unity-style).
     */
    static float smoothDamp(float current, float target, float& currentVelocity,
                           float smoothTime, float maxSpeed, float deltaTime) {
        smoothTime = std::max(0.0001f, smoothTime);
        float omega = 2.0f / smoothTime;
        float x = omega * deltaTime;
        float exp = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
        
        float change = current - target;
        float originalTo = target;
        
        float maxChange = maxSpeed * smoothTime;
        change = std::clamp(change, -maxChange, maxChange);
        target = current - change;
        
        float temp = (currentVelocity + omega * change) * deltaTime;
        currentVelocity = (currentVelocity - omega * temp) * exp;
        
        float output = target + (change + temp) * exp;
        
        if ((originalTo - current > 0.0f) == (output > originalTo)) {
            output = originalTo;
            currentVelocity = (output - originalTo) / deltaTime;
        }
        
        return output;
    }

    /**
     * @brief Smooth damp for Vector3.
     */
    static Vector3 smoothDampVec3(const Vector3& current, const Vector3& target,
                                  Vector3& currentVelocity, float smoothTime,
                                  float maxSpeed, float deltaTime) {
        return Vector3(
            smoothDamp(current.x, target.x, currentVelocity.x, smoothTime, maxSpeed, deltaTime),
            smoothDamp(current.y, target.y, currentVelocity.y, smoothTime, maxSpeed, deltaTime),
            smoothDamp(current.z, target.z, currentVelocity.z, smoothTime, maxSpeed, deltaTime)
        );
    }
};

} // namespace nexus::utility::physics
