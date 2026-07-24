#include <gtest/gtest.h>

#include <nexus/geometry/NurbsCurveClosestPoint.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static NurbsCurve makeLineCurve()
{
    return NurbsCurve(
        1,
        {0.f, 0.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f},
            Vec3{10.f, 0.f, 0.f},
        });
}

// Exact rational unit circle in the xy-plane. On a curve, Newton roots f(u) =
// <C'(u), C(u)-Q>, so a wrong tangent direction converges to the wrong parameter — which is
// exactly what the previous curve derivative (about 70 degrees off on a circle) produced.
static NurbsCurve makeUnitCircle()
{
    const float s = 1.f / std::sqrt(2.f);
    NurbsCurve c(
        2,
        {0.f, 0.f, 0.f, 0.25f, 0.25f, 0.5f, 0.5f, 0.75f, 0.75f, 1.f, 1.f, 1.f},
        {
            Vec3{1.f, 0.f, 0.f}, Vec3{1.f, 1.f, 0.f}, Vec3{0.f, 1.f, 0.f},
            Vec3{-1.f, 1.f, 0.f}, Vec3{-1.f, 0.f, 0.f}, Vec3{-1.f, -1.f, 0.f},
            Vec3{0.f, -1.f, 0.f}, Vec3{1.f, -1.f, 0.f}, Vec3{1.f, 0.f, 0.f},
        });
    c.setWeights({1.f, s, 1.f, s, 1.f, s, 1.f, s, 1.f});
    return c;
}

TEST(NurbsCurveClosestPoint, PointOnCurveReturnsSame)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {5.f, 0.f, 0.f});
    EXPECT_NEAR(result.distance, 0.f, 1e-5f);
    EXPECT_NEAR(result.param, 0.5f, 1e-4f);
    EXPECT_NEAR(result.point.x, 5.f, 1e-4f);
    EXPECT_NEAR(result.point.y, 0.f, 1e-4f);
}

TEST(NurbsCurveClosestPoint, ExternalPointProjectsRadiallyOntoCircle)
{
    auto curve = makeUnitCircle();
    ASSERT_TRUE(curve.isValid());

    NurbsCurveClosestPointOptions opts;
    opts.samples = 64;  // dense enough to seed Newton in the correct span at any angle

    // For any point outside the unit circle the closest point is the radial projection
    // Q/|Q|, at distance |Q| - 1. A tangent that is off in direction converges Newton to a
    // different arc parameter, so the projected point lands off the radial line.
    const float queries[][2] = {
        {3.f, 0.f}, {0.f, 3.f}, {-3.f, 0.f}, {0.f, -3.f},
        {2.f, 2.f}, {-2.f, 1.5f}, {1.5f, -2.5f}, {-1.8f, -1.2f},
    };
    for (const auto& q : queries) {
        const float len = std::sqrt(q[0] * q[0] + q[1] * q[1]);
        auto result = NurbsCurveClosestPoint::project(curve, {q[0], q[1], 0.f}, opts);
        EXPECT_NEAR(result.point.x, q[0] / len, 2e-3f) << "q=(" << q[0] << "," << q[1] << ")";
        EXPECT_NEAR(result.point.y, q[1] / len, 2e-3f) << "q=(" << q[0] << "," << q[1] << ")";
        EXPECT_NEAR(result.distance, len - 1.f, 2e-3f)  << "q=(" << q[0] << "," << q[1] << ")";
    }
}

TEST(NurbsCurveClosestPoint, PointAboveLineReturnsCorrectProjection)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {5.f, 2.f, 0.f});
    EXPECT_NEAR(result.point.x, 5.f, 1e-4f);
    EXPECT_NEAR(result.point.y, 0.f, 1e-4f);
    EXPECT_NEAR(result.distance, 2.f, 1e-3f);
}

TEST(NurbsCurveClosestPoint, PointBeforeStartSnapsToEndpoint)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {-3.f, 0.f, 0.f});
    EXPECT_NEAR(result.param, 0.f, 1e-3f);
    EXPECT_NEAR(result.point.x, 0.f, 1e-3f);
}

TEST(NurbsCurveClosestPoint, PointAfterEndSnapsToEndpoint)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {13.f, 0.f, 0.f});
    EXPECT_NEAR(result.param, 1.f, 1e-3f);
    EXPECT_NEAR(result.point.x, 10.f, 1e-3f);
}

TEST(NurbsCurveClosestPoint, WorksWithDefaultParameters)
{
    auto curve = makeLineCurve();
    auto result = NurbsCurveClosestPoint::project(curve, {7.f, 1.f, 0.f});
    EXPECT_TRUE(result.converged);
    EXPECT_GT(result.distance, 0.f);
    EXPECT_TRUE(std::isfinite(result.param));
    EXPECT_TRUE(std::isfinite(result.point.x));
}

} // namespace
