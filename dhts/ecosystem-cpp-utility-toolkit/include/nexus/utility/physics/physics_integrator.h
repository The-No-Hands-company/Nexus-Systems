#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <functional>

namespace nexus::utility::physics {

/**
 * @brief Physics integration methods (Euler, Verlet, RK4).
 */
class PhysicsIntegrator {
public:
    using Vector3 = nexus::utility::math::Vector3;
    struct PhysicsState {
        Vector3 position;
        Vector3 velocity;
        Vector3 acceleration;
        float mass = 1.0f;
    };

    /**
     * @brief Euler integration (simple, less accurate).
     */
    static void eulerStep(PhysicsState& state, float dt) {
        state.velocity += state.acceleration * dt;
        state.position += state.velocity * dt;
    }

    /**
     * @brief Semi-implicit Euler (better energy conservation).
     */
    static void semiImplicitEulerStep(PhysicsState& state, float dt) {
        state.velocity += state.acceleration * dt;
        state.position += state.velocity * dt;
    }

    /**
     * @brief Verlet integration (good for constraints).
     */
    static void verletStep(PhysicsState& state, Vector3& previousPosition, float dt) {
        Vector3 temp = state.position;
        state.position = state.position * 2.0f - previousPosition + state.acceleration * (dt * dt);
        previousPosition = temp;
        state.velocity = (state.position - previousPosition) / dt;
    }

    /**
     * @brief RK4 integration (4th order Runge-Kutta, most accurate).
     */
    static void rk4Step(PhysicsState& state, float dt,
                       std::function<Vector3(const PhysicsState&)> accelerationFunc) {
        auto derivative = [&](const PhysicsState& s) -> std::pair<Vector3, Vector3> {
            return {s.velocity, accelerationFunc(s)};
        };

        auto k1 = derivative(state);
        
        PhysicsState state2 = state;
        state2.position += k1.first * (dt * 0.5f);
        state2.velocity += k1.second * (dt * 0.5f);
        auto k2 = derivative(state2);
        
        PhysicsState state3 = state;
        state3.position += k2.first * (dt * 0.5f);
        state3.velocity += k2.second * (dt * 0.5f);
        auto k3 = derivative(state3);
        
        PhysicsState state4 = state;
        state4.position += k3.first * dt;
        state4.velocity += k3.second * dt;
        auto k4 = derivative(state4);
        
        state.position += (k1.first + k2.first * 2.0f + k3.first * 2.0f + k4.first) * (dt / 6.0f);
        state.velocity += (k1.second + k2.second * 2.0f + k3.second * 2.0f + k4.second) * (dt / 6.0f);
    }

    /**
     * @brief Applies force to state.
     */
    static void applyForce(PhysicsState& state, const Vector3& force) {
        state.acceleration += force / state.mass;
    }

    /**
     * @brief Applies impulse (instant velocity change).
     */
    static void applyImpulse(PhysicsState& state, const Vector3& impulse) {
        state.velocity += impulse / state.mass;
    }

    /**
     * @brief Applies drag/damping.
     */
    static void applyDrag(PhysicsState& state, float dragCoefficient) {
        state.velocity *= (1.0f - dragCoefficient);
    }
};

} // namespace nexus::utility::physics
