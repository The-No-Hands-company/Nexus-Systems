#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — analytic B-rep surface/surface intersection
//
//  Exact intersection of two analytic brep::Surface, returned as an analytic
//  brep::Curve. This is the geometric core the B-rep boolean's imprint step
//  needs: where two solids' faces meet, the intersection is a known analytic
//  curve (a line where two planes cross, a circle where a plane cuts a sphere,
//  etc.) rather than a sampled polyline. NURBS/freeform pairs fall back to
//  Unsupported (wired to the general NURBS SSI toolkit later).
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/AnalyticBRep.h>

namespace nexus::geometry::brep {

enum class SurfaceIntersectionKind : uint8_t {
    None,        // surfaces do not meet
    Point,       // tangency
    Line,        // `curve` is a Line
    Circle,      // `curve` is a Circle
    TwoLines,    // `curve` and `curve2` are Lines
    Unsupported  // not handled analytically (e.g. skew cylinder∩plane, NURBS)
};

struct SurfaceIntersection {
    SurfaceIntersectionKind kind = SurfaceIntersectionKind::None;
    Curve curve;    // Line / Circle (first branch)
    Curve curve2;   // second Line for TwoLines
    Vec3  point{};  // tangency point (Point)
};

// Analytic intersection of two surfaces (plane / sphere / cylinder-perp-to-axis
// / sphere∩sphere). Handles either argument order.
[[nodiscard]] SurfaceIntersection intersectSurfaces(const Surface& a, const Surface& b,
                                                    Tolerance tol = {});

// Signed implicit distance from a point to an analytic surface (0 on-surface):
// plane = dot(p-origin, normal); sphere = |p-center|-R; cylinder = axial
// radial-distance - R. Used to verify intersection curves lie on both surfaces.
[[nodiscard]] float surfaceDistance(const Surface& s, const Vec3& p);

}  // namespace nexus::geometry::brep
