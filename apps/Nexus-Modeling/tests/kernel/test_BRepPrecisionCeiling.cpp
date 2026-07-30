// Foundation — WHERE THE B-REP'S PRECISION CEILING ACTUALLY IS.
//
// The analytic B-rep stores coordinates and curve parameters as double, so a vertex and
// the curve it sits on should agree to somewhere near double's own resolution. For a long
// time they agreed only to ~4.4e-8 — single-precision resolution — even after every
// stored field had been widened, and the reason was three lines of file-local helper:
//
//     float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
//     float length(const Vec3& a)             { return std::sqrt(dot(a, a)); }
//
// Every arc parameter in the kernel is an `atan2(dot(w, bi), dot(w, ref))`, and atan2
// cannot return a double angle from float arguments. So each parameter was rounded to
// float on its way out of the helper, whatever width the points had. The fingerprint was
// unmistakable once measured: the worst residual was 4.3711390e-08, and 0.5·sin(float π)
// is 4.3711390e-08 exactly — a radius times the error in a float-rounded π.
//
// These tests pin the ceiling by measurement rather than by inspecting types, because the
// defect was invisible in the types: every declaration involved was already double, and
// the narrowing happened in a return value three calls down.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace nexus::geometry::brep::testing {


namespace {

// The worst distance, over every live edge of `b`, between an edge endpoint vertex and
// the point its own curve evaluates to at the matching parameter. This is the tightest
// self-consistency the representation has: the two should be the same point.
double worstCurveVertexMismatch(const Body& b)
{
    double worst = 0.0;
    for (uint32_t e = 0; e < static_cast<uint32_t>(b.edgeCount()); ++e) {
        const auto& ed = b.edge(e);
        if (!ed.alive || ed.curve == kInvalid) continue;
        const auto& c = b.curve(ed.curve);
        for (int end = 0; end < 2; ++end) {
            const Vec3 d = c.eval(end ? ed.t1 : ed.t0) - b.vertex(end ? ed.v1 : ed.v0).point;
            worst = std::max(worst, std::sqrt(d.dot(d)));
        }
    }
    return worst;
}

// Single precision resolves about 1e-7 relative; the measured value after the fix is ~2e-16
// at every scale. This bound sits four orders below float and four above what the kernel
// actually achieves, so it catches a float slipping back in without pinning the last bits.
constexpr double kFloatFloor = 1e-12;

}  // namespace

// THE assertion this file exists for. A cylinder-through-box imprint builds arcs by
// projecting each vertex onto its circle's frame, which is the exact path that was being
// rounded. The box's own edges must now agree with their curves at double resolution.
TEST(BRepPrecisionCeiling, ImprintedArcsAgreeWithTheirVerticesAtDoubleResolution)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(0.5f, 4.f, 16);
    ASSERT_TRUE(imprintMutually(box, cyl));

    // BOTH operands. The box's arcs come from the imprint's parameter pair and the
    // cylinder's from the seam ring, and the two were being rounded by different helpers —
    // fixing one left the other at 4.371e-08, so a single-body assertion would have passed
    // a body that was still entirely single-precision.
    for (const Body* b : {&box, &cyl}) {
        const double worst = worstCurveVertexMismatch(*b);
        EXPECT_LT(worst, kFloatFloor)
            << "worst curve/vertex mismatch is " << worst
            << ", which is single-precision resolution — an arc parameter is being rounded "
               "to float somewhere in the construction chain (the historical value was "
               "4.371e-08, i.e. 0.5·sin(float pi))";
    }
}

// The ceiling must be a RELATIVE one. A float in the chain shows up as a fixed ~1e-7
// relative error that grows in absolute terms with the model; genuine double arithmetic
// keeps the relative error near 1e-16 at every scale. Testing one scale cannot tell the
// two apart, which is why the original defect survived so long.
TEST(BRepPrecisionCeiling, TheCeilingIsRelativeNotAbsolute)
{
    for (const float scale : {1.f, 100.f, 10000.f}) {
        Body box = makeBox(2.f * scale, 2.f * scale, 2.f * scale);
        Body cyl = makeCylinder(0.5f * scale, 4.f * scale, 16);
        ASSERT_TRUE(imprintMutually(box, cyl)) << "scale=" << scale;

        const double relative = std::max(worstCurveVertexMismatch(box),
                                         worstCurveVertexMismatch(cyl))
                                / static_cast<double>(scale);
        EXPECT_LT(relative, kFloatFloor)
            << "at scale " << scale << " the relative mismatch is " << relative
            << " — a fixed relative error near 1e-7 at every scale is the signature of a "
               "float in the chain, not of accumulated rounding";
    }
}

// The direct fingerprint. An arc that runs to the far side of its circle should have a
// parameter equal to double pi, not to float pi. Before the fix `t` came back as
// 3.1415927410125732f — the float value, widened — and every point evaluated from it
// inherited that 8.7e-8 angular error.
TEST(BRepPrecisionCeiling, ArcParametersCarryDoublePiNotFloatPi)
{
    // A plain cylinder's rim is built as per-segment arcs, none of which is a half turn.
    // The half-turn parameters live on the SEAM RING an imprint builds, which is also the
    // code path that was doing the rounding.
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(0.5f, 4.f, 16);
    ASSERT_TRUE(imprintMutually(box, cyl));

    const double floatPi = static_cast<double>(static_cast<float>(M_PI));
    ASSERT_NE(floatPi, M_PI) << "the two pis must differ for this test to mean anything";

    int sawNearPi = 0;
    for (const Body* b : {&box, &cyl}) {
        for (uint32_t e = 0; e < static_cast<uint32_t>(b->edgeCount()); ++e) {
            const auto& ed = b->edge(e);
            if (!ed.alive || ed.curve == kInvalid) continue;
            if (b->curve(ed.curve).kind != CurveKind::Circle) continue;
            for (const double t : {ed.t0, ed.t1}) {
                if (std::abs(std::abs(t) - M_PI) > 1e-6) continue;  // not a half-turn
                ++sawNearPi;
                EXPECT_LT(std::abs(std::abs(t) - M_PI), 1e-15)
                    << "half-turn arc parameter " << t << " differs from pi by "
                    << std::abs(std::abs(t) - M_PI) << "; float pi would differ by "
                    << std::abs(floatPi - M_PI);
            }
        }
    }
    EXPECT_GT(sawNearPi, 0) << "no half-turn arc parameter found — the fixture no longer "
                               "exercises what this test is about";
}

// Widening the helpers must not have cost exactness anywhere: the same imprint still
// produces a valid, closed, correctly-measured pair of solids. Precision work that
// quietly breaks topology is not an improvement.
TEST(BRepPrecisionCeiling, WideningTheHelpersKeptTheImprintSound)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(0.5f, 4.f, 16);
    ASSERT_TRUE(imprintMutually(box, cyl));

    for (const Body* b : {&box, &cyl}) {
        EXPECT_TRUE(b->checkIntegrity().ok) << b->checkIntegrity().reason;
        EXPECT_TRUE(b->checkGeometry().ok) << b->checkGeometry().reason;
        EXPECT_TRUE(b->isClosed());
        EXPECT_EQ(b->checkIntegrity().boundaryEdges, 0u);
    }
    // An imprint segments boundaries and never removes material, so both volumes stand.
    EXPECT_NEAR(box.massProperties().volume, 8.0, 1e-9);
}

}  // namespace nexus::geometry::brep::testing
