#include <gtest/gtest.h>

#include <nexus/geometry/NurbsCurve.h>

#include <cmath>
#include <vector>

using namespace nexus::geometry;

static NurbsCurve makeLinearCurve() {
    return NurbsCurve(1, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {2,0,0}});
}

static NurbsCurve makeCubicCurve() {
    return NurbsCurve(3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {{0,0,0}, {1,2,0}, {3,2,0}, {4,0,0}});
}

TEST(NurbsCurve, CreateValidReturnsValue) {
    NurbsCurve c(2, {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
                 {{0,0,0}, {0,1,0}, {1,1,0}});
    EXPECT_TRUE(c.isValid());
}

TEST(NurbsCurve, InvalidDegreeZeroReturnsNullopt) {
    NurbsCurve c(0, {0.f, 0.f, 1.f, 1.f}, {{0,0,0}, {1,0,0}});
    EXPECT_FALSE(c.isValid());
}

TEST(NurbsCurve, TooFewControlPointsReturnsNullopt) {
    NurbsCurve c(3, {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
                 {{0,0,0}, {1,0,0}, {2,0,0}});
    EXPECT_FALSE(c.isValid());
}

TEST(NurbsCurve, InconsistentKnotsReturnsNullopt) {
    NurbsCurve c(2, {0.f, 0.f, 1.f, 1.f},
                 {{0,0,0}, {1,1,0}, {2,0,0}});
    EXPECT_FALSE(c.isValid());
}

TEST(NurbsCurve, LinearCurveEvalAtEndpointsAndMidpoint) {
    NurbsCurve c = makeLinearCurve();
    ASSERT_TRUE(c.isValid());

    Vec3 p0 = c.evaluate(0.f);
    EXPECT_FLOAT_EQ(p0.x, 0.f);
    EXPECT_FLOAT_EQ(p0.y, 0.f);
    EXPECT_FLOAT_EQ(p0.z, 0.f);

    Vec3 p1 = c.evaluate(1.f);
    EXPECT_FLOAT_EQ(p1.x, 2.f);
    EXPECT_FLOAT_EQ(p1.y, 0.f);
    EXPECT_FLOAT_EQ(p1.z, 0.f);

    Vec3 pmid = c.evaluate(0.5f);
    EXPECT_FLOAT_EQ(pmid.x, 1.f);
    EXPECT_FLOAT_EQ(pmid.y, 0.f);
    EXPECT_FLOAT_EQ(pmid.z, 0.f);
}

TEST(NurbsCurve, CubicCurveEndsInterpolate) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());

    Vec3 p0 = c.evaluate(0.f);
    EXPECT_FLOAT_EQ(p0.x, 0.f);
    EXPECT_FLOAT_EQ(p0.y, 0.f);
    EXPECT_FLOAT_EQ(p0.z, 0.f);

    Vec3 p1 = c.evaluate(1.f);
    EXPECT_FLOAT_EQ(p1.x, 4.f);
    EXPECT_FLOAT_EQ(p1.y, 0.f);
    EXPECT_FLOAT_EQ(p1.z, 0.f);
}

TEST(NurbsCurve, FirstDerivativeNonZero) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    Vec3 d = c.derivative(0.5f, 1);
    float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    EXPECT_GT(len, 0.f);
}

TEST(NurbsCurve, SecondDerivativeNearZeroOnStraight) {
    NurbsCurve c(3,
        {0.f, 0.f, 0.f, 0.f, 1.f, 1.f, 1.f, 1.f},
        {{0,0,0}, {1,0,0}, {2,0,0}, {3,0,0}});
    ASSERT_TRUE(c.isValid());
    Vec3 d2 = c.derivative(0.5f, 2);
    float len = std::sqrt(d2.x * d2.x + d2.y * d2.y + d2.z * d2.z);
    EXPECT_NEAR(len, 0.f, 1e-4f);
}

