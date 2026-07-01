#include <nexus/geometry/SnappingEngine.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>

namespace nexus::geometry {
using Vec3 = nexus::render::Vec3;

void SnappingEngine::build(const Mesh& mesh) {
    m_bvh.build(mesh);
    m_built = true;
}

void SnappingEngine::rebuild(const Mesh& mesh) {
    m_bvh.build(mesh);
    m_built = true;
}

SnapResult SnappingEngine::snap(const Vec3& position, const Vec3& normal) const {
    SnapResult best;
    best.distance = std::numeric_limits<float>::max();
    best.target = SnapTarget::None;

    if (!m_built) return best;

    auto dist2 = [](const Vec3& a, const Vec3& b) {
        float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
        return dx * dx + dy * dy + dz * dz;
    };

    // Vertex snap
    if (static_cast<uint8_t>(m_config.targets) & static_cast<uint8_t>(SnapTarget::Vertex)) {
        auto hit = m_bvh.closestPoint(position);
        if (hit.valid) {
            float d = std::sqrt(hit.distanceSquared);
            if (d < m_config.vertexSnapRadius && d < best.distance) {
                best.distance = d;
                best.target = SnapTarget::Vertex;
                best.position = hit.point;
            }
        }
    }

    (void)normal;

    // Grid snap
    if (m_config.enableGrid) {
        Vec3 snapped = snapToGrid(position);
        float d = std::sqrt(dist2(position, snapped));
        if (d < m_config.gridSpacing * 0.5f && d < best.distance) {
            best.distance = d;
            best.target = SnapTarget::Grid;
            best.position = snapped;
            best.normal = {0, 1, 0};
        }
    }

    return best;
}

std::vector<SnapResult> SnappingEngine::snapCandidates(
    const Vec3& position, uint32_t maxCount) const {
    std::vector<SnapResult> results;
    if (!m_built) return results;

    auto hit = m_bvh.closestPoint(position);
    if (hit.valid) {
        SnapResult r;
        r.target = SnapTarget::Vertex;
        r.position = hit.point;
        r.distance = std::sqrt(hit.distanceSquared);
        results.push_back(r);
    }

    (void)maxCount;
    return results;
}

Vec3 SnappingEngine::snapToGrid(const Vec3& position) const {
    float s = m_config.gridSpacing;
    if (s < 1e-10f) return position;
    return {
        std::round(position.x / s) * s,
        std::round(position.y / s) * s,
        std::round(position.z / s) * s,
    };
}

const MeshBVH& SnappingEngine::bvh() const noexcept { return m_bvh; }
bool SnappingEngine::isBuilt() const noexcept { return m_built; }
void SnappingEngine::setConfig(const SnappingConfig& config) noexcept { m_config = config; }
const SnappingConfig& SnappingEngine::config() const noexcept { return m_config; }

} // namespace nexus::geometry
