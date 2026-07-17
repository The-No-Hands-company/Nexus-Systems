// Foundation — non-destructive analytic B-rep feature stack. A parametric history
// (base + ordered modifiers) whose evaluate() replays from scratch, so editing a
// feature's parameters and re-evaluating cascades the change while the base is
// untouched — the modern (non-destructive) modelling pipeline for the B-rep.

#include <nexus/geometry/BRepFeatureStack.h>

#include <gtest/gtest.h>

#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
double vol(const Body& b) { return static_cast<double>(b.massProperties().volume); }
}  // namespace

TEST(BRepFeatureStack, EvaluatesBaseAndModifier)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::chamfer(0, +1, +1, 0.4f));  // wedge ½·0.4²·2 = 0.16

    const Body r = s.evaluate();
    EXPECT_TRUE(r.checkIntegrity().ok);
    EXPECT_TRUE(r.isClosed());
    EXPECT_NEAR(vol(r), 8.0 - 0.16, 1e-4);
}

TEST(BRepFeatureStack, EditingAParameterCascadesAndIsPure)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::chamfer(0, +1, +1, 0.4f));

    const double before = vol(s.evaluate());  // 8 − 0.16
    // Re-evaluation is pure: the same stack yields byte-identical output.
    EXPECT_EQ(s.evaluate().serialize(), s.evaluate().serialize());

    s.at(1).amount = 0.3f;  // smaller chamfer
    const double after = vol(s.evaluate());  // 8 − 0.09
    EXPECT_GT(after, before);                // removed less material
    EXPECT_NEAR(after, 8.0 - 0.09, 1e-4);

    // The base feature is untouched by evaluation/editing.
    EXPECT_EQ(s.at(0).kind, FeatureKind::Box);
    EXPECT_FLOAT_EQ(s.at(0).w, 2.f);
}

TEST(BRepFeatureStack, InsertAndRemove)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::chamfer(0, +1, +1, 0.4f));
    const double justChamfer = vol(s.evaluate());

    // Add a fillet on a non-interacting (opposite) edge → removes more material.
    s.add(Feature::fillet(0, -1, -1, 0.3f, 8));
    const double withFillet = vol(s.evaluate());
    EXPECT_LT(withFillet, justChamfer);
    EXPECT_TRUE(s.evaluate().isClosed());

    // Removing it returns to the chamfer-only result.
    s.remove(2);
    EXPECT_NEAR(vol(s.evaluate()), justChamfer, 1e-4);
}

TEST(BRepFeatureStack, MultipleModifiers)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::chamfer(0, +1, +1, 0.3f));   // 8 − 0.09
    s.add(Feature::chamfer(0, -1, -1, 0.3f));   // − 0.09 more
    const Body r = s.evaluate();
    EXPECT_TRUE(r.checkIntegrity().ok);
    EXPECT_TRUE(r.isClosed());
    EXPECT_NEAR(vol(r), 8.0 - 0.18, 1e-4);
}

TEST(BRepFeatureStack, BaseVariety)
{
    for (const Feature& base :
         {Feature::box(2.f, 2.f, 2.f), Feature::facetedCylinder(1.f, 2.f, 12),
          Feature::facetedSphere(1.f, 6, 8)}) {
        FeatureStack s;
        s.add(base);
        const Body r = s.evaluate();
        EXPECT_TRUE(r.checkIntegrity().ok);
        EXPECT_TRUE(r.isClosed());
        EXPECT_GT(vol(r), 0.0);
    }
    // Extrude base.
    FeatureStack e;
    e.add(Feature::extrude({{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}}, {0, 0, 3}));
    EXPECT_NEAR(vol(e.evaluate()), 3.0, 1e-4);
}

TEST(BRepFeatureStack, EmptyOrNonBaseFirstIsCleanEmpty)
{
    EXPECT_EQ(FeatureStack{}.evaluate().faceCount(), 0u);  // empty stack

    FeatureStack s;
    s.add(Feature::chamfer(0, +1, +1, 0.4f));  // first feature is a modifier, not a base
    EXPECT_EQ(s.evaluate().faceCount(), 0u);
    EXPECT_TRUE(s.evaluate().checkIntegrity().ok);  // clean empty, not corrupt
}

TEST(BRepFeatureStack, EvaluateNeverCorruptAndDeterministic)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 3.f, 4.f));
    s.add(Feature::fillet(1, +1, +1, 0.4f, 8));
    // A degenerate modifier (a chamfer that fails to sew) is skipped, never
    // corrupting the running solid.
    s.add(Feature::chamfer(2, +1, +1, 0.35f));

    const Body r = s.evaluate();
    EXPECT_TRUE(r.checkIntegrity().ok) << r.checkIntegrity().reason;
    EXPECT_EQ(s.evaluate().serialize(), s.evaluate().serialize());  // deterministic
}

}  // namespace nexus::geometry::brep::testing
