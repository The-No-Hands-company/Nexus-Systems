#pragma once
// ─── Nexus Geometry ── SectionTool ────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

class SectionTool {
public:
    [[nodiscard]] static std::vector<Vec3> computeSection(const Mesh& m,
                                                           const Vec3& planePoint,
                                                           const Vec3& planeNormal);

    [[nodiscard]] static Mesh sectionMesh(const std::vector<Vec3>& section);

    [[nodiscard]] static std::vector<std::vector<Vec3>> multiSection(const Mesh& m,
                                                                      const Vec3& origin,
                                                                      const Vec3& axis,
                                                                      uint32_t count,
                                                                      float spacing);
};

} // namespace nexus::geometry
