#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Per-triangle retriangulation along intersection segments
//
//  Given one triangle and a set of intersection segments lying on its plane
//  (produced by TriTriIntersect), split the triangle into sub-triangles that
//  respect those segments as constrained edges. Uses ConstrainedDelaunay2D in
//  the triangle's own plane basis, then lifts the result back to 3D.
//
//  This is boolean-rebuild increment 2: the retriangulated surface is what gets
//  classified (inside/outside) and stitched, giving a clean cut along the true
//  intersection curve instead of the current whole-triangle classification.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>  // nexus::render::Vec3

#include <array>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct IntersectionSegment {
    nexus::render::Vec3 p0{};
    nexus::render::Vec3 p1{};
};

struct RetriangulationResult {
    std::vector<nexus::render::Vec3>    points;
    std::vector<std::array<uint32_t, 3>> triangles;  // index into points
};

class TriangleRetriangulate {
public:
    // Splits triangle (a,b,c) along the given segments. With no segments (or a
    // degenerate triangle, or on CDT failure) returns the single input triangle.
    // Non-finite inputs are rejected (fall back to the single triangle / skipped
    // segments). The sub-triangles tile the original triangle (area-preserving).
    [[nodiscard]] static RetriangulationResult apply(
        const nexus::render::Vec3& a, const nexus::render::Vec3& b, const nexus::render::Vec3& c,
        const std::vector<IntersectionSegment>& segments) noexcept;
};

} // namespace nexus::geometry
