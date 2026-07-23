#include <gtest/gtest.h>
#include <vector>
#include <algorithm>

#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>

namespace {

using namespace nexus::geometry;

static Mesh makeCubeMesh(float size)
{
    auto box = primitives::makeBox(size, size, size);
    (void)box.topology().triangulate();
    return box;
}

TEST(MeshMassProperties, UnitCubeVolumeApproxOne)
{
    auto cube = makeCubeMesh(1.f);
    ASSERT_TRUE(cube.isValid());
    auto mp = MeshMassProperties::compute(cube);
    EXPECT_NEAR(mp.volume, 1.f, 0.1f);
}

TEST(MeshMassProperties, CentroidAtOriginForCenteredCube)
{
    auto cube = makeCubeMesh(2.f);
    ASSERT_TRUE(cube.isValid());
    auto mp = MeshMassProperties::compute(cube);
    EXPECT_NEAR(mp.centroid.x, 0.f, 0.1f);
    EXPECT_NEAR(mp.centroid.y, 0.f, 0.1f);
    EXPECT_NEAR(mp.centroid.z, 0.f, 0.1f);
}

TEST(MeshMassProperties, PrincipalMomentsSortedDescending)
{
    auto cube = makeCubeMesh(2.f);
    ASSERT_TRUE(cube.isValid());
    auto mp = MeshMassProperties::compute(cube);
    EXPECT_GE(mp.principalMoments[0], mp.principalMoments[1]);
    EXPECT_GE(mp.principalMoments[1], mp.principalMoments[2]);
}

TEST(MeshMassProperties, EmptyMeshNotValid)
{
    Mesh empty;
    auto mp = MeshMassProperties::compute(empty);
    EXPECT_FLOAT_EQ(mp.volume, 0.f);
}

} // namespace

// ── Inertia against closed form ─────────────────────────────────────────────────
//
// Volume and centroid were always right; the inertia tensor was not. A centred 2x3x4 box,
// whose moments are 50, 40 and 26 with zero products, reported -10, -8 and -5.2 with
// products of -22.8, -45.6 and -30.4. Negative diagonal moments are not an approximation
// error — a moment of inertia integrates a squared distance against a positive mass, so it
// cannot be negative for any solid whatsoever. That single fact is the cheapest possible
// guard and no test was making it.
//
// It stayed broken long enough that the analytic B-rep worked around it with its own copy
// of the integrals. These check the answers, and that the two agree so the copy can stay
// gone.

TEST(MeshMassProperties, BoxInertiaMatchesClosedForm) {
    // Solid box (a,b,c), unit density: Ixx = m(b²+c²)/12 and cyclic, products zero.
    const double a = 2.0, b = 3.0, c = 4.0, m = a * b * c;
    const MassProperties p = MeshMassProperties::compute(primitives::makeBox(2.f, 3.f, 4.f));

    EXPECT_NEAR(p.volume, m, 1e-3);
    EXPECT_NEAR(p.inertia[0][0], m * (b * b + c * c) / 12.0, 1e-2);
    EXPECT_NEAR(p.inertia[1][1], m * (a * a + c * c) / 12.0, 1e-2);
    EXPECT_NEAR(p.inertia[2][2], m * (a * a + b * b) / 12.0, 1e-2);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            if (i != j) EXPECT_NEAR(p.inertia[i][j], 0.f, 1e-3) << "product " << i << j
                                                                << " is non-zero on a symmetric box";
        }
    }
}

TEST(MeshMassProperties, SphereAndCylinderInertiaMatchClosedForm) {
    {   // Sphere: I = 2/5 m r² about every axis.
        const double r = 1.0, V = 4.0 / 3.0 * 3.14159265358979 * r * r * r;
        const MassProperties p = MeshMassProperties::compute(primitives::makeSphere(1.f, 32, 40));
        const double I = 0.4 * V * r * r;
        EXPECT_NEAR(p.volume, V, V * 0.02);
        for (int i = 0; i < 3; ++i) EXPECT_NEAR(p.inertia[i][i], I, I * 0.05);
    }
    {   // Cylinder about its own axis (+Y here): 1/2 m r².
        const double r = 1.0, h = 2.0, V = 3.14159265358979 * r * r * h;
        const MassProperties p = MeshMassProperties::compute(primitives::makeCylinder(1.f, 2.f, 64));
        EXPECT_NEAR(p.volume, V, V * 0.02);
        EXPECT_NEAR(p.inertia[1][1], 0.5 * V * r * r, V * r * r * 0.05);
        const double transverse = V * (3 * r * r + h * h) / 12.0;
        EXPECT_NEAR(p.inertia[0][0], transverse, transverse * 0.05);
        EXPECT_NEAR(p.inertia[2][2], transverse, transverse * 0.05);
    }
}

// The cheapest guard there is, and it would have caught the original outright.
TEST(MeshMassProperties, DiagonalMomentsAreNeverNegative) {
    const std::vector<Mesh> solids = {
        primitives::makeBox(2.f, 3.f, 4.f),
        primitives::makeSphere(1.2f, 16, 20),
        primitives::makeCylinder(0.8f, 2.5f, 24),
        primitives::makeTorus(1.f, 0.3f, 16, 12),
    };
    for (std::size_t s = 0; s < solids.size(); ++s) {
        const MassProperties p = MeshMassProperties::compute(solids[s]);
        ASSERT_GT(p.volume, 0.f) << "solid " << s << " integrated to no volume";
        for (int i = 0; i < 3; ++i) {
            EXPECT_GT(p.inertia[i][i], 0.f)
                << "solid " << s << " reported a negative moment on axis " << i
                << " — impossible for any solid";
        }
        // The tensor must also satisfy the triangle inequality on its diagonal.
        EXPECT_GE(p.inertia[0][0] + p.inertia[1][1], p.inertia[2][2] * 0.999f);
        EXPECT_GE(p.inertia[1][1] + p.inertia[2][2], p.inertia[0][0] * 0.999f);
        EXPECT_GE(p.inertia[2][2] + p.inertia[0][0], p.inertia[1][1] * 0.999f);
    }
}

// Translating a solid moves its centroid and leaves its centroidal inertia alone.
TEST(MeshMassProperties, CentroidalInertiaIsTranslationInvariant) {
    const Mesh box = primitives::makeBox(2.f, 3.f, 4.f);
    Mesh moved = box;
    auto pts = moved.attributes().positions();
    for (auto& v : pts) { v.x += 17.f; v.y -= 5.f; v.z += 3.f; }
    moved.attributes().setPositions(std::move(pts));

    const MassProperties a = MeshMassProperties::compute(box);
    const MassProperties b = MeshMassProperties::compute(moved);

    EXPECT_NEAR(b.centroid.x, 17.f, 1e-2);
    EXPECT_NEAR(b.centroid.y, -5.f, 1e-2);
    EXPECT_NEAR(b.centroid.z, 3.f, 1e-2);
    EXPECT_NEAR(a.volume, b.volume, 1e-2);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            EXPECT_NEAR(a.inertia[i][j], b.inertia[i][j], std::max(1e-2, std::abs(a.inertia[i][j]) * 0.01))
                << "centroidal inertia changed under translation at " << i << j;
        }
    }
}
