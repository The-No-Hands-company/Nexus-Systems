#include "nexus/sim/FluidSolver.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace nexus {

// ── Serialization helpers ─────────────────────────────────────────────────────

namespace {

inline constexpr uint32_t kFluidMagic   = 0x314c4446u; // 'FDL1'
inline constexpr uint32_t kFluidVersion = 1u;

template<typename T>
void appendBytesF(std::vector<uint8_t>& buf, T val) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto n = buf.size();
    buf.resize(n + sizeof(T));
    std::memcpy(buf.data() + n, &val, sizeof(T));
}

template<typename T>
bool readBytesF(const std::vector<uint8_t>& buf, std::size_t& offset, T& out) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (offset + sizeof(T) > buf.size()) return false;
    std::memcpy(&out, buf.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

inline uint32_t toBitsFF(float f)  { return std::bit_cast<uint32_t>(f); }
inline uint64_t toBitsFD(double d) { return std::bit_cast<uint64_t>(d); }
inline float    fromBitsFF(uint32_t u) { return std::bit_cast<float>(u); }
inline double   fromBitsFD(uint64_t u) { return std::bit_cast<double>(u); }

inline bool isFiniteFloat(float value) noexcept {
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    return (bits & 0x7F800000u) != 0x7F800000u;
}

inline bool isPositiveFiniteDouble(double value) noexcept {
    const std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
    constexpr std::uint64_t kSignMask = 0x8000000000000000ULL;
    constexpr std::uint64_t kExpMask = 0x7FF0000000000000ULL;
    constexpr std::uint64_t kMagnitudeMask = 0x7FFFFFFFFFFFFFFFULL;
    return (bits & kSignMask) == 0ULL && (bits & kExpMask) != kExpMask && (bits & kMagnitudeMask) != 0ULL;
}

inline bool isFiniteVec3(const FluidVec3& value) noexcept {
    return isFiniteFloat(value.x) && isFiniteFloat(value.y) && isFiniteFloat(value.z);
}

// ── Internal particle data ────────────────────────────────────────────────────

struct FluidParticle {
    float     mass;
    FluidVec3 position;
    FluidVec3 velocity;
    float     restDensity;
    float     density;   ///< Updated each step.
};

} // anonymous namespace

// ── Equality operators ────────────────────────────────────────────────────────

bool operator==(const FluidParticleSnapshot& a, const FluidParticleSnapshot& b) noexcept {
    return a.id == b.id && a.position == b.position && a.velocity == b.velocity
        && a.density == b.density;
}

bool operator==(const FluidState& a, const FluidState& b) noexcept {
    if (a.simulationTime != b.simulationTime) return false;
    if (a.particles.size() != b.particles.size()) return false;
    for (std::size_t i = 0; i < a.particles.size(); ++i) {
        if (!(a.particles[i] == b.particles[i])) return false;
    }
    return true;
}

// ── Serialization ─────────────────────────────────────────────────────────────

std::vector<uint8_t> serializeFluidState(const FluidState& state) {
    auto sorted = state.particles;
    std::sort(sorted.begin(), sorted.end(),
              [](const FluidParticleSnapshot& x, const FluidParticleSnapshot& y){ return x.id < y.id; });

    std::vector<uint8_t> buf;
    appendBytesF(buf, kFluidMagic);
    appendBytesF(buf, kFluidVersion);
    appendBytesF(buf, toBitsFD(state.simulationTime));
    appendBytesF(buf, static_cast<uint32_t>(sorted.size()));

    for (const auto& p : sorted) {
        appendBytesF(buf, p.id);
        appendBytesF(buf, toBitsFF(p.position.x));
        appendBytesF(buf, toBitsFF(p.position.y));
        appendBytesF(buf, toBitsFF(p.position.z));
        appendBytesF(buf, toBitsFF(p.velocity.x));
        appendBytesF(buf, toBitsFF(p.velocity.y));
        appendBytesF(buf, toBitsFF(p.velocity.z));
        appendBytesF(buf, toBitsFF(p.density));
    }
    return buf;
}

