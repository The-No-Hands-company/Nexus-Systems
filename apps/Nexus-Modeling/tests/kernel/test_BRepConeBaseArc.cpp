// A cone whose base was a ring of chords, under faces claiming an exact cone.
//
// This is the last watertight-or-empty violation in the fuzz corpus, and it turned out not
// to be a Boolean defect at all. `booleanToBody` promises a result that is either watertight
// or empty; across 2000 configurations exactly two results were neither, and both came from
// one cone.
//
// A cone's side face declares SurfaceKind::Cone — an exact conical surface, since tagging it
// Cylinder was fixed long ago as a false statement. Its bottom boundary therefore has to be
// a curve lying ON that cone. The circle where the cone meets its base plane does. The CHORD
// between two points of that circle does not: it dips inside everywhere except at its two
// endpoints. `makeCylinder` upgrades its two rims to Circle arcs after `fromFaces` for
// exactly this reason. `makeCone` never got that step, so its base stayed a ring of Line
// edges derived by `fromFaces`.
//
// Nothing caught it, and the reason is worth keeping: checkGeometry tests VERTICES against
// their faces' surfaces, and a chord's two endpoints are on the cone. The edge between them
// is not, but no vertex lives there — until an imprint puts one there.
//
// MEASURED (fuzz seed 0xA17E51, iteration 923: a cone of radius 1.3289, height 2.2657
// against a box). The imprint placed crossings on eight base chords, and the imprinted CONE
// ALONE — before any Boolean ran — failed checkGeometry, with deviations from 0.019 to
// 0.025. That propagated into the Union and the Difference, which were the two offending
// results: closed, integral, Euler-correct, and carrying vertices off their own surfaces.
//
// Making the base an arc fixes more than the invariant. Across the corpus the non-empty
// results failing validation went 2 -> 0, the sews went UP (2101 -> 2107, because a
// consistent cone imprints where an inconsistent one bailed), and the volume-conservation
// identities went from one violation of 1.3e-06 to none at all.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>

