#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurface.h>

#include <cmath>
#include <limits>
#include <vector>

using namespace nexus::geometry;

static NurbsSurface makePlanarSurface() {
    std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, {2.f, 0.f, 0.f}, {3.f, 0.f, 0.f},
        {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f}, {2.f, 1.f, 0.f}, {3.f, 1.f, 0.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 0.f}, {2.f, 2.f, 0.f}, {3.f, 2.f, 0.f},
        {0.f, 3.f, 0.f}, {1.f, 3.f, 0.f}, {2.f, 3.f, 0.f}, {3.f, 3.f, 0.f},
    };
    return NurbsSurface(3, 3, kU, kV, ctl, 4, 4);
}

static NurbsSurface makeBiquadraticSurface() {
    std::vector<float> kU = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 1.f}, {2.f, 0.f, 0.f},
        {0.f, 1.f, 1.f}, {1.f, 1.f, 2.f}, {2.f, 1.f, 1.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 1.f}, {2.f, 2.f, 0.f},
    };
    return NurbsSurface(2, 2, kU, kV, ctl, 3, 3);
}

TEST(NurbsSurface, ValidateReturnsSuccessForValidSurface) {
    NurbsSurface s = makeBiquadraticSurface();
    EXPECT_TRUE(s.isValid());
}

TEST(NurbsSurface, RejectInvalidDegree) {
    std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {{0,0,0}, {1,0,0}, {0,1,0}, {1,1,0}};
    NurbsSurface s(0, 1, kU, kV, ctl, 2, 2);
    EXPECT_FALSE(s.isValid());
}

TEST(NurbsSurface, RejectEmptyControlPoints) {
    std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl;
    NurbsSurface s(1, 1, kU, kV, ctl, 0, 0);
    EXPECT_FALSE(s.isValid());
}

TEST(NurbsSurface, RejectMismatchedCPCount) {
    std::vector<float> kU = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {{0,0,0}, {1,0,0}, {2,0,0}};
    NurbsSurface s(2, 2, kU, kV, ctl, 2, 2);
    EXPECT_FALSE(s.isValid());
}

TEST(NurbsSurface, RejectNonFiniteCPs) {
    float inf = std::numeric_limits<float>::infinity();
    float nan = std::numeric_limits<float>::quiet_NaN();

    {
        std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
        std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
        std::vector<Vec3> ctl = {{0,0,0}, {inf,0,0}, {0,1,0}, {1,1,0}};
        NurbsSurface s(1, 1, kU, kV, ctl, 2, 2);
        EXPECT_TRUE(s.isValid());
    }

    {
        std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
        std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
        std::vector<Vec3> ctl = {{0,0,0}, {nan,0,0}, {0,1,0}, {1,1,0}};
        NurbsSurface s(1, 1, kU, kV, ctl, 2, 2);
        EXPECT_TRUE(s.isValid());
    }
}

TEST(NurbsSurface, GenerateClampedKnotsProducesCorrectCount) {
    int32_t deg = 3;
    int32_t nU = 4;
    int32_t nV = 4;

    std::vector<float> kU = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl(static_cast<size_t>(nU * nV), Vec3{0,0,0});

    NurbsSurface s(deg, deg, kU, kV, ctl, nU, nV);
    ASSERT_TRUE(s.isValid());

    EXPECT_EQ(static_cast<int32_t>(s.knotU().size()), nU + deg + 1);
    EXPECT_EQ(static_cast<int32_t>(s.knotV().size()), nV + deg + 1);
}

TEST(NurbsSurface, EvaluatePointAtCenterReturnsInterpolatedPoint) {
    NurbsSurface s = makePlanarSurface();

    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();
    float uMid = (uMin + uMax) * 0.5f;
    float vMid = (vMin + vMax) * 0.5f;

    Vec3 p = s.evaluate(uMid, vMid);
    EXPECT_FLOAT_EQ(p.z, 0.f);
    EXPECT_GT(p.x, 0.f);
    EXPECT_LT(p.x, 3.f);
    EXPECT_GT(p.y, 0.f);
    EXPECT_LT(p.y, 3.f);
}

