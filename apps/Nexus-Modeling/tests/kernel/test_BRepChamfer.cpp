// Foundation — flat chamfer of a box edge (a real modeling op) via a boolean
// difference with a triangular cutting prism. Watertight, with the exact removed
// wedge volume ½·setback²·edgeLength.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
void expectWatertight(const Body& b)
{
    const auto ig = b.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_TRUE(b.isClosed());
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
}
}  // namespace

TEST(BRepChamfer, SingleEdgeExactVolumeEachAxis)
{
    // 2×2×2 box, edge length 2, chamfer setback 0.4 → wedge ½·0.4²·2 = 0.16.
    const float s = 0.4f;
    for (int axis = 0; axis < 3; ++axis) {
        const Body r = chamferBoxEdge(makeBox(2.f, 2.f, 2.f), axis, +1, +1, s);
        expectWatertight(r);
        EXPECT_NEAR(r.massProperties().volume, 8.f - s * s, 1e-4f) << "axis " << axis;
        // 6 box faces trimmed + 1 new chamfer face.
        EXPECT_EQ(r.checkIntegrity().faces, 7u) << "axis " << axis;
    }
}

TEST(BRepChamfer, NonCubicBox)
{
    // 2×4×6 box, X edge length 2 → wedge ½·s²·2 = s².
    const float s = 0.5f;
    const Body r = chamferBoxEdge(makeBox(2.f, 4.f, 6.f), 0, +1, +1, s);
    expectWatertight(r);
    EXPECT_NEAR(r.massProperties().volume, 48.f - s * s, 1e-4f);
}

TEST(BRepChamfer, MultipleEdges)
{
    const float s = 0.4f;
    const Body box = makeBox(2.f, 2.f, 2.f);
    // Two opposite X edges.
    const Body opp = chamferBoxEdge(chamferBoxEdge(box, 0, +1, +1, s), 0, -1, -1, s);
    expectWatertight(opp);
    EXPECT_NEAR(opp.massProperties().volume, 8.f - 2.f * s * s, 1e-4f);
    // Two adjacent X edges (sharing the +Z face).
    const Body adj = chamferBoxEdge(chamferBoxEdge(box, 0, +1, +1, s), 0, -1, +1, s);
    expectWatertight(adj);
    EXPECT_NEAR(adj.massProperties().volume, 8.f - 2.f * s * s, 1e-4f);
}

TEST(BRepChamfer, DegenerateSetbacksHandled)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    // Non-positive setback → the box unchanged.
    EXPECT_NEAR(chamferBoxEdge(box, 0, +1, +1, 0.f).massProperties().volume, 8.f, 1e-4f);
    EXPECT_NEAR(chamferBoxEdge(box, 0, +1, +1, -1.f).massProperties().volume, 8.f, 1e-4f);
    // A setback larger than the box is clamped to a valid, watertight chamfer.
    const Body big = chamferBoxEdge(box, 0, +1, +1, 100.f);
    expectWatertight(big);
    EXPECT_LT(big.massProperties().volume, 8.f);
    EXPECT_GT(big.massProperties().volume, 0.f);
}

TEST(BRepChamfer, Deterministic)
{
    const float v1 = chamferBoxEdge(makeBox(2.f, 2.f, 2.f), 1, +1, -1, 0.3f).massProperties().volume;
    const float v2 = chamferBoxEdge(makeBox(2.f, 2.f, 2.f), 1, +1, -1, 0.3f).massProperties().volume;
    EXPECT_EQ(v1, v2);
}

}  // namespace nexus::geometry::brep::testing
