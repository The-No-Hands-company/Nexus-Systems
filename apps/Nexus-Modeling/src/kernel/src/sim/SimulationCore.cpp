#include "nexus/sim/SimulationCore.h"

#include <cassert>
#include <bit>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace nexus {

namespace {

constexpr std::uint32_t kSimStateMagic = 0x314d5353u; // 'SSM1' little-endian marker
constexpr std::uint32_t kSimStateVersion = 2u;        // v2 adds orientation + angular velocity
constexpr std::uint32_t kSimStateVersionLegacy = 1u;  // v1: position + velocity only

template <typename T>
void appendBytes(std::vector<std::uint8_t>& out, T value)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const std::uint8_t* begin = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), begin, begin + sizeof(T));
}

template <typename T>
bool readBytes(const std::vector<std::uint8_t>& bytes, std::size_t& offset, T& out)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (offset + sizeof(T) > bytes.size()) {
        return false;
    }
    std::memcpy(&out, bytes.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

std::uint32_t toBits(float value)
{
    return std::bit_cast<std::uint32_t>(value);
}

std::uint64_t toBits(double value)
{
    return std::bit_cast<std::uint64_t>(value);
}

float fromBitsFloat(std::uint32_t bits)
{
    return std::bit_cast<float>(bits);
}

double fromBitsDouble(std::uint64_t bits)
{
    return std::bit_cast<double>(bits);
}

bool isFiniteFloat(float v) noexcept;
bool isFiniteVec3(const SimVec3& v) noexcept;
bool isFiniteQuat(const SimQuat& q) noexcept;
bool isFiniteInertia(const SimVec3& i) noexcept;

} // namespace

bool operator==(const SimBodySnapshot& a, const SimBodySnapshot& b) noexcept
{
    return a.id == b.id && a.position == b.position && a.velocity == b.velocity
        && a.orientation == b.orientation && a.angularVelocity == b.angularVelocity;
}

bool operator==(const SimState& a, const SimState& b) noexcept
{
    return a.simulationTime == b.simulationTime && a.bodies == b.bodies;
}

std::vector<std::uint8_t> serializeSimState(const SimState& state)
{
    SimState sorted = state;
    std::sort(sorted.bodies.begin(), sorted.bodies.end(), [](const SimBodySnapshot& lhs,
                                                             const SimBodySnapshot& rhs) {
        return lhs.id < rhs.id;
    });

    std::vector<std::uint8_t> bytes;
    bytes.reserve(16u + sorted.bodies.size() * 56u);

    appendBytes(bytes, kSimStateMagic);
    appendBytes(bytes, kSimStateVersion);
    appendBytes(bytes, toBits(sorted.simulationTime));

    const std::uint32_t bodyCount = static_cast<std::uint32_t>(sorted.bodies.size());
    appendBytes(bytes, bodyCount);

    for (const SimBodySnapshot& body : sorted.bodies) {
        appendBytes(bytes, static_cast<std::uint32_t>(body.id));
        appendBytes(bytes, toBits(body.position.x));
        appendBytes(bytes, toBits(body.position.y));
        appendBytes(bytes, toBits(body.position.z));
        appendBytes(bytes, toBits(body.velocity.x));
        appendBytes(bytes, toBits(body.velocity.y));
        appendBytes(bytes, toBits(body.velocity.z));
        appendBytes(bytes, toBits(body.orientation.x));
        appendBytes(bytes, toBits(body.orientation.y));
        appendBytes(bytes, toBits(body.orientation.z));
        appendBytes(bytes, toBits(body.orientation.w));
        appendBytes(bytes, toBits(body.angularVelocity.x));
        appendBytes(bytes, toBits(body.angularVelocity.y));
        appendBytes(bytes, toBits(body.angularVelocity.z));
    }

    return bytes;
}

