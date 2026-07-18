// Foundation — EXACT degeneracy-free segment-vs-triangle crossing with
// Simulation-of-Simplicity (segmentCrossesTriangleSoS). SoS step 2: the ray-parity
// core for an exact classifyPoint. The query point p is symbolically perturbed by
// (ε,ε²,ε³) so NO test is ever ambiguous — a ray exactly through an edge/vertex,
// or a p exactly coplanar with a (pole) triangle, resolves to the perturbation
// limit. Crucially, a ray through a SHARED edge of two triangles is counted for
// EXACTLY ONE of them, so parity stays watertight.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/RobustPredicates.h>

#include <gtest/gtest.h>

#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
// Ground truth: evaluate the whole crossing at the concretely-perturbed endpoints
// p+(ε,ε²,ε³), B+(ε,ε²,ε³) for a tiny ε — the limit segmentCrossesTriangleSoS models.
bool bruteCross(const Vec3& p, const Vec3& B, const Vec3& v0, const Vec3& v1, const Vec3& v2,
                double e)
{
    using RP = nexus::geometry::RobustPredicates;
    const Vec3 A{static_cast<float>(p.x + e), static_cast<float>(p.y + e * e),
                 static_cast<float>(p.z + e * e * e)};
    const Vec3 Bp{static_cast<float>(B.x + e), static_cast<float>(B.y + e * e),
                  static_cast<float>(B.z + e * e * e)};
    const double oa = RP::orient3D(v0, v1, v2, A), ob = RP::orient3D(v0, v1, v2, Bp);
    if (oa == 0.0 || ob == 0.0) return false;
    if ((oa > 0.0) == (ob > 0.0)) return false;
    const double e0 = RP::orient3D(A, Bp, v0, v1), e1 = RP::orient3D(A, Bp, v1, v2),
                 e2 = RP::orient3D(A, Bp, v2, v0);
    if (e0 == 0.0 || e1 == 0.0 || e2 == 0.0) return false;
    const bool p0 = e0 > 0.0, p1 = e1 > 0.0, p2 = e2 > 0.0;
    return p0 == p1 && p1 == p2;
}
}  // namespace

// The SoS crossing matches the true perturbation limit on clean AND degenerate
// rays (through a vertex, an edge, the hypotenuse; a coplanar query point).
TEST(SegmentTriangleSoS, MatchesPerturbationLimit)
{
    const Vec3 a{0, 0, 0}, b{4, 0, 0}, c{0, 4, 0};  // triangle in z=0
    struct Case { const char* name; Vec3 p, B; };
    const std::vector<Case> cases = {
        {"clean-through", {1, 1, 5}, {1, 1, -5}},
        {"clean-miss", {3, 3, 5}, {3, 3, -5}},
        {"through-vertex", {0, 0, 5}, {0, 0, -5}},        // ray exactly through v0
        {"through-edge-mid", {2, 0, 5}, {2, 0, -5}},      // ray through edge v0-v1
        {"through-hypotenuse", {2, 2, 5}, {2, 2, -5}},    // ray through the far edge
        {"p-coplanar-inside", {1, 1, 0}, {1, 1, -5}},     // p exactly on the plane, interior
        {"p-coplanar-outside", {3, 3, 0}, {3, 3, -5}},    // p on the plane, outside
    };
    for (const Case& cs : cases) {
        const bool sos = segmentCrossesTriangleSoS(cs.p, cs.B, a, b, c);
        EXPECT_EQ(sos, bruteCross(cs.p, cs.B, a, b, c, 1e-3)) << cs.name;
        EXPECT_EQ(sos, bruteCross(cs.p, cs.B, a, b, c, 1e-6)) << cs.name;  // stable as ε shrinks
    }
}

// The pole-degeneracy that stalls the plain exact test: a query point coplanar
// with a triangle whose plane passes through it → SoS still returns a definite,
// perturbation-matching answer (this is what makes classifyPoint on curved shells
// possible).
TEST(SegmentTriangleSoS, PoleLikeCoplanarTriangleResolved)
{
    const Vec3 t0{0, 0, -1}, t1{2, 0, -0.5f}, t2{1, 2, -0.5f};
    const Vec3 p{0, 0, 0}, B{0, 0, 10};  // ray up the z-axis from a point on t's plane region
    const bool sos = segmentCrossesTriangleSoS(p, B, t0, t1, t2);
    EXPECT_EQ(sos, bruteCross(p, B, t0, t1, t2, 1e-6));
}

// THE watertight-parity property: a ray passing exactly through the SHARED edge of
// two adjacent triangles is counted for EXACTLY ONE of them (never 0, never 2) —
// so the SoS perturbation keeps the odd/even parity count correct at seams.
TEST(SegmentTriangleSoS, SharedEdgeCountedExactlyOnce)
{
    // Two triangles of a quad in z=0 sharing edge v0-v1 (opposite winding on it).
    const Vec3 v0{0, 0, 0}, v1{4, 0, 0}, v2{2, 2, 0}, v3{2, -2, 0};
    // Vertical ray straight through a point ON the shared edge.
    const Vec3 p{2, 0, 5}, B{2, 0, -5};
    const bool a = segmentCrossesTriangleSoS(p, B, v0, v1, v2);  // upper tri
    const bool b = segmentCrossesTriangleSoS(p, B, v1, v0, v3);  // lower tri (shared edge reversed)
    EXPECT_NE(a, b);  // exactly one — the ray is on one definite side of the seam
}

// A genuinely degenerate (zero-area) triangle contributes nothing to parity.
TEST(SegmentTriangleSoS, DegenerateTriangleNoCrossing)
{
    const Vec3 a{0, 0, 0}, b{2, 2, 2}, c{4, 4, 4};  // collinear
    EXPECT_FALSE(segmentCrossesTriangleSoS({1, 1, 5}, {1, 1, -5}, a, b, c));
}

TEST(SegmentTriangleSoS, Deterministic)
{
    const Vec3 a{0, 0, 0}, b{4, 0, 0}, c{0, 4, 0};
    const Vec3 p{2, 0, 5}, B{2, 0, -5};  // a degenerate (through-edge) ray
    EXPECT_EQ(segmentCrossesTriangleSoS(p, B, a, b, c), segmentCrossesTriangleSoS(p, B, a, b, c));
}

}  // namespace nexus::geometry::brep::testing
