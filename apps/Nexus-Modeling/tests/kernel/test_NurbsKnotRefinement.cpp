// Knot refinement changes a curve's REPRESENTATION and must never change the curve. That is
// the whole contract, and nothing here used to check it: every assertion was a control-point
// COUNT, plus endpoint positions, which a clamped knot vector preserves no matter what the
// interior does. Under those assertions three defects lived in public API:
//
//   * bezierDecomposition CRASHED. It held `const auto& knots = result.knots()` and then
//     reassigned `result` inside the loop, so every later read of knots[i] was a
//     use-after-free — a segfault on an ordinary degree-2 curve with one interior knot. It
//     also inserted at the MIDPOINT between adjacent knots instead of raising the
//     multiplicity of the knots already there, so it decomposed nothing.
//   * degreeElevate did not elevate. It inserted a knot at the domain midpoint, and knot
//     insertion cannot change a degree, so it returned the ORIGINAL degree plus spurious
//     knots. degreeElevate(curve, 3) on a degree-2 curve returned degree 2.
//   * insertKnot moved RATIONAL curves. Control points were blended in Cartesian
//     coordinates and weights separately; the affine combination has to happen in
//     homogeneous (w*x, w*y, w*z, w) and be projected after. MEASURED on the standard
//     rational quarter circle: inserting a knot at 0.5 pushed it 6.1e-02 off the unit
//     circle. A polynomial curve has all weights 1, so only the rational path was wrong —
//     and there was no rational test.

#include <gtest/gtest.h>

#include <nexus/geometry/NurbsKnotRefinement.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using namespace nexus::geometry;

namespace {

// The invariant: sample the curve across its interior, not just at its ends.
double maxDeviation(const NurbsCurve& a, const NurbsCurve& b, int samples = 64)
{
    const auto [lo, hi] = a.domain();
    double worst = 0.0;
    for (int i = 0; i <= samples; ++i) {
        const float t = lo + (hi - lo) * static_cast<float>(i) / static_cast<float>(samples);
        const Vec3 p = a.evaluate(t), q = b.evaluate(t);
        worst = std::max(worst, std::sqrt((double)(p.x - q.x) * (p.x - q.x) +
                                          (double)(p.y - q.y) * (p.y - q.y) +
                                          (double)(p.z - q.z) * (p.z - q.z)));
    }
    return worst;
}

// The standard exact quarter circle: a rational quadratic. Its whole point is that it is
// EXACTLY circular, which makes "still on the unit circle" an oracle no polynomial curve
// can provide.
NurbsCurve makeRationalQuarterCircle()
{
    return NurbsCurve(2, {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
                      {{1, 0, 0}, {1, 1, 0}, {0, 1, 0}},
                      {1.f, static_cast<float>(std::sqrt(2.0) / 2.0), 1.f});
}

double maxRadialError(const NurbsCurve& c, int samples = 64)
{
    double worst = 0.0;
    for (int i = 0; i <= samples; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(samples);
        const Vec3 p = c.evaluate(t);
        worst = std::max(worst, std::abs(std::sqrt((double)p.x * p.x + (double)p.y * p.y) - 1.0));
    }
    return worst;
}

}  // namespace

static NurbsCurve makeCubicCurve() {
    return NurbsCurve(3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {{0,0,0}, {1,2,0}, {3,2,0}, {4,0,0}});
}

static NurbsSurface makeBilinearSurface() {
    std::vector<float> kU = {0.f, 0.f, 1.f, 1.f};
    std::vector<float> kV = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 1.f},
        {0.f, 1.f, 1.f}, {1.f, 1.f, 0.f},
    };
    return NurbsSurface(1, 1, kU, kV, ctl, 2, 2);
}

TEST(NurbsKnotRefinement, InsertKnotSucceedsAndIncreasesCPCount) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int32_t origCP = c.controlPointCount();

    NurbsCurve refined = NurbsKnotRefinement::insertKnot(c, 0.5f);
    ASSERT_TRUE(refined.isValid());
    EXPECT_EQ(refined.controlPointCount(), origCP + 1);
}

TEST(NurbsKnotRefinement, RefineCurveWithMultipleKnotsIncreasesCPCount) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int32_t origCP = c.controlPointCount();

    std::vector<float> knots = {0.25f, 0.5f, 0.75f};
    NurbsCurve refined = NurbsKnotRefinement::refineCurve(c, knots);
    ASSERT_TRUE(refined.isValid());
    EXPECT_EQ(refined.controlPointCount(), origCP + static_cast<int32_t>(knots.size()));
}

TEST(NurbsKnotRefinement, OutOfRangeKnotInsertionFails) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int32_t origCP = c.controlPointCount();

    NurbsCurve before = NurbsKnotRefinement::insertKnot(c, -0.5f);
    EXPECT_EQ(before.controlPointCount(), origCP);

    NurbsCurve after = NurbsKnotRefinement::insertKnot(c, 1.5f);
    EXPECT_EQ(after.controlPointCount(), origCP);
}

