// Foundation — WHY a boolean returned an empty Body.
//
// booleanToBody's watertight-or-empty invariant makes a failure and a genuinely empty
// result the same VALUE. For a long time they were also the same EVENT: intersectSurfaces
// answers Unsupported for a surface pair it cannot express, imprintOneWay handled
// Unsupported in the same switch arm as None ("nothing to imprint"), and the imprint then
// returned SUCCESS having done no work. Three missing cone rows in the intersection table
// hid behind that for the entire life of the file, and the only symptom was a clean empty
// body — which is also what a correct answer looks like when two solids do not overlap.
//
// The value these tests protect is DISCRIMINATION. A diagnostic that said
// "UnexpressibleSeam" whenever anything went wrong would be worthless; the assertions
// below therefore pin the negative cases as hard as the positive ones, and the sharpest
// one is the arrangement fixture, whose failure is real but is NOT the SSI's fault.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <random>

namespace nexus::geometry::brep::testing {

namespace {

BooleanDiagnostic diagnose(const Body& a, const Body& b, BooleanOp op)
{
    BooleanDiagnostic d = BooleanDiagnostic::Ok;
    (void)booleanToBody(a, b, op, Tolerance{}, &d);
    return d;
}

Body translated(Body b, Vec3 t)
{
    b.translate(t);
    return b;
}

}  // namespace

TEST(BRepBooleanDiagnostic, DisjointOperandsAreEmptyResultNotAFailure)
{
    // Nothing went wrong here: two solids ten units apart genuinely have no intersection.
    // Reporting a failure would be as misleading as the old silence was.
    EXPECT_EQ(diagnose(makeBox(1.f, 1.f, 1.f), translated(makeBox(1.f, 1.f, 1.f), {10, 0, 0}),
                       BooleanOp::Intersection),
              BooleanDiagnostic::EmptyResult);
    EXPECT_EQ(diagnose(makeSphere(1.f, 8, 12), translated(makeSphere(1.f, 8, 12), {10, 0, 0}),
                       BooleanOp::Intersection),
              BooleanDiagnostic::EmptyResult);
}

TEST(BRepBooleanDiagnostic, QuarticSeamsSayUnexpressibleSeam)
{
    // A cylinder whose axis misses a sphere's centre cuts it in a quartic space curve, not
    // a circle — the pair intersectSurfaces declines. The body is empty either way; what
    // this asserts is that the empty is attributed to a MISSING CAPABILITY.
    EXPECT_EQ(diagnose(makeSphere(1.f, 8, 12), translated(makeCylinder(0.3f, 6.f, 16), {0.4, 0, 0}),
                       BooleanOp::Union),
              BooleanDiagnostic::UnexpressibleSeam);
    EXPECT_EQ(diagnose(makeCone(1.f, 2.f, 16), translated(makeCylinder(0.3f, 6.f, 16), {0.4, 0, 0}),
                       BooleanOp::Union),
              BooleanDiagnostic::UnexpressibleSeam);
}

TEST(BRepBooleanDiagnostic, TheArrangementGapIsNotBlamedOnTheSurfaceIntersector)
{
    // THE test in this file. A plate with a cylindrical bore, cut by a square bar whose
    // half-width leaves the centre cell's material in four disconnected slivers, fails —
    // and it is a real, separately-named architectural gap (the imprint's face-splitting
    // model cannot produce a face whose material is disconnected).
    //
    // Every surface involved is a plane or a cylinder, and plane∩plane and plane∩cylinder
    // are both fully supported. So the SSI is NOT the cause, and a diagnostic that blamed
    // it here would be actively misdirecting — it would inflate the measured size of the
    // SSI gap with failures no amount of surface intersection can fix.
    const Body plate = makeBox(3.f, 3.f, 1.f);
    BooleanDiagnostic bored = BooleanDiagnostic::Ok;
    const Body holed = booleanToBody(plate, makeCylinder(0.5f, 4.f, 16), BooleanOp::Difference,
                                     Tolerance{}, &bored);
    ASSERT_EQ(bored, BooleanDiagnostic::Ok) << "the bore itself must succeed for this fixture";
    ASSERT_GT(holed.faceCount(), 0u);

    for (double h : {0.40, 0.45, 0.48}) {
        const Body bar = makeBox(static_cast<float>(h * 2.0), 8.f, 4.f);
        const BooleanDiagnostic d = diagnose(holed, bar, BooleanOp::Difference);
        EXPECT_NE(d, BooleanDiagnostic::UnexpressibleSeam)
            << "half-width " << h << ": planes and cylinders only — blaming the surface "
               "intersector here would misattribute the arrangement gap";
        EXPECT_EQ(d, BooleanDiagnostic::SewFailed) << "half-width " << h;
    }
}

TEST(BRepBooleanDiagnostic, SucceedingBooleansSayOk)
{
    EXPECT_EQ(diagnose(makeBox(2.f, 2.f, 2.f), makeCylinder(0.3f, 6.f, 16), BooleanOp::Difference),
              BooleanDiagnostic::Ok);
    // Curved, and one the cone rows unblocked.
    EXPECT_EQ(diagnose(makeCone(1.f, 2.f, 16), makeCylinder(0.3f, 6.f, 16), BooleanOp::Difference),
              BooleanDiagnostic::Ok);
}

TEST(BRepBooleanDiagnostic, TheDiagnosticNeverContradictsTheBodyItAccompanies)
{
    // Ok if and only if a solid came back. A diagnostic that can disagree with the value
    // it describes is worse than none, because it will be believed.
    std::mt19937 rng(0xD1A6u);
    std::uniform_real_distribution<float> dim(0.5f, 1.8f);
    std::uniform_real_distribution<double> tr(-1.5, 1.5);
    int ok = 0, empty = 0;

    for (int i = 0; i < 240; ++i) {
        const Body a = (i % 2 == 0) ? makeBox(dim(rng), dim(rng), dim(rng))
                                    : makeCone(dim(rng), dim(rng) + 1.f, 16);
        Body b = (i % 3 == 0) ? makeSphere(dim(rng), 8, 12)
                              : makeCylinder(dim(rng) * 0.6f, dim(rng) + 2.f, 16);
        b.translate({tr(rng), tr(rng), tr(rng)});
        const BooleanOp op = (i % 3 == 0)   ? BooleanOp::Union
                             : (i % 3 == 1) ? BooleanOp::Intersection
                                            : BooleanOp::Difference;

        BooleanDiagnostic d = BooleanDiagnostic::Ok;
        const Body r = booleanToBody(a, b, op, Tolerance{}, &d);
        ASSERT_EQ(d == BooleanDiagnostic::Ok, r.faceCount() > 0u)
            << "i=" << i << " diagnostic=" << static_cast<int>(d) << " faces=" << r.faceCount();
        if (r.faceCount() > 0u) {
            ++ok;
            EXPECT_TRUE(r.isClosed()) << "i=" << i;
        } else {
            ++empty;
        }
    }
    // Both branches have to be exercised or the assertion above proves nothing.
    EXPECT_GT(ok, 0);
    EXPECT_GT(empty, 0);
}

TEST(BRepBooleanDiagnostic, OmittingTheDiagnosticChangesNothing)
{
    // The parameter is defaulted, so every existing caller keeps compiling and keeps
    // getting the identical Body. Asserted rather than assumed, because a diagnostic that
    // perturbed the result would be a cure worse than the disease.
    const Body a = makeCone(1.f, 2.f, 16);
    const Body b = makeCylinder(0.3f, 6.f, 16);
    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
        BooleanDiagnostic d = BooleanDiagnostic::Ok;
        const Body with = booleanToBody(a, b, op, Tolerance{}, &d);
        const Body without = booleanToBody(a, b, op);
        ASSERT_EQ(with.faceCount(), without.faceCount()) << "op " << static_cast<int>(op);
        ASSERT_EQ(with.vertexCount(), without.vertexCount()) << "op " << static_cast<int>(op);
        for (uint32_t v = 0; v < static_cast<uint32_t>(with.vertexCount()); ++v) {
            EXPECT_EQ(with.vertex(v).point.x, without.vertex(v).point.x) << "v=" << v;
            EXPECT_EQ(with.vertex(v).point.y, without.vertex(v).point.y) << "v=" << v;
            EXPECT_EQ(with.vertex(v).point.z, without.vertex(v).point.z) << "v=" << v;
        }
    }
}