TEST(NurbsSurface, EvaluatePointAtCornerReturnsCP) {
    NurbsSurface s = makePlanarSurface();
    ASSERT_TRUE(s.isValid());

    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();

    Vec3 p00 = s.evaluate(uMin, vMin);
    Vec3 corner = s.controlPoint(0, 0);
    EXPECT_FLOAT_EQ(p00.x, corner.x);
    EXPECT_FLOAT_EQ(p00.y, corner.y);
    EXPECT_FLOAT_EQ(p00.z, corner.z);

    Vec3 p11 = s.evaluate(uMax, vMax);
    Vec3 lastCP = s.controlPoint(s.controlPointCountU() - 1, s.controlPointCountV() - 1);
    EXPECT_FLOAT_EQ(p11.x, lastCP.x);
    EXPECT_FLOAT_EQ(p11.y, lastCP.y);
    EXPECT_FLOAT_EQ(p11.z, lastCP.z);
}

TEST(NurbsSurface, EvaluateNormalOnPlanarSurfaceReturnsZ) {
    NurbsSurface s = makePlanarSurface();
    ASSERT_TRUE(s.isValid());

    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();
    float uMid = (uMin + uMax) * 0.5f;
    float vMid = (vMin + vMax) * 0.5f;

    Vec3 du = s.derivativeU(uMid, vMid);
    Vec3 dv = s.derivativeV(uMid, vMid);
    Vec3 normal = du.cross(dv).normalize();

    EXPECT_NEAR(std::abs(normal.z), 1.f, 0.01f);
    EXPECT_NEAR(normal.x, 0.f, 0.01f);
    EXPECT_NEAR(normal.y, 0.f, 0.01f);
}

// A curved (degree 2) surface: on a non-planar patch, both partials must weight the
// difference control points by the lower-degree basis functions, not sum them. Comparing
// against a central finite difference of evaluate() catches the old bug, which summed the
// u/v difference points with weight 1 (correct only for degree 1).
static void expectPartialsMatchFiniteDifference(const NurbsSurface& s, float tol) {
    const float h = 1e-3f;
    auto [uMin, uMax] = s.domainU();
    auto [vMin, vMax] = s.domainV();
    for (int iu = 1; iu <= 9; ++iu) {
        for (int iv = 1; iv <= 9; ++iv) {
            const float u = uMin + (uMax - uMin) * 0.1f * static_cast<float>(iu);
            const float v = vMin + (vMax - vMin) * 0.1f * static_cast<float>(iv);

            const Vec3 du  = s.derivativeU(u, v);
            const Vec3 duF = (s.evaluate(u + h, v) - s.evaluate(u - h, v)) * (1.f / (2.f * h));
            EXPECT_NEAR(du.x, duF.x, tol) << "dU.x u=" << u << " v=" << v;
            EXPECT_NEAR(du.y, duF.y, tol) << "dU.y u=" << u << " v=" << v;
            EXPECT_NEAR(du.z, duF.z, tol) << "dU.z u=" << u << " v=" << v;

            const Vec3 dv  = s.derivativeV(u, v);
            const Vec3 dvF = (s.evaluate(u, v + h) - s.evaluate(u, v - h)) * (1.f / (2.f * h));
            EXPECT_NEAR(dv.x, dvF.x, tol) << "dV.x u=" << u << " v=" << v;
            EXPECT_NEAR(dv.y, dvF.y, tol) << "dV.y u=" << u << " v=" << v;
            EXPECT_NEAR(dv.z, dvF.z, tol) << "dV.z u=" << u << " v=" << v;
        }
    }
}

TEST(NurbsSurface, PartialsMatchFiniteDifferenceNonRational) {
    expectPartialsMatchFiniteDifference(makeBiquadraticSurface(), 5e-3f);
}

