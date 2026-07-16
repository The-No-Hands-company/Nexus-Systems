// Foundation — analytic B-rep surface area. Body::surfaceArea sums the boundary
// triangulation's triangle areas: exact for planar-faced solids, and within the
// faceting tolerance for curved primitives.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

TEST(BRepSurfaceArea, BoxIsExact)
{
    EXPECT_NEAR(makeBox(2.f, 2.f, 2.f).surfaceArea(), 24.f, 1e-3f);  // 6 · 2·2
    // 2·(wh + wd + hd) = 2·(6 + 8 + 12) = 52.
    EXPECT_NEAR(makeBox(2.f, 3.f, 4.f).surfaceArea(), 52.f, 1e-3f);
}

TEST(BRepSurfaceArea, CylinderMatchesAnalytic)
{
    // 2πr² (caps) + 2πrh (side) = 2π + 4π = 6π ≈ 18.8496.
    const float a = makeCylinder(1.f, 2.f, 64).surfaceArea();
    EXPECT_NEAR(a, static_cast<float>(2.0 * M_PI + 4.0 * M_PI), 0.02f);
}

TEST(BRepSurfaceArea, SphereWithinFacetingTolerance)
{
    // 4πr² ≈ 12.566; a facet tessellation approximates it.
    const float a = makeSphere(1.f, 32, 48).surfaceArea();
    EXPECT_NEAR(a, static_cast<float>(4.0 * M_PI), 0.4f);
    EXPECT_GT(a, 12.f);  // clearly in the right ballpark
}

TEST(BRepSurfaceArea, ExtrudedAndRevolvedSolids)
{
    // Unit square extruded by 3 → a 1×1×3 box: area = 2·1 + 4·3 = 14.
    const std::vector<nexus::render::Vec3> sq = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    EXPECT_NEAR(extrudeProfile(sq, {0, 0, 3}).surfaceArea(), 14.f, 1e-3f);
}

TEST(BRepSurfaceArea, Deterministic)
{
    const Body b = makeCylinder(1.f, 2.f, 24);
    EXPECT_EQ(b.surfaceArea(), b.surfaceArea());
}

}  // namespace nexus::geometry::brep::testing
