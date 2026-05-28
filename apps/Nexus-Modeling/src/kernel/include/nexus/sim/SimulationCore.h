#pragma once

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nexus {

using BodyId = uint32_t;
inline constexpr BodyId kInvalidBodyId = 0u;

using ConstraintId = uint32_t;
inline constexpr ConstraintId kInvalidConstraintId = 0u;

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

/// Minimal unit-quaternion (xyzw) used by the simulation subsystem for body
/// orientation. Kept independent of render math types; defaults to identity.
struct SimQuat {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;

    [[nodiscard]] bool operator==(const SimQuat& o) const noexcept {
        return x==o.x && y==o.y && z==o.z && w==o.w;
    }
};

/// Initial description used when adding a body to the solver.
struct SimBodyDesc {
    float   mass     = 1.0f;              ///< kg. Zero means static (kinematic) — not integrated.
    SimVec3 position = {0.0f, 0.0f, 0.0f};
    SimVec3 velocity = {0.0f, 0.0f, 0.0f};
    SimVec3 inertia        = {1.0f, 1.0f, 1.0f}; ///< principal moments of inertia (body space); each finite and > 0.
    SimQuat orientation    = {};          ///< initial orientation (identity by default).
    SimVec3 angularVelocity = {0.0f, 0.0f, 0.0f}; ///< rad/s about each axis.
    float   linearDamping  = 0.0f;        ///< per-second linear velocity decay (>= 0, finite). 0 = none.
    float   angularDamping = 0.0f;        ///< per-second angular velocity decay (>= 0, finite). 0 = none.
    float   collisionRadius = 0.0f;       ///< collider radius (>= 0, finite). 0 = no collision.
    float   collisionHalfHeight = 0.0f;   ///< capsule half-length along body-local +Y (>= 0, finite). 0 = sphere.
    SimVec3 collisionHalfExtents = {0.0f, 0.0f, 0.0f}; ///< box half-extents (each >= 0, finite). Any > 0 makes the
                                          ///< collider an oriented box (radius/halfHeight ignored).
};

/// Distance (rod/rope) constraint between two body centres, or between one body
/// and a fixed world point. Holds their separation at `distance`.
struct DistanceConstraintDesc {
    BodyId  bodyA = kInvalidBodyId;            ///< must be a valid body
    BodyId  bodyB = kInvalidBodyId;            ///< second body, or kInvalidBodyId to pin to `worldAnchor`
    SimVec3 worldAnchor = {0.0f, 0.0f, 0.0f};  ///< the fixed point when bodyB is invalid
    float   distance = 0.0f;                   ///< target separation (>= 0, finite)
};

/// Cacheable per-body state for snapshot/restore.
struct SimBodySnapshot {
    BodyId  id;
    SimVec3 position;
    SimVec3 velocity;
    SimQuat orientation     = {};                  ///< identity by default (back-compat).
    SimVec3 angularVelocity = {0.0f, 0.0f, 0.0f};
};

/// Full simulation state snapshot for replay and rollback.
struct SimState {
    std::vector<SimBodySnapshot> bodies;
    double simulationTime = 0.0;
};

[[nodiscard]] bool operator==(const SimBodySnapshot& a, const SimBodySnapshot& b) noexcept;
[[nodiscard]] bool operator==(const SimState& a, const SimState& b) noexcept;

/// Deterministic cacheable byte format for replay and rollback snapshots.
/// Format: magic + version + simulation time + ordered body records. Each body
/// record carries id, position, velocity, orientation (quaternion), and angular
/// velocity. Current emitted version is 2.
[[nodiscard]] std::vector<std::uint8_t> serializeSimState(const SimState& state);

/// Restores a SimState from serializeSimState(). Returns false on malformed input.
/// Legacy version-1 blobs (no angular data) are accepted and decoded with an
/// identity orientation and zero angular velocity for forward compatibility.
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

    /// Returns the body's current orientation and angular velocity.
    /// Returns false if id is unknown (static bodies report their stored state).
    [[nodiscard]] bool getBodyAngularState(BodyId id,
                                           SimQuat& outOrientation,
                                           SimVec3& outAngularVelocity) const noexcept;

    // ── Force accumulation ───────────────────────────────────────────────────

    /// Accumulate a force on a body. Applied at the next step, then cleared.
    /// Returns false if id unknown or body is static.
    [[nodiscard]] bool applyForce(BodyId id, SimVec3 force) noexcept;

    /// Accumulate a torque (world space) on a body. Applied at the next step via
    /// the angular-momentum integrator, then cleared. Returns false if id unknown,
    /// body is static (mass == 0), or torque is non-finite.
    [[nodiscard]] bool applyTorque(BodyId id, SimVec3 torque) noexcept;

    /// Set gravity applied to all dynamic bodies each step.
    /// Default: {0, -9.81, 0}.
    void setGravity(SimVec3 gravity) noexcept;
    [[nodiscard]] SimVec3 gravity() const noexcept;

    // ── Ground-plane collision ─────────────────────────────────────────────────

    /// Enable a static collision half-space. After each step, dynamic bodies with
    /// collisionRadius > 0 whose sphere penetrates the plane are pushed out along
    /// the normal and bounce: the inbound normal velocity is reflected scaled by
    /// `restitution` (0 = inelastic, 1 = perfectly elastic). The plane is
    /// { p : dot(normal, p) = offset }, with `normal` pointing toward the allowed
    /// (open) side; it is normalized internally. Rejects non-finite inputs or a
    /// degenerate (zero-length) normal, leaving any previous plane unchanged.
    /// Disabled by default — with no plane set, trajectories are unchanged.
    void setGroundPlane(SimVec3 normal, float offset, float restitution) noexcept;

    /// Disable ground-plane collision.
    void clearGroundPlane() noexcept;

    [[nodiscard]] bool hasGroundPlane() const noexcept;

    // ── Body-body collision ────────────────────────────────────────────────────

    /// Enable sphere-vs-sphere collision among bodies with collisionRadius > 0.
    /// After integration, overlapping pairs are separated by a mass-weighted
    /// positional correction and bounce: the approaching relative normal velocity
    /// is reflected scaled by the shared `restitution` [0,1]. Static bodies
    /// (mass == 0) act as immovable. Pairs are resolved once per step in
    /// ascending-BodyId order so the result is deterministic. Disabled by default;
    /// rejects a non-finite restitution (leaving the prior setting unchanged).
    void setBodyCollision(float restitution) noexcept;

    /// Disable body-body collision.
    void clearBodyCollision() noexcept;

    [[nodiscard]] bool hasBodyCollision() const noexcept;

    // ── Contact solver ─────────────────────────────────────────────────────────

    /// Number of contact-resolution iterations applied per step. Each iteration
    /// re-runs the body-body and ground passes, which lets stacked/resting contacts
    /// settle instead of jittering. Clamped to >= 1; default 1. Higher values cost
    /// more but converge tighter for piles and stacks.
    void setSolverIterations(uint32_t iterations) noexcept;
    [[nodiscard]] uint32_t solverIterations() const noexcept;

    /// Coulomb friction coefficient applied at ground and body-body contacts
    /// (>= 0; 0 = frictionless, the default). After the normal impulse a tangential
    /// impulse opposes sliding, capped at friction * normal impulse. This is linear
    /// (no angular coupling yet) — it models sliding friction, not rolling. Rejects
    /// non-finite input; negative values are clamped to 0.
    void setFriction(float coefficient) noexcept;
    [[nodiscard]] float friction() const noexcept;

    // ── Constraints (joints) ───────────────────────────────────────────────────

    /// Add a distance constraint. Returns kInvalidConstraintId if bodyA is unknown,
    /// bodyB is set but unknown, or distance is non-finite/negative. Solved each
    /// step (Baumgarte-stabilised) after integration and contacts.
    [[nodiscard]] ConstraintId addDistanceConstraint(const DistanceConstraintDesc& desc);

    /// Remove a constraint. Returns false if the id is unknown.
    [[nodiscard]] bool removeConstraint(ConstraintId id) noexcept;

    [[nodiscard]] bool        hasConstraint(ConstraintId id) const noexcept;
    [[nodiscard]] std::size_t constraintCount() const noexcept;

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
        SimVec3 inertia;      ///< principal moments of inertia, body space (each > 0)
        SimQuat orientation;  ///< unit quaternion
        SimVec3 angularVelocity;
        SimVec3 torque;       ///< accumulated, cleared after each step
        float   linearDamping;  ///< per-second linear velocity decay (>= 0)
        float   angularDamping; ///< per-second angular velocity decay (>= 0)
        float   collisionRadius; ///< collider radius (>= 0); 0 = no collision
        float   collisionHalfHeight; ///< capsule half-length along body-local +Y; 0 = sphere
        SimVec3 collisionHalfExtents; ///< box half-extents; any component > 0 = oriented box
    };

    [[nodiscard]] static bool  isBoxCollider(const Body& b) noexcept;
    [[nodiscard]] static bool  hasCollider(const Body& b)   noexcept;
    [[nodiscard]] static float boundingRadius(const Body& b) noexcept;

    std::unordered_map<BodyId, Body> m_bodies;
    BodyId            m_nextId  = 1u;
    double            m_time    = 0.0;
    SimVec3           m_gravity = {0.0f, -9.81f, 0.0f};

    bool    m_groundEnabled     = false;
    SimVec3 m_groundNormal      = {0.0f, 1.0f, 0.0f};
    float   m_groundOffset      = 0.0f;
    float   m_groundRestitution = 0.0f;

    bool     m_bodyCollisionEnabled = false;
    float    m_bodyRestitution      = 0.0f;
    uint32_t m_solverIterations     = 1u;
    float    m_friction             = 0.0f;

    // Warm-start cache: accumulated (normal, tangent) impulse per box-box contact
    // feature, carried to the next frame so persistent stacks start near solution.
    std::unordered_map<std::uint64_t, std::pair<float, float>> m_boxWarmStart;

    struct DistanceConstraint {
        ConstraintId id;
        BodyId  bodyA;
        BodyId  bodyB;        // kInvalidBodyId => pinned to worldAnchor
        SimVec3 worldAnchor;
        float   distance;
    };
    std::vector<DistanceConstraint> m_constraints;
    ConstraintId                    m_nextConstraintId = 1u;

    /// Baumgarte-stabilised iterated solve of all distance constraints (dt seconds).
    void solveConstraints(float dt) noexcept;

    Body*       findBody(BodyId id)       noexcept;
    const Body* findBody(BodyId id) const noexcept;

    /// Run the configured number of contact-resolution iterations (body-body then
    /// ground). No-op when neither collision feature is enabled.
    void resolveContacts() noexcept;

    /// Sphere-vs-plane positional correction + restitution. No-op when the ground
    /// is disabled, the body has no collider, or the sphere is clear of the plane.
    void resolveGroundContact(Body& b) const noexcept;

    /// Single deterministic pass of sphere-vs-sphere resolution over all collider
    /// bodies (ascending-BodyId pair order). No-op when body collision is disabled.
    void resolveBodyCollisions() noexcept;

    /// Mass-weighted positional + impulse resolution for one overlapping pair.
    void resolveBodyPair(Body& a, Body& b) const noexcept;

    /// Resolve a single contact: separates along `normal` (unit, pointing from a
    /// toward the obstacle) and applies normal + friction impulses with angular
    /// coupling. b == nullptr models a static immovable obstacle (the ground plane).
    /// `correctPosition` == false skips the positional push (velocity impulse only),
    /// used for multi-point manifolds where the correction is applied once.
    void resolveContact(Body& a, Body* b, const SimVec3& contactPoint,
                        const SimVec3& normal, float penetration,
                        float restitution, bool correctPosition = true) const noexcept;

    /// Box-box (OBB) warm-started contact pass. Runs once per step: builds every
    /// box-box manifold (SAT + vertex incidence), applies the impulses cached from
    /// the previous frame (warm start), then iterates a sequential-impulse solve
    /// with accumulated, clamped impulses. Warm starting + accumulation is what
    /// keeps box stacks from drifting, which a naive per-iteration solve cannot.
    void solveBoxBoxStep() noexcept;
};

} // namespace nexus
