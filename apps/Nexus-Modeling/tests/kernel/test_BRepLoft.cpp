// Foundation — analytic B-rep loft (ruled solid between two corresponding
// profiles). loftProfiles connects a bottom and a top closed planar profile with
// one quad side per matching edge pair plus two caps → a watertight genus-0
// solid. Generalises extrudeProfile; for similar scaled profiles it is an exact
// pyramidal frustum.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

// Equal profiles offset vertically → a prism (here a 2×2×3 box, volume 24/2=12).
TEST(BRepLoft, EqualProfilesIsPrism)
{
    const std::vector<Vec3> b = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    const std::vector<Vec3> t = {{-1, -1, 3}, {1, -1, 3}, {1, 1, 3}, {-1, 1, 3}};
    const Body loft = loftProfiles(b, t);

    const auto ig = loft.checkIntegrity();
    ASSERT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);            // genus 0
    EXPECT_EQ(ig.boundaryEdges, 0u);   // watertight
    EXPECT_EQ(ig.vertices, 8u);
    EXPECT_EQ(ig.faces, 6u);           // 4 sides + 2 caps
    EXPECT_TRUE(loft.isClosed());
    EXPECT_TRUE(loft.checkGeometry().ok) << loft.checkGeometry().reason;
    EXPECT_NEAR(loft.massProperties().volume, 12.f, 1e-3f);  // 2·2·3
}

// A scaled-square loft is an exact pyramidal frustum: V = h/3·(A₀+A₁+√(A₀A₁)).
TEST(BRepLoft, ScaledProfilesIsExactFrustum)
{
    const std::vector<Vec3> b = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};      // A₀=4
    const std::vector<Vec3> t = {{-0.5, -0.5, 2}, {0.5, -0.5, 2}, {0.5, 0.5, 2}, {-0.5, 0.5, 2}};  // A₁=1
    const Body loft = loftProfiles(b, t);

    const auto ig = loft.checkIntegrity();
    ASSERT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_TRUE(loft.isClosed());
    EXPECT_TRUE(loft.checkGeometry().ok);
    const double v = 2.0 / 3.0 * (4.0 + 1.0 + std::sqrt(4.0 * 1.0));  // = 14/3
    EXPECT_GT(loft.massProperties().volume, 0.f);
    EXPECT_NEAR(loft.massProperties().volume, static_cast<float>(v), 1e-3f);
}

// Orientation is derived from the bottom→top offset, so swapping which profile is
// passed first still yields a valid, positive-volume solid.
TEST(BRepLoft, OrientationIndependentOfArgumentOrder)
{
    const std::vector<Vec3> lo = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    const std::vector<Vec3> hi = {{-1, -1, 3}, {1, -1, 3}, {1, 1, 3}, {-1, 1, 3}};
    const Body up = loftProfiles(lo, hi);
    const Body down = loftProfiles(hi, lo);  // top passed as "bottom"
    ASSERT_TRUE(up.checkIntegrity().ok);
    ASSERT_TRUE(down.checkIntegrity().ok);
    EXPECT_GT(down.massProperties().volume, 0.f);
    EXPECT_NEAR(up.massProperties().volume, down.massProperties().volume, 1e-3f);
}

// A pentagon loft (odd vertex count) is watertight euler-2 for any matching count.
TEST(BRepLoft, PentagonLoftIsWatertight)
{
    std::vector<Vec3> b, t;
    const float twoPi = 6.28318530717958647692f;
    for (int k = 0; k < 5; ++k) {
        const float a = twoPi * static_cast<float>(k) / 5.f;
        b.push_back({std::cos(a), std::sin(a), 0.f});
        t.push_back({0.6f * std::cos(a), 0.6f * std::sin(a), 1.5f});
    }
    const Body loft = loftProfiles(b, t);
    const auto ig = loft.checkIntegrity();
    ASSERT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);
    EXPECT_EQ(ig.faces, 7u);  // 5 sides + 2 caps
    EXPECT_TRUE(loft.isClosed());
    EXPECT_GT(loft.massProperties().volume, 0.f);
}

TEST(BRepLoft, DegenerateInputRejected)
{
    const std::vector<Vec3> sq = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    const std::vector<Vec3> sqTop = {{-1, -1, 2}, {1, -1, 2}, {1, 1, 2}, {-1, 1, 2}};
    // Mismatched vertex counts.
    EXPECT_EQ(loftProfiles(sq, {{-1, -1, 2}, {1, -1, 2}, {1, 1, 2}}).faceCount(), 0u);
    // Fewer than 3 points.
    EXPECT_EQ(loftProfiles({{0, 0, 0}, {1, 0, 0}}, {{0, 0, 1}, {1, 0, 1}}).faceCount(), 0u);
    // Top coplanar with the bottom (no offset → undefined loft).
    EXPECT_EQ(loftProfiles(sq, {{-2, -2, 0}, {2, -2, 0}, {2, 2, 0}, {-2, 2, 0}}).faceCount(), 0u);
    // Degenerate (collinear/zero-area) profile.
    EXPECT_EQ(loftProfiles({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {3, 0, 0}}, sqTop).faceCount(), 0u);
    // Non-finite coordinate.
    const float inf = std::bit_cast<float>(0x7F800000u);
    std::vector<Vec3> bad = sqTop;
    bad[0].x = inf;
    EXPECT_EQ(loftProfiles(sq, bad).faceCount(), 0u);
}

TEST(BRepLoft, Deterministic)
{
    const std::vector<Vec3> b = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    const std::vector<Vec3> t = {{-0.5, -0.5, 2}, {0.5, -0.5, 2}, {0.5, 0.5, 2}, {-0.5, 0.5, 2}};
    EXPECT_EQ(loftProfiles(b, t).serialize(), loftProfiles(b, t).serialize());
}

}  // namespace nexus::geometry::brep::testing
