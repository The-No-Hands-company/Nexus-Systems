#include "nexus/sim/SimulationCore.h"

#include <cassert>
#include <bit>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>

namespace nexus {

namespace {

constexpr std::uint32_t kSimStateMagic = 0x314d5353u; // 'SSM1' little-endian marker
constexpr std::uint32_t kSimStateVersion = 1u;

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

} // namespace

bool operator==(const SimBodySnapshot& a, const SimBodySnapshot& b) noexcept
{
    return a.id == b.id && a.position == b.position && a.velocity == b.velocity;
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
    bytes.reserve(16u + sorted.bodies.size() * 32u);

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
    if (!readBytes(bytes, offset, version) || version != kSimStateVersion) return false;
    if (!readBytes(bytes, offset, timeBits)) return false;
    if (!readBytes(bytes, offset, bodyCount)) return false;

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

        state.bodies.push_back(SimBodySnapshot{
            static_cast<BodyId>(id),
            {fromBitsFloat(px), fromBitsFloat(py), fromBitsFloat(pz)},
            {fromBitsFloat(vx), fromBitsFloat(vy), fromBitsFloat(vz)}
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
        !isFiniteVec3(desc.position) || !isFiniteVec3(desc.velocity)) {
        return kInvalidBodyId;
    }

    BodyId id = m_nextId++;
    m_bodies.emplace(id, Body{
        id,
        desc.mass,
        desc.position,
        desc.velocity,
        /*force=*/{0.0f, 0.0f, 0.0f}
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

// ── Force accumulation ────────────────────────────────────────────────────────

bool RigidBodySolver::applyForce(BodyId id, SimVec3 force) noexcept {
    Body* b = findBody(id);
    if (!b || b->mass == 0.0f || !isFiniteVec3(force)) return false;
    b->force += force;
    return true;
}

void RigidBodySolver::setGravity(SimVec3 gravity) noexcept {
    m_gravity = gravity;
}

SimVec3 RigidBodySolver::gravity() const noexcept {
    return m_gravity;
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
            !isFiniteVec3(b.force)) {
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

        // Explicit Euler integration.
        SimVec3 acceleration = total * (1.0f / b.mass);
        b.velocity = b.velocity + acceleration * fdt;
        b.position = b.position + b.velocity * fdt;

        // Clear per-step accumulated force.
        b.force = {0.0f, 0.0f, 0.0f};
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
            !isFiniteVec3(b.force)) {
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
            b.position = b.position + b.velocity * fdt;
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
        b.force = {0.0f, 0.0f, 0.0f};
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
        state.bodies.push_back(SimBodySnapshot{b.id, b.position, b.velocity});
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
        if (!isFiniteVec3(snap.position) || !isFiniteVec3(snap.velocity)) {
            return false;
        }
    }

    m_time = state.simulationTime;
    for (const SimBodySnapshot& snap : state.bodies) {
        Body* b = findBody(snap.id);
        if (!b) continue; // body absent in solver — ignore
        b->position = snap.position;
        b->velocity = snap.velocity;
        b->force    = {0.0f, 0.0f, 0.0f}; // clear accumulated force on restore
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
