// Phase 1 of the true-analytic-curved-boolean arc — CURVE-CARRYING ASSEMBLY.
//
// booleanToBody sews its result with Body::fromFaces, which is handed vertex
// rings and nothing else, so it builds every edge as a straight CHORD between
// its endpoints. A curved operand's arc edges therefore used to degrade to Lines
// through ANY boolean: measured on makeCylinder(1,2,12) ∪ a box lying strictly
// inside it — where the union IS the cylinder, and the topology came back
// correct (14 faces / 36 edges) — all 24 rim arcs returned as chords. The
// analytic geometry toMesh(subdivisions) needs in order to retessellate was
// silently lost, on a boolean that is geometrically a no-op.
//
// booleanToBody now harvests the kept edges' Circle geometry (keyed by welded
// endpoint pair, so the key survives loop winding and the outward-flip) and
// re-applies it after the sew via setEdgeArc, which re-derives the param range
// from the edge's own endpoints and REFUSES an edge whose endpoints are not on
// the circle — so a refused edge keeps its chord and behaviour degrades to
// exactly what it was, never to geometry that contradicts the topology.
//
// The load-bearing assertion is the TESSELLATION one: a Circle edge refines
// under toMesh(subdivisions) and a chord does not, so the refinement series is
// what proves the restored arcs are real geometry rather than a relabelled Line.

#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {

struct CurveCensus { size_t line = 0, circle = 0; };

CurveCensus census(const Body& b)
{
    CurveCensus c;
    for (uint32_t e = 0; e < static_cast<uint32_t>(b.edgeCount()); ++e) {
        if (!b.edge(e).alive) continue;
        const uint32_t cu = b.edge(e).curve;
        if (cu == kInvalid) continue;
        if (b.curve(cu).kind == CurveKind::Circle) ++c.circle;
        else if (b.curve(cu).kind == CurveKind::Line) ++c.line;
    }
    return c;
}

float tessellatedVolume(const Body& b, uint32_t subdivisions)
{
    return MeshMassProperties::compute(b.toMesh(subdivisions)).volume;
}

// A unit box centred at the origin lies strictly inside makeCylinder(1,2,12):
// its corner radius is √0.5 ≈ 0.707 < the 12-gon inradius cos(π/12) ≈ 0.966,
// and |z| = 0.5 < 1. So the union is the cylinder itself.
Body interiorBox() { return makeBox(1.f, 1.f, 1.f); }

}  // namespace

// The rim arcs of a cylinder survive a boolean whose result IS that cylinder.
TEST(BRepBooleanCurvePreservation, IdentityLikeUnionKeepsEveryRimArc)
{
    const Body cyl = makeCylinder(1.f, 2.f, 12);
    const CurveCensus in = census(cyl);
    ASSERT_EQ(in.circle, 24u) << "makeCylinder should carry 2 rings × 12 arc edges";

    const Body uni = booleanToBody(cyl, interiorBox(), BooleanOp::Union);
    ASSERT_EQ(uni.faceCount(), cyl.faceCount()) << "union with an interior box is the cylinder";

    const CurveCensus out = census(uni);
    EXPECT_EQ(out.circle, in.circle) << "boolean flattened rim arcs to chords";
    EXPECT_EQ(out.line, in.line);
}

// THE load-bearing check: the restored arcs are real geometry, because they
// REFINE under tessellation. A chord-only body's toMesh volume is constant in
// `subdivisions`; an arc-carrying one climbs from the 12-gon prism (6.0) toward
// the true cylinder π·r²·h ≈ 6.2832.
TEST(BRepBooleanCurvePreservation, RestoredArcsRefineUnderTessellation)
{
    const Body uni = booleanToBody(makeCylinder(1.f, 2.f, 12), interiorBox(), BooleanOp::Union);
    ASSERT_GT(uni.faceCount(), 0u);

    const float v0 = tessellatedVolume(uni, 0);
    const float v1 = tessellatedVolume(uni, 1);
    const float v3 = tessellatedVolume(uni, 3);

    EXPECT_NEAR(v0, 6.f, 1e-3f) << "unrefined, the 12-gon prism volume";
    EXPECT_GT(v1, v0 + 1e-3f) << "arcs did not refine — the edges are chords, not circles";
    EXPECT_GT(v3, v1);
    EXPECT_LT(v3, 3.14159265f * 2.f) << "refinement must stay under the true cylinder volume";
}

// The boolean output tessellates exactly like the operand it reproduces.
TEST(BRepBooleanCurvePreservation, OutputTessellatesLikeTheInputCylinder)
{
    const Body cyl = makeCylinder(1.f, 2.f, 12);
    const Body uni = booleanToBody(cyl, interiorBox(), BooleanOp::Union);
    ASSERT_GT(uni.faceCount(), 0u);

    for (uint32_t s : {0u, 1u, 2u, 3u})
        EXPECT_NEAR(tessellatedVolume(uni, s), tessellatedVolume(cyl, s), 1e-4f)
            << "refinement diverges from the input cylinder at subdivisions=" << s;
}

// Arcs are preserved for a boolean that genuinely recombines faces, not only the
// identity-like one — and the watertight-or-empty contract still holds with the
// analytic geometry attached.
TEST(BRepBooleanCurvePreservation, ArcsSurviveADisjointUnionAndStayValid)
{
    const Body cyl = makeCylinder(1.f, 2.f, 12);
    Body far = makeBox(1.f, 1.f, 1.f);
    far.translate({2.5f, 0.f, 0.f});  // disjoint from the cylinder

    const Body uni = booleanToBody(cyl, far, BooleanOp::Union);
    ASSERT_GT(uni.faceCount(), 0u);
    EXPECT_EQ(census(uni).circle, 24u);

    // Attaching the arcs must not break either validator, and the flat box adds
    // exactly its own 1.0 while only the cylinder's arcs refine.
    EXPECT_TRUE(uni.isClosed());
    EXPECT_TRUE(uni.checkIntegrity().ok) << uni.checkIntegrity().reason;
    EXPECT_TRUE(uni.checkGeometry().ok) << uni.checkGeometry().reason;
    EXPECT_NEAR(tessellatedVolume(uni, 3) - tessellatedVolume(cyl, 3), 1.f, 1e-3f);
}

// A purely planar boolean is untouched by the arc pass (no Circle edges to carry,
// so nothing changes) — guards against the pass inventing curvature.
TEST(BRepBooleanCurvePreservation, PlanarBooleanGainsNoCurvature)
{
    Body b2 = makeBox(2.f, 2.f, 2.f);
    b2.translate({1.f, 0.f, 0.f});
    const Body uni = booleanToBody(makeBox(2.f, 2.f, 2.f), b2, BooleanOp::Union);
    ASSERT_GT(uni.faceCount(), 0u);
    EXPECT_EQ(census(uni).circle, 0u) << "a box/box union must stay entirely straight-edged";
    EXPECT_TRUE(uni.checkGeometry().ok) << uni.checkGeometry().reason;
}

}  // namespace nexus::geometry::brep::testing