TEST(NurbsCurve, FindSpanAtDomainEnd) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int span0 = c.findSpan(0.f);
    int span1 = c.findSpan(1.f);
    EXPECT_GE(span0, c.degree());
    EXPECT_LT(span1, c.controlPointCount());
}

TEST(NurbsCurve, DomainReturnsCorrectRange) {
    NurbsCurve c = makeLinearCurve();
    ASSERT_TRUE(c.isValid());
    auto [u0, u1] = c.domain();
    EXPECT_FLOAT_EQ(u0, 0.f);
    EXPECT_FLOAT_EQ(u1, 1.f);
}

TEST(NurbsCurve, KnotInsertionPreservesEndpoints) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    Vec3 before0 = c.evaluate(0.f);
    Vec3 before1 = c.evaluate(1.f);

    NurbsCurve refined = c.insertKnot(0.4f);
    EXPECT_TRUE(refined.isValid());
    Vec3 after0 = refined.evaluate(0.f);
    Vec3 after1 = refined.evaluate(1.f);

    EXPECT_NEAR(after0.x, before0.x, 1e-5f);
    EXPECT_NEAR(after0.y, before0.y, 1e-5f);
    EXPECT_NEAR(after1.x, before1.x, 1e-5f);
    EXPECT_NEAR(after1.y, before1.y, 1e-5f);
}

TEST(NurbsCurve, KnotInsertionIncreasesCPCount) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int before = c.controlPointCount();
    NurbsCurve refined = c.insertKnot(0.5f, 2);
    EXPECT_EQ(refined.controlPointCount(), before + 2);
}

TEST(NurbsCurve, OutOfRangeKnotInsertionClamped) {
    NurbsCurve c = makeLinearCurve();
    ASSERT_TRUE(c.isValid());
    NurbsCurve r = c.insertKnot(-0.5f, 1);
    EXPECT_TRUE(r.isValid());
    EXPECT_GE(r.controlPointCount(), c.controlPointCount());
}

TEST(NurbsCurve, RationalCurveWithWeights) {
    NurbsCurve c(2, {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
                 {{0,0,0}, {0,1,0}, {1,1,0}},
                 {1.f, 2.f, 1.f});
    EXPECT_TRUE(c.isValid());
    EXPECT_TRUE(c.isRational());
    EXPECT_FLOAT_EQ(c.weights()[1], 2.f);
}

TEST(NurbsCurve, NonRationalWhenNoWeights) {
    NurbsCurve c = makeCubicCurve();
    EXPECT_TRUE(c.isValid());
    EXPECT_FALSE(c.isRational());
}

TEST(NurbsCurve, BasisFunsSumsToOne) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    int span = c.findSpan(0.3f);
    std::vector<float> basis(c.degree() + 1);
    c.basisFuns(span, 0.3f, basis);
    float sum = 0.f;
    for (float b : basis) sum += b;
    EXPECT_NEAR(sum, 1.f, 1e-5f);
}

TEST(NurbsCurve, DerivativeDifferentOrders) {
    NurbsCurve c = makeCubicCurve();
    ASSERT_TRUE(c.isValid());
    Vec3 d1 = c.derivative(0.3f, 1);
    Vec3 d2 = c.derivative(0.3f, 2);
    Vec3 d3 = c.derivative(0.3f, 3);
    EXPECT_GT(d1.x * d1.x + d1.y * d1.y + d1.z * d1.z, 0.f);
    EXPECT_GE(d3.x * d3.x + d3.y * d3.y + d3.z * d3.z, 0.f);
}

// The derivative had been fundamentally wrong: after differencing the control points it
// returned a single derivative control point instead of combining them with the
// lower-degree basis, so it was correct only for a linear curve. A unit circle's tangent
// came out ~70 degrees off, and anything built on it — arc length, closest point, offsets,
// swept frames — was wrong. These check it against finite differences and against the exact
// geometry of a rational NURBS circle. The prior test only asserted |derivative| >= 0,
// which no broken direction could ever fail.

