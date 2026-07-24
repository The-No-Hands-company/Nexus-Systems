#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurfaceClosestPoint.h>

#include <cmath>
#include <limits>

namespace {

using namespace nexus::geometry;

static NurbsSurface makeFlatSurface()
{
    return NurbsSurface(
        2, 2,
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {
            Vec3{0.f, 0.f, 0.f}, Vec3{0.f, 1.f, 0.f}, Vec3{0.f, 2.f, 0.f},
            Vec3{1.f, 0.f, 0.f}, Vec3{1.f, 1.f, 0.f}, Vec3{1.f, 2.f, 0.f},
            Vec3{2.f, 0.f, 0.f}, Vec3{2.f, 1.f, 0.f}, Vec3{2.f, 2.f, 0.f},
        },
        3, 3);
}

// Exact quarter-cylinder (unit radius, height 1, rational quarter circle in v). Surface
// closest-point runs a 2D Newton on the u/v partials, so a wrong partial lands the
// projection on the wrong point of a curved surface.
static NurbsSurface makeQuarterCylinder()
{
    const float s = 1.f / std::sqrt(2.f);
    return NurbsSurface(
        1, 2,
        {0.f, 0.f, 1.f, 1.f},
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {
            Vec3{1.f, 0.f, 0.f}, Vec3{1.f, 1.f, 0.f}, Vec3{0.f, 1.f, 0.f},
            Vec3{1.f, 0.f, 1.f}, Vec3{1.f, 1.f, 1.f}, Vec3{0.f, 1.f, 1.f},
        },
        2, 3,
        {1.f, s, 1.f, 1.f, s, 1.f});
}

TEST(NurbsSurfaceClosestPoint, PointOnSurfaceReturnsItself)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    Vec3 onSurf = surface.evaluate(0.5f, 0.5f);
    auto result = NurbsSurfaceClosestPoint::project(surface, onSurf);

    EXPECT_NEAR(result.distance, 0.f, 1e-3f);
    EXPECT_NEAR(result.point.x, onSurf.x, 1e-3f);
    EXPECT_NEAR(result.point.y, onSurf.y, 1e-3f);
    EXPECT_NEAR(result.point.z, onSurf.z, 1e-3f);
}

TEST(NurbsSurfaceClosestPoint, PointAbovePlaneReturnsCorrectProjection)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    Vec3 query{1.f, 1.f, 3.f};
    auto result = NurbsSurfaceClosestPoint::project(surface, query);

    EXPECT_NEAR(result.point.z, 0.f, 1e-3f);
    EXPECT_NEAR(result.distance, 3.f, 1e-2f);
    EXPECT_TRUE(result.converged);
}

TEST(NurbsSurfaceClosestPoint, ExternalPointProjectsRadiallyOntoCylinder)
{
    auto surface = makeQuarterCylinder();
    ASSERT_TRUE(surface.isValid());

    NurbsSurfaceClosestPointOptions opts;
    opts.gridSizeU = 16;
    opts.gridSizeV = 16;

    // A point outside the cylinder, with its angle inside the first-quadrant arc and its
    // height inside [0,1], projects to (xy/|xy|, z): radial in xy, unchanged in z.
    const float queries[][3] = {
        {2.f, 2.f, 0.5f}, {3.f, 0.6f, 0.3f}, {0.6f, 3.f, 0.75f}, {2.5f, 1.2f, 0.2f},
    };
    for (const auto& q : queries) {
        const float rxy = std::sqrt(q[0] * q[0] + q[1] * q[1]);
        auto result = NurbsSurfaceClosestPoint::project(surface, {q[0], q[1], q[2]}, opts);
        EXPECT_NEAR(result.point.x, q[0] / rxy, 3e-3f) << "q=(" << q[0] << "," << q[1] << ")";
        EXPECT_NEAR(result.point.y, q[1] / rxy, 3e-3f) << "q=(" << q[0] << "," << q[1] << ")";
        EXPECT_NEAR(result.point.z, q[2], 3e-3f)       << "q=(" << q[0] << "," << q[1] << ")";
        EXPECT_NEAR(result.distance, rxy - 1.f, 3e-3f) << "q=(" << q[0] << "," << q[1] << ")";
    }
}

TEST(NurbsSurfaceClosestPoint, EmptySurfaceFails)
{
    NurbsSurface empty;
    EXPECT_FALSE(empty.isValid());

    auto result = NurbsSurfaceClosestPoint::project(empty, {0.f, 0.f, 0.f});
    EXPECT_FALSE(result.converged);
    EXPECT_FLOAT_EQ(result.distance, 0.f);
}

TEST(NurbsSurfaceClosestPoint, NonFiniteQueryFails)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    float nan = std::numeric_limits<float>::quiet_NaN();
    Vec3 query{nan, 0.f, 0.f};
    auto result = NurbsSurfaceClosestPoint::project(surface, query);

    EXPECT_TRUE(std::isnan(result.distance) || result.distance > 1e6f || !result.converged);
}

TEST(NurbsSurfaceClosestPoint, ConvergesToCornerForFarAwayPoint)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    Vec3 farPoint{0.f, 0.f, 100.f};
    auto result = NurbsSurfaceClosestPoint::project(surface, farPoint);

    EXPECT_NEAR(result.point.x, 0.f, 1e-4f);
    EXPECT_NEAR(result.point.y, 0.f, 1e-4f);
    EXPECT_NEAR(result.point.z, 0.f, 1e-4f);
}

TEST(NurbsSurfaceClosestPoint, DefaultOptionsWork)
{
    auto surface = makeFlatSurface();
    ASSERT_TRUE(surface.isValid());

    NurbsSurfaceClosestPointOptions opts;
    Vec3 query{1.f, 1.f, 5.f};
    auto result = NurbsSurfaceClosestPoint::project(surface, query, opts);

    EXPECT_GT(result.iterations, 0);
    EXPECT_TRUE(result.converged);
    EXPECT_NEAR(result.point.z, 0.f, 1e-3f);
}

} // namespace
