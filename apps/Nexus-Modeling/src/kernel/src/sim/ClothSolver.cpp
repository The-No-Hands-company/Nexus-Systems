#include "nexus/sim/ClothSolver.h"

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

inline constexpr uint32_t kClothMagic   = 0x314c5343u; // 'CSL1'
inline constexpr uint32_t kClothVersion = 1u;

template<typename T>
void appendBytes(std::vector<uint8_t>& buf, T val) {
    static_assert(std::is_trivially_copyable_v<T>);
    const auto n = buf.size();
    buf.resize(n + sizeof(T));
    std::memcpy(buf.data() + n, &val, sizeof(T));
}

template<typename T>
bool readBytes(const std::vector<uint8_t>& buf, std::size_t& offset, T& out) {
    static_assert(std::is_trivially_copyable_v<T>);
    if (offset + sizeof(T) > buf.size()) return false;
    std::memcpy(&out, buf.data() + offset, sizeof(T));
    offset += sizeof(T);
    return true;
}

inline uint32_t toBitsF(float f) { return std::bit_cast<uint32_t>(f); }
inline uint64_t toBitsD(double d) { return std::bit_cast<uint64_t>(d); }
inline float    fromBitsF(uint32_t u) { return std::bit_cast<float>(u); }
inline double   fromBitsD(uint64_t u) { return std::bit_cast<double>(u); }

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

inline bool isFiniteVec3(const ClothVec3& value) noexcept {
    return isFiniteFloat(value.x) && isFiniteFloat(value.y) && isFiniteFloat(value.z);
}

// ── Internal solver data ──────────────────────────────────────────────────────

struct ClothNode {
    float     mass;
    ClothVec3 position;
    ClothVec3 velocity;
    ClothVec3 force;
};

struct ClothEdge {
    ClothNodeId a, b;
    float restLength;
    float stiffness;
};

} // anonymous namespace

// ── Equality operators ────────────────────────────────────────────────────────

bool operator==(const ClothNodeSnapshot& a, const ClothNodeSnapshot& b) noexcept {
    return a.id == b.id && a.position == b.position && a.velocity == b.velocity;
}

bool operator==(const ClothState& a, const ClothState& b) noexcept {
    if (a.simulationTime != b.simulationTime) return false;
    if (a.nodes.size() != b.nodes.size()) return false;
    for (std::size_t i = 0; i < a.nodes.size(); ++i) {
        if (!(a.nodes[i] == b.nodes[i])) return false;
    }
    return true;
}

// ── Serialization ─────────────────────────────────────────────────────────────

std::vector<uint8_t> serializeClothState(const ClothState& state) {
    // Sort by ascending NodeId for determinism.
    auto sorted = state.nodes;
    std::sort(sorted.begin(), sorted.end(),
              [](const ClothNodeSnapshot& x, const ClothNodeSnapshot& y){ return x.id < y.id; });

    std::vector<uint8_t> buf;
    appendBytes(buf, kClothMagic);
    appendBytes(buf, kClothVersion);
    appendBytes(buf, toBitsD(state.simulationTime));
    appendBytes(buf, static_cast<uint32_t>(sorted.size()));
    for (const auto& n : sorted) {
        appendBytes(buf, n.id);
        appendBytes(buf, toBitsF(n.position.x));
        appendBytes(buf, toBitsF(n.position.y));
        appendBytes(buf, toBitsF(n.position.z));
        appendBytes(buf, toBitsF(n.velocity.x));
        appendBytes(buf, toBitsF(n.velocity.y));
        appendBytes(buf, toBitsF(n.velocity.z));
    }
    return buf;
}

bool deserializeClothState(const std::vector<uint8_t>& bytes, ClothState& outState) {
    std::size_t off = 0;

    uint32_t magic = 0;
    if (!readBytes(bytes, off, magic) || magic != kClothMagic) return false;

    uint32_t version = 0;
    if (!readBytes(bytes, off, version) || version != kClothVersion) return false;

    uint64_t timeBits = 0;
    if (!readBytes(bytes, off, timeBits)) return false;

    uint32_t count = 0;
    if (!readBytes(bytes, off, count)) return false;

    ClothState result;
    result.simulationTime = fromBitsD(timeBits);
    result.nodes.resize(count);

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t id = 0;
        if (!readBytes(bytes, off, id)) return false;
        uint32_t px=0, py=0, pz=0, vx=0, vy=0, vz=0;
        if (!readBytes(bytes, off, px)) return false;
        if (!readBytes(bytes, off, py)) return false;
        if (!readBytes(bytes, off, pz)) return false;
        if (!readBytes(bytes, off, vx)) return false;
        if (!readBytes(bytes, off, vy)) return false;
        if (!readBytes(bytes, off, vz)) return false;
        result.nodes[i] = {id,
                           {fromBitsF(px), fromBitsF(py), fromBitsF(pz)},
                           {fromBitsF(vx), fromBitsF(vy), fromBitsF(vz)}};
    }

    if (off != bytes.size()) return false;

    outState = std::move(result);
    return true;
}

// ── ClothSolver::Impl ─────────────────────────────────────────────────────────

struct ClothSolver::Impl {
    ClothNodeId                             nextId = 1u;
    std::unordered_map<ClothNodeId, ClothNode> nodes;
    std::vector<ClothEdge>                  edges;
    ClothVec3                               grav  = {0.0f, -9.81f, 0.0f};
    double                                  time  = 0.0;
};

// ── ClothSolver public API ────────────────────────────────────────────────────

