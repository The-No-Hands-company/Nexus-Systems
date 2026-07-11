#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Triangle/Triangle intersection SEGMENT (robust)
//
//  Computes the segment where two triangles cross — not merely a yes/no test.
//  All topological decisions (which edges cross the other triangle's plane) use
//  RobustPredicates::orient3D (adaptive-exact), so the intersection *structure*
//  is exact even though the returned endpoint coordinates are floating-point.
//
//  This is the core primitive for a robust mesh boolean: intersection segments
//  are what the two surfaces get retriangulated along (vs. the current cosmetic
//  whole-triangle centroid classification). Coplanar overlap (a 2D polygon, not
//  a segment) is reported as Coplanar and handled by a later stage.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>  // nexus::render::Vec3

#include <cstdint>

namespace nexus::geometry {

struct TriTriResult {
    enum class Kind : uint8_t { None, Segment, Coplanar };
    Kind                kind = Kind::None;
    nexus::render::Vec3 p0{};  // segment endpoints, valid when kind == Segment
    nexus::render::Vec3 p1{};
};

class TriTriIntersect {
public:
    // Robust triangle-triangle intersection. Returns the crossing segment, or
    // None (disjoint / touching only at a vertex), or Coplanar. Rejects
    // non-finite inputs (returns None).
    [[nodiscard]] static TriTriResult intersect(
        const nexus::render::Vec3& a0, const nexus::render::Vec3& a1, const nexus::render::Vec3& a2,
        const nexus::render::Vec3& b0, const nexus::render::Vec3& b1, const nexus::render::Vec3& b2) noexcept;
};

} // namespace nexus::geometry