TEST(BRepBooleanDiagnostic, ImprintMutuallyReportsADeclinedSeamSeparatelyFromRunningOut)
{
    // The imprint's return value answers "did I terminate normally", which stayed TRUE
    // through the entire cone defect. `declinedSeam` answers the different question that
    // nobody was asking: "did I skip work I was asked to do?"
    {
        Body a = makeSphere(1.f, 8, 12);
        Body b = translated(makeCylinder(0.3f, 6.f, 16), {0.4, 0, 0});
        bool declined = false;
        EXPECT_TRUE(imprintMutually(a, b, Tolerance{}, &declined)) << "it still terminates";
        EXPECT_TRUE(declined) << "and it still skipped a quartic seam";
    }
    {
        // A supported pair skips nothing.
        Body a = makeBox(2.f, 2.f, 2.f);
        Body b = makeCylinder(0.3f, 6.f, 16);
        bool declined = false;
        EXPECT_TRUE(imprintMutually(a, b, Tolerance{}, &declined));
        EXPECT_FALSE(declined);
    }
    {
        // Neither do solids that never meet — an Unsupported pair whose faces do not
        // straddle each other is not a skipped seam, it is no seam.
        Body a = makeSphere(1.f, 8, 12);
        Body b = translated(makeCylinder(0.3f, 6.f, 16), {40, 0, 0});
        bool declined = false;
        EXPECT_TRUE(imprintMutually(a, b, Tolerance{}, &declined));
        EXPECT_FALSE(declined) << "far apart: nothing was declined because nothing crossed";
    }
}

}  // namespace nexus::geometry::brep::testing
