// Foundation — analytic surface/surface intersection (the geometric core of the
// B-rep boolean's imprint step). Each intersection is returned as an exact
// brep::Curve; these tests verify the curve lies on BOTH input surfaces (via the
// implicit surfaceDistance) and that non-intersecting configurations return None.

#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
Surface plane(Vec3 o, Vec3 n) { Surface s; s.kind = SurfaceKind::Plane; s.origin = o; s.normal = n; return s; }
Surface sphere(Vec3 c, float r) { Surface s; s.kind = SurfaceKind::Sphere; s.origin = c; s.radius = r; return s; }
Surface cylinder(Vec3 o, Vec3 ax, float r) { Surface s; s.kind = SurfaceKind::Cylinder; s.origin = o; s.normal = ax; s.radius = r; return s; }

// Max distance of the intersection curve (sampled) to both surfaces.
float curveOnBoth(const SurfaceIntersection& r, const Surface& a, const Surface& b)
{
    float maxd = 0.f;
    const bool circle = (r.kind == SurfaceIntersectionKind::Circle);
    for (int i = 0; i < 24; ++i) {
        const float t = circle ? (6.2831853f * static_cast<float>(i) / 24.f)
                               : static_cast<float>(i - 12);
        const Vec3 p = r.curve.eval(t);
        maxd = std::max(maxd, std::max(std::abs(surfaceDistance(a, p)), std::abs(surfaceDistance(b, p))));
    }
    return maxd;
}
}  // namespace

TEST(BRepSurfaceIntersect, PlanePlaneIsLineOnBothPlanes)
{
    const Surface a = plane({0, 0, 0}, {0, 0, 1});
    const Surface b = plane({0, 0, 0}, {1, 0, 0});
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Line);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);
}

TEST(BRepSurfaceIntersect, ParallelPlanesDoNotMeet)
{
    EXPECT_EQ(intersectSurfaces(plane({0, 0, 0}, {0, 0, 1}), plane({0, 0, 3}, {0, 0, 1})).kind,
              SurfaceIntersectionKind::None);
}

TEST(BRepSurfaceIntersect, PlaneSphereIsCircleOnBoth)
{
    const Surface p = plane({0, 0, 0.5f}, {0, 0, 1});
    const Surface s = sphere({0, 0, 0}, 2.f);
    const auto r = intersectSurfaces(p, s);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_NEAR(r.curve.radius, std::sqrt(2.f * 2.f - 0.5f * 0.5f), 1e-4f);
    EXPECT_LT(curveOnBoth(r, p, s), 1e-4f);
    // Argument order is handled symmetrically.
    EXPECT_EQ(intersectSurfaces(s, p).kind, SurfaceIntersectionKind::Circle);
}

TEST(BRepSurfaceIntersect, PlaneMissingSphereReturnsNone)
{
    EXPECT_EQ(intersectSurfaces(plane({0, 0, 5}, {0, 0, 1}), sphere({0, 0, 0}, 2.f)).kind,
              SurfaceIntersectionKind::None);
}

TEST(BRepSurfaceIntersect, PlanePerpendicularToCylinderIsCircle)
{
    const Surface p = plane({0, 0, 1}, {0, 0, 1});
    const Surface c = cylinder({0, 0, 0}, {0, 0, 1}, 1.5f);
    const auto r = intersectSurfaces(p, c);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_NEAR(r.curve.radius, 1.5f, 1e-4f);
    EXPECT_LT(curveOnBoth(r, p, c), 1e-4f);
}

TEST(BRepSurfaceIntersect, SphereSphereIsCircleOnBoth)
{
    const Surface a = sphere({0, 0, 0}, 2.f);
    const Surface b = sphere({3, 0, 0}, 2.f);
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);
}

TEST(BRepSurfaceIntersect, DisjointSpheresReturnNone)
{
    EXPECT_EQ(intersectSurfaces(sphere({0, 0, 0}, 1.f), sphere({5, 0, 0}, 1.f)).kind,
              SurfaceIntersectionKind::None);
}

}  // namespace nexus::geometry::brep::testing
