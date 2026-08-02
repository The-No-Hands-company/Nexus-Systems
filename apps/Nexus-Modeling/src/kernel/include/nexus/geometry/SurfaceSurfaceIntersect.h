#pragma once
// ── Nexus Geometry — SurfaceSurfaceIntersect

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct IntersectSeedGrid {
    int32_t resU = 16;
    int32_t resV = 16;
};

// Numerical intersection of two NURBS surfaces, returned as one polyline per branch.
//
// Seeds are found on `seedGrid`, Newton-refined onto both surfaces, then marched along
// the curve in BOTH directions until it closes, leaves a domain, or reaches a tangency.
// Branches already covered by an earlier trace are skipped, so a single intersection
// curve is returned once.
//
// `marchSteps` and `stepSize` are budgets, and a NON-POSITIVE value means "choose one
// proportional to the model" — which is the default, because a fixed step can only be
// right for a surface of about the size it was tuned on. `stepSize` is an arc length in
// MODEL units, not a parameter increment.
class SurfaceSurfaceIntersect {
public:
    using Vec3 = nexus::render::Vec3;

    [[nodiscard]] static std::vector<std::vector<Vec3>> intersect(
        const NurbsSurface& a,
        const NurbsSurface& b,
        const IntersectSeedGrid& seedGrid = {},
        int32_t marchSteps = 0,
        float stepSize = 0.f);
};

} // namespace nexus::geometry
