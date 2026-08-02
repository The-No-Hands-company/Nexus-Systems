// Foundation — numerical intersection of two NURBS surfaces.
//
// These tests used to assert that the routine RAN. The load-bearing line was
//
//     EXPECT_GE(branches.size(), 0u);
//
// which, size() being unsigned, is true of every possible outcome — inside a test named
// "PerpendicularPlanesIntersectionRunsWithoutError". Two perpendicular planes meet in a
// line whose equation is known outright, and nothing checked it. Behind that, measured:
//
//   * a single straight line came back as THIRTY-TWO curves and 3186 points, because
//     nothing deduplicated seeds lying on a curve already traced;
//   * every curve ran to its full step budget, because there was no closure test;
//   * the marching step divided by |dA/du|^2 * |dA/dv|^2 — the PRODUCT of two squared
//     lengths — so on a 3x3 plane every step was 9x too short and 100 steps covered 0.119
//     of a 3-unit line. That formula is right only for a unit-speed parameterisation,
//     which is exactly what a hand-made test plane tends to be;
//   * a curve was marched in ONE direction only, so a seed in the middle of an open curve
//     returned half of it, and two of the measured curves ran backwards;
//   * seeds were accepted on a fixed absolute distance of 0.105 model units, so on the
//     curved fixture below NO seed qualified and a real two-branch intersection was
//     reported as no intersection at all;
//   * the Newton residual was the whole vector (pa - b.evaluate(ub, vb)), which folds in
//     the closest-point projection's own IN-SURFACE error, so refinement walked points
//     that were already exactly on the curve off it — a sample at x-1 = 0.000e+00 was
//     moved to -4.46e-05, and more iterations made the answer worse rather than better.
//
// So every assertion below names a property that DEFINES the answer: how many curves,
// where they lie, how far they run, and whether the same question twice gives the same
// bits. The oracles are exact and independent of this code.

#include <gtest/gtest.h>

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/geometry/SurfaceSurfaceIntersect.h>
#include <nexus/geometry/Tolerance.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

using namespace nexus::geometry;

// Control points are indexed i*nV + j, so V varies FASTEST — every fixture builds its
// grid u-outer / v-inner. Getting this backwards silently transposes the surface.
NurbsSurface bilinearPatch(Vec3 p00, Vec3 p01, Vec3 p10, Vec3 p11)
{
    const std::vector<float> k = {0.f, 0.f, 1.f, 1.f};
    return NurbsSurface(1, 1, k, k, {p00, p01, p10, p11}, 2, 2);
}

NurbsSurface xyPlane(float s, float z)  // z = const over x,y in [0,s]
{
    return bilinearPatch({0, 0, z}, {0, s, z}, {s, 0, z}, {s, s, z});
}

NurbsSurface xzPlane(float s, float c)  // y = c over x in [0,s], z in [-s,s]
{
    return bilinearPatch({0, c, -s}, {0, c, s}, {s, c, -s}, {s, c, s});
}

// The parabolic cylinder z = x^2/k, uniformly SCALED by k — an exact quadratic Bezier in
// u from control x {-2k, 0, 2k} and z {4k, -4k, 4k}, giving x = 4kt - 2k and z = x^2/k,
// over y in [0, 3k]. It crosses the plane z = k at x = +/-k.
//
// The z controls are linear in k, not k^2, because the shape has to be a SIMILARITY of
// the k=1 case for a scale test to mean anything: mapping x -> kx while leaving z = x^2
// gives z -> k^2 x^2, which is a differently-proportioned surface (far deeper relative to
// its width), not the same one viewed larger. Asserting scale-invariance against that
// would be asserting it against a different question.
NurbsSurface parabolicCylinder(float k)
{
    const std::vector<float> kU = {0.f, 0.f, 0.f, 1.f, 1.f, 1.f};
    const std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    const float xs[3] = {-2.f * k, 0.f, 2.f * k};
    const float zs[3] = {4.f * k, -4.f * k, 4.f * k};
    std::vector<Vec3> ctl;
    for (int i = 0; i < 3; ++i) {
        ctl.push_back({xs[i], 0.f, zs[i]});
        ctl.push_back({xs[i], 3.f * k, zs[i]});
    }
    return NurbsSurface(2, 1, kU, kV, ctl, 3, 2);
}

