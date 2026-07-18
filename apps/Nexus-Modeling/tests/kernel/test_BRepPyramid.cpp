// Foundation — analytic B-rep pyramid / cone (makePyramid). A planar base polygon
// fanned to an apex: n triangular sides + the base cap → a watertight genus-0
// solid with exact volume ⅓·A·h. An n-gon base gives a faceted cone.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

// A square pyramid: base area 4, apex height 3 → volume ⅓·4·3 = 4.
TEST(BRepPyramid, SquarePyramidExactVolume)
{
    const std::vector<Vec3> base = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    const Body p = makePyramid(base, {0, 0, 3});
    const auto ig = p.checkIntegrity();
    ASSERT_TRUE(ig.ok) << ig.reason;
    EXPECT_TRUE(p.isClosed());
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_EQ(ig.euler, 2);            // genus 0
    EXPECT_EQ(ig.vertices, 5u);        // 4 base + apex
    EXPECT_EQ(ig.faces, 5u);           // 4 sides + base
    EXPECT_TRUE(p.checkGeometry().ok) << p.checkGeometry().reason;
    EXPECT_NEAR(p.massProperties().volume, 4.f, 1e-3f);
}

// Volume is ⅓·A·h for ANY apex position at perpendicular height h (oblique
// pyramid) and for an apex on EITHER side of the base plane (outward winding).
TEST(BRepPyramid, VolumeIsThirdBaseTimesHeightRegardlessOfApex)
{
    const std::vector<Vec3> base = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};  // area 4
    EXPECT_NEAR(makePyramid(base, {0, 0, 3}).massProperties().volume, 4.f, 1e-3f);   // above
    EXPECT_NEAR(makePyramid(base, {0, 0, -3}).massProperties().volume, 4.f, 1e-3f);  // below
    EXPECT_NEAR(makePyramid(base, {2, 1, 3}).massProperties().volume, 4.f, 1e-3f);   // oblique
    for (const Vec3& apex : {Vec3{0, 0, 3}, Vec3{0, 0, -3}, Vec3{2, 1, 3}})
        EXPECT_EQ(makePyramid(base, apex).checkIntegrity().euler, 2);
}

// An n-gon base gives a faceted cone whose volume ⅓·(½n·R²·sin(2π/n))·h converges
// to the smooth cone ⅓·πR²·h as n grows.
TEST(BRepPyramid, FacetedConeVolumeAndConvergence)
{
    const float R = 2.f, h = 3.f;
    const float twoPi = 6.28318530717958647692f;
    for (uint32_t n : {6u, 12u, 32u}) {
        std::vector<Vec3> ring;
        for (uint32_t k = 0; k < n; ++k) {
            const float t = twoPi * static_cast<float>(k) / static_cast<float>(n);
            ring.push_back({R * std::cos(t), R * std::sin(t), 0.f});
        }
        const Body c = makePyramid(ring, {0, 0, h});
        ASSERT_TRUE(c.checkIntegrity().ok) << "n=" << n;
        EXPECT_EQ(c.checkIntegrity().euler, 2) << "n=" << n;
        EXPECT_EQ(c.checkIntegrity().faces, n + 1u) << "n=" << n;  // n sides + base
        const double expected = (1.0 / 3.0) * (0.5 * n * R * R * std::sin(twoPi / n)) * h;
        EXPECT_GT(c.massProperties().volume, 0.f) << "n=" << n;
        EXPECT_NEAR(c.massProperties().volume, static_cast<float>(expected), 1e-3f) << "n=" << n;
    }
    const double smooth = (1.0 / 3.0) * M_PI * R * R * h;  // = 4π ≈ 12.566
    const float v256 = makePyramid([&] {
        std::vector<Vec3> ring;
        for (uint32_t k = 0; k < 256; ++k) {
            const float t = twoPi * static_cast<float>(k) / 256.f;
            ring.push_back({R * std::cos(t), R * std::sin(t), 0.f});
        }
        return ring;
    }(), {0, 0, h}).massProperties().volume;
    EXPECT_LT(v256, static_cast<float>(smooth));
    EXPECT_NEAR(v256, static_cast<float>(smooth), 0.01f);
}

TEST(BRepPyramid, DegenerateInputRejected)
{
    const std::vector<Vec3> base = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    EXPECT_EQ(makePyramid({{0, 0, 0}, {1, 0, 0}}, {0, 0, 1}).faceCount(), 0u);        // <3 base
    EXPECT_EQ(makePyramid(base, {0.5f, 0.5f, 0.f}).faceCount(), 0u);                   // apex in plane
    EXPECT_EQ(makePyramid({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}, {0, 0, 1}).faceCount(), 0u);  // collinear
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_EQ(makePyramid(base, {0, 0, inf}).faceCount(), 0u);                          // non-finite apex
    std::vector<Vec3> bad = base;
    bad[0].x = inf;
    EXPECT_EQ(makePyramid(bad, {0, 0, 3}).faceCount(), 0u);                             // non-finite base
}

TEST(BRepPyramid, Deterministic)
{
    const std::vector<Vec3> base = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
    EXPECT_EQ(makePyramid(base, {0, 0, 3}).serialize(), makePyramid(base, {0, 0, 3}).serialize());
}

}  // namespace nexus::geometry::brep::testing
