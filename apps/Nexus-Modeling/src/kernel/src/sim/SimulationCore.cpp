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

// Used by the contact-resolution member functions below (defined further down).
SimVec3 cross(const SimVec3& a, const SimVec3& b) noexcept;
SimVec3 angularVelocityFromMomentum(const SimQuat& orientation,
                                    const SimVec3& momentum,
                                    const SimVec3& inertia) noexcept;
SimVec3 rotateByQuat(const SimQuat& q, const SimVec3& v) noexcept;
float   dotv(const SimVec3& a, const SimVec3& b) noexcept;
// Closest points between segments [p1,q1] and [p2,q2], written to c1/c2.
void closestPtSegmentSegment(const SimVec3& p1, const SimVec3& q1,
                             const SimVec3& p2, const SimVec3& q2,
                             SimVec3& c1, SimVec3& c2) noexcept;
SimVec3 closestPtPointSegment(const SimVec3& p, const SimVec3& a, const SimVec3& b) noexcept;
// Closest point on an oriented box (center/orientation/half-extents) to point p.
SimVec3 closestPtPointObb(const SimVec3& center, const SimQuat& orientation,
                          const SimVec3& halfExtents, const SimVec3& p) noexcept;

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
        !isFiniteFloat(desc.collisionRadius) || desc.collisionRadius < 0.0f ||
        !isFiniteFloat(desc.collisionHalfHeight) || desc.collisionHalfHeight < 0.0f ||
        !isFiniteVec3(desc.collisionHalfExtents) ||
        desc.collisionHalfExtents.x < 0.0f || desc.collisionHalfExtents.y < 0.0f ||
        desc.collisionHalfExtents.z < 0.0f) {
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
        desc.collisionRadius,
        desc.collisionHalfHeight,
        desc.collisionHalfExtents
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

bool RigidBodySolver::isBoxCollider(const Body& b) noexcept {
    return b.collisionHalfExtents.x > 0.0f
        || b.collisionHalfExtents.y > 0.0f
        || b.collisionHalfExtents.z > 0.0f;
}

bool RigidBodySolver::hasCollider(const Body& b) noexcept {
    return b.collisionRadius > 0.0f || isBoxCollider(b);
}

float RigidBodySolver::boundingRadius(const Body& b) noexcept {
    if (isBoxCollider(b)) {
        const SimVec3& e = b.collisionHalfExtents;
        return std::sqrt(e.x*e.x + e.y*e.y + e.z*e.z); // circumscribed sphere
    }
    return b.collisionRadius + b.collisionHalfHeight; // capsule reach (sphere when h=0)
}

void RigidBodySolver::resolveContact(Body& a, Body* b, const SimVec3& contactPoint,
                                     const SimVec3& normal, float penetration,
                                     float restitution, bool correctPosition) const noexcept
{
    const float invMassA = (a.mass > 0.0f) ? (1.0f / a.mass) : 0.0f;
    const float invMassB = (b && b->mass > 0.0f) ? (1.0f / b->mass) : 0.0f;
    const float invSum = invMassA + invMassB;
    if (invSum <= 0.0f) {
        return; // both immovable
    }

    // Positional correction along the normal (which points from a toward the
    // obstacle), split by inverse mass: a moves along -normal, b along +normal.
    // Skipped for manifold points whose correction is applied once by the caller.
    if (correctPosition && penetration > 0.0f) {
        const SimVec3 corr = normal * (penetration / invSum);
        a.position += corr * (-invMassA);
        if (b) b->position += corr * invMassB;
    }

    const SimVec3 rA = contactPoint - a.position;
    const SimVec3 rB = b ? (contactPoint - b->position) : SimVec3{};

    auto contactVel = [](const Body& body, const SimVec3& r) {
        return body.velocity + cross(body.angularVelocity, r);
    };
    SimVec3 relVel = (b ? contactVel(*b, rB) : SimVec3{}) - contactVel(a, rA);
    const float vn = dotv(relVel, normal);
    if (vn >= 0.0f) {
        return; // separating — no impulse
    }

    // Effective inverse mass resisting an impulse along `dir`, including both
    // bodies' angular contributions; also returns each body's I^-1 (r x dir).
    auto effMass = [&](const SimVec3& dir, SimVec3& iitA, SimVec3& iitB) {
        iitA = (invMassA > 0.0f) ? angularVelocityFromMomentum(a.orientation, cross(rA, dir), a.inertia) : SimVec3{};
        iitB = (b && invMassB > 0.0f) ? angularVelocityFromMomentum(b->orientation, cross(rB, dir), b->inertia) : SimVec3{};
        return invMassA + invMassB + dotv(dir, cross(iitA, rA)) + dotv(dir, cross(iitB, rB));
    };

    // ── Normal impulse (restitution) ──
    SimVec3 iitAn, iitBn;
    const float kn = effMass(normal, iitAn, iitBn);
    if (kn <= 1e-12f) {
        return;
    }
    const float jn = -(1.0f + restitution) * vn / kn; // > 0
    const SimVec3 Jn = normal * jn;                    // impulse on b; -Jn on a
    a.velocity += Jn * (-invMassA);
    a.angularVelocity += iitAn * (-jn);
    if (b) {
        b->velocity += Jn * invMassB;
        b->angularVelocity += iitBn * jn;
    }

    // ── Coulomb friction (tangential impulse with angular coupling) ──
    if (m_friction > 0.0f) {
        const SimVec3 rel2 = (b ? contactVel(*b, rB) : SimVec3{}) - contactVel(a, rA);
        const SimVec3 vt = rel2 - normal * dotv(rel2, normal);
        const float vtLen = std::sqrt(dotv(vt, vt));
        if (vtLen > 1e-6f) {
            const SimVec3 tdir = vt * (1.0f / vtLen);
            SimVec3 iitAt, iitBt;
            const float kt = effMass(tdir, iitAt, iitBt);
            if (kt > 1e-12f) {
                float jt = vtLen / kt;
                const float maxJt = m_friction * jn; // Coulomb cone
                if (jt > maxJt) jt = maxJt;
                const SimVec3 Jf = tdir * (-jt);     // friction impulse on b; -Jf on a
                a.velocity += Jf * (-invMassA);
                a.angularVelocity += iitAt * jt;
                if (b) {
                    b->velocity += Jf * invMassB;
                    b->angularVelocity += iitBt * (-jt);
                }
            }
        }
    }
}

void RigidBodySolver::resolveGroundContact(Body& b) const noexcept {
    if (!m_groundEnabled || !hasCollider(b)) {
        return;
    }
    const SimVec3& pn = m_groundNormal;          // plane normal (toward the allowed side)
    const SimVec3 toObstacle = pn * (-1.0f);     // contact normal: body -> plane

    if (isBoxCollider(b)) {
        // Box vs plane: each of the 8 corners is a potential contact. Resting on a
        // face gives 4 contacts, which keeps the box stable. Iterate a fixed corner
        // order for determinism; correction from earlier corners shifts the body.
        const SimVec3& e = b.collisionHalfExtents;
        for (int s = 0; s < 8; ++s) {
            const SimVec3 ax = rotateByQuat(b.orientation, {1.0f, 0.0f, 0.0f});
            const SimVec3 ay = rotateByQuat(b.orientation, {0.0f, 1.0f, 0.0f});
            const SimVec3 az = rotateByQuat(b.orientation, {0.0f, 0.0f, 1.0f});
            const float sx = (s & 1) ? 1.0f : -1.0f;
            const float sy = (s & 2) ? 1.0f : -1.0f;
            const float sz = (s & 4) ? 1.0f : -1.0f;
            const SimVec3 corner = b.position + ax * (sx*e.x) + ay * (sy*e.y) + az * (sz*e.z);
            const float signedDist = dotv(pn, corner) - m_groundOffset;
            if (signedDist >= 0.0f) {
                continue; // corner above the plane
            }
            resolveContact(b, nullptr, corner, toObstacle, -signedDist, m_groundRestitution);
        }
        return;
    }

    // Round collider — a sphere is one point; a capsule contacts at each of its two
    // endpoints, which keeps a capsule lying flat on the floor stable.
    const int count = (b.collisionHalfHeight > 0.0f) ? 2 : 1;
    for (int i = 0; i < count; ++i) {
        // Recompute per endpoint: the previous endpoint's correction moves the body.
        const SimVec3 axis = rotateByQuat(b.orientation, {0.0f, 1.0f, 0.0f}) * b.collisionHalfHeight;
        const SimVec3 c = (i == 0) ? (b.position + axis) : (b.position - axis);
        const float dist = dotv(pn, c) - m_groundOffset;
        const float pen = b.collisionRadius - dist;
        if (pen <= 0.0f) {
            continue; // this end is clear of the plane
        }
        const SimVec3 contactPoint = c - pn * b.collisionRadius;
        resolveContact(b, nullptr, contactPoint, toObstacle, pen, m_groundRestitution);
    }
}

// ── Body-body collision ─────────────────────────────────────────────────────────

void RigidBodySolver::setBodyCollision(float restitution) noexcept {
    if (!isFiniteFloat(restitution)) {
        return; // reject non-finite; leave prior setting unchanged
    }
    m_bodyRestitution = restitution < 0.0f ? 0.0f : (restitution > 1.0f ? 1.0f : restitution);
    m_bodyCollisionEnabled = true;
}

void RigidBodySolver::clearBodyCollision() noexcept {
    m_bodyCollisionEnabled = false;
}

bool RigidBodySolver::hasBodyCollision() const noexcept {
    return m_bodyCollisionEnabled;
}

void RigidBodySolver::setSolverIterations(uint32_t iterations) noexcept {
    m_solverIterations = iterations < 1u ? 1u : iterations;
}

uint32_t RigidBodySolver::solverIterations() const noexcept {
    return m_solverIterations;
}

void RigidBodySolver::setFriction(float coefficient) noexcept {
    if (!isFiniteFloat(coefficient)) {
        return; // reject non-finite; leave prior value unchanged
    }
    m_friction = coefficient < 0.0f ? 0.0f : coefficient;
}

float RigidBodySolver::friction() const noexcept {
    return m_friction;
}

// ── Constraints (joints) ────────────────────────────────────────────────────────

ConstraintId RigidBodySolver::addDistanceConstraint(const DistanceConstraintDesc& desc)
{
    if (!findBody(desc.bodyA)) return kInvalidConstraintId;
    if (desc.bodyB != kInvalidBodyId && !findBody(desc.bodyB)) return kInvalidConstraintId;
    if (!isFiniteFloat(desc.distance) || desc.distance < 0.0f) return kInvalidConstraintId;
    if (!isFiniteVec3(desc.worldAnchor)) return kInvalidConstraintId;

    const ConstraintId id = m_nextConstraintId++;
    m_constraints.push_back({ id, desc.bodyA, desc.bodyB, desc.worldAnchor, desc.distance });
    return id;
}

bool RigidBodySolver::removeConstraint(ConstraintId id) noexcept
{
    for (auto it = m_constraints.begin(); it != m_constraints.end(); ++it) {
        if (it->id == id) { m_constraints.erase(it); return true; }
    }
    return false;
}

bool RigidBodySolver::hasConstraint(ConstraintId id) const noexcept
{
    for (const auto& c : m_constraints) {
        if (c.id == id) return true;
    }
    return false;
}

std::size_t RigidBodySolver::constraintCount() const noexcept
{
    return m_constraints.size();
}

void RigidBodySolver::solveConstraints(float dt) noexcept
{
    if (m_constraints.empty()) return;
    const float invDt = (dt > 1e-8f) ? (1.0f / dt) : 0.0f;
    constexpr float kBaumgarte = 0.2f; // fraction of the position error fed back per step

    for (uint32_t iter = 0; iter < m_solverIterations; ++iter) {
        for (const auto& c : m_constraints) {
            Body* A = findBody(c.bodyA);
            if (!A) continue; // body removed since the constraint was added
            const float imA = (A->mass > 0.0f) ? (1.0f / A->mass) : 0.0f;

            Body*   B = nullptr;
            SimVec3 posB = c.worldAnchor;
            SimVec3 velB{0.0f, 0.0f, 0.0f};
            float   imB = 0.0f;
            if (c.bodyB != kInvalidBodyId) {
                B = findBody(c.bodyB);
                if (!B) continue;
                posB = B->position;
                velB = B->velocity;
                imB  = (B->mass > 0.0f) ? (1.0f / B->mass) : 0.0f;
            }

            const float invSum = imA + imB;
            if (invSum <= 0.0f) continue; // both immovable

            SimVec3 d = posB - A->position;        // axis A -> B
            const float dist = std::sqrt(dotv(d, d));
            const SimVec3 n = (dist > 1e-6f) ? d * (1.0f / dist) : SimVec3{1.0f, 0.0f, 0.0f};
            const float cError = dist - c.distance; // > 0 stretched, < 0 compressed

            const SimVec3 relV = velB - A->velocity;
            const float vn = dotv(relV, n);
            // Drive the distance rate to cancel the error (Baumgarte) and relative speed.
            const float jn = -(vn + kBaumgarte * invDt * cError) / invSum;
            const SimVec3 J = n * jn; // impulse on B; -J on A
            A->velocity += J * (-imA);
            if (B) B->velocity += J * imB;
        }
    }
}

void RigidBodySolver::resolveContacts() noexcept {
    if (!m_groundEnabled && !m_bodyCollisionEnabled) {
        return;
    }
    // Box-box runs once per step with warm-starting + accumulated impulses (stable
    // stacks); sphere/capsule and ground keep the simpler per-iteration solve.
    if (m_bodyCollisionEnabled) {
        solveBoxBoxStep();
    }
    for (uint32_t it = 0; it < m_solverIterations; ++it) {
        if (m_bodyCollisionEnabled) {
            resolveBodyCollisions();
        }
        if (m_groundEnabled) {
            for (auto& [id, b] : m_bodies) {
                (void)id;
                if (b.mass != 0.0f) {
                    resolveGroundContact(b);
                }
            }
        }
    }
}

void RigidBodySolver::resolveBodyPair(Body& a, Body& b) const noexcept {
    const bool aBox = isBoxCollider(a);
    const bool bBox = isBoxCollider(b);

    if (aBox && bBox) {
        return; // box-box is handled once per step by solveBoxBoxStep (warm-started)
    }

    if (aBox || bBox) {
        // Box vs round (sphere/capsule): reduce the round shape to a sphere at the
        // point on its segment closest to the box centre, then test that sphere
        // against the closest point on the box surface.
        Body& box   = aBox ? a : b;
        Body& round = aBox ? b : a;
        if (round.collisionRadius <= 0.0f) {
            return;
        }
        const SimVec3 axis = rotateByQuat(round.orientation, {0.0f, 1.0f, 0.0f}) * round.collisionHalfHeight;
        const SimVec3 sphereCenter = closestPtPointSegment(box.position,
                                                           round.position - axis,
                                                           round.position + axis);
        const SimVec3 onBox = closestPtPointObb(box.position, box.orientation,
                                                box.collisionHalfExtents, sphereCenter);
        const SimVec3 delta = sphereCenter - onBox; // box surface -> round centre
        const float distSq = dotv(delta, delta);
        if (distSq >= round.collisionRadius * round.collisionRadius) {
            return; // no overlap
        }
        const float dist = std::sqrt(distSq);
        const SimVec3 nBoxToRound = (dist > 1e-6f) ? delta * (1.0f / dist) : SimVec3{0.0f, 1.0f, 0.0f};
        const float penetration = round.collisionRadius - dist;
        // resolveContact separates its first body along -normal; pass (box, round)
        // with the box->round normal so the round is pushed out of the box.
        resolveContact(box, &round, onBox, nBoxToRound, penetration, m_bodyRestitution);
        return;
    }

    // Round vs round. Each collider is a round segment (capsule); a sphere is the
    // zero-length case. The closest points between the segments give the contact.
    const float rSum = a.collisionRadius + b.collisionRadius;
    if (rSum <= 0.0f) {
        return;
    }
    const SimVec3 axisA = rotateByQuat(a.orientation, {0.0f, 1.0f, 0.0f}) * a.collisionHalfHeight;
    const SimVec3 axisB = rotateByQuat(b.orientation, {0.0f, 1.0f, 0.0f}) * b.collisionHalfHeight;
    SimVec3 cA, cB;
    closestPtSegmentSegment(a.position - axisA, a.position + axisA,
                            b.position - axisB, b.position + axisB, cA, cB);

    const SimVec3 delta = cB - cA; // contact normal points a -> b
    const float distSq  = dotv(delta, delta);
    if (distSq >= rSum * rSum) {
        return; // segments farther apart than the combined radii — no contact
    }
    const float dist = std::sqrt(distSq);
    // Coincident closest points fall back to an arbitrary axis so the response stays finite.
    const SimVec3 normal = (dist > 1e-6f) ? delta * (1.0f / dist) : SimVec3{1.0f, 0.0f, 0.0f};
    const float penetration = rSum - dist;
    // One contact point at the midpoint of the overlap, shared by both lever arms.
    const SimVec3 contactPoint = cA + normal * (a.collisionRadius - 0.5f * penetration);

    resolveContact(a, &b, contactPoint, normal, penetration, m_bodyRestitution);
}

void RigidBodySolver::solveBoxBoxStep() noexcept {
    std::vector<Body*> boxes;
    boxes.reserve(m_bodies.size());
    for (auto& [id, b] : m_bodies) {
        (void)id;
        if (hasCollider(b) && isBoxCollider(b)) boxes.push_back(&b);
    }
    if (boxes.size() < 2) { m_boxWarmStart.clear(); return; }
    std::sort(boxes.begin(), boxes.end(),
              [](const Body* a, const Body* c) noexcept { return a->id < c->id; });

    struct BBContact {
        Body* a; Body* b;
        SimVec3 normal, rA, rB, tangent; // normal points a -> b
        float nMass = 0.f, tMass = 0.f;  // 1 / effective mass along normal / tangent
        float restBias = 0.f;            // target separating speed from restitution
        std::uint64_t key = 0;
        float nImp = 0.f, tImp = 0.f;    // accumulated impulses
    };

    // Effective inverse mass resisting an impulse along `dir` at lever arms rA/rB.
    auto effInvMass = [](const Body& A, float imA, const SimVec3& rA,
                         const Body& B, float imB, const SimVec3& rB, const SimVec3& dir) -> float {
        float k = imA + imB;
        if (imA > 0.0f) { const SimVec3 i = angularVelocityFromMomentum(A.orientation, cross(rA, dir), A.inertia); k += dotv(dir, cross(i, rA)); }
        if (imB > 0.0f) { const SimVec3 i = angularVelocityFromMomentum(B.orientation, cross(rB, dir), B.inertia); k += dotv(dir, cross(i, rB)); }
        return k;
    };
    auto relVel = [](const BBContact& c) {
        return (c.b->velocity + cross(c.b->angularVelocity, c.rB))
             - (c.a->velocity + cross(c.a->angularVelocity, c.rA));
    };
    auto applyImpulse = [](BBContact& c, const SimVec3& J) {
        const float imA = (c.a->mass > 0.0f) ? 1.0f / c.a->mass : 0.0f;
        const float imB = (c.b->mass > 0.0f) ? 1.0f / c.b->mass : 0.0f;
        c.a->velocity += J * (-imA);
        c.b->velocity += J * imB;
        if (imA > 0.0f) c.a->angularVelocity += angularVelocityFromMomentum(c.a->orientation, cross(c.rA, J * -1.0f), c.a->inertia);
        if (imB > 0.0f) c.b->angularVelocity += angularVelocityFromMomentum(c.b->orientation, cross(c.rB, J), c.b->inertia);
    };

    std::vector<BBContact> contacts;

    for (std::size_t bi = 0; bi < boxes.size(); ++bi) {
        for (std::size_t bj = bi + 1; bj < boxes.size(); ++bj) {
            Body& a = *boxes[bi];
            Body& b = *boxes[bj];
            const SimVec3 ax[3] = { rotateByQuat(a.orientation, {1,0,0}), rotateByQuat(a.orientation, {0,1,0}), rotateByQuat(a.orientation, {0,0,1}) };
            const SimVec3 bx[3] = { rotateByQuat(b.orientation, {1,0,0}), rotateByQuat(b.orientation, {0,1,0}), rotateByQuat(b.orientation, {0,0,1}) };
            const float ae[3] = { a.collisionHalfExtents.x, a.collisionHalfExtents.y, a.collisionHalfExtents.z };
            const float be[3] = { b.collisionHalfExtents.x, b.collisionHalfExtents.y, b.collisionHalfExtents.z };
            const SimVec3 dCenter = b.position - a.position;

            auto projRadius = [](const SimVec3 axes[3], const float ext[3], const SimVec3& L) {
                return std::fabs(dotv(L, axes[0]))*ext[0] + std::fabs(dotv(L, axes[1]))*ext[1] + std::fabs(dotv(L, axes[2]))*ext[2];
            };
            float minOverlap = std::numeric_limits<float>::max();
            SimVec3 N{0,0,0};
            bool separated = false;
            auto testAxis = [&](SimVec3 L) {
                const float len2 = dotv(L, L);
                if (len2 < 1e-8f) return;
                L = L * (1.0f / std::sqrt(len2));
                const float overlap = projRadius(ax, ae, L) + projRadius(bx, be, L) - std::fabs(dotv(L, dCenter));
                if (overlap <= 0.0f) { separated = true; return; }
                if (overlap < minOverlap) { minOverlap = overlap; N = (dotv(L, dCenter) < 0.0f) ? L * -1.0f : L; }
            };
            for (int i = 0; i < 3 && !separated; ++i) testAxis(ax[i]);
            for (int i = 0; i < 3 && !separated; ++i) testAxis(bx[i]);
            for (int i = 0; i < 3 && !separated; ++i)
                for (int j = 0; j < 3 && !separated; ++j) testAxis(cross(ax[i], bx[j]));
            if (separated || (N.x == 0.f && N.y == 0.f && N.z == 0.f)) continue;

            const float imA = (a.mass > 0.0f) ? 1.0f / a.mass : 0.0f;
            const float imB = (b.mass > 0.0f) ? 1.0f / b.mass : 0.0f;
            const float imSum = imA + imB;
            if (imSum <= 0.0f) continue;

            // Mass-weighted positional correction (once per step).
            const SimVec3 corr = N * (minOverlap / imSum);
            a.position += corr * (-imA);
            b.position += corr * imB;

            // Incident-vertex manifold (reference = box whose face aligns with N).
            auto maxAlign = [&](const SimVec3 axes[3]) {
                return std::max({ std::fabs(dotv(N, axes[0])), std::fabs(dotv(N, axes[1])), std::fabs(dotv(N, axes[2])) });
            };
            const bool refIsA = maxAlign(ax) >= maxAlign(bx);
            const SimVec3& refC = refIsA ? a.position : b.position;
            const SimVec3* refAx = refIsA ? ax : bx;
            const float*   refE  = refIsA ? ae : be;
            const SimVec3& incC = refIsA ? b.position : a.position;
            const SimVec3* incAx = refIsA ? bx : ax;
            const float*   incE  = refIsA ? be : ae;
            auto inBox = [](const SimVec3& c, const SimVec3 axes[3], const float ext[3], const SimVec3& p) {
                const SimVec3 d = p - c;
                return std::fabs(dotv(d, axes[0])) <= ext[0]+1e-3f && std::fabs(dotv(d, axes[1])) <= ext[1]+1e-3f && std::fabs(dotv(d, axes[2])) <= ext[2]+1e-3f;
            };

            auto addContact = [&](const SimVec3& point, int feature) {
                BBContact c{};
                c.a = &a; c.b = &b; c.normal = N;
                c.rA = point - a.position;
                c.rB = point - b.position;
                const SimVec3 vc = (b.velocity + cross(b.angularVelocity, c.rB)) - (a.velocity + cross(a.angularVelocity, c.rA));
                const float vn = dotv(vc, N);
                c.restBias = (vn < -0.5f) ? -m_bodyRestitution * vn : 0.0f; // bounce only above a slop speed
                SimVec3 vt = vc - N * vn;
                const float vtLen = std::sqrt(dotv(vt, vt));
                if (vtLen > 1e-4f) {
                    c.tangent = vt * (1.0f / vtLen);
                } else { // arbitrary tangent perpendicular to the normal
                    const SimVec3 ref = (std::fabs(N.x) < 0.9f) ? SimVec3{1,0,0} : SimVec3{0,1,0};
                    SimVec3 t = cross(N, ref);
                    const float tl = std::sqrt(dotv(t, t));
                    c.tangent = (tl > 1e-6f) ? t * (1.0f / tl) : SimVec3{0,0,1};
                }
                const float kn = effInvMass(a, imA, c.rA, b, imB, c.rB, N);
                const float kt = effInvMass(a, imA, c.rA, b, imB, c.rB, c.tangent);
                c.nMass = kn > 1e-12f ? 1.0f / kn : 0.0f;
                c.tMass = kt > 1e-12f ? 1.0f / kt : 0.0f;
                c.key = (static_cast<std::uint64_t>(a.id) << 32) ^ (static_cast<std::uint64_t>(b.id) << 12)
                      ^ (static_cast<std::uint64_t>(refIsA ? 1u : 0u) << 8) ^ static_cast<std::uint64_t>(feature);
                contacts.push_back(c);
            };

            int made = 0;
            for (int s = 0; s < 8; ++s) {
                const float sx = (s & 1) ? 1.f : -1.f, sy = (s & 2) ? 1.f : -1.f, sz = (s & 4) ? 1.f : -1.f;
                const SimVec3 vert = incC + incAx[0]*(sx*incE[0]) + incAx[1]*(sy*incE[1]) + incAx[2]*(sz*incE[2]);
                if (inBox(refC, refAx, refE, vert)) { addContact(vert, s); ++made; }
            }
            if (made == 0) addContact((a.position + b.position) * 0.5f, 8); // edge-edge fallback
        }
    }

    // Warm start: re-apply last frame's accumulated impulses so persistent stacks
    // begin near the solution (this is what stops the slow resting drift).
    for (auto& c : contacts) {
        const auto it = m_boxWarmStart.find(c.key);
        if (it != m_boxWarmStart.end()) { c.nImp = it->second.first; c.tImp = it->second.second; }
        applyImpulse(c, c.normal * c.nImp + c.tangent * c.tImp);
    }

    // Sequential impulse with accumulated, clamped impulses.
    for (uint32_t iter = 0; iter < m_solverIterations; ++iter) {
        for (auto& c : contacts) {
            const float vn = dotv(relVel(c), c.normal);
            const float dN = -(vn - c.restBias) * c.nMass;
            const float newN = std::max(0.0f, c.nImp + dN); // non-penetrating: λn >= 0
            applyImpulse(c, c.normal * (newN - c.nImp));
            c.nImp = newN;

            if (m_friction > 0.0f) {
                const float vt = dotv(relVel(c), c.tangent);
                const float dT = -vt * c.tMass;
                const float maxF = m_friction * c.nImp; // Coulomb cone
                float newT = c.tImp + dT;
                if (newT >  maxF) newT =  maxF;
                if (newT < -maxF) newT = -maxF;
                applyImpulse(c, c.tangent * (newT - c.tImp));
                c.tImp = newT;
            }
        }
    }

    // Cache accumulated impulses for next frame's warm start.
    m_boxWarmStart.clear();
    for (const auto& c : contacts) m_boxWarmStart[c.key] = { c.nImp, c.tImp };
}

void RigidBodySolver::resolveBodyCollisions() noexcept {
    std::vector<Body*> colliders;
    colliders.reserve(m_bodies.size());
    for (auto& [id, b] : m_bodies) {
        (void)id;
        if (hasCollider(b)) {
            colliders.push_back(&b);
        }
    }

    // Broadphase — sweep-and-prune on the X axis. Sorting by AABB min.x (tie-break
    // by id for a deterministic order) lets the inner sweep stop as soon as a
    // body's min.x is past the current body's max.x, so non-overlapping pairs are
    // skipped without an exact test. Spheres that overlap in 3D always overlap in
    // their x-projection, so no real contact is ever pruned; this only drops the
    // pairs the O(n^2) loop would have rejected anyway.
    // AABB x-extent uses a conservative bounding radius per collider (sphere/capsule
    // reach, or a box's circumscribed sphere) so no overlapping pair is pruned.
    std::sort(colliders.begin(), colliders.end(),
              [](const Body* a, const Body* c) noexcept {
                  const float amin = a->position.x - boundingRadius(*a);
                  const float cmin = c->position.x - boundingRadius(*c);
                  if (amin != cmin) return amin < cmin;
                  return a->id < c->id;
              });

    // Generate candidate pairs from the (stable) post-integration positions, then
    // resolve them — keeping broadphase reads independent of the position changes
    // that resolution makes.
    std::vector<std::pair<Body*, Body*>> candidates;
    for (std::size_t i = 0; i < colliders.size(); ++i) {
        const float iMaxX = colliders[i]->position.x + boundingRadius(*colliders[i]);
        for (std::size_t k = i + 1; k < colliders.size(); ++k) {
            const float kMinX = colliders[k]->position.x - boundingRadius(*colliders[k]);
            if (kMinX > iMaxX) {
                break; // sorted by min.x: no later body can overlap i on x
            }
            candidates.emplace_back(colliders[i], colliders[k]);
        }
    }

    for (const auto& [a, b] : candidates) {
        resolveBodyPair(*a, *b);
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

inline float dotv(const SimVec3& a, const SimVec3& b) noexcept
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline float clampUnit(float x) noexcept
{
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

/// Closest points between segments [p1,q1] and [p2,q2] (Ericson, RTCD). Degenerate
/// (zero-length) segments collapse to their endpoint, so a capsule of zero
/// half-height reduces to a sphere at its center.
void closestPtSegmentSegment(const SimVec3& p1, const SimVec3& q1,
                             const SimVec3& p2, const SimVec3& q2,
                             SimVec3& c1, SimVec3& c2) noexcept
{
    const SimVec3 d1 = q1 - p1;
    const SimVec3 d2 = q2 - p2;
    const SimVec3 r  = p1 - p2;
    const float a = dotv(d1, d1);
    const float e = dotv(d2, d2);
    const float f = dotv(d2, r);
    constexpr float eps = 1e-12f;
    float s = 0.0f, t = 0.0f;
    if (a <= eps && e <= eps) {
        // both segments are points
    } else if (a <= eps) {
        t = clampUnit(f / e);                 // first segment is a point
    } else {
        const float c = dotv(d1, r);
        if (e <= eps) {
            s = clampUnit(-c / a);            // second segment is a point
        } else {
            const float b = dotv(d1, d2);
            const float denom = a * e - b * b;
            s = (denom > eps) ? clampUnit((b * f - c * e) / denom) : 0.0f;
            t = (b * s + f) / e;
            if (t < 0.0f)      { t = 0.0f; s = clampUnit(-c / a); }
            else if (t > 1.0f) { t = 1.0f; s = clampUnit((b - c) / a); }
        }
    }
    c1 = p1 + d1 * s;
    c2 = p2 + d2 * t;
}

/// Closest point on segment [a,b] to point p.
SimVec3 closestPtPointSegment(const SimVec3& p, const SimVec3& a, const SimVec3& b) noexcept
{
    const SimVec3 ab = b - a;
    const float denom = dotv(ab, ab);
    if (denom <= 1e-12f) {
        return a; // degenerate segment
    }
    const float t = clampUnit(dotv(p - a, ab) / denom);
    return a + ab * t;
}

/// Closest point on an oriented box to point p: project (p - center) onto each box
/// axis, clamp to the half-extent, and rebuild in world space.
SimVec3 closestPtPointObb(const SimVec3& center, const SimQuat& orientation,
                          const SimVec3& halfExtents, const SimVec3& p) noexcept
{
    const SimVec3 d = p - center;
    const SimVec3 axes[3] = {
        rotateByQuat(orientation, {1.0f, 0.0f, 0.0f}),
        rotateByQuat(orientation, {0.0f, 1.0f, 0.0f}),
        rotateByQuat(orientation, {0.0f, 0.0f, 1.0f}),
    };
    const float ext[3] = { halfExtents.x, halfExtents.y, halfExtents.z };
    SimVec3 result = center;
    for (int i = 0; i < 3; ++i) {
        float dist = dotv(d, axes[i]);
        if (dist >  ext[i]) dist =  ext[i];
        if (dist < -ext[i]) dist = -ext[i];
        result += axes[i] * dist;
    }
    return result;
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

        // Clear per-step accumulated force and torque.
        b.force  = {0.0f, 0.0f, 0.0f};
        b.torque = {0.0f, 0.0f, 0.0f};
    }

    // Contact resolution (body-body + ground), iterated for stable stacks. No-op
    // when no collision feature is enabled.
    resolveContacts();
    solveConstraints(fdt);

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

    // Contact resolution after the integrated substeps (collision was previously
    // absent from the fixed-step path). No-op when no collision feature is enabled,
    // so existing fixed-step trajectories are unchanged.
    resolveContacts();
    solveConstraints(static_cast<float>(dt));

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
    m_constraints.clear();
    m_boxWarmStart.clear();
    m_time = 0.0;
    // m_nextId / m_nextConstraintId not reset; m_gravity preserved.
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
