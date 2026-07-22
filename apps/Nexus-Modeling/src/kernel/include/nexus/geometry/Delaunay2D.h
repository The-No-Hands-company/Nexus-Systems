#pragma once
// --- Nexus Geometry — Delaunay2D

#include <nexus/geometry/Mesh.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

struct DelaunayResult {
    bool ok = false;
    std::string error;
    std::vector<std::array<uint32_t, 3>> triangles;
    std::vector<Vec2> vertices;
};

class Delaunay2D {
public:
    [[nodiscard]] static DelaunayResult triangulate(const std::vector<Vec2>& points);

private:
    // One Bowyer-Watson pass with the super-triangle sized at `scale` times the input's
    // bounding-box extent. `triangulate` retries at larger scales until the result tiles
    // the input's convex hull. See src/geometry/DelaunaySuperTriangle.h.
    [[nodiscard]] static DelaunayResult triangulateAtScale(const std::vector<Vec2>& points,
                                                           float scale);
};

} // namespace nexus::geometry