TEST(NurbsKnotRefinement, RefinedCurvePreservesEndpointPositions) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());

    Vec3 origStart = c.evaluate(0.f);
    Vec3 origEnd = c.evaluate(1.f);

    std::vector<float> knots = {0.3f, 0.6f};
    NurbsCurve refined = NurbsKnotRefinement::refineCurve(c, knots);
    ASSERT_TRUE(refined.isValid());

    Vec3 refStart = refined.evaluate(0.f);
    Vec3 refEnd = refined.evaluate(1.f);

    EXPECT_NEAR(refStart.x, origStart.x, 0.001f);
    EXPECT_NEAR(refStart.y, origStart.y, 0.001f);
    EXPECT_NEAR(refStart.z, origStart.z, 0.001f);

    EXPECT_NEAR(refEnd.x, origEnd.x, 0.001f);
    EXPECT_NEAR(refEnd.y, origEnd.y, 0.001f);
    EXPECT_NEAR(refEnd.z, origEnd.z, 0.001f);
}

// ── The contract the count assertions above could not see ───────────────────────────────

TEST(NurbsKnotRefinement, InsertKnotDoesNotMoveThePolynomialCurve) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    for (const float u : {0.1f, 0.25f, 0.5f, 0.75f, 0.9f}) {
        const NurbsCurve r = NurbsKnotRefinement::insertKnot(c, u);
        ASSERT_TRUE(r.isValid()) << "u=" << u;
        EXPECT_LT(maxDeviation(c, r), 1e-5) << "insertKnot(" << u << ") moved the curve";
    }
}

// LOAD-BEARING for the homogeneous-coordinate fix: this is the one that measured 6.1e-02.
TEST(NurbsKnotRefinement, InsertKnotDoesNotMoveARationalCurve) {
    const NurbsCurve c = makeRationalQuarterCircle();
    ASSERT_TRUE(c.isValid());
    ASSERT_TRUE(c.isRational());
    ASSERT_LT(maxRadialError(c), 1e-5) << "the fixture is not a circle";

    for (const float u : {0.25f, 0.5f, 0.75f}) {
        const NurbsCurve r = NurbsKnotRefinement::insertKnot(c, u);
        ASSERT_TRUE(r.isValid()) << "u=" << u;
        ASSERT_TRUE(r.isRational()) << "weights were dropped at u=" << u;
        EXPECT_LT(maxRadialError(r), 1e-5)
            << "after inserting a knot at " << u << " the quarter circle is no longer circular";
        EXPECT_LT(maxDeviation(c, r), 1e-5) << "insertKnot(" << u << ") moved the rational curve";
    }
}

TEST(NurbsKnotRefinement, RefineCurveDoesNotMoveTheCurve) {
    NurbsCurve c = makeCubicCurve();
    const NurbsCurve r = NurbsKnotRefinement::refineCurve(c, {0.2f, 0.4f, 0.6f, 0.8f});
    ASSERT_TRUE(r.isValid());
    EXPECT_EQ(r.controlPointCount(), c.controlPointCount() + 4);
    EXPECT_LT(maxDeviation(c, r), 1e-5);
}

// degreeElevate returned the ORIGINAL degree before this.
TEST(NurbsKnotRefinement, DegreeElevateActuallyRaisesTheDegreeAndKeepsTheCurve) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_EQ(c.degree(), 3);
    for (const int32_t target : {4, 5, 6}) {
        const NurbsCurve e = NurbsKnotRefinement::degreeElevate(c, target);
        ASSERT_TRUE(e.isValid()) << "target=" << target;
        EXPECT_EQ(e.degree(), target) << "degreeElevate did not reach the requested degree";
        EXPECT_LT(maxDeviation(c, e), 1e-4) << "degreeElevate to " << target << " moved the curve";
    }
    // a target at or below the current degree is a no-op, not an error
    EXPECT_EQ(NurbsKnotRefinement::degreeElevate(c, 3).degree(), 3);
    EXPECT_EQ(NurbsKnotRefinement::degreeElevate(c, 1).degree(), 3);
}

TEST(NurbsKnotRefinement, DegreeElevateKeepsARationalCurveExactlyCircular) {
    const NurbsCurve c = makeRationalQuarterCircle();
    for (const int32_t target : {3, 4, 5}) {
        const NurbsCurve e = NurbsKnotRefinement::degreeElevate(c, target);
        ASSERT_TRUE(e.isValid()) << "target=" << target;
        EXPECT_EQ(e.degree(), target);
        ASSERT_TRUE(e.isRational()) << "weights were dropped elevating to " << target;
        EXPECT_LT(maxRadialError(e), 1e-5)
            << "elevating to degree " << target << " left the circle";
    }
}

