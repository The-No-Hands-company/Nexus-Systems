#include <gtest/gtest.h>

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsCurveArcLength.h>

#include <cmath>

using namespace nexus::geometry;

namespace {

NurbsCurve makeLine(float length)
{
    std::vector<float> knots = {0.f, 0.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {0.f, 0.f, 0.f},
        {length, 0.f, 0.f},
    };
    return NurbsCurve(1, std::move(knots), std::move(ctl));
}

// The exact rational-quadratic full unit circle (9 control points, 4 quarter arcs). Its arc
// length is 2*pi. With the previous broken curve derivative it integrated to about 8.03; a
// straight line was the only curve any arc-length test exercised, and degree 1 is exactly
// the case where that bug happened to give the right answer.
NurbsCurve makeUnitCircle()
{
    const float s = 1.f / std::sqrt(2.f);
    std::vector<float> knots = {0.f, 0.f, 0.f, 0.25f, 0.25f, 0.5f,
                                0.5f, 0.75f, 0.75f, 1.f, 1.f, 1.f};
    std::vector<Vec3> ctl = {
        {1.f, 0.f, 0.f}, {1.f, 1.f, 0.f}, {0.f, 1.f, 0.f}, {-1.f, 1.f, 0.f},
        {-1.f, 0.f, 0.f}, {-1.f, -1.f, 0.f}, {0.f, -1.f, 0.f}, {1.f, -1.f, 0.f},
        {1.f, 0.f, 0.f},
    };
    std::vector<float> w = {1.f, s, 1.f, s, 1.f, s, 1.f, s, 1.f};
    NurbsCurve c(2, std::move(knots), std::move(ctl));
    c.setWeights(std::move(w));
    return c;
}

} // namespace

TEST(NurbsCurveArcLength, StraightLineLength)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    EXPECT_NEAR(result.totalLength, 10.f, 0.02f);
}

TEST(NurbsCurveArcLength, UnitCircleLengthIsTwoPi)
{
    NurbsCurve curve = makeUnitCircle();
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    // Five-point Gauss-Legendre per quarter-arc span integrates the (rational) circle speed
    // to well under 1e-3; the point is that it is 2*pi, not the 8.03 the broken derivative
    // produced.
    EXPECT_NEAR(result.totalLength, 2.f * static_cast<float>(M_PI), 2e-3f);
}

TEST(NurbsCurveArcLength, UnitCircleHalfLengthParameterIsAntipode)
{
    NurbsCurve curve = makeUnitCircle();
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);
    // Half the perimeter lands at the antipodal point (-1, 0) — a check that the
    // length-to-parameter inversion rides on a correct speed profile.
    float param = NurbsCurveArcLength::parameterAtLength(
        curve, static_cast<float>(M_PI), result);
    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, -1.f, 0.02f);
    EXPECT_NEAR(pt.y, 0.f, 0.02f);
}

TEST(NurbsCurveArcLength, ParameterAtHalfLength)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    float param = NurbsCurveArcLength::parameterAtLength(curve, 5.f, result);

    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, 5.f, 0.02f);
    EXPECT_NEAR(pt.y, 0.f, 1e-4f);
    EXPECT_NEAR(pt.z, 0.f, 1e-4f);
}

TEST(NurbsCurveArcLength, LengthAtEndReturnsEndpoint)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    float param = NurbsCurveArcLength::parameterAtLength(curve, result.totalLength, result);

    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, 10.f, 0.02f);
    EXPECT_NEAR(pt.y, 0.f, 1e-4f);
    EXPECT_NEAR(pt.z, 0.f, 1e-4f);
}

TEST(NurbsCurveArcLength, BeyondTotalLengthClamps)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    float param = NurbsCurveArcLength::parameterAtLength(curve, 100.f, result);

    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, 10.f, 0.02f);
}

TEST(NurbsCurveArcLength, BelowZeroClampsToStart)
{
    NurbsCurve curve = makeLine(10.f);
    ASSERT_TRUE(curve.isValid());

    NurbsCurveArcLengthResult result = NurbsCurveArcLength::compute(curve);

    float param = NurbsCurveArcLength::parameterAtLength(curve, -5.f, result);

    Vec3 pt = curve.evaluate(param);
    EXPECT_NEAR(pt.x, 0.f, 0.02f);
}