// The plane z = k, wide enough to cut the parabola above; they meet at x = +/-k.
NurbsSurface cuttingPlane(float k)
{
    return bilinearPatch({-3 * k, 0, k}, {-3 * k, 3 * k, k},
                         {3 * k, 0, k}, {3 * k, 3 * k, k});
}

double maxOffX(const std::vector<Vec3>& c, double t)
{
    double w = 0.0;
    for (const Vec3& p : c) w = std::max(w, std::abs(static_cast<double>(p.x) - t));
    return w;
}
double maxOffY(const std::vector<Vec3>& c, double t)
{
    double w = 0.0;
    for (const Vec3& p : c) w = std::max(w, std::abs(static_cast<double>(p.y) - t));
    return w;
}
double maxOffZ(const std::vector<Vec3>& c, double t)
{
    double w = 0.0;
    for (const Vec3& p : c) w = std::max(w, std::abs(static_cast<double>(p.z) - t));
    return w;
}
void spanX(const std::vector<Vec3>& c, double& lo, double& hi)
{
    lo = 1e30; hi = -1e30;
    for (const Vec3& p : c) { lo = std::min(lo, (double)p.x); hi = std::max(hi, (double)p.x); }
}
void spanY(const std::vector<Vec3>& c, double& lo, double& hi)
{
    lo = 1e30; hi = -1e30;
    for (const Vec3& p : c) { lo = std::min(lo, (double)p.y); hi = std::max(hi, (double)p.y); }
}

}  // namespace

TEST(SurfaceSurfaceIntersect, ParallelPlanesReturnNoBranches)
{
    EXPECT_TRUE(SurfaceSurfaceIntersect::intersect(xyPlane(3.f, 0.f), xyPlane(3.f, 2.f)).empty());
}

TEST(SurfaceSurfaceIntersect, EmptySurfaceFails)
{
    NurbsSurface emptyA;
    NurbsSurface emptyB;
    EXPECT_TRUE(SurfaceSurfaceIntersect::intersect(emptyA, emptyB).empty());
}

TEST(SurfaceSurfaceIntersect, PerpendicularPlanesGiveExactlyOneLineOnTheKnownEquation)
{
    // z = 0 meets y = 1.5 in the segment (x, 1.5, 0), x in [0,3]. ONE curve, not 32.
    const auto branches =
        SurfaceSurfaceIntersect::intersect(xyPlane(3.f, 0.f), xzPlane(3.f, 1.5f));
    ASSERT_EQ(branches.size(), 1u) << "one intersection curve, reported once";

    const auto& c = branches[0];
    ASSERT_GE(c.size(), 4u);
    EXPECT_LT(maxOffY(c, 1.5), 1e-4) << "a sample left the plane y = 1.5";
    EXPECT_LT(maxOffZ(c, 0.0), 1e-4) << "a sample left the plane z = 0";

    // And it spans the whole segment — the previous marcher covered 0.119 of it.
    double lo = 0.0, hi = 0.0;
    spanX(c, lo, hi);
    EXPECT_LT(lo, 0.05) << "the curve does not reach x = 0";
    EXPECT_GT(hi, 2.95) << "the curve does not reach x = 3";
}

