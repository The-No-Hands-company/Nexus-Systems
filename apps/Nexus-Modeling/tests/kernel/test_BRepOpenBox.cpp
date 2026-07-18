// Foundation — analytic B-rep OPEN box shell (makeOpenBox). The kernel's
// open-surface / sheet-body path: a floor + four walls with the top OMITTED. The
// result is NOT a closed solid — its top rim is a loop of boundary edges,
// isClosed() is false, and checkIntegrity() is clean (it permits boundary edges).

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <limits>

namespace nexus::geometry::brep::testing {

// The open shell is validator-clean but NOT closed: euler 1 (disk topology), the
// four top-rim edges are boundary edges, and the surface area is floor + walls.
TEST(BRepOpenBox, OpenShellTopologyAndArea)
{
    const Body ob = makeOpenBox(4.f, 4.f, 2.f);  // 4×4 floor, wall height 2
    const auto ig = ob.checkIntegrity();
    ASSERT_TRUE(ig.ok) << ig.reason;
    EXPECT_FALSE(ob.isClosed());          // OPEN — the defining property
    EXPECT_EQ(ig.boundaryEdges, 4u);       // the top rim (one free edge per wall)
    EXPECT_EQ(ig.euler, 1);                // a disk-topology sheet (V−E+F = 8−12+5)
    EXPECT_EQ(ig.vertices, 8u);
    EXPECT_EQ(ig.edges, 12u);
    EXPECT_EQ(ig.faces, 5u);               // floor + 4 walls (no top)
    EXPECT_TRUE(ob.checkGeometry().ok) << ob.checkGeometry().reason;
    // Area = floor (w·d) + walls (2·(w+d)·h) = 16 + 2·8·2 = 48.
    EXPECT_NEAR(ob.surfaceArea(), 48.f, 1e-3f);
}

// Area tracks non-cubic dimensions: 6×4 floor, height 3 → 24 + 2·10·3 = 84.
TEST(BRepOpenBox, AreaTracksDimensions)
{
    const Body ob = makeOpenBox(6.f, 4.f, 3.f);
    ASSERT_TRUE(ob.checkIntegrity().ok);
    EXPECT_FALSE(ob.isClosed());
    EXPECT_EQ(ob.checkIntegrity().boundaryEdges, 4u);
    const float expected = 6.f * 4.f + 2.f * (6.f + 4.f) * 3.f;  // 24 + 60 = 84
    EXPECT_NEAR(ob.surfaceArea(), expected, 1e-3f);
}

TEST(BRepOpenBox, DegenerateInputRejected)
{
    EXPECT_EQ(makeOpenBox(0.f, 4.f, 2.f).faceCount(), 0u);
    EXPECT_EQ(makeOpenBox(4.f, -1.f, 2.f).faceCount(), 0u);
    EXPECT_EQ(makeOpenBox(4.f, 4.f, 0.f).faceCount(), 0u);
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_EQ(makeOpenBox(inf, 4.f, 2.f).faceCount(), 0u);
}

TEST(BRepOpenBox, Deterministic)
{
    EXPECT_EQ(makeOpenBox(4.f, 4.f, 2.f).serialize(), makeOpenBox(4.f, 4.f, 2.f).serialize());
}

}  // namespace nexus::geometry::brep::testing
