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

}  // namespace nexus::geometry::brep::testing
