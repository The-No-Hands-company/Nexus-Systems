#pragma once
// ─── Nexus Geometry ── ProfileTool ────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

class ProfileTool {
public:
    [[nodiscard]] static std::vector<Vec3> extractProfile(const Mesh& m,
                                                           const Vec3& planePoint,
                                                           const Vec3& planeNormal);

    [[nodiscard]] static Mesh profileToCurve(const std::vector<Vec3>& profile);

    [[nodiscard]] static std::vector<std::vector<Vec3>> extractAllProfiles(const Mesh& m,
                                                                            const Vec3& axis);
};

} // namespace nexus::geometry