// This one used to segfault.
TEST(NurbsKnotRefinement, BezierDecompositionDoesNotCrashAndKeepsTheCurve) {
    // degree 2 with a single interior knot — the minimal case that crashed
    NurbsCurve c(2, {0.f, 0.f, 0.f, 0.5f, 1.f, 1.f, 1.f},
                 {{0, 0, 0}, {1, 2, 0}, {2, -1, 0}, {3, 1, 0}});
    ASSERT_TRUE(c.isValid());
    const NurbsCurve b = NurbsKnotRefinement::bezierDecomposition(c);
    ASSERT_TRUE(b.isValid());
    EXPECT_EQ(b.degree(), c.degree()) << "decomposition must not change the degree";
    EXPECT_LT(maxDeviation(c, b), 1e-5);

    // every interior knot now sits at multiplicity == degree, which is what makes the
    // segments Bezier
    const auto [lo, hi] = b.domain();
    for (const float k : b.knots()) {
        if (k <= lo + 1e-6f || k >= hi - 1e-6f) continue;
        EXPECT_EQ(NurbsKnotRefinement::knotMultiplicity(b, k), static_cast<uint32_t>(b.degree()))
            << "interior knot " << k << " is not at full multiplicity";
    }
    // and the control point count is exactly segments*degree + 1
    EXPECT_EQ(b.controlPointCount(), 2 * b.degree() + 1) << "two Bezier segments expected";
}

TEST(NurbsKnotRefinement, BezierDecompositionIsIdempotent) {
    NurbsCurve c(3, {0.f, 0.f, 0.f, 0.f, 0.33f, 0.66f, 1.f, 1.f, 1.f, 1.f},
                 {{0, 0, 0}, {1, 2, 0}, {3, 2, 0}, {4, 0, 0}, {5, 1, 0}, {6, -1, 0}});
    ASSERT_TRUE(c.isValid());
    const NurbsCurve b1 = NurbsKnotRefinement::bezierDecomposition(c);
    ASSERT_TRUE(b1.isValid());
    const NurbsCurve b2 = NurbsKnotRefinement::bezierDecomposition(b1);
    ASSERT_TRUE(b2.isValid());
    EXPECT_EQ(b2.controlPointCount(), b1.controlPointCount())
        << "decomposing an already-decomposed curve added knots";
    EXPECT_LT(maxDeviation(c, b1), 1e-4);
    EXPECT_LT(maxDeviation(b1, b2), 1e-5);
}

TEST(NurbsKnotRefinement, KnotMultiplicityCountsRepeatedKnots) {
    NurbsCurve c = makeCubicCurve();  // {0,0,0,0, 1,1,1,1}
    EXPECT_EQ(NurbsKnotRefinement::knotMultiplicity(c, 0.f), 4u);
    EXPECT_EQ(NurbsKnotRefinement::knotMultiplicity(c, 1.f), 4u);
    EXPECT_EQ(NurbsKnotRefinement::knotMultiplicity(c, 0.5f), 0u);
    const NurbsCurve r = NurbsKnotRefinement::insertKnot(c, 0.5f);
    EXPECT_EQ(NurbsKnotRefinement::knotMultiplicity(r, 0.5f), 1u);
}

// A sweep over degrees and knot layouts: none of the three operations may move the curve.
TEST(NurbsKnotRefinement, NoRefinementOperationMovesTheCurveAcrossASweep) {
    std::mt19937 rng(20260801u);
    std::uniform_real_distribution<float> co(-1.f, 1.f);

    int cases = 0;
    for (int it = 0; it < 60; ++it) {
        const int32_t deg = 2 + (it % 3);
        const int32_t n = deg + 2 + (it % 4);
        std::vector<Vec3> ctl;
        for (int32_t i = 0; i < n; ++i) ctl.push_back({co(rng), co(rng), co(rng)});
        std::vector<float> knots;
        for (int32_t i = 0; i <= deg; ++i) knots.push_back(0.f);
        const int32_t inner = n - deg - 1;
        for (int32_t i = 1; i <= inner; ++i)
            knots.push_back(static_cast<float>(i) / static_cast<float>(inner + 1));
        for (int32_t i = 0; i <= deg; ++i) knots.push_back(1.f);

        const NurbsCurve c(deg, knots, ctl);
        if (!c.isValid()) continue;
        ++cases;

        const NurbsCurve ins = NurbsKnotRefinement::insertKnot(c, 0.13f + 0.7f * ((it % 7) / 7.f));
        const NurbsCurve elev = NurbsKnotRefinement::degreeElevate(c, deg + 1);
        const NurbsCurve bez = NurbsKnotRefinement::bezierDecomposition(c);

        ASSERT_TRUE(ins.isValid()) << "it=" << it;
        ASSERT_TRUE(elev.isValid()) << "it=" << it;
        ASSERT_TRUE(bez.isValid()) << "it=" << it;
        EXPECT_EQ(elev.degree(), deg + 1) << "it=" << it;
        EXPECT_LT(maxDeviation(c, ins), 1e-4) << "insertKnot moved the curve at it=" << it;
        EXPECT_LT(maxDeviation(c, elev), 1e-4) << "degreeElevate moved the curve at it=" << it;
        EXPECT_LT(maxDeviation(c, bez), 1e-4) << "bezierDecomposition moved the curve at it=" << it;
    }
    EXPECT_GT(cases, 40) << "the sweep did not run";
}
