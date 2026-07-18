// Foundation — analytic B-rep hollow/shell (hollowBox). A concentric inner box is
// subtracted from an outer box via the robust boolean difference, leaving a
// SEALED wall of a given thickness: a valid solid with TWO boundary shells (outer
// surface + inner cavity) → euler 4, watertight, material volume = outer − inner.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <bit>

namespace nexus::geometry::brep::testing {

TEST(BRepHollow, SealedShellVolumeAndTwoBoundaries)
{
    // 4×4×4 outer, thickness 1 → inner 2×2×2, material volume 64 − 8 = 56.
    const Body h = hollowBox(4.f, 4.f, 4.f, 1.f);
    const auto ig = h.checkIntegrity();
    ASSERT_TRUE(ig.ok) << ig.reason;
    EXPECT_TRUE(h.isClosed());
    EXPECT_EQ(ig.boundaryEdges, 0u);          // watertight (both shells closed)
    EXPECT_EQ(ig.euler, 4);                    // two genus-0 shells (outer + cavity)
    EXPECT_EQ(ig.vertices, 16u);               // 8 outer + 8 inner
    EXPECT_EQ(ig.faces, 12u);                  // 6 + 6
    EXPECT_TRUE(h.checkGeometry().ok) << h.checkGeometry().reason;
    EXPECT_NEAR(h.massProperties().volume, 56.f, 1e-3f);
}

TEST(BRepHollow, WallVolumeTracksThicknessAndDimensions)
{
    for (float t : {0.5f, 1.0f, 1.5f}) {
        const Body h = hollowBox(4.f, 4.f, 4.f, t);
        ASSERT_TRUE(h.checkIntegrity().ok) << "t=" << t;
        const float inner = (4.f - 2.f * t) * (4.f - 2.f * t) * (4.f - 2.f * t);
        EXPECT_NEAR(h.massProperties().volume, 64.f - inner, 1e-3f) << "t=" << t;
    }
    // Non-cubic box: 6×4×2, thickness 0.5 → inner 5×3×1, volume 48 − 15 = 33.
    const Body nb = hollowBox(6.f, 4.f, 2.f, 0.5f);
    ASSERT_TRUE(nb.checkIntegrity().ok);
    EXPECT_EQ(nb.checkIntegrity().euler, 4);
    EXPECT_NEAR(nb.massProperties().volume, 33.f, 1e-3f);
}

TEST(BRepHollow, ThicknessTooLargeReturnsSolidBox)
{
    // 2·thickness ≥ smallest dimension → no room for a cavity → the plain box.
    const Body solid = hollowBox(4.f, 4.f, 4.f, 2.5f);
    const auto ig = solid.checkIntegrity();
    ASSERT_TRUE(ig.ok);
    EXPECT_EQ(ig.euler, 2);                     // a single solid box, not a shell
    EXPECT_EQ(ig.faces, 6u);
    EXPECT_NEAR(solid.massProperties().volume, 64.f, 1e-3f);

    // Exactly at the half-dimension boundary is still solid (no cavity).
    EXPECT_EQ(hollowBox(4.f, 4.f, 4.f, 2.f).checkIntegrity().euler, 2);
}

TEST(BRepHollow, DegenerateInputHandled)
{
    // Non-positive / non-finite thickness → the plain solid box.
    EXPECT_EQ(hollowBox(4.f, 4.f, 4.f, 0.f).faceCount(), 6u);
    EXPECT_EQ(hollowBox(4.f, 4.f, 4.f, -1.f).faceCount(), 6u);
    const float inf = std::bit_cast<float>(0x7F800000u);
    EXPECT_EQ(hollowBox(4.f, 4.f, 4.f, inf).faceCount(), 6u);
    // Non-positive / non-finite dimensions → empty.
    EXPECT_EQ(hollowBox(0.f, 4.f, 4.f, 0.5f).faceCount(), 0u);
    EXPECT_EQ(hollowBox(4.f, -2.f, 4.f, 0.5f).faceCount(), 0u);
    EXPECT_EQ(hollowBox(inf, 4.f, 4.f, 0.5f).faceCount(), 0u);
}

TEST(BRepHollow, Deterministic)
{
    EXPECT_EQ(hollowBox(4.f, 4.f, 4.f, 1.f).serialize(),
              hollowBox(4.f, 4.f, 4.f, 1.f).serialize());
}

}  // namespace nexus::geometry::brep::testing