bool deserializeSimState(const std::vector<std::uint8_t>& bytes, SimState& outState)
{
    std::size_t offset = 0u;
    std::uint32_t magic = 0u;
    std::uint32_t version = 0u;
    std::uint64_t timeBits = 0u;
    std::uint32_t bodyCount = 0u;

    if (!readBytes(bytes, offset, magic) || magic != kSimStateMagic) return false;
    if (!readBytes(bytes, offset, version)) return false;
    if (version != kSimStateVersion && version != kSimStateVersionLegacy) return false;
    if (!readBytes(bytes, offset, timeBits)) return false;
    if (!readBytes(bytes, offset, bodyCount)) return false;

    const bool hasAngular = (version >= kSimStateVersion);

    SimState state;
    state.simulationTime = fromBitsDouble(timeBits);
    state.bodies.reserve(bodyCount);

    for (std::uint32_t i = 0; i < bodyCount; ++i) {
        std::uint32_t id = 0u;
        std::uint32_t px = 0u, py = 0u, pz = 0u;
        std::uint32_t vx = 0u, vy = 0u, vz = 0u;

        if (!readBytes(bytes, offset, id)) return false;
        if (!readBytes(bytes, offset, px)) return false;
        if (!readBytes(bytes, offset, py)) return false;
        if (!readBytes(bytes, offset, pz)) return false;
        if (!readBytes(bytes, offset, vx)) return false;
        if (!readBytes(bytes, offset, vy)) return false;
        if (!readBytes(bytes, offset, vz)) return false;

        SimQuat orientation{};              // identity for legacy blobs
        SimVec3 angularVelocity{};          // zero for legacy blobs
        if (hasAngular) {
            std::uint32_t ox = 0u, oy = 0u, oz = 0u, ow = 0u;
            std::uint32_t ax = 0u, ay = 0u, az = 0u;
            if (!readBytes(bytes, offset, ox)) return false;
            if (!readBytes(bytes, offset, oy)) return false;
            if (!readBytes(bytes, offset, oz)) return false;
            if (!readBytes(bytes, offset, ow)) return false;
            if (!readBytes(bytes, offset, ax)) return false;
            if (!readBytes(bytes, offset, ay)) return false;
            if (!readBytes(bytes, offset, az)) return false;
            orientation = {fromBitsFloat(ox), fromBitsFloat(oy),
                           fromBitsFloat(oz), fromBitsFloat(ow)};
            angularVelocity = {fromBitsFloat(ax), fromBitsFloat(ay), fromBitsFloat(az)};
        }

        state.bodies.push_back(SimBodySnapshot{
            static_cast<BodyId>(id),
            {fromBitsFloat(px), fromBitsFloat(py), fromBitsFloat(pz)},
            {fromBitsFloat(vx), fromBitsFloat(vy), fromBitsFloat(vz)},
            orientation,
            angularVelocity
        });
    }

    if (offset != bytes.size()) {
        return false;
    }

    outState = std::move(state);
    return true;
}

RigidBodySolver::RigidBodySolver()  = default;
RigidBodySolver::~RigidBodySolver() = default;

// ── Body management ───────────────────────────────────────────────────────────

BodyId RigidBodySolver::addBody(const SimBodyDesc& desc) {
    if (!isFiniteFloat(desc.mass) || desc.mass < 0.0f ||
        !isFiniteVec3(desc.position) || !isFiniteVec3(desc.velocity) ||
        !isFiniteInertia(desc.inertia) ||
        !isFiniteQuat(desc.orientation) || !isFiniteVec3(desc.angularVelocity) ||
        !isFiniteFloat(desc.linearDamping)  || desc.linearDamping  < 0.0f ||
        !isFiniteFloat(desc.angularDamping) || desc.angularDamping < 0.0f ||
        !isFiniteFloat(desc.collisionRadius) || desc.collisionRadius < 0.0f) {
        return kInvalidBodyId;
    }

    BodyId id = m_nextId++;
    m_bodies.emplace(id, Body{
        id,
        desc.mass,
        desc.position,
        desc.velocity,
        /*force=*/{0.0f, 0.0f, 0.0f},
        desc.inertia,
        desc.orientation,
        desc.angularVelocity,
        /*torque=*/{0.0f, 0.0f, 0.0f},
        desc.linearDamping,
        desc.angularDamping,
        desc.collisionRadius
    });
    return id;
}

bool RigidBodySolver::removeBody(BodyId id) noexcept {
    return m_bodies.erase(id) > 0;
}

bool RigidBodySolver::hasBody(BodyId id) const noexcept {
    return findBody(id) != nullptr;
}

std::size_t RigidBodySolver::bodyCount() const noexcept {
    return m_bodies.size();
}

bool RigidBodySolver::getBodyState(BodyId id,
                                   SimVec3& outPosition,
                                   SimVec3& outVelocity) const noexcept {
    const Body* b = findBody(id);
    if (!b) return false;
    outPosition = b->position;
    outVelocity = b->velocity;
    return true;
}

