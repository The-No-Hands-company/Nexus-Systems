// Foundation — non-destructive analytic B-rep feature stack. A parametric history
// (base + ordered modifiers) whose evaluate() replays from scratch, so editing a
// feature's parameters and re-evaluating cascades the change while the base is
// untouched — the modern (non-destructive) modelling pipeline for the B-rep.

#include <nexus/geometry/BRepFeatureStack.h>

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
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

namespace {
// A non-trivial editable model: an extruded profile, a non-identity transform,
// and two edge treatments on non-interacting edges.
FeatureStack sampleStack()
{
    nexus::render::Mat4 t = nexus::render::Mat4::identity();
    t.m[0][3] = 5.f;
    t.m[1][3] = -2.f;
    FeatureStack s;
    s.add(Feature::extrude({{0, 0, 0}, {2, 0, 0}, {2, 1, 0}, {0, 1, 0}}, {0, 0, 3}));
    s.add(Feature::transform(t));
    s.add(Feature::chamfer(0, +1, +1, 0.3f));
    s.add(Feature::fillet(2, -1, -1, 0.25f, 8));
    return s;
}
}  // namespace

TEST(BRepFeatureStackSerialize, RoundTripsHistoryAndResult)
{
    const FeatureStack s = sampleStack();
    const std::vector<std::uint8_t> bytes = s.serialize();
    const auto rt = FeatureStack::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());

    EXPECT_EQ(rt->size(), s.size());
    // The parametric HISTORY is canonical: re-serialisation is byte-identical.
    EXPECT_EQ(bytes, rt->serialize());
    // And the RESULT survives: the decoded stack evaluates to the same solid.
    EXPECT_EQ(s.evaluate().serialize(), rt->evaluate().serialize());
    EXPECT_EQ(s.serialize(), s.serialize());  // deterministic output
}

TEST(BRepFeatureStackSerialize, DecodedStackIsStillEditable)
{
    auto rt = FeatureStack::deserialize(sampleStack().serialize());
    ASSERT_TRUE(rt.has_value());
    const double before = static_cast<double>(rt->evaluate().massProperties().volume);
    rt->at(2).amount = 0.15f;  // shrink the chamfer
    const double after = static_cast<double>(rt->evaluate().massProperties().volume);
    EXPECT_GT(after, before);  // removed less material
}

TEST(BRepFeatureStackSerialize, EmptyStackRoundTrips)
{
    const auto rt = FeatureStack::deserialize(FeatureStack{}.serialize());
    ASSERT_TRUE(rt.has_value());
    EXPECT_EQ(rt->size(), 0u);
    EXPECT_EQ(rt->evaluate().faceCount(), 0u);
}

TEST(BRepFeatureStackSerialize, RejectsBadAndNonFiniteBuffers)
{
    EXPECT_FALSE(FeatureStack::deserialize({}).has_value());                       // empty
    EXPECT_FALSE(FeatureStack::deserialize({1, 2, 3, 4, 5, 6, 7, 8}).has_value());  // bad magic

    std::vector<std::uint8_t> good = sampleStack().serialize();
    std::vector<std::uint8_t> truncated(good.begin(), good.begin() + good.size() / 2);
    EXPECT_FALSE(FeatureStack::deserialize(truncated).has_value());

    // Corrupt the first feature's `w` float (after 4-byte magic + version + count
    // + 1-byte kind) to +Inf.
    std::vector<std::uint8_t> nf = good;
    ASSERT_GT(nf.size(), 17u);
    nf[13] = 0x00;
    nf[14] = 0x00;
    nf[15] = 0x80;
    nf[16] = 0x7F;  // 0x7F800000 = +Inf
    EXPECT_FALSE(FeatureStack::deserialize(nf).has_value());
}

// ──────────── 2-body Boolean feature (parametric CSG tree) ───────────────────

namespace {
// A translation matrix (row-major, translation in the last column).
nexus::render::Mat4 translate(float x, float y, float z)
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    return m;
}
// A box centred at the origin, translated by (t,t,t): [−1,1]³ → [t−1,t+1]³.
std::vector<Feature> shiftedUnitCube(float t)
{
    return {Feature::box(2.f, 2.f, 2.f), Feature::transform(translate(t, t, t))};
}
}  // namespace

TEST(BRepFeatureStackBoolean, UnionMergesOverlappingCubes)
{
    // Base [−1,1]³ (vol 8) ∪ secondary [0,2]³ (vol 8), overlap [0,1]³ (vol 1).
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::booleanWith(BooleanOp::Union, shiftedUnitCube(1.f)));

    const Body r = s.evaluate();
    EXPECT_TRUE(r.checkIntegrity().ok);
    EXPECT_TRUE(r.isClosed());
    EXPECT_NEAR(vol(r), 8.0 + 8.0 - 1.0, 1e-4);  // 15
}

