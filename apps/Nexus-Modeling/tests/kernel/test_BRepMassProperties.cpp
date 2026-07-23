// Foundation — analytic B-rep mass properties. Body::massProperties integrates
// volume, centroid and the inertia tensor (about the centroid) over the
// watertight boundary via Eberly's exact polyhedral surface integrals, matching
// the analytic box/sphere/cylinder tensors; density scales the inertia.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
Body boxAt(Vec3 c, float w, float h, float d)
{
    Body b = makeBox(w, h, d);
    b.translate(c);
    return b;
}
}  // namespace

TEST(BRepMassProperties, BoxMatchesAnalyticTensor)
{
    // Box 2×3×4 centred at origin: m = 24, Ixx = m(h²+d²)/12, etc. (exact — the
    // faces are planar so the tessellation is exact).
    const MassProperties mp = makeBox(2.f, 3.f, 4.f).massProperties();
    EXPECT_NEAR(mp.volume, 24.f, 1e-3f);
    EXPECT_NEAR(mp.centroid.x, 0.f, 1e-4f);
    EXPECT_NEAR(mp.centroid.y, 0.f, 1e-4f);
    EXPECT_NEAR(mp.centroid.z, 0.f, 1e-4f);
    EXPECT_NEAR(mp.inertia[0][0], 50.f, 1e-2f);  // 24*(9+16)/12
    EXPECT_NEAR(mp.inertia[1][1], 40.f, 1e-2f);  // 24*(4+16)/12
    EXPECT_NEAR(mp.inertia[2][2], 26.f, 1e-2f);  // 24*(4+9)/12
    EXPECT_NEAR(mp.inertia[0][1], 0.f, 1e-3f);   // symmetric solid → no products
    EXPECT_NEAR(mp.inertia[0][2], 0.f, 1e-3f);
    EXPECT_NEAR(mp.inertia[1][2], 0.f, 1e-3f);
}

TEST(BRepMassProperties, SphereMatchesAnalytic)
{
    const float r = 1.f;
    const MassProperties mp = makeSphere(r, 32, 48).massProperties();
    const double m = 4.0 / 3.0 * M_PI * r * r * r;
    EXPECT_NEAR(mp.volume, static_cast<float>(m), 0.05f);
    const double I = 0.4 * m * r * r;  // 2/5 m r²
    EXPECT_NEAR(mp.inertia[0][0], static_cast<float>(I), 0.04f);
    EXPECT_NEAR(mp.inertia[1][1], static_cast<float>(I), 0.04f);
    EXPECT_NEAR(mp.inertia[2][2], static_cast<float>(I), 0.04f);
    EXPECT_NEAR(mp.centroid.x, 0.f, 1e-3f);
}

TEST(BRepMassProperties, CylinderMatchesAnalytic)
{
    const float r = 1.f, h = 2.f;
    const MassProperties mp = makeCylinder(r, h, 64).massProperties();
    const double m = M_PI * r * r * h;
    EXPECT_NEAR(mp.volume, static_cast<float>(m), 0.02f);
    EXPECT_NEAR(mp.inertia[2][2], static_cast<float>(0.5 * m * r * r), 0.02f);  // axial ½ m r²
    EXPECT_NEAR(mp.inertia[0][0], static_cast<float>(m * (3 * r * r + h * h) / 12.0), 0.02f);
    EXPECT_NEAR(mp.centroid.z, 0.f, 1e-3f);
}

TEST(BRepMassProperties, TranslationMovesCentroidNotVolume)
{
    const MassProperties mp = boxAt({5.f, 3.f, -2.f}, 2, 2, 2).massProperties();
    EXPECT_NEAR(mp.volume, 8.f, 1e-3f);
    EXPECT_NEAR(mp.centroid.x, 5.f, 1e-3f);
    EXPECT_NEAR(mp.centroid.y, 3.f, 1e-3f);
    EXPECT_NEAR(mp.centroid.z, -2.f, 1e-3f);
    // Inertia about the (moved) centroid is unchanged: 8*(4+4)/12 = 5.333.
    EXPECT_NEAR(mp.inertia[0][0], 8.f * 8.f / 12.f, 1e-2f);
}

TEST(BRepMassProperties, DensityScalesInertia)
{
    const MassProperties m1 = makeBox(2.f, 2.f, 2.f).massProperties(1.f);
    const MassProperties m2 = makeBox(2.f, 2.f, 2.f).massProperties(2.f);
    EXPECT_NEAR(m2.volume, m1.volume, 1e-4f);              // volume is geometric
    EXPECT_NEAR(m2.inertia[0][0], 2.f * m1.inertia[0][0], 1e-3f);  // inertia ∝ density
}

TEST(BRepMassProperties, BooleanResultVolume)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    EXPECT_NEAR(booleanToBody(a, b, BooleanOp::Union).massProperties().volume, 15.f, 1e-3f);
    EXPECT_NEAR(booleanToBody(a, b, BooleanOp::Difference).massProperties().volume, 7.f, 1e-3f);
}

