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

// The cylinder's total area is now exact: the curved lateral surface (2*pi*r*h) is
// integrated over its parameter domain, and its two flat caps — which are bounded by
// circular ARCS, so they are true disks — are integrated over their boundary by Green's
// theorem, giving pi*r^2 each rather than the inscribed n-gon a triangulation would.
TEST(BRepSurfaceArea, CylinderIsExact)
{
    const double r = 1.0, h = 2.0;
    const double total = 2.0 * M_PI * r * h + 2.0 * M_PI * r * r;
    for (const uint32_t n : {8u, 16u, 64u}) {
        const float a = makeCylinder(1.f, 2.f, n).surfaceArea();
        EXPECT_NEAR(a, static_cast<float>(total), static_cast<float>(total) * 1e-5)
            << "cylinder area not exact at n=" << n;
    }
}

// The cone's area is now exact throughout — lateral pi*r*slant plus a true disk base —
// and, like the cylinder's, independent of the segment count.
//
// This test previously asserted the opposite for the base: that it is an inscribed n-gon,
// because `fromFaces` derives Line edges and the cone was left with them. That was a
// faithful description of the body as built, and the body as built was WRONG. A cone's side
// face declares an exact Cone surface, so its bottom boundary must be a curve lying on that
// cone; the chord between two base points is not one. Nothing caught it because
// checkGeometry tests vertices, and a chord's endpoints do lie on the cone — but put a
// vertex anywhere else along that chord, which is precisely what an imprint does, and it
// sits up to 0.025 off the surface its own face claims.
//
// The base ring is now upgraded to Circle arcs exactly as the cylinder's rims always were,
// so the base is a true disk and the n-gon reading is gone. See BRepConeBaseArc.
TEST(BRepSurfaceArea, ConeIsExact)
{
    const double r = 1.0, hh = 2.0, slant = std::sqrt(r * r + hh * hh);
    const double total = M_PI * r * slant + M_PI * r * r;
    for (const uint32_t n : {8u, 16u, 64u}) {
        const float a = makeCone(1.f, 2.f, n).surfaceArea();
        EXPECT_NEAR(a, static_cast<float>(total), static_cast<float>(total) * 1e-5)
            << "cone area is not (exact lateral + exact disk base) at n=" << n;
    }
}

TEST(BRepSurfaceArea, ExtrudedAndRevolvedSolids)
{
    // Unit square extruded by 3 → a 1×1×3 box: area = 2·1 + 4·3 = 14.
    const std::vector<Vec3> sq = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    EXPECT_NEAR(extrudeProfile(sq, {0, 0, 3}).surfaceArea(), 14.f, 1e-3f);
}

TEST(BRepSurfaceArea, Deterministic)
{
    const Body b = makeCylinder(1.f, 2.f, 24);
    EXPECT_EQ(b.surfaceArea(), b.surfaceArea());
}

}  // namespace nexus::geometry::brep::testing
