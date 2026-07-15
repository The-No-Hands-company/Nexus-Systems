// Foundation — analytic B-rep simplify (minimal cleanup). mergeCollinearEdges
// removes redundant degree-2 vertices whose two incident edges are collinear
// Lines; simplify() alternates it with mergeCoplanarFaces to a combined fixpoint.
// Together they drive a boolean's over-segmented output toward a minimal B-rep
// without changing the solid (valid/closed/euler-2/volume-exact).

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
double signedVolume(const Mesh& m)
{
    const auto& p = m.attributes().positions();
    const auto& t = m.topology();
    double vol = 0.0;
    for (size_t i = 0; i < t.faceCount(); ++i) {
        const auto& id = t.face(i).indices;
        if (id.size() != 3) continue;
        vol += static_cast<double>(p[id[0]].dot(p[id[1]].cross(p[id[2]]))) / 6.0;
    }
    return vol;
}

Body boxAt(Vec3 c, float w, float h, float d)
{
    Body b = makeBox(w, h, d);
    b.translate(c);
    return b;
}
}  // namespace

TEST(BRepSimplify, ReducesBooleanBelowCoplanarOnly)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);

    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Difference}) {
        const Body raw = booleanToBody(a, b, op);
        const auto rawI = raw.checkIntegrity();
        const double vol0 = signedVolume(raw.toMesh());

        // Coplanar-merge only vs. the full simplify (which also merges the
        // collinear edges that coplanar-merge exposes).
        Body coplanar = raw;
        coplanar.mergeCoplanarFaces();
        const auto cI = coplanar.checkIntegrity();

        Body full = raw;
        const uint32_t ops = full.simplify();
        EXPECT_GT(ops, 0u);
        const auto sI = full.checkIntegrity();

        // Still a valid closed solid of the same volume.
        EXPECT_TRUE(sI.ok) << sI.reason;
        EXPECT_EQ(sI.euler, 2);
        EXPECT_EQ(sI.boundaryEdges, 0u);
        EXPECT_TRUE(full.isClosed());
        EXPECT_TRUE(full.checkGeometry().ok) << full.checkGeometry().reason;
        EXPECT_NEAR(signedVolume(full.toMesh()), vol0, 1e-4);

        // simplify beats raw on faces, and beats coplanar-only on edges AND
        // vertices (the collinear-edge pass's contribution).
        EXPECT_LT(sI.faces, rawI.faces);
        EXPECT_LE(sI.faces, cI.faces);
        EXPECT_LT(sI.edges, cI.edges);
        EXPECT_LT(sI.vertices, cI.vertices);

        EXPECT_EQ(full.simplify(), 0u);  // fixpoint
    }
}

TEST(BRepSimplify, MergeCollinearEdgesRemovesRedundantVertices)
{
    // Degree-2 collinear vertices only appear once coplanar-merge has removed the
    // interior edges, so run that first, then the collinear pass in isolation.
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    Body r = booleanToBody(a, b, BooleanOp::Union);
    const double vol0 = signedVolume(r.toMesh());
    r.mergeCoplanarFaces();
    const uint32_t vBefore = r.checkIntegrity().vertices;

    const uint32_t removed = r.mergeCollinearEdges();
    EXPECT_GT(removed, 0u);

    const auto ig = r.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);
    EXPECT_TRUE(r.isClosed());
    EXPECT_TRUE(r.checkGeometry().ok) << r.checkGeometry().reason;
    EXPECT_EQ(ig.vertices, vBefore - removed);        // each removes one vertex
    EXPECT_NEAR(signedVolume(r.toMesh()), vol0, 1e-4);  // solid unchanged
    EXPECT_EQ(r.mergeCollinearEdges(), 0u);            // fixpoint
}

TEST(BRepSimplify, MinimalSolidUnchanged)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    EXPECT_EQ(box.simplify(), 0u);
    const auto ig = box.checkIntegrity();
    EXPECT_EQ(ig.faces, 6u);
    EXPECT_EQ(ig.edges, 12u);
    EXPECT_EQ(ig.vertices, 8u);
}

TEST(BRepSimplify, Deterministic)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 0.5f, 0.5f}, 2, 2, 2);
    Body r1 = booleanToBody(a, b, BooleanOp::Union);
    Body r2 = booleanToBody(a, b, BooleanOp::Union);
    EXPECT_EQ(r1.simplify(), r2.simplify());
    const auto i1 = r1.checkIntegrity();
    const auto i2 = r2.checkIntegrity();
    EXPECT_EQ(i1.faces, i2.faces);
    EXPECT_EQ(i1.edges, i2.edges);
    EXPECT_EQ(i1.vertices, i2.vertices);
    EXPECT_NEAR(signedVolume(r1.toMesh()), signedVolume(r2.toMesh()), 1e-6);
}

}  // namespace nexus::geometry::brep::testing