TEST(SurfaceSurfaceIntersect, ACurvedSurfaceGivesBothBranchesWhereTheyBelong)
{
    // z = x^2 cut by z = 1 is x = +1 and x = -1. This is the configuration that returned
    // NOTHING while seed acceptance was a fixed absolute distance.
    const auto branches =
        SurfaceSurfaceIntersect::intersect(parabolicCylinder(1.f), cuttingPlane(1.f));
    ASSERT_EQ(branches.size(), 2u) << "z = x^2 cut by z = 1 has exactly two branches";

    bool sawPlus = false, sawMinus = false;
    for (const auto& c : branches) {
        ASSERT_GE(c.size(), 4u);
        double sum = 0.0;
        for (const Vec3& p : c) sum += p.x;
        const double target = (sum / static_cast<double>(c.size())) > 0.0 ? 1.0 : -1.0;
        if (target > 0.0) sawPlus = true; else sawMinus = true;

        EXPECT_LT(maxOffX(c, target), 1e-4) << "branch drifted off x = " << target;
        EXPECT_LT(maxOffZ(c, 1.0), 1e-4) << "branch left the plane z = 1";

        // Both directions from the seed: each branch runs the full height of the patch.
        double lo = 0.0, hi = 0.0;
        spanY(c, lo, hi);
        EXPECT_LT(lo, 0.1) << "branch does not reach y = 0";
        EXPECT_GT(hi, 2.9) << "branch does not reach y = 3";
    }
    EXPECT_TRUE(sawPlus) << "the x = +1 branch is missing";
    EXPECT_TRUE(sawMinus) << "the x = -1 branch is missing";
}

TEST(SurfaceSurfaceIntersect, TheAnswerHasTheSameShapeAtAThousandTimesTheScale)
{
    // Nothing here may depend on the model being about one unit across: seed acceptance is
    // a grid CELL, the march step is a fraction of the extent, and the on-curve test is
    // relative. So the same configuration at 1000x must give the same branch count and the
    // same RELATIVE accuracy. A fixed 0.105-unit seed threshold and a fixed 0.01 step
    // cannot do that in either direction.
    for (float k : {1.f, 1000.f}) {
        const auto branches =
            SurfaceSurfaceIntersect::intersect(parabolicCylinder(k), cuttingPlane(k));
        ASSERT_EQ(branches.size(), 2u) << "scale k=" << k;
        for (const auto& c : branches) {
            double sum = 0.0;
            for (const Vec3& p : c) sum += p.x;
            const double target =
                (sum / static_cast<double>(c.size())) > 0.0 ? (double)k : -(double)k;
            EXPECT_LT(maxOffX(c, target) / (double)k, 1e-4) << "scale k=" << k;
        }
    }
}

TEST(SurfaceSurfaceIntersect, IsBitwiseDeterministic)
{
    const auto run = [] {
        return SurfaceSurfaceIntersect::intersect(parabolicCylinder(1.f), cuttingPlane(1.f));
    };
    const auto a = run();
    const auto b = run();
    ASSERT_EQ(a.size(), b.size());
    ASSERT_GT(a.size(), 0u);
    for (size_t i = 0; i < a.size(); ++i) {
        ASSERT_EQ(a[i].size(), b[i].size()) << "branch " << i;
        for (size_t j = 0; j < a[i].size(); ++j) {
            EXPECT_EQ(a[i][j].x, b[i][j].x) << i << "/" << j;
            EXPECT_EQ(a[i][j].y, b[i][j].y) << i << "/" << j;
            EXPECT_EQ(a[i][j].z, b[i][j].z) << i << "/" << j;
        }
    }
}

TEST(SurfaceSurfaceIntersect, EverySampleIsFiniteAndNoBranchIsDegenerate)
{
    const auto branches =
        SurfaceSurfaceIntersect::intersect(xyPlane(3.f, 0.f), xzPlane(3.f, 1.5f));
    ASSERT_FALSE(branches.empty());
    for (const auto& c : branches) {
        EXPECT_GE(c.size(), 2u);
        for (const Vec3& p : c) {
            // geometry::isFinite inspects the exponent bits; std::isfinite returns TRUE
            // for NaN and Inf under fast-math, which would make this guard dead code.
            EXPECT_TRUE(isFinite(p.x) && isFinite(p.y) && isFinite(p.z));
        }
    }
}