bool deserializeFluidState(const std::vector<uint8_t>& bytes, FluidState& outState) {
    std::size_t off = 0;

    uint32_t magic = 0;
    if (!readBytesF(bytes, off, magic) || magic != kFluidMagic) return false;

    uint32_t version = 0;
    if (!readBytesF(bytes, off, version) || version != kFluidVersion) return false;

    uint64_t timeBits = 0;
    if (!readBytesF(bytes, off, timeBits)) return false;

    uint32_t count = 0;
    if (!readBytesF(bytes, off, count)) return false;

    FluidState result;
    result.simulationTime = fromBitsFD(timeBits);
    result.particles.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t id = 0;
        if (!readBytesF(bytes, off, id)) return false;
        uint32_t px=0, py=0, pz=0, vx=0, vy=0, vz=0, den=0;
        if (!readBytesF(bytes, off, px))  return false;
        if (!readBytesF(bytes, off, py))  return false;
        if (!readBytesF(bytes, off, pz))  return false;
        if (!readBytesF(bytes, off, vx))  return false;
        if (!readBytesF(bytes, off, vy))  return false;
        if (!readBytesF(bytes, off, vz))  return false;
        if (!readBytesF(bytes, off, den)) return false;
        result.particles[i] = {id,
                                {fromBitsFF(px), fromBitsFF(py), fromBitsFF(pz)},
                                {fromBitsFF(vx), fromBitsFF(vy), fromBitsFF(vz)},
                                fromBitsFF(den)};
    }

    if (off != bytes.size()) return false;

    outState = std::move(result);
    return true;
}

// ── FluidSolver::Impl ─────────────────────────────────────────────────────────

struct FluidSolver::Impl {
    FluidParticleId                               nextId   = 1u;
    std::unordered_map<FluidParticleId, FluidParticle> particles;
    FluidVec3                                     grav     = {0.0f, -9.81f, 0.0f};
    float                                         h        = 0.1f;   // smoothing radius
    float                                         kStiff   = 200.0f; // pressure stiffness
    double                                        time     = 0.0;
};

// ── FluidSolver public API ────────────────────────────────────────────────────

FluidSolver::FluidSolver()  : m_impl(new Impl) {}
FluidSolver::~FluidSolver() { delete m_impl; }

FluidParticleId FluidSolver::addParticle(const FluidParticleDesc& desc) {
    const FluidParticleId id = m_impl->nextId++;
    m_impl->particles.insert({id, FluidParticle{desc.mass, desc.position, desc.velocity,
                                                 desc.density, desc.density}});
    return id;
}

bool FluidSolver::removeParticle(FluidParticleId id) noexcept {
    if (!m_impl->particles.count(id)) return false;
    m_impl->particles.erase(id);
    return true;
}

bool FluidSolver::hasParticle(FluidParticleId id) const noexcept {
    return m_impl->particles.count(id) > 0;
}

std::size_t FluidSolver::particleCount() const noexcept {
    return m_impl->particles.size();
}

bool FluidSolver::getParticleState(FluidParticleId id,
                                    FluidVec3& outPos,
                                    FluidVec3& outVel,
                                    float& outDensity) const noexcept {
    auto it = m_impl->particles.find(id);
    if (it == m_impl->particles.end()) return false;
    if (it->second.mass == 0.0f) return false;
    outPos     = it->second.position;
    outVel     = it->second.velocity;
    outDensity = it->second.density;
    return true;
}

void  FluidSolver::setGravity(FluidVec3 gravity) noexcept { m_impl->grav = gravity; }
FluidVec3 FluidSolver::gravity() const noexcept            { return m_impl->grav; }

void  FluidSolver::setSmoothingRadius(float h) noexcept {
    if (std::isfinite(h) && h > 0.0f) {
        m_impl->h = h;
    }
}
float FluidSolver::smoothingRadius()  const noexcept       { return m_impl->h; }

void  FluidSolver::setPressureStiffness(float k) noexcept  { m_impl->kStiff = k; }
float FluidSolver::pressureStiffness()  const noexcept     { return m_impl->kStiff; }

