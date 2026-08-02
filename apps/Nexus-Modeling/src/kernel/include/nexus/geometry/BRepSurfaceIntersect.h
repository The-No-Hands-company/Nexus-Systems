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
    TwoCircles,  // `curve` and `curve2` are Circles — a sphere centred on a cylinder's
                 // axis meets it in TWO rings, one either side of the sphere's centre
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

// ── Numerical surface/surface intersection (the quartic cases) ───────────────
//
// intersectSurfaces above is the ANALYTIC path: it answers only where the section is
// exactly a Line or a Circle, which is to say where the configuration is axially
// symmetric, and says Unsupported everywhere else. Everything else is a quartic space
// curve with no closed form in this vocabulary — a sphere met by an off-axis cylinder,
// two cylinders with crossing axes, a cone against anything curved and off its axis.
//
// Measured over 3592 chained boolean steps, those declines are 35.2% of all outcomes and
// roughly five times the next-largest gap, which is what makes a numerical tracer worth
// building rather than deferring again.
//
// The tracer produces a POLYLINE on both surfaces rather than an analytic curve, because
// that is what a quartic honestly is here. It is deliberately kept separate from the
// brep::Curve store for now: this is the geometric core, verified on its own against the
// closed forms it must reproduce, before anything in the imprint or the sew depends on it.

// One branch of an intersection. An SSI can produce several disjoint ones (two cylinders
// crossing at equal radius meet in two ellipses; a sphere on a cylinder's axis in two
// rings), so a branch is not the whole answer.
struct TracedBranch {
    std::vector<Vec3> points;  // consecutive samples, chord-to-curve sag under tolerance
    bool closed = false;       // the trace returned to its start rather than leaving the box
};

struct SurfaceTrace {
    std::vector<TracedBranch> branches;
    bool ok = false;  // every branch terminated cleanly (closed, or ran out of the box)
};

// Trace the intersection of two analytic surfaces numerically, restricted to the axis-
// aligned box [lo, hi] (an unbounded surface pair otherwise has an unbounded intersection).
// Seeds are found on a deterministic lattice and Newton-projected onto BOTH surfaces; each
// branch is then marched along the tangent (gradA × gradB) with the step halved wherever
// the chord's sag exceeds tolerance. Fully deterministic: no randomness, no hash ordering.
//
// Tangency is the honest failure: where the two surfaces touch rather than cross, the
// gradients are parallel, the tangent direction is undefined and the trace declines that
// branch rather than inventing a direction for it.
[[nodiscard]] SurfaceTrace traceSurfaceIntersection(const Surface& a, const Surface& b,
                                                    const Vec3& lo, const Vec3& hi,
                                                    Tolerance tol = {});

}  // namespace nexus::geometry::brep
