// Foundation — analytic B-rep twisted extrude (twistExtrude). A profile swept
// along a direction while rotating about the extrusion axis, in stacked layers.
// A rotation preserves cross-sectional area, so the true volume is base-area ×
// height (Cavalieri); the faceted result converges to it from below as layers
// grow. A zero twist is a plain prism (exact volume).

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
const std::vector<Vec3> kSquare = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};  // area 4
}

// Zero twist is a plain prism: exact volume = area·height, genus 0.
TEST(BRepTwistExtrude, ZeroTwistIsExactPrism)
{
    const Body b = twistExtrude(kSquare, {0, 0, 3}, 0.f, 8);
    const auto ig = b.checkIntegrity();
    ASSERT_TRUE(ig.ok) << ig.reason;
    EXPECT_TRUE(b.isClosed());
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_EQ(ig.euler, 2);
    EXPECT_TRUE(b.checkGeometry().ok);
    EXPECT_NEAR(b.massProperties().volume, 12.f, 1e-3f);  // 4·3
}

// A twisted prism is watertight/genus-0 and its faceted volume converges to the
// exact base-area·height (12) FROM BELOW as the layer count grows.
TEST(BRepTwistExtrude, TwistedVolumeConvergesToAreaTimesHeight)
{
    const float twist = static_cast<float>(M_PI) / 4.f;  // 45°
    const float v16 = twistExtrude(kSquare, {0, 0, 3}, twist, 16).massProperties().volume;
    const float v64 = twistExtrude(kSquare, {0, 0, 3}, twist, 64).massProperties().volume;
    const float v256 = twistExtrude(kSquare, {0, 0, 3}, twist, 256).massProperties().volume;

    for (uint32_t L : {16u, 64u, 256u}) {
        const Body b = twistExtrude(kSquare, {0, 0, 3}, twist, L);
        const auto ig = b.checkIntegrity();
        ASSERT_TRUE(ig.ok) << "L=" << L << ": " << ig.reason;
        EXPECT_EQ(ig.euler, 2) << "L=" << L;
        EXPECT_TRUE(b.isClosed()) << "L=" << L;
        EXPECT_TRUE(b.checkGeometry().ok) << "L=" << L;
    }
    EXPECT_LT(v16, v64);            // monotone increase toward the limit
    EXPECT_LT(v64, v256);
    EXPECT_LT(v256, 12.f);          // still under the exact area·height
    EXPECT_NEAR(v256, 12.f, 0.05f);  // close at 256 layers
}

// Orientation is derived from dir, so a negative extrusion direction still yields
// a positive-volume outward solid.
TEST(BRepTwistExtrude, NegativeDirectionStaysOutward)
{
    const Body b = twistExtrude(kSquare, {0, 0, -3}, 0.5f, 32);
    ASSERT_TRUE(b.checkIntegrity().ok);
    EXPECT_EQ(b.checkIntegrity().euler, 2);
    EXPECT_GT(b.massProperties().volume, 0.f);
}

TEST(BRepTwistExtrude, DegenerateInputRejected)
{
    EXPECT_EQ(twistExtrude({{0, 0, 0}, {1, 0, 0}}, {0, 0, 1}, 0.5f, 4).faceCount(), 0u);  // <3 pts
    EXPECT_EQ(twistExtrude(kSquare, {0, 0, 3}, 0.5f, 0).faceCount(), 0u);                  // layers<1
    EXPECT_EQ(twistExtrude(kSquare, {1, 0, 0}, 0.5f, 8).faceCount(), 0u);                  // dir in plane
    EXPECT_EQ(twistExtrude(kSquare, {0, 0, 0}, 0.5f, 8).faceCount(), 0u);                  // zero dir
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_EQ(twistExtrude(kSquare, {0, 0, 3}, inf, 8).faceCount(), 0u);                   // non-finite twist
    EXPECT_EQ(twistExtrude({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}, {0, 0, 1}, 0.5f, 8).faceCount(),
              0u);                                                                          // collinear
}

TEST(BRepTwistExtrude, Deterministic)
{
    EXPECT_EQ(twistExtrude(kSquare, {0, 0, 3}, 0.6f, 24).serialize(),
              twistExtrude(kSquare, {0, 0, 3}, 0.6f, 24).serialize());
}

}  // namespace nexus::geometry::brep::testing
