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

// A sphere is all curved faces, so its area is now integrated exactly over the parameter
// domain — 4*pi*r^2 to float precision, at ANY tessellation, where before it was tens of
// percent low at coarse counts. Even longitude counts, which keep every face off the seam.
TEST(BRepSurfaceArea, SphereIsExact)
{
    const double Around = 4.0 * M_PI;  // r = 1
    for (const uint32_t lat : {4u, 8u, 16u}) {
        for (const uint32_t lon : {6u, 12u, 24u}) {
            const float a = makeSphere(1.f, lat, lon).surfaceArea();
            EXPECT_NEAR(a, static_cast<float>(Around), Around * 1e-5)
                << "sphere area not exact at " << lat << "x" << lon;
        }
    }
}

// The cylinder's curved LATERAL surface is integrated exactly (2*pi*r*h, independent of
// segment count); its flat caps are triangulated, so they contribute their inscribed
// n-gon area rather than the round pi*r^2. The total therefore equals the exact round wall
// plus the exact n-gon caps — pinning that the wall is exact-round and only the planar
// caps facet (the same residual as the cylinder's transverse moment; exact planar-face-
// with-curved-boundary integration is a separate piece of work).
TEST(BRepSurfaceArea, CylinderLateralIsExactAndCapsAreInscribedPolygons)
{
    const double r = 1.0, h = 2.0, roundWall = 2.0 * M_PI * r * h;
    for (const uint32_t n : {8u, 16u, 64u}) {
        const double capNgon = 2.0 * (0.5 * n * std::sin(2.0 * M_PI / n) * r * r);
        const float a = makeCylinder(1.f, 2.f, n).surfaceArea();
        EXPECT_NEAR(a, static_cast<float>(roundWall + capNgon),
                    static_cast<float>(roundWall + capNgon) * 1e-5)
            << "cylinder area is not (exact round wall + exact n-gon caps) at n=" << n;
        EXPECT_LT(std::abs(a - (roundWall + 2.0 * M_PI * r * r)),
                  std::abs(makeCylinder(1.f, 2.f, n / 2).surfaceArea()
                           - (roundWall + 2.0 * M_PI * r * r)));
    }
}

// The cone's lateral surface is likewise exact (pi*r*slant), its base a flat n-gon.
TEST(BRepSurfaceArea, ConeLateralIsExact)
{
    const double r = 1.0, hh = 2.0, slant = std::sqrt(r * r + hh * hh);
    const double lateral = M_PI * r * slant;
    for (const uint32_t n : {8u, 16u, 64u}) {
        const double baseNgon = 0.5 * n * std::sin(2.0 * M_PI / n) * r * r;
        const float a = makeCone(1.f, 2.f, n).surfaceArea();
        EXPECT_NEAR(a, static_cast<float>(lateral + baseNgon),
                    static_cast<float>(lateral + baseNgon) * 1e-5)
            << "cone area is not (exact lateral + exact n-gon base) at n=" << n;
    }
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