FluidStepReport FluidSolver::step(double dt) {
    if (!isPositiveFiniteDouble(dt) || !isFiniteVec3(m_impl->grav) || !isFiniteFloat(m_impl->h) || m_impl->h <= 0.0f || !isFiniteFloat(m_impl->kStiff)) {
        return FluidStepReport{false, m_impl->time, 0};
    }

    for (const auto& [id, particle] : m_impl->particles) {
        (void)id;
        if (!isFiniteFloat(particle.mass) || particle.mass < 0.0f || !isFiniteVec3(particle.position) || !isFiniteVec3(particle.velocity) || !isFiniteFloat(particle.restDensity) || !isFiniteFloat(particle.density)) {
            return FluidStepReport{false, m_impl->time, 0};
        }
    }

    const float fdt = static_cast<float>(dt);
    auto& parts = m_impl->particles;

    // ── Density estimation ────────────────────────────────────────────────────
    // Simple radial kernel: W(r,h) = max(0, 1 - r/h).  Not physically correct
    // but deterministic and sufficient for the v0 API contract.
    const float h2 = m_impl->h * m_impl->h;

    // Build ordered id list for deterministic iteration.
    std::vector<FluidParticleId> ids;
    ids.reserve(parts.size());
    for (const auto& [id, _] : parts) ids.push_back(id);
    std::sort(ids.begin(), ids.end());

    for (const auto aid : ids) {
        auto& a = parts.at(aid);
        float densAccum = 0.0f;
        for (const auto bid : ids) {
            const auto& b = parts.at(bid);
            const FluidVec3 d = b.position - a.position;
            const float r2 = d.x*d.x + d.y*d.y + d.z*d.z;
            if (r2 >= h2) continue;
            const float r   = std::sqrt(r2);
            const float w   = 1.0f - r / m_impl->h;
            densAccum += b.mass * w;
        }
        a.density = densAccum;
    }

    // ── Pressure force + gravity integration ──────────────────────────────────
    std::size_t advanced = 0;
    for (const auto aid : ids) {
        auto& a = parts.at(aid);
        if (a.mass == 0.0f) continue;

        FluidVec3 fPressure{};
        const float pA = m_impl->kStiff * (a.density - a.restDensity);

        for (const auto bid : ids) {
            if (bid == aid) continue;
            const auto& b = parts.at(bid);
            const FluidVec3 d   = b.position - a.position;
            const float     r2  = d.x*d.x + d.y*d.y + d.z*d.z;
            if (r2 >= h2 || r2 < 1e-12f) continue;
            const float r    = std::sqrt(r2);
            const float pB   = m_impl->kStiff * (b.density - b.restDensity);
            const float grad = -(1.0f / (m_impl->h * r));
            const float fmag = -b.mass * (pA + pB) * 0.5f * grad;
            fPressure += d * fmag;
        }

        const float invMass = 1.0f / a.mass;
        const FluidVec3 acc = m_impl->grav + (fPressure * invMass);
        a.velocity += acc * fdt;
        a.position += a.velocity * fdt;
        ++advanced;
    }

    m_impl->time += dt;
    return FluidStepReport{true, m_impl->time, advanced};
}

FluidState FluidSolver::captureState() const {
    FluidState s;
    s.simulationTime = m_impl->time;
    s.particles.reserve(m_impl->particles.size());
    for (const auto& [id, p] : m_impl->particles) {
        s.particles.push_back({id, p.position, p.velocity, p.density});
    }
    std::sort(s.particles.begin(), s.particles.end(),
              [](const FluidParticleSnapshot& a, const FluidParticleSnapshot& b){ return a.id < b.id; });
    return s;
}

bool FluidSolver::restoreState(const FluidState& state) {
    for (const auto& snap : state.particles) {
        if (!m_impl->particles.count(snap.id)) return false;
    }
    for (const auto& snap : state.particles) {
        auto& p    = m_impl->particles.at(snap.id);
        p.position = snap.position;
        p.velocity = snap.velocity;
        p.density  = snap.density;
    }
    m_impl->time = state.simulationTime;
    return true;
}

} // namespace nexus