namespace nexus::geometry::brep::testing {

namespace {

Body fixtureCone()
{
    return makeCone(static_cast<float>(1.3289277660091334),
                    static_cast<float>(2.2657047371119479), 16);
}

Body fixtureBox()
{
    Body b = makeBox(static_cast<float>(0.60951220500687442),
                     static_cast<float>(1.0439983770025751),
                     static_cast<float>(1.7052497605297787));
    b.translate({-0.073120304127956781, -0.17843850846756082, -0.97760562157793995});
    return b;
}

bool watertightOrEmpty(const Body& b)
{
    return b.faceCount() == 0u ||
           (b.isClosed() && b.checkIntegrity().ok && b.checkGeometry().ok);
}

}  // namespace

// THE property the whole family rests on: every edge of a face must lie on that face's
// surface, not merely touch it at its endpoints. Sampled ALONG each edge, which is what
// distinguishes an arc from the chord that shares its ends — and what checkGeometry, being
// a vertex test, cannot see.
TEST(BRepConeBaseArc, EveryEdgeOfAConicalFaceLiesOnItsCone)
{
    for (const uint32_t n : {8u, 16u, 32u}) {
        const Body c = makeCone(1.f, 2.f, n);
        int checked = 0;
        for (uint32_t f = 0; f < static_cast<uint32_t>(c.faceCount()); ++f) {
            if (!c.face(f).alive) continue;
            const Surface& s = c.surface(c.face(f).surface);
            if (s.kind != SurfaceKind::Cone) continue;
            for (uint32_t e = 0; e < static_cast<uint32_t>(c.edgeCount()); ++e) {
                const Edge& ed = c.edge(e);
                if (!ed.alive) continue;
                // only the edges of this face's own loop
                bool mine = false;
                for (const uint32_t v : c.faceVertices(f))
                    if (v == ed.v0) mine = true;
                if (!mine) continue;
                for (int k = 1; k < 8; ++k) {
                    const double t = ed.t0 + (ed.t1 - ed.t0) * (k / 8.0);
                    const Vec3 p = c.curve(ed.curve).eval(t);
                    const Vec3 w = p - s.origin;
                    const double axial = w.dot(s.normal);
                    const Vec3 rad = w - s.normal * axial;
                    EXPECT_NEAR(std::sqrt(rad.dot(rad)), s.radius * axial, 1e-9)
                        << "n=" << n << " face " << f << " edge " << e
                        << ": the edge leaves the cone between its endpoints";
                    ++checked;
                }
            }
        }
        EXPECT_GT(checked, 0) << "n=" << n << ": the sweep did not run";
    }
}

// The base ring is arcs, like the cylinder's rims — stated directly so the mechanism is
// pinned and not just its consequence.
TEST(BRepConeBaseArc, TheBaseRingIsArcsAndTheApexSpokesAreNot)
{
    const double h = 1.0;  // makeCone(1, 2, n) → base at z = -1
    const Body c = makeCone(1.f, 2.f, 16);
    int baseArcs = 0, baseLines = 0, spokes = 0;
    for (uint32_t e = 0; e < static_cast<uint32_t>(c.edgeCount()); ++e) {
        const Edge& ed = c.edge(e);
        if (!ed.alive) continue;
        const Vec3 p0 = c.vertex(ed.v0).point, p1 = c.vertex(ed.v1).point;
        const bool onBase = std::abs(p0.z + h) < 1e-6 && std::abs(p1.z + h) < 1e-6;
        if (!onBase) {
            ++spokes;
            EXPECT_EQ(c.curve(ed.curve).kind, CurveKind::Line)
                << "an apex spoke is a straight ruling and must stay one";
            continue;
        }
        if (c.curve(ed.curve).kind == CurveKind::Circle) ++baseArcs;
        else ++baseLines;
    }
    EXPECT_EQ(baseArcs, 16) << "the base ring is not arcs — the chord ring is back";
    EXPECT_EQ(baseLines, 0);
    EXPECT_EQ(spokes, 16);
}

// The fixture that exposed it: the imprinted cone alone, with no Boolean involved.
TEST(BRepConeBaseArc, TheImprintedConeIsGeometricallyValidOnItsOwn)
{
    Body cone = fixtureCone(), box = fixtureBox();
    ASSERT_TRUE(cone.checkGeometry().ok) << "the cone is invalid before imprinting";
    ASSERT_TRUE(imprintMutually(cone, box));

    const auto g = cone.checkGeometry();
    EXPECT_TRUE(g.ok) << "the imprinted cone has a vertex off its own surface: " << g.reason;
    EXPECT_TRUE(cone.checkIntegrity().ok);
    EXPECT_TRUE(cone.isClosed());
    EXPECT_TRUE(box.checkGeometry().ok);
}

// And the invariant it was breaking. Both offending results were this configuration's.
TEST(BRepConeBaseArc, TheFixtureBooleansAreWatertightOrEmpty)
{
    const Body A = fixtureCone(), B = fixtureBox();
    for (const BooleanOp op :
         {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference})
        EXPECT_TRUE(watertightOrEmpty(booleanToBody(A, B, op)))
            << "a non-empty result fails validation — watertight-or-empty is the one "
               "guarantee booleanToBody makes";
}

// A cone must still be a cone, and the ANALYTIC volume is the thing to say it with: the
// exact pi*r^2*h/3, independent of the segment count, exactly as the cylinder's is.
//
// That was already true before this change, which is the sharpest way to put what was
// wrong. The surfaces declared an exact cone and massProperties integrated one, so the body
// asserted a smooth cone twice over; only its base EDGES disagreed, bounding a region that
// was an inscribed pyramid instead. The three now agree.
TEST(BRepConeBaseArc, TheConeStillHasTheVolumeAndTopologyOfACone)
{
    const double r = 1.0, hh = 2.0;
    const double smooth = M_PI * r * r * hh / 3.0;
    for (const uint32_t n : {8u, 16u, 64u}) {
        const Body c = makeCone(1.f, 2.f, n);
        ASSERT_TRUE(c.isClosed()) << "n=" << n;
        ASSERT_TRUE(c.checkIntegrity().ok) << "n=" << n;
        ASSERT_TRUE(c.checkGeometry().ok) << "n=" << n;
        EXPECT_EQ(c.checkIntegrity().euler, 2) << "n=" << n;
        EXPECT_NEAR(c.massProperties().volume, static_cast<float>(smooth),
                    static_cast<float>(smooth) * 1e-6)
            << "n=" << n << ": the analytic volume is no longer the exact cone";
    }
}

// CHARACTERIZATION of what the TESSELLATION does with it, which is a different question and
// worth pinning because it is easy to mistake for a defect in the above.
//
// The tessellated volume converges in the SEGMENT count and barely at all in the
// subdivision: 90.0% of the true cone at n=8 sub0, still only 93.7% at sub6, against 99.9%
// at n=64. A cone side face is fanned, and its fan apex is a point on the base rather than
// the cone's apex, so the triangles chord across the base arc no matter how finely that arc
// is refined. That is the already-named limitation of the tessellator under-refining the
// inside of a curved patch; it is not introduced here.
//
// Before this change the n=8 sub0 figure was 1.885618 — which is EXACTLY the inscribed
// pyramid, because with a chord base that is genuinely what the body enclosed, and the
// tessellation was faithful to it. The body is right now and the tessellation lags it,
// which is the better way round of the two.
TEST(BRepConeBaseArc, TheTessellatedConeConvergesInSegmentsNotSubdivisions)
{
    const double smooth = M_PI * 2.0 / 3.0;
    struct Case { uint32_t n; double lo; };
    const Case cases[] = {{8u, 0.93}, {16u, 0.98}, {64u, 0.998}};
    for (const Case& c : cases) {
        const double v = MeshMassProperties::compute(makeCone(1.f, 2.f, c.n).toMesh(4)).volume;
        EXPECT_GT(v / smooth, c.lo)
            << "n=" << c.n << ": tessellated volume fell to " << (v / smooth) << " of the cone";
        EXPECT_LT(v / smooth, 1.0005)
            << "n=" << c.n << ": the tessellation now exceeds the solid it approximates";
    }
    // CONTRACT CHANGED, exactly where this test said to look. It used to assert
    // `coarse6 / smooth < 0.95` — that refining the arcs does NOT rescue a coarse cone —
    // with the note "subdivision now resolves a coarse cone, the fan apex on a conical face
    // must have changed, so re-read this test's reasoning". It has changed: a conical face is
    // no longer fanned from a base point, it is RULED, so a subdivided arc actually carries its
    // refinement into the surface. Measured at n=8: 0.900 of the true cone at subdivisions 0,
    // then 0.989, 0.996, 0.998 — where it used to stall around 0.937.
    const double coarse0 = MeshMassProperties::compute(makeCone(1.f, 2.f, 8).toMesh(0)).volume;
    const double coarse6 = MeshMassProperties::compute(makeCone(1.f, 2.f, 8).toMesh(6)).volume;
    EXPECT_GT(coarse6, coarse0) << "subdivision should help";
    EXPECT_GT(coarse6 / smooth, 0.99)
        << "subdivision no longer resolves a coarse cone — a conical face has stopped being "
           "ruled, so re-read triangulateCurvedFaceStrips";
    EXPECT_LT(coarse6 / smooth, 1.0005)
        << "the tessellation exceeds the solid it approximates";
}

}  // namespace nexus::geometry::brep::testing