TEST(BRepMassProperties, Deterministic)
{
    const Body b = makeCylinder(1.f, 2.f, 24);
    const MassProperties m1 = b.massProperties();
    const MassProperties m2 = b.massProperties();
    EXPECT_EQ(m1.volume, m2.volume);
    EXPECT_EQ(m1.inertia[2][2], m2.inertia[2][2]);
    EXPECT_EQ(m1.centroid.x, m2.centroid.x);
}

// The B-rep integrates its CURVED analytic faces exactly over their own parameter domain
// rather than tessellating them. For a cylinder and a cone — surfaces this kernel
// represents truthfully — that makes the side-wall contribution exact to float precision
// and, crucially, INDEPENDENT of segment count: doubling the facets does not change the
// answer, because the answer is not coming from the facets.
TEST(BRepMassProperties, CylinderVolumeAndAxialMomentAreExactRegardlessOfSegments)
{
    const double PI = 3.14159265358979323846;
    const double r = 1.0, h = 2.0;
    const double Vround = PI * r * r * h;
    const double IzzRound = 0.5 * Vround * r * r;  // about the cylinder's axis (+Z here)

    for (const uint32_t segs : {8u, 16u, 64u}) {
        const MassProperties p = makeCylinder(1.f, 2.f, segs).massProperties(1.f);
        EXPECT_NEAR(p.volume, Vround, Vround * 1e-5)
            << "cylinder volume is not exact at " << segs << " segments";
        // The axial moment [2][2] comes entirely from the exactly-integrated side wall.
        // The transverse moments carry the flat-cap facet error and are NOT exact — which
        // is correct, a faceted cap really is an inscribed n-gon.
        EXPECT_NEAR(p.inertia[2][2], IzzRound, IzzRound * 1e-5)
            << "cylinder axial moment is not exact at " << segs << " segments";
    }
}

TEST(BRepMassProperties, ConeVolumeAndAxialMomentAreExactRegardlessOfSegments)
{
    const double PI = 3.14159265358979323846;
    const double r = 1.0, h = 2.0;
    const double Vround = PI * r * r * h / 3.0;
    const double IzzRound = 0.3 * Vround * r * r;  // solid cone about its axis (+Z here)

    for (const uint32_t segs : {8u, 16u, 64u}) {
        const MassProperties p = makeCone(1.f, 2.f, segs).massProperties(1.f);
        EXPECT_NEAR(p.volume, Vround, Vround * 1e-5)
            << "cone volume is not exact at " << segs << " segments";
        EXPECT_NEAR(p.inertia[2][2], IzzRound, IzzRound * 1e-5)
            << "cone axial moment is not exact at " << segs << " segments";
    }
}

// Planar bodies stay exactly what they were — a box goes entirely through the flat path.
TEST(BRepMassProperties, BoxStaysExactThroughTheFlatPath)
{
    const double a = 2, b = 3, c = 4, m = a * b * c;
    const MassProperties p = makeBox(2.f, 3.f, 4.f).massProperties(1.f);
    EXPECT_NEAR(p.volume, m, 1e-4);
    EXPECT_NEAR(p.inertia[0][0], m * (b * b + c * c) / 12.0, 1e-3);
    EXPECT_NEAR(p.inertia[1][1], m * (a * a + c * c) / 12.0, 1e-3);
    EXPECT_NEAR(p.inertia[2][2], m * (a * a + b * b) / 12.0, 1e-3);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (i != j) EXPECT_NEAR(p.inertia[i][j], 0.f, 1e-3);
}

// The sphere is deliberately still tessellated (its parameter convention here does not
// match the exact patch), so it must remain CORRECT and converge with density rather than
// return a confidently-wrong "exact" value. This guards that the gating did not let a
// broken exact sphere through.
TEST(BRepMassProperties, SphereStaysCorrectAndConvergesWithTessellation)
{
    const double PI = 3.14159265358979323846;
    const double r = 1.0, Vround = 4.0 / 3.0 * PI * r * r * r;

    const double coarse = std::abs(makeSphere(1.f, 8, 12).massProperties(1.f).volume - Vround);
    const double fine   = std::abs(makeSphere(1.f, 48, 64).massProperties(1.f).volume - Vround);
    EXPECT_LT(fine, coarse) << "finer tessellation did not improve the sphere";
    EXPECT_LT(fine, Vround * 0.01) << "the fine sphere is still more than 1% off";
    // And never wildly wrong (the broken exact path inflated it past the true volume).
    EXPECT_LT(makeSphere(1.f, 8, 12).massProperties(1.f).volume, Vround * 1.1);
}

// Density scales the inertia and nothing else.
TEST(BRepMassProperties, DensityScalesInertiaOnly)
{
    const MassProperties unit = makeCylinder(1.f, 2.f, 32).massProperties(1.f);
    const MassProperties dense = makeCylinder(1.f, 2.f, 32).massProperties(2.5f);
    EXPECT_NEAR(dense.volume, unit.volume, 1e-5);
    EXPECT_NEAR(dense.centroid.y, unit.centroid.y, 1e-5);
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            EXPECT_NEAR(dense.inertia[i][j], 2.5f * unit.inertia[i][j],
                        std::max(1e-4, std::abs(unit.inertia[i][j]) * 1e-4));
}

}  // namespace nexus::geometry::brep::testing
