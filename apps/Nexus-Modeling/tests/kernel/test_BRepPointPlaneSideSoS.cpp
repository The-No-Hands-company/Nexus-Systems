// Foundation — EXACT point-vs-triangle-plane side with Simulation-of-Simplicity
// (pointPlaneSideSoS). The first proven building block toward an exact ray-parity
// classifyPoint on curved shells: it returns a DEFINITE ±1 for any non-degenerate
// triangle — even when the query point is EXACTLY coplanar (orient3D == 0), where
// a plain exact test stalls — by a consistent symbolic point perturbation. It
// returns 0 only for a genuinely degenerate (zero-area) triangle.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/RobustPredicates.h>

#include <gtest/gtest.h>

#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
// Ground truth for the coplanar case: the sign of orient3D at the symbolically
// perturbed point p + (ε, ε², ε³) evaluated in double for a tiny concrete ε.
int perturbedSign(const Vec3& v0, const Vec3& v1, const Vec3& v2, const Vec3& p, double e)
{
    const Vec3 pd{static_cast<float>(p.x + e), static_cast<float>(p.y + e * e),
                  static_cast<float>(p.z + e * e * e)};
    const double o = nexus::geometry::RobustPredicates::orient3D(v0, v1, v2, pd);
    return o > 0.0 ? 1 : (o < 0.0 ? -1 : 0);
}
}  // namespace

// For a point clearly OFF the plane, the SoS side is exactly orient3D's sign.
TEST(PointPlaneSideSoS, MatchesOrient3DOffPlane)
{
    const Vec3 v0{0, 0, 0}, v1{4, 0, 0}, v2{0, 4, 0};  // z=0 plane
    EXPECT_EQ(pointPlaneSideSoS(v0, v1, v2, {1, 1, 5}),
              nexus::geometry::RobustPredicates::orient3D(v0, v1, v2, {1, 1, 5}) > 0 ? 1 : -1);
    EXPECT_EQ(pointPlaneSideSoS(v0, v1, v2, {1, 1, -5}),
              nexus::geometry::RobustPredicates::orient3D(v0, v1, v2, {1, 1, -5}) > 0 ? 1 : -1);
    // The two opposite sides must get opposite signs.
    EXPECT_EQ(pointPlaneSideSoS(v0, v1, v2, {1, 1, 5}),
              -pointPlaneSideSoS(v0, v1, v2, {1, 1, -5}));
}

// A point EXACTLY on a non-degenerate triangle's plane (orient3D == 0) gets a
// DEFINITE ±1 that matches the symbolic-perturbation limit — axis-aligned and
// tilted planes, at small and large float32-exact coordinates.
TEST(PointPlaneSideSoS, CoplanarResolvedToPerturbationLimit)
{
    struct Case { Vec3 v0, v1, v2, p; };
    const std::vector<Case> cases = {
        {{0, 0, 0}, {4, 0, 0}, {0, 4, 0}, {1, 1, 0}},           // z=0, interior
        {{0, 0, 0}, {4, 0, 0}, {0, 4, 0}, {2, 2, 0}},           // on the hypotenuse
        {{7, 0, -3}, {0, 7, -5}, {-7, 0, 3}, {0, 0, 0}},        // tilt 3x+5y+7z=0, origin
        {{7, 0, -3}, {0, 7, -5}, {-7, 0, 3}, {7, 0, -3}},       // a vertex (still on plane)
        // Large float32-exact integer coords on the same tilted plane.
        {{7000000, 0, -3000000}, {0, 7000000, -5000000}, {-7000000, 0, 3000000}, {0, 0, 0}},
    };
    for (const Case& c : cases) {
        // orient3D must actually be zero here (p really is coplanar).
        ASSERT_EQ(nexus::geometry::RobustPredicates::orient3D(c.v0, c.v1, c.v2, c.p), 0.0);
        const int s = pointPlaneSideSoS(c.v0, c.v1, c.v2, c.p);
        EXPECT_NE(s, 0);  // definite — never ambiguous for a real triangle
        // Matches the perturbation limit, stable across shrinking ε.
        EXPECT_EQ(s, perturbedSign(c.v0, c.v1, c.v2, c.p, 1e-3));
        EXPECT_EQ(s, perturbedSign(c.v0, c.v1, c.v2, c.p, 1e-5));
    }
}

// Only a genuinely degenerate (zero-area / collinear) triangle yields 0.
TEST(PointPlaneSideSoS, DegenerateTriangleReturnsZero)
{
    EXPECT_EQ(pointPlaneSideSoS({0, 0, 0}, {1, 1, 1}, {2, 2, 2}, {5, 5, 0}), 0);  // collinear
    EXPECT_EQ(pointPlaneSideSoS({0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {1, 2, 3}), 0);  // all coincident
    // A non-degenerate triangle NEVER returns 0, on-plane or off.
    const Vec3 v0{0, 0, 0}, v1{3, 0, 0}, v2{0, 5, 0};
    EXPECT_NE(pointPlaneSideSoS(v0, v1, v2, {1, 1, 0}), 0);
    EXPECT_NE(pointPlaneSideSoS(v0, v1, v2, {1, 1, 2}), 0);
}

// The virtual perturbation is CONSISTENT: the same query point resolves
// deterministically, and independently, against different coplanar triangles.
TEST(PointPlaneSideSoS, ConsistentAndDeterministic)
{
    const Vec3 v0{0, 0, 0}, v1{4, 0, 0}, v2{0, 4, 0};
    const Vec3 p{1, 1, 0};
    const int a = pointPlaneSideSoS(v0, v1, v2, p);
    EXPECT_EQ(a, pointPlaneSideSoS(v0, v1, v2, p));  // deterministic

    // The SAME p is coplanar with a second, differently-wound triangle; each is
    // resolved to its own definite sign (opposite winding → opposite side).
    const int flipped = pointPlaneSideSoS(v0, v2, v1, p);  // reversed winding
    EXPECT_NE(flipped, 0);
    EXPECT_EQ(flipped, -a);  // flipping the triangle's orientation flips the side
}

}  // namespace nexus::geometry::brep::testing
