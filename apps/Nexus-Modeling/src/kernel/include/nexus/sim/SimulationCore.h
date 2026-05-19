#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nexus {

using BodyId = uint32_t;
inline constexpr BodyId kInvalidBodyId = 0u;

/// Minimal 3-component vector used by the simulation subsystem.
/// Kept independent so the sim layer has no dependency on render math types.
struct SimVec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    [[nodiscard]] SimVec3 operator+(const SimVec3& o) const noexcept { return {x+o.x, y+o.y, z+o.z}; }
    [[nodiscard]] SimVec3 operator-(const SimVec3& o) const noexcept { return {x-o.x, y-o.y, z-o.z}; }
    [[nodiscard]] SimVec3 operator*(float s)          const noexcept { return {x*s,   y*s,   z*s};   }
    SimVec3& operator+=(const SimVec3& o) noexcept { x+=o.x; y+=o.y; z+=o.z; return *this; }
    [[nodiscard]] bool operator==(const SimVec3& o) const noexcept { return x==o.x && y==o.y && z==o.z; }
};

/// Initial description used when adding a body to the solver.
struct SimBodyDesc {
    float   mass     = 1.0f;              ///< kg. Zero means static (kinematic) — not integrated.
    SimVec3 position = {0.0f, 0.0f, 0.0f};
    SimVec3 velocity = {0.0f, 0.0f, 0.0f};
};

/// Cacheable per-body state for snapshot/restore.
struct SimBodySnapshot {
    BodyId  id;
    SimVec3 position;
    SimVec3 velocity;
};

/// Full simulation state snapshot for replay and rollback.
struct SimState {
    std::vector<SimBodySnapshot> bodies;
    double simulationTime = 0.0;
};

[[nodiscard]] bool operator==(const SimBodySnapshot& a, const SimBodySnapshot& b) noexcept;
[[nodiscard]] bool operator==(const SimState& a, const SimState& b) noexcept;

/// Deterministic cacheable byte format for replay and rollback snapshots.
/// Format: magic + version + simulation time + ordered body records.
[[nodiscard]] std::vector<std::uint8_t> serializeSimState(const SimState& state);

/// Restores a SimState from serializeSimState(). Returns false on malformed input.
[[nodiscard]] bool deserializeSimState(const std::vector<std::uint8_t>& bytes, SimState& outState);

/// Report returned by RigidBodySolver::step().
struct StepReport {
    bool        ok               = true;
    double      simulationTime   = 0.0;   ///< Accumulated time after this step.
    std::size_t bodiesIntegrated = 0;     ///< Dynamic bodies that were advanced.
};

/// Rigid-body solver baseline.
///
/// Integration model: explicit Euler with gravity + per-body force accumulation.
/// Forces are applied once per step and cleared afterwards.
/// Static bodies (mass == 0) are not integrated and ignore applied forces.
///
/// Determinism contract: identical initial state + identical step sequence →
/// identical trajectory.  Floating-point order is preserved across calls.
///
/// Thread-safety: none.
class RigidBodySolver {
public:
    RigidBodySolver();
    ~RigidBodySolver();

    // ── Body management ──────────────────────────────────────────────────────

    /// Add a body. Returns a stable BodyId that is never reused within a session.
    [[nodiscard]] BodyId addBody(const SimBodyDesc& desc);

    /// Remove a body. Returns false if id unknown.
    [[nodiscard]] bool removeBody(BodyId id) noexcept;

    [[nodiscard]] bool        hasBody(BodyId id)    const noexcept;
    [[nodiscard]] std::size_t bodyCount()           const noexcept;

    /// Returns false if id unknown or body is static (mass == 0).
    [[nodiscard]] bool getBodyState(BodyId id, SimVec3& outPosition, SimVec3& outVelocity) const noexcept;

    // ── Force accumulation ───────────────────────────────────────────────────

    /// Accumulate a force on a body. Applied at the next step, then cleared.
    /// Returns false if id unknown or body is static.
    [[nodiscard]] bool applyForce(BodyId id, SimVec3 force) noexcept;

    /// Set gravity applied to all dynamic bodies each step.
    /// Default: {0, -9.81, 0}.
    void setGravity(SimVec3 gravity) noexcept;
    [[nodiscard]] SimVec3 gravity() const noexcept;

    // ── Simulation step ──────────────────────────────────────────────────────

    /// Advance the simulation by dt seconds (must be finite and > 0).
    /// Returns a report with ok == false and bodiesIntegrated == 0 if dt is
    /// non-finite or <= 0.
    /// dt is double to prevent float-precision leakage into time accumulation;
    /// positions and velocities are still integrated at float precision.
    [[nodiscard]] StepReport step(double dt);

    /// Advance the simulation by splitting dt into fixed-size substeps.
    /// Both dt and fixedSubstep must be finite and > 0. Any remainder smaller
    /// than fixedSubstep is integrated as a final shorter step.
    ///
    /// Force accumulation is consumed once across the full call (after all
    /// substeps), so an applied force affects every internal substep and then
    /// is cleared.
    [[nodiscard]] StepReport stepFixed(double dt, double fixedSubstep);

    // ── Replay and rollback ──────────────────────────────────────────────────

    /// Capture all body states and the current simulation time.
    /// Snapshot bodies are ordered by ascending BodyId for deterministic replay
    /// serialization and stable comparisons.
    [[nodiscard]] SimState captureState() const;

    /// Restore all body states from a snapshot.
    /// Bodies present in state but absent in solver are ignored.
    /// Bodies present in solver but absent in state retain their current values.
    /// Returns false if state is structurally invalid (e.g., empty while solver
    /// has bodies — treated as a no-op with false return for safety).
    [[nodiscard]] bool restoreState(const SimState& state);

    // ── Time ─────────────────────────────────────────────────────────────────

    [[nodiscard]] double simulationTime() const noexcept;

    // ── Lifetime ─────────────────────────────────────────────────────────────

    void clear() noexcept;

private:
    struct Body {
        BodyId  id;
        float   mass;         ///< 0 = static
        SimVec3 position;
        SimVec3 velocity;
        SimVec3 force;        ///< accumulated, cleared after each step
    };

    std::unordered_map<BodyId, Body> m_bodies;
    BodyId            m_nextId  = 1u;
    double            m_time    = 0.0;
    SimVec3           m_gravity = {0.0f, -9.81f, 0.0f};

    Body*       findBody(BodyId id)       noexcept;
    const Body* findBody(BodyId id) const noexcept;
};

} // namespace nexus