TEST(BRepFeatureStackBoolean, DifferenceCutsOverlap)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::booleanWith(BooleanOp::Difference, shiftedUnitCube(1.f)));

    const Body r = s.evaluate();
    EXPECT_TRUE(r.checkIntegrity().ok);
    EXPECT_TRUE(r.isClosed());
    EXPECT_NEAR(vol(r), 8.0 - 1.0, 1e-4);  // 7
}

TEST(BRepFeatureStackBoolean, IntersectionKeepsOverlap)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::booleanWith(BooleanOp::Intersection, shiftedUnitCube(1.f)));

    const Body r = s.evaluate();
    EXPECT_TRUE(r.checkIntegrity().ok);
    EXPECT_TRUE(r.isClosed());
    EXPECT_NEAR(vol(r), 1.0, 1e-4);  // [0,1]³
}

TEST(BRepFeatureStackBoolean, EditingSecondaryParameterCascades)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::booleanWith(BooleanOp::Difference, shiftedUnitCube(1.f)));

    const double before = vol(s.evaluate());  // 8 − 1 = 7
    // Push the cutter further out so the overlap shrinks (t=1.5 → [0.5,1]³ = 0.125).
    s.at(1).secondary[1] = Feature::transform(translate(1.5f, 1.5f, 1.5f));
    const double after = vol(s.evaluate());    // 8 − 0.125 = 7.875
    EXPECT_GT(after, before);
    EXPECT_NEAR(after, 8.0 - 0.125, 1e-4);
}

TEST(BRepFeatureStackBoolean, NeverCorruptOnDegenerateSecondary)
{
    // A secondary whose first feature is a modifier (not a base) evaluates to an
    // empty Body → the Boolean is skipped, leaving the base untouched.
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::booleanWith(BooleanOp::Union, {Feature::chamfer(0, 1, 1, 0.3f)}));

    const Body r = s.evaluate();
    EXPECT_TRUE(r.checkIntegrity().ok);
    EXPECT_NEAR(vol(r), 8.0, 1e-4);  // unchanged base
}

TEST(BRepFeatureStackBoolean, SerializesAndRoundTrips)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::booleanWith(BooleanOp::Difference, shiftedUnitCube(1.f)));
    s.add(Feature::chamfer(2, -1, -1, 0.2f));  // a modifier AFTER the boolean

    const std::vector<std::uint8_t> bytes = s.serialize();
    const auto rt = FeatureStack::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());
    EXPECT_EQ(rt->size(), s.size());
    EXPECT_EQ(bytes, rt->serialize());  // history is canonical
    EXPECT_EQ(rt->at(1).secondary.size(), 2u);
    EXPECT_EQ(rt->at(1).booleanOp, BooleanOp::Difference);
    EXPECT_EQ(s.evaluate().serialize(), rt->evaluate().serialize());
}

TEST(BRepFeatureStackBoolean, NestedBooleanRoundTrips)
{
    // A Boolean whose secondary itself contains a Boolean — recursive history.
    std::vector<Feature> inner;
    inner.push_back(Feature::box(2.f, 2.f, 2.f));
    inner.push_back(Feature::booleanWith(BooleanOp::Difference, shiftedUnitCube(1.f)));

    FeatureStack s;
    s.add(Feature::box(4.f, 4.f, 4.f));
    s.add(Feature::booleanWith(BooleanOp::Union, std::move(inner)));

    const auto rt = FeatureStack::deserialize(s.serialize());
    ASSERT_TRUE(rt.has_value());
    EXPECT_EQ(s.serialize(), rt->serialize());
    EXPECT_EQ(rt->at(1).secondary.at(1).secondary.size(), 2u);
}

TEST(BRepFeatureStackBoolean, DecodesLegacyV1Blob)
{
    // Build a v1-era stack (no Boolean features) and patch the version byte back
    // to 1 — it must still decode identically under the v2 reader, proving the
    // common record layout is unchanged.
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::chamfer(0, +1, +1, 0.3f));
    s.add(Feature::fillet(2, -1, -1, 0.25f, 8));

    std::vector<std::uint8_t> bytes = s.serialize();
    ASSERT_GT(bytes.size(), 8u);
    // Layout: [magic u32][version u32]... — version starts at byte 4.
    EXPECT_EQ(bytes[4], 2u);  // current writer stamps v2
    bytes[4] = 1u;            // masquerade as a v1 blob

    const auto rt = FeatureStack::deserialize(bytes);
    ASSERT_TRUE(rt.has_value());
    EXPECT_EQ(rt->size(), 3u);
    EXPECT_EQ(s.evaluate().serialize(), rt->evaluate().serialize());
}

TEST(BRepFeatureStackBoolean, IsDeterministic)
{
    FeatureStack s;
    s.add(Feature::box(2.f, 2.f, 2.f));
    s.add(Feature::booleanWith(BooleanOp::Intersection, shiftedUnitCube(1.f)));
    EXPECT_EQ(s.evaluate().serialize(), s.evaluate().serialize());
    EXPECT_EQ(s.serialize(), s.serialize());
}

}  // namespace nexus::geometry::brep::testing
