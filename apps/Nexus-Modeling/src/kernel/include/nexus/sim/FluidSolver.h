#pragma once

#include <cstdint>
#include <vector>

namespace nexus {

using FluidParticleId = uint32_t;
inline constexpr FluidParticleId kInvalidFluidParticleId = 0u;

/// Minimal 3-component vector for fluid simulation.
struct FluidVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] FluidVec3 operator+(const FluidVec3& o) const noexcept { return {x+o.x, y+o.y, z+o.z}; }
    [[nodiscard]] FluidVec3 operator-(const FluidVec3& o) const noexcept { return {x-o.x, y-o.y, z-o.z}; }
    [[nodiscard]] FluidVec3 operator*(float s)            const noexcept { return {x*s,   y*s,   z*s};   }
    FluidVec3& operator+=(const FluidVec3& o) noexcept { x+=o.x; y+=o.y; z+=o.z; return *this; }
    [[nodiscard]] bool operator==(const FluidVec3& o) const noexcept { return x==o.x && y==o.y && z==o.z; }
};

/// Initial description for a fluid particle.
struct FluidParticleDesc {
    float     mass     = 1.0f;               ///< kg. Zero means static boundary marker.
    FluidVec3 position = {0.0f, 0.0f, 0.0f};
    FluidVec3 velocity = {0.0f, 0.0f, 0.0f};
    float     density  = 1000.0f;            ///< Rest density (kg/m³).
};

/// Per-particle state snapshot.
struct FluidParticleSnapshot {
    FluidParticleId id;
    FluidVec3       position;
    FluidVec3       velocity;
    float           density;   ///< Instantaneous computed density.
};

/// Full fluid state snapshot for replay and rollback.
struct FluidState {
    std::vector<FluidParticleSnapshot> particles;
    double simulationTime = 0.0;
};

[[nodiscard]] bool operator==(const FluidParticleSnapshot& a, const FluidParticleSnapshot& b) noexcept;
[[nodiscard]] bool operator==(const FluidState& a, const FluidState& b) noexcept;

/// Deterministic cacheable byte format for fluid state snapshots.
[[nodiscard]] std::vector<std::uint8_t> serializeFluidState(const FluidState& state);

/// Restores a FluidState from serializeFluidState(). Returns false on malformed input.
[[nodiscard]] bool deserializeFluidState(const std::vector<std::uint8_t>& bytes, FluidState& outState);

/// Step report returned by FluidSolver::step().
struct FluidStepReport {
    bool        ok                = true;
    double      simulationTime    = 0.0;
    std::size_t particlesAdvanced = 0;  ///< Dynamic particles that were integrated.
};

/// Fluid solver baseline: SPH-inspired particle integration with explicit Euler.
///
/// Particles represent fluid elements. Static particles (mass == 0) act as
/// boundary markers and are never integrated.
/// Gravity is applied to dynamic particles each step.
/// Pressure forces are computed from local density estimates using a simple
/// nearest-neighbor kernel; the model is intentionally minimal for the v0 contract.
///
/// Determinism contract: identical initial state + identical step sequence →
/// identical trajectory.
///
/// Thread-safety: none.
class FluidSolver {
public:
    FluidSolver();
    ~FluidSolver();

    // ── Particle management ──────────────────────────────────────────────────

    /// Add a fluid particle. Returns a stable id never reused within a session.
    [[nodiscard]] FluidParticleId addParticle(const FluidParticleDesc& desc);

    /// Remove a particle. Returns false if unknown.
    [[nodiscard]] bool removeParticle(FluidParticleId id) noexcept;

    [[nodiscard]] bool        hasParticle(FluidParticleId id) const noexcept;
    [[nodiscard]] std::size_t particleCount()                 const noexcept;

    /// Returns false if id unknown or particle is static (mass == 0).
    [[nodiscard]] bool getParticleState(FluidParticleId id,
                                        FluidVec3& outPosition,
                                        FluidVec3& outVelocity,
                                        float&     outDensity) const noexcept;

    // ── Solver parameters ────────────────────────────────────────────────────

    void setGravity(FluidVec3 gravity) noexcept;
    [[nodiscard]] FluidVec3 gravity() const noexcept;

    /// Smoothing radius for density estimation (m). Must be > 0. Default: 0.1.
    void setSmoothingRadius(float h) noexcept;
    [[nodiscard]] float smoothingRadius() const noexcept;

    /// Pressure stiffness constant k. Default: 200.
    void setPressureStiffness(float k) noexcept;
    [[nodiscard]] float pressureStiffness() const noexcept;

    // ── Simulation step ──────────────────────────────────────────────────────

    /// Advance the solver by dt seconds (must be > 0).
    [[nodiscard]] FluidStepReport step(double dt);

    // ── Snapshot / rollback ──────────────────────────────────────────────────

    [[nodiscard]] FluidState captureState() const;

    /// Restore previously captured state. Returns false if particle sets are incompatible.
    [[nodiscard]] bool restoreState(const FluidState& state);

private:
    struct Impl;
    Impl* m_impl;
};

} // namespace nexus
