// Foundation — analytic B-rep coplanar face-merge cleanup. mergeCoplanarFaces
// collapses the coplanar over-segmentation left by imprint / boolean (via the
// mergeFaces Euler op on single-shared-edge coplanar pairs) without changing the
// solid: fewer live faces, still valid/closed, same volume, and a fixpoint (no
// coplanar single-shared-edge pair remains).

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

TEST(BRepMergeCoplanar, ReducesBooleanOverSegmentationPreservingSolid)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);

    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Difference}) {
        Body r = booleanToBody(a, b, op);
        const uint32_t liveBefore = r.checkIntegrity().faces;
        const double volBefore = signedVolume(r.toMesh());

        const uint32_t merges = r.mergeCoplanarFaces();
        EXPECT_GT(merges, 0u);  // boolean output is over-segmented

        const auto ig = r.checkIntegrity();
        EXPECT_TRUE(ig.ok) << ig.reason;
        EXPECT_EQ(ig.euler, 2);
        EXPECT_EQ(ig.boundaryEdges, 0u);
        EXPECT_TRUE(r.isClosed());
        EXPECT_TRUE(r.checkGeometry().ok) << r.checkGeometry().reason;

        // Live faces drop by exactly the number of merges; the solid is unchanged.
        EXPECT_EQ(ig.faces, liveBefore - merges);
        EXPECT_LT(ig.faces, liveBefore);
        EXPECT_NEAR(signedVolume(r.toMesh()), volBefore, 1e-4);
    }
}

TEST(BRepMergeCoplanar, IsAFixpoint)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 0.5f, 0.5f}, 2, 2, 2);
    Body r = booleanToBody(a, b, BooleanOp::Union);
    EXPECT_GT(r.mergeCoplanarFaces(), 0u);
    EXPECT_EQ(r.mergeCoplanarFaces(), 0u);  // second pass finds nothing to merge
    EXPECT_TRUE(r.checkIntegrity().ok);
    EXPECT_TRUE(r.checkGeometry().ok);
}

TEST(BRepMergeCoplanar, MinimalSolidIsUnchanged)
{
    // A plain box (and a clean intersection cube) have no coplanar adjacent
    // faces, so there is nothing to merge.
    Body box = makeBox(2.f, 2.f, 2.f);
    EXPECT_EQ(box.mergeCoplanarFaces(), 0u);
    EXPECT_EQ(box.checkIntegrity().faces, 6u);

    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    Body inter = booleanToBody(a, b, BooleanOp::Intersection);  // the overlap cube
    EXPECT_EQ(inter.mergeCoplanarFaces(), 0u);
    EXPECT_TRUE(inter.checkIntegrity().ok);
}

TEST(BRepMergeCoplanar, Deterministic)
{
    const Body a = makeBox(2.f, 2.f, 2.f);
    const Body b = boxAt({1.f, 1.f, 1.f}, 2, 2, 2);
    Body r1 = booleanToBody(a, b, BooleanOp::Union);
    Body r2 = booleanToBody(a, b, BooleanOp::Union);
    const uint32_t m1 = r1.mergeCoplanarFaces();
    const uint32_t m2 = r2.mergeCoplanarFaces();
    EXPECT_EQ(m1, m2);
    EXPECT_EQ(r1.checkIntegrity().faces, r2.checkIntegrity().faces);
    EXPECT_NEAR(signedVolume(r1.toMesh()), signedVolume(r2.toMesh()), 1e-6);
}

}  // namespace nexus::geometry::brep::testing