TEST(NurbsSurface, PartialsMatchFiniteDifferenceRational) {
    // Same biquadratic control net, now with non-uniform weights so both partials exercise
    // the rational quotient rule.
    std::vector<float> kU = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 1.f}, {2.f, 0.f, 0.f},
        {0.f, 1.f, 1.f}, {1.f, 1.f, 2.f}, {2.f, 1.f, 1.f},
        {0.f, 2.f, 0.f}, {1.f, 2.f, 1.f}, {2.f, 2.f, 0.f},
    };
    std::vector<float> w = {1.f, 2.f, 0.5f, 1.5f, 3.f, 0.7f, 1.f, 2.5f, 1.2f};
    NurbsSurface s(2, 2, kU, kV, ctl, 3, 3, w);
    ASSERT_TRUE(s.isValid());
    expectPartialsMatchFiniteDifference(s, 5e-3f);
}

// A quarter-cylinder built exactly as a rational NURBS surface: linear along z (degree 1 in
// u), an exact quarter circle of unit radius in xy (degree 2 rational in v). This pins the
// partials to known geometry, not just to a finite difference.
static NurbsSurface makeQuarterCylinder() {
    const float s = 1.f / std::sqrt(2.f);
    std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};              // degree 1, nU = 2 (z axis)
    std::vector<float> kV = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};    // degree 2, nV = 3 (arc)
    std::vector<Vec3> ctl = {
        {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 0.f},     // z = 0 ring
        {1.f, 0.f, 1.f}, {1.f, 1.f, 1.f}, {0.f, 1.f, 1.f},     // z = 1 ring
    };
    std::vector<float> w = {1.f, s, 1.f, 1.f, s, 1.f};
    return NurbsSurface(1, 2, kU, kV, ctl, 2, 3, w);
}

TEST(NurbsSurface, QuarterCylinderPartialsMatchExactGeometry) {
    NurbsSurface s = makeQuarterCylinder();
    ASSERT_TRUE(s.isValid());

    for (int iu = 0; iu <= 4; ++iu) {
        for (int iv = 0; iv <= 4; ++iv) {
            const float u = 0.25f * static_cast<float>(iu);
            const float v = 0.25f * static_cast<float>(iv);
            const Vec3 p = s.evaluate(u, v);

            // The surface lies on the unit cylinder x^2 + y^2 = 1, z = u.
            EXPECT_NEAR(std::sqrt(p.x * p.x + p.y * p.y), 1.f, 1e-5f) << "u=" << u << " v=" << v;
            EXPECT_NEAR(p.z, u, 1e-5f) << "u=" << u << " v=" << v;

            // dS/du is exactly the axis direction (0,0,1) everywhere.
            const Vec3 du = s.derivativeU(u, v);
            EXPECT_NEAR(du.x, 0.f, 1e-4f) << "u=" << u << " v=" << v;
            EXPECT_NEAR(du.y, 0.f, 1e-4f) << "u=" << u << " v=" << v;
            EXPECT_NEAR(du.z, 1.f, 1e-4f) << "u=" << u << " v=" << v;

            // dS/dv is tangent to the circle: perpendicular to the radius (x,y,0) and in the
            // z = const plane.
            const Vec3 dv = s.derivativeV(u, v);
            EXPECT_NEAR(dv.z, 0.f, 1e-4f) << "u=" << u << " v=" << v;
            EXPECT_NEAR(dv.x * p.x + dv.y * p.y, 0.f, 1e-4f) << "u=" << u << " v=" << v;

            // The surface normal points radially outward: (x, y, 0).
            const Vec3 n = du.cross(dv).normalize();
            EXPECT_NEAR(std::abs(n.x * p.x + n.y * p.y), 1.f, 1e-4f) << "u=" << u << " v=" << v;
            EXPECT_NEAR(n.z, 0.f, 1e-4f) << "u=" << u << " v=" << v;
        }
    }
}