namespace {
NurbsCurve makeUnitCircle()
{
    const float w = 1.f / std::sqrt(2.f);
    return NurbsCurve(
        2,
        {0, 0, 0, 0.25f, 0.25f, 0.5f, 0.5f, 0.75f, 0.75f, 1, 1, 1},
        {{1, 0, 0}, {1, 1, 0}, {0, 1, 0}, {-1, 1, 0}, {-1, 0, 0},
         {-1, -1, 0}, {0, -1, 0}, {1, -1, 0}, {1, 0, 0}},
        {1, w, 1, w, 1, w, 1, w, 1});
}
}  // namespace

TEST(NurbsCurve, FirstDerivativeMatchesFiniteDifference)
{
    // A rational cubic with genuinely varying weights (exercises the quotient rule).
    NurbsCurve c(3, {0, 0, 0, 0, 0.5f, 1, 1, 1, 1},
                 {{0, 0, 0}, {1, 2, 0}, {3, -1, 1}, {4, 1, 0}, {5, 0, 2}},
                 {1, 2, 0.5f, 3, 1});
    const float h = 1e-3f;
    for (int i = 5; i < 95; ++i) {
        const float u = 0.01f * static_cast<float>(i);
        const Vec3 d = c.derivative(u, 1);
        const Vec3 a = c.evaluate(u - h), b = c.evaluate(u + h);
        EXPECT_NEAR(d.x, (b.x - a.x) / (2 * h), 5e-3f) << "at u=" << u;
        EXPECT_NEAR(d.y, (b.y - a.y) / (2 * h), 5e-3f) << "at u=" << u;
        EXPECT_NEAR(d.z, (b.z - a.z) / (2 * h), 5e-3f) << "at u=" << u;
    }
}

TEST(NurbsCurve, SecondDerivativeMatchesFiniteDifference)
{
    NurbsCurve c(3, {0, 0, 0, 0, 0.5f, 1, 1, 1, 1},
                 {{0, 0, 0}, {1, 2, 0}, {3, -1, 1}, {4, 1, 0}, {5, 0, 2}},
                 {1, 2, 0.5f, 3, 1});
    const float h = 2e-3f;
    for (int i = 10; i < 90; ++i) {
        const float u = 0.01f * static_cast<float>(i);
        // Skip the interior knot at 0.5: a centered finite difference of the first
        // derivative straddles the span boundary there and is not a valid reference, even
        // though the analytic second derivative is continuous (the curve is C2 at a
        // multiplicity-1 knot). The first-derivative test already covers this point.
        if (std::abs(u - 0.5f) < 2.f * h) continue;
        const Vec3 d2 = c.derivative(u, 2);
        const Vec3 da = c.derivative(u - h, 1), db = c.derivative(u + h, 1);
        EXPECT_NEAR(d2.x, (db.x - da.x) / (2 * h), 2e-2f) << "at u=" << u;
        EXPECT_NEAR(d2.y, (db.y - da.y) / (2 * h), 2e-2f) << "at u=" << u;
        EXPECT_NEAR(d2.z, (db.z - da.z) / (2 * h), 2e-2f) << "at u=" << u;
    }
}

TEST(NurbsCurve, CircleTangentIsPerpendicularToTheRadius)
{
    const NurbsCurve circle = makeUnitCircle();
    for (int i = 1; i < 200; ++i) {
        const float u = static_cast<float>(i) / 200.f;
        const Vec3 p = circle.evaluate(u);
        const Vec3 d = circle.derivative(u, 1);
        const float dl = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        const float pl = std::sqrt(p.x * p.x + p.y * p.y);
        ASSERT_GT(dl, 1e-6f);
        ASSERT_GT(pl, 1e-6f);
        // Tangent of a circle is perpendicular to the position (radius) vector.
        EXPECT_NEAR((d.x * p.x + d.y * p.y) / (dl * pl), 0.f, 1e-4f) << "at u=" << u;
    }
}