bool RigidBodySolver::getBodyAngularState(BodyId id,
                                          SimQuat& outOrientation,
                                          SimVec3& outAngularVelocity) const noexcept {
    const Body* b = findBody(id);
    if (!b) return false;
    outOrientation     = b->orientation;
    outAngularVelocity = b->angularVelocity;
    return true;
}

// ── Force accumulation ────────────────────────────────────────────────────────

bool RigidBodySolver::applyForce(BodyId id, SimVec3 force) noexcept {
    Body* b = findBody(id);
    if (!b || b->mass == 0.0f || !isFiniteVec3(force)) return false;
    b->force += force;
    return true;
}

bool RigidBodySolver::applyTorque(BodyId id, SimVec3 torque) noexcept {
    Body* b = findBody(id);
    if (!b || b->mass == 0.0f || !isFiniteVec3(torque)) return false;
    b->torque += torque;
    return true;
}

void RigidBodySolver::setGravity(SimVec3 gravity) noexcept {
    m_gravity = gravity;
}

SimVec3 RigidBodySolver::gravity() const noexcept {
    return m_gravity;
}

// ── Ground-plane collision ──────────────────────────────────────────────────────

void RigidBodySolver::setGroundPlane(SimVec3 normal, float offset, float restitution) noexcept {
    if (!isFiniteVec3(normal) || !isFiniteFloat(offset) || !isFiniteFloat(restitution)) {
        return; // reject non-finite; leave any previous plane unchanged
    }
    const float lenSq = normal.x*normal.x + normal.y*normal.y + normal.z*normal.z;
    if (!isFiniteFloat(lenSq) || lenSq <= 0.0f) {
        return; // degenerate (zero-length) normal
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    m_groundNormal = { normal.x * invLen, normal.y * invLen, normal.z * invLen };
    m_groundOffset = offset;
    m_groundRestitution = restitution < 0.0f ? 0.0f : (restitution > 1.0f ? 1.0f : restitution);
    m_groundEnabled = true;
}

void RigidBodySolver::clearGroundPlane() noexcept {
    m_groundEnabled = false;
}

bool RigidBodySolver::hasGroundPlane() const noexcept {
    return m_groundEnabled;
}

void RigidBodySolver::resolveGroundContact(Body& b) const noexcept {
    if (!m_groundEnabled || b.collisionRadius <= 0.0f) {
        return;
    }
    const SimVec3& n = m_groundNormal;
    // Signed distance of the sphere center from the plane (positive on the allowed side).
    const float dist = (n.x*b.position.x + n.y*b.position.y + n.z*b.position.z) - m_groundOffset;
    const float penetration = b.collisionRadius - dist;
    if (penetration <= 0.0f) {
        return; // sphere clear of the plane
    }
    // Positional correction: lift the center along the normal until the sphere is
    // tangent to the plane.
    b.position += n * penetration;
    // Reflect only the inbound normal velocity component with restitution; never add
    // energy when the body is already separating (vn >= 0).
    const float vn = n.x*b.velocity.x + n.y*b.velocity.y + n.z*b.velocity.z;
    if (vn < 0.0f) {
        b.velocity += n * (-(1.0f + m_groundRestitution) * vn);
    }
}

// ── Simulation step ───────────────────────────────────────────────────────────

namespace {

inline bool isPositiveFinite(double v) noexcept
{
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(v);
    constexpr std::uint64_t kSignMask = 0x8000000000000000ULL;
    constexpr std::uint64_t kExpMask = 0x7FF0000000000000ULL;
    constexpr std::uint64_t kMagnitudeMask = 0x7FFFFFFFFFFFFFFFULL;

    const bool finite = (bits & kExpMask) != kExpMask;
    const bool positive = (bits & kSignMask) == 0ULL;
    const bool nonZero = (bits & kMagnitudeMask) != 0ULL;
    return finite && positive && nonZero;
}

inline bool isFiniteFloat(float v) noexcept
{
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(v);
    constexpr std::uint32_t kExpMask = 0x7F800000u;
    return (bits & kExpMask) != kExpMask;
}

inline bool isFiniteVec3(const SimVec3& v) noexcept
{
    return isFiniteFloat(v.x) && isFiniteFloat(v.y) && isFiniteFloat(v.z);
}

inline bool isFiniteQuat(const SimQuat& q) noexcept
{
    return isFiniteFloat(q.x) && isFiniteFloat(q.y) && isFiniteFloat(q.z) && isFiniteFloat(q.w);
}

/// Per-step velocity decay multiplier for `damping` (>= 0) over `dt`.
/// Uses the standard explicit form (1 - damping·dt), clamped to [0,1] so a
/// large damping·dt cannot drive velocity negative (reverse it).
inline float dampingFactor(float damping, float dt) noexcept
{
    const float factor = 1.0f - damping * dt;
    if (factor < 0.0f) return 0.0f;
    if (factor > 1.0f) return 1.0f;
    return factor;
}

inline SimVec3 cross(const SimVec3& a, const SimVec3& b) noexcept
{
    return { a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x };
}

inline SimVec3 compMul(const SimVec3& a, const SimVec3& b) noexcept
{
    return { a.x*b.x, a.y*b.y, a.z*b.z };
}

inline SimVec3 compDiv(const SimVec3& a, const SimVec3& b) noexcept
{
    return { a.x/b.x, a.y/b.y, a.z/b.z }; // divisors validated > 0 at body entry
}

inline bool isFiniteInertia(const SimVec3& i) noexcept
{
    return isFiniteVec3(i) && i.x > 0.0f && i.y > 0.0f && i.z > 0.0f;
}

/// Rotate vector v by unit quaternion q: v' = v + 2w(u×v) + 2(u×(u×v)), u = q.xyz.
inline SimVec3 rotateByQuat(const SimQuat& q, const SimVec3& v) noexcept
{
    const SimVec3 u{q.x, q.y, q.z};
    const SimVec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

/// Rotate vector v by the conjugate of q (the inverse for a unit quaternion).
inline SimVec3 invRotateByQuat(const SimQuat& q, const SimVec3& v) noexcept
{
    const SimVec3 u{-q.x, -q.y, -q.z};
    const SimVec3 t = cross(u, v) * 2.0f;
    return v + t * q.w + cross(u, t);
}

/// World angular momentum L = I_world * w, with I_world = R * diag(inertia) * R^T.
inline SimVec3 angularMomentum(const SimQuat& orientation,
                               const SimVec3& angularVelocity,
                               const SimVec3& inertia) noexcept
{
    return rotateByQuat(orientation,
                        compMul(invRotateByQuat(orientation, angularVelocity), inertia));
}

/// World angular velocity w = I_world^-1 * L -- the inverse of angularMomentum().
inline SimVec3 angularVelocityFromMomentum(const SimQuat& orientation,
                                           const SimVec3& momentum,
                                           const SimVec3& inertia) noexcept
{
    return rotateByQuat(orientation,
                        compDiv(invRotateByQuat(orientation, momentum), inertia));
}

/// Hamilton product a * b.
inline SimQuat quatMul(const SimQuat& a, const SimQuat& b) noexcept
{
    return {
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z
    };
}

/// Renormalize to a unit quaternion; falls back to identity if degenerate.
inline SimQuat normalizeQuat(const SimQuat& q) noexcept
{
    const float lenSq = q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w;
    if (!isFiniteFloat(lenSq) || lenSq <= 0.0f) {
        return SimQuat{}; // identity
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    return {q.x*invLen, q.y*invLen, q.z*invLen, q.w*invLen};
}

/// First-order quaternion integration from a body-frame-independent angular
/// velocity: dq/dt = 0.5 * (omega as pure quaternion) * q, then renormalize.
inline SimQuat integrateOrientation(const SimQuat& q, const SimVec3& omega, float dt) noexcept
{
    const SimQuat omegaQuat{omega.x, omega.y, omega.z, 0.0f};
    const SimQuat dq = quatMul(omegaQuat, q);
    const float half = 0.5f * dt;
    const SimQuat integrated{
        q.x + half * dq.x,
        q.y + half * dq.y,
        q.z + half * dq.z,
        q.w + half * dq.w
    };
    return normalizeQuat(integrated);
}

} // namespace

StepReport RigidBodySolver::step(double dt) {
    StepReport report;
    report.simulationTime   = m_time;
    report.bodiesIntegrated = 0;

    if (!isPositiveFinite(dt)) {
        report.ok = false;
        return report;
    }

    if (!isFiniteVec3(m_gravity)) {
        report.ok = false;
        return report;
    }

    for (const auto& [id, b] : m_bodies) {
        (void)id;
        if (!isFiniteFloat(b.mass) || b.mass < 0.0f ||
            !isFiniteVec3(b.position) ||
            !isFiniteVec3(b.velocity) ||
            !isFiniteVec3(b.force) ||
            !isFiniteInertia(b.inertia) ||
            !isFiniteQuat(b.orientation) ||
            !isFiniteVec3(b.angularVelocity) ||
            !isFiniteVec3(b.torque)) {
            report.ok = false;
            return report;
        }
    }

    size_t dynamicBodies = 0u;
    for (const auto& [id, b] : m_bodies) {
        (void)id;
        if (b.mass != 0.0f) {
            ++dynamicBodies;
        }
    }

    if (dynamicBodies == 0u) {
        m_time += dt;
        report.simulationTime = m_time;
        return report;
    }

    // Physics integration runs at float precision (positions/velocities are float).
    // Time accumulation uses full double precision.
    const float fdt = static_cast<float>(dt);

    for (auto& [id, b] : m_bodies) {
        if (b.mass == 0.0f) continue; // static — skip

        // Gravity acceleration (uniform, applied every step).
        SimVec3 gravity_force = m_gravity * b.mass;

        // Total force = gravity + accumulated external forces.
        SimVec3 total = gravity_force + b.force;

        // Explicit Euler integration with per-step damping applied to velocity.
        SimVec3 acceleration = total * (1.0f / b.mass);
        b.velocity = b.velocity + acceleration * fdt;
        b.velocity = b.velocity * dampingFactor(b.linearDamping, fdt);
        b.position = b.position + b.velocity * fdt;

        // Angular: momentum-based integration (stable; captures precession/tumbling
        // for anisotropic inertia, reduces to scalar behavior when isotropic).
        // Derive world momentum from the current state, advance the orientation,
        // then recover omega from the (conserved) momentum through the rotated
        // inertia tensor -- the orientation change is what produces precession.
        SimVec3 worldMomentum = angularMomentum(b.orientation, b.angularVelocity, b.inertia);
        worldMomentum += b.torque * fdt;
        b.orientation = integrateOrientation(b.orientation, b.angularVelocity, fdt);
        b.angularVelocity = angularVelocityFromMomentum(b.orientation, worldMomentum, b.inertia);
        b.angularVelocity = b.angularVelocity * dampingFactor(b.angularDamping, fdt);

        // Resolve collision against the optional static ground plane (no-op when
        // disabled or the body has no collider).
        resolveGroundContact(b);

        // Clear per-step accumulated force and torque.
        b.force  = {0.0f, 0.0f, 0.0f};
        b.torque = {0.0f, 0.0f, 0.0f};
    }

    m_time += dt;
    report.simulationTime = m_time;
    report.bodiesIntegrated = dynamicBodies;
    return report;
}

StepReport RigidBodySolver::stepFixed(double dt, double fixedSubstep)
{
    StepReport report;
    report.simulationTime = m_time;
    report.bodiesIntegrated = 0;

    if (!isPositiveFinite(dt) || !isPositiveFinite(fixedSubstep)) {
        report.ok = false;
        return report;
    }

    if (!isFiniteVec3(m_gravity)) {
        report.ok = false;
        return report;
    }

    for (const auto& [id, b] : m_bodies) {
        (void)id;
        if (!isFiniteFloat(b.mass) || b.mass < 0.0f ||
            !isFiniteVec3(b.position) ||
            !isFiniteVec3(b.velocity) ||
            !isFiniteVec3(b.force) ||
            !isFiniteInertia(b.inertia) ||
            !isFiniteQuat(b.orientation) ||
            !isFiniteVec3(b.angularVelocity) ||
            !isFiniteVec3(b.torque)) {
            report.ok = false;
            return report;
        }
    }

    size_t dynamicBodies = 0u;
    for (const auto& [id, b] : m_bodies) {
        (void)id;
        if (b.mass != 0.0f) {
            ++dynamicBodies;
        }
    }

    if (dynamicBodies == 0u) {
        m_time += dt;
        report.simulationTime = m_time;
        return report;
    }

    auto integrateSubstep = [this](double substepDt) {
        const float fdt = static_cast<float>(substepDt);
        for (auto& [id, b] : m_bodies) {
            (void)id;
            if (b.mass == 0.0f) {
                continue;
            }

            const SimVec3 gravityForce = m_gravity * b.mass;
            const SimVec3 totalForce = gravityForce + b.force;
            const SimVec3 acceleration = totalForce * (1.0f / b.mass);
            b.velocity = b.velocity + acceleration * fdt;
            b.velocity = b.velocity * dampingFactor(b.linearDamping, fdt);
            b.position = b.position + b.velocity * fdt;

            // Angular integration mirrors the linear path; torque persists across
            // substeps and is consumed once after the full call (see below).
            SimVec3 worldMomentum = angularMomentum(b.orientation, b.angularVelocity, b.inertia);
            worldMomentum += b.torque * fdt;
            b.orientation = integrateOrientation(b.orientation, b.angularVelocity, fdt);
            b.angularVelocity = angularVelocityFromMomentum(b.orientation, worldMomentum, b.inertia);
            b.angularVelocity = b.angularVelocity * dampingFactor(b.angularDamping, fdt);
        }
        m_time += substepDt;
    };

    double remaining = dt;
    while (remaining > fixedSubstep) {
        integrateSubstep(fixedSubstep);
        remaining -= fixedSubstep;
    }
    if (remaining > 0.0) {
        integrateSubstep(remaining);
    }

    for (auto& [id, b] : m_bodies) {
        (void)id;
        if (b.mass == 0.0f) {
            continue;
        }
        b.force  = {0.0f, 0.0f, 0.0f};
        b.torque = {0.0f, 0.0f, 0.0f};
    }

    report.simulationTime = m_time;
    report.bodiesIntegrated = dynamicBodies;
    return report;
}

// ── Replay and rollback ───────────────────────────────────────────────────────

SimState RigidBodySolver::captureState() const {
    SimState state;
    state.simulationTime = m_time;
    state.bodies.reserve(m_bodies.size());
    for (const auto& [id, b] : m_bodies) {
        state.bodies.push_back(SimBodySnapshot{b.id, b.position, b.velocity,
                                               b.orientation, b.angularVelocity});
    }
    std::sort(state.bodies.begin(), state.bodies.end(), [](const SimBodySnapshot& a,
                                                            const SimBodySnapshot& b) {
        return a.id < b.id;
    });
    assert(std::adjacent_find(state.bodies.begin(), state.bodies.end(),
                              [](const SimBodySnapshot& a, const SimBodySnapshot& b) {
                                  return a.id == b.id;
                              }) == state.bodies.end());
    return state;
}

bool RigidBodySolver::restoreState(const SimState& state) {
    if (!isPositiveFinite(state.simulationTime) && state.simulationTime != 0.0) {
        return false;
    }

    if (state.bodies.empty() && !m_bodies.empty()) {
        // Structurally invalid restore: snapshot has no bodies but solver does.
        return false;
    }

    std::vector<SimBodySnapshot> orderedBodies = state.bodies;
    std::sort(orderedBodies.begin(), orderedBodies.end(), [](const SimBodySnapshot& a,
                                                             const SimBodySnapshot& b) {
        return a.id < b.id;
    });
    if (std::adjacent_find(orderedBodies.begin(), orderedBodies.end(),
                           [](const SimBodySnapshot& a, const SimBodySnapshot& b) {
                               return a.id == b.id;
                           }) != orderedBodies.end()) {
        return false;
    }

    for (const SimBodySnapshot& snap : state.bodies) {
        if (!isFiniteVec3(snap.position) || !isFiniteVec3(snap.velocity) ||
            !isFiniteQuat(snap.orientation) || !isFiniteVec3(snap.angularVelocity)) {
            return false;
        }
    }

    m_time = state.simulationTime;
    for (const SimBodySnapshot& snap : state.bodies) {
        Body* b = findBody(snap.id);
        if (!b) continue; // body absent in solver — ignore
        b->position        = snap.position;
        b->velocity        = snap.velocity;
        b->orientation     = snap.orientation;
        b->angularVelocity = snap.angularVelocity;
        b->force           = {0.0f, 0.0f, 0.0f}; // clear accumulated force on restore
        b->torque          = {0.0f, 0.0f, 0.0f}; // clear accumulated torque on restore
    }
    return true;
}

// ── Time ──────────────────────────────────────────────────────────────────────

double RigidBodySolver::simulationTime() const noexcept {
    return m_time;
}

// ── Lifetime ──────────────────────────────────────────────────────────────────

void RigidBodySolver::clear() noexcept {
    m_bodies.clear();
    m_time = 0.0;
    // m_nextId not reset; m_gravity preserved.
}

// ── Private helpers ───────────────────────────────────────────────────────────

RigidBodySolver::Body* RigidBodySolver::findBody(BodyId id) noexcept {
    auto it = m_bodies.find(id);
    return it != m_bodies.end() ? &it->second : nullptr;
}

const RigidBodySolver::Body* RigidBodySolver::findBody(BodyId id) const noexcept {
    auto it = m_bodies.find(id);
    return it != m_bodies.end() ? &it->second : nullptr;
}

} // namespace nexus
