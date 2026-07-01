#pragma once
// ─── Nexus Geometry ── SnappingEngine ──────────────────────────────────────
//  Vertex, edge, face, grid, and volume snapping with spatial index.
//  Rebuilds incrementally on mesh deformation.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

enum class SnapTarget : uint8_t {
    None,
    Vertex,
    Edge,
    Face,
    Grid,
    Volume,
};

struct SnapResult {
    SnapTarget target = SnapTarget::None;
    Vec3 position;
    Vec3 normal;
    float distance = std::numeric_limits<float>::max();
};

struct SnappingConfig {
    SnapTarget targets = SnapTarget::Vertex;
    float vertexSnapRadius = 0.1f;
    float edgeSnapRadius = 0.1f;
    float faceSnapRadius = 0.1f;
    float gridSpacing = 1.0f;
    bool enableGrid = false;
    uint32_t maxCandidates = 16;
    uint32_t kdSearchK = 8;
};

class SnappingEngine {
public:
    using Vec3 = nexus::render::Vec3;

    void build(const Mesh& mesh);
    void rebuild(const Mesh& mesh);

    [[nodiscard]] SnapResult snap(const Vec3& position,
                                   const Vec3& normal = {0, 1, 0}) const;

    [[nodiscard]] std::vector<SnapResult> snapCandidates(
        const Vec3& position, uint32_t maxCount = 8) const;

    [[nodiscard]] Vec3 snapToGrid(const Vec3& position) const;

    [[nodiscard]] const MeshBVH& bvh() const noexcept;
    [[nodiscard]] bool isBuilt() const noexcept;

    void setConfig(const SnappingConfig& config) noexcept;
    [[nodiscard]] const SnappingConfig& config() const noexcept;

private:
    MeshBVH m_bvh;
    SnappingConfig m_config;
    bool m_built = false;
};

} // namespace nexus::geometry