ClothSolver::ClothSolver()  : m_impl(new Impl) {}
ClothSolver::~ClothSolver() { delete m_impl; }

ClothNodeId ClothSolver::addNode(const ClothNodeDesc& desc) {
    const ClothNodeId id = m_impl->nextId++;
    m_impl->nodes.insert({id, ClothNode{desc.mass, desc.position, desc.velocity, {}}});
    return id;
}

bool ClothSolver::removeNode(ClothNodeId id) noexcept {
    if (!m_impl->nodes.count(id)) return false;
    m_impl->nodes.erase(id);
    // Remove edges touching this node.
    auto& edges = m_impl->edges;
    edges.erase(std::remove_if(edges.begin(), edges.end(),
                               [id](const ClothEdge& e){ return e.a == id || e.b == id; }),
                edges.end());
    return true;
}

bool ClothSolver::hasNode(ClothNodeId id) const noexcept {
    return m_impl->nodes.count(id) > 0;
}

std::size_t ClothSolver::nodeCount() const noexcept {
    return m_impl->nodes.size();
}

bool ClothSolver::getNodeState(ClothNodeId id, ClothVec3& outPos, ClothVec3& outVel) const noexcept {
    auto it = m_impl->nodes.find(id);
    if (it == m_impl->nodes.end()) return false;
    if (it->second.mass == 0.0f) return false;
    outPos = it->second.position;
    outVel = it->second.velocity;
    return true;
}

bool ClothSolver::addEdge(ClothNodeId a, ClothNodeId b, float restLength, float stiffness) noexcept {
    if (!m_impl->nodes.count(a) || !m_impl->nodes.count(b)) return false;
    if (a == b) return false;
    if (restLength < 0.0f) return false;
    if (stiffness <= 0.0f) return false;

    for (const auto& e : m_impl->edges) {
        if ((e.a == a && e.b == b) || (e.a == b && e.b == a)) {
            return false;
        }
    }

    m_impl->edges.push_back({a, b, restLength, stiffness});
    return true;
}

void ClothSolver::setGravity(ClothVec3 gravity) noexcept { m_impl->grav = gravity; }
ClothVec3 ClothSolver::gravity() const noexcept { return m_impl->grav; }

ClothStepReport ClothSolver::step(double dt) {
    if (!isPositiveFiniteDouble(dt) || !isFiniteVec3(m_impl->grav)) {
        return ClothStepReport{false, m_impl->time, 0};
    }

    for (const auto& [id, node] : m_impl->nodes) {
        (void)id;
        if (!isFiniteFloat(node.mass) || node.mass < 0.0f || !isFiniteVec3(node.position) || !isFiniteVec3(node.velocity)) {
            return ClothStepReport{false, m_impl->time, 0};
        }
    }

    for (const auto& edge : m_impl->edges) {
        if (!isFiniteFloat(edge.restLength) || edge.restLength < 0.0f) {
            return ClothStepReport{false, m_impl->time, 0};
        }
        if (!isFiniteFloat(edge.stiffness) || edge.stiffness <= 0.0f) {
            return ClothStepReport{false, m_impl->time, 0};
        }
    }

    const float fdt = static_cast<float>(dt);
    auto& nodes = m_impl->nodes;

    // Accumulate spring forces.
    for (const auto& e : m_impl->edges) {
        auto ia = nodes.find(e.a);
        auto ib = nodes.find(e.b);
        if (ia == nodes.end() || ib == nodes.end()) continue;

        const ClothVec3 delta  = ib->second.position - ia->second.position;
        // length via dot product
        const float     len2   = delta.x*delta.x + delta.y*delta.y + delta.z*delta.z;
        if (len2 < 1e-12f) continue;
        const float     len    = std::sqrt(len2);
        const float     stretch = len - e.restLength;
        const float     fmag   = e.stiffness * stretch / len;
        const ClothVec3 fvec   = delta * fmag;

        if (ia->second.mass > 0.0f) ia->second.force += fvec;
        if (ib->second.mass > 0.0f) ib->second.force += (fvec * -1.0f);
    }

    // Integrate.
    std::size_t integrated = 0;
    for (auto& [id, n] : nodes) {
        if (n.mass == 0.0f) continue;
        const float invMass = 1.0f / n.mass;
        // gravity + accumulated force → acceleration
        const ClothVec3 acc = m_impl->grav + (n.force * invMass);
        n.velocity  += acc * fdt;
        n.position  += n.velocity * fdt;
        n.force      = {};
        ++integrated;
    }

    m_impl->time += dt;
    return ClothStepReport{true, m_impl->time, integrated};
}

ClothState ClothSolver::captureState() const {
    ClothState s;
    s.simulationTime = m_impl->time;
    s.nodes.reserve(m_impl->nodes.size());
    for (const auto& [id, n] : m_impl->nodes) {
        s.nodes.push_back({id, n.position, n.velocity});
    }
    // Sort for determinism.
    std::sort(s.nodes.begin(), s.nodes.end(),
              [](const ClothNodeSnapshot& a, const ClothNodeSnapshot& b){ return a.id < b.id; });
    return s;
}

bool ClothSolver::restoreState(const ClothState& state) {
    // Verify all snapshot node ids exist in solver.
    for (const auto& snap : state.nodes) {
        if (!m_impl->nodes.count(snap.id)) return false;
    }
    for (const auto& snap : state.nodes) {
        auto& n    = m_impl->nodes.at(snap.id);
        n.position = snap.position;
        n.velocity = snap.velocity;
        n.force    = {};
    }
    m_impl->time = state.simulationTime;
    return true;
}

} // namespace nexus
