// Foundation — analytic B-rep extrude (the fundamental parametric creation op).
// extrudeProfile builds a prism solid from a closed planar profile + a direction:
// two caps and one quad side per edge, watertight, with volume = area·height.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

TEST(BRepExtrude, SquareExtrudesToBox)
{
    const std::vector<Vec3> square = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    const Body b = extrudeProfile(square, {0, 0, 3});

    const auto ig = b.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_EQ(ig.faces, 6u);  // 2 caps + 4 sides
    EXPECT_TRUE(b.isClosed());
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;

    const auto mp = b.massProperties();
    EXPECT_NEAR(mp.volume, 3.f, 1e-4f);  // area 1 · height 3
    EXPECT_NEAR(mp.centroid.x, 0.5f, 1e-4f);
    EXPECT_NEAR(mp.centroid.y, 0.5f, 1e-4f);
    EXPECT_NEAR(mp.centroid.z, 1.5f, 1e-4f);  // profile centroid + dir/2
}

TEST(BRepExtrude, TrianglePrism)
{
    const std::vector<Vec3> tri = {{0, 0, 0}, {2, 0, 0}, {0, 1, 0}};  // area 1
    const Body b = extrudeProfile(tri, {0, 0, 2});
    EXPECT_TRUE(b.checkIntegrity().ok);
    EXPECT_EQ(b.checkIntegrity().euler, 2);
    EXPECT_EQ(b.checkIntegrity().faces, 5u);  // 2 caps + 3 sides
    EXPECT_TRUE(b.checkGeometry().ok);
    EXPECT_NEAR(b.massProperties().volume, 2.f, 1e-4f);
}

TEST(BRepExtrude, PentagonPrism)
{
    std::vector<Vec3> pent;
    for (int k = 0; k < 5; ++k) {
        const float a = 2.f * static_cast<float>(M_PI) * static_cast<float>(k) / 5.f;
        pent.push_back({std::cos(a), std::sin(a), 0.f});
    }
    const Body b = extrudeProfile(pent, {0, 0, 1});
    EXPECT_TRUE(b.checkIntegrity().ok);
    EXPECT_EQ(b.checkIntegrity().euler, 2);
    EXPECT_EQ(b.checkIntegrity().faces, 7u);  // 2 caps + 5 sides
    EXPECT_TRUE(b.checkGeometry().ok);
    const double area = 2.5 * std::sin(2.0 * M_PI / 5.0);  // regular unit-circumradius pentagon
    EXPECT_NEAR(b.massProperties().volume, static_cast<float>(area), 1e-4f);
}

TEST(BRepExtrude, DownwardAndSlantedAreValidPositiveVolume)
{
    const std::vector<Vec3> square = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};

    // Extruding along −Z still yields a valid, positively-oriented solid.
    const Body down = extrudeProfile(square, {0, 0, -2});
    EXPECT_TRUE(down.checkIntegrity().ok);
    EXPECT_EQ(down.checkIntegrity().euler, 2);
    EXPECT_TRUE(down.isClosed());
    EXPECT_NEAR(down.massProperties().volume, 2.f, 1e-4f);

    // Slanted extrusion: volume = area · projected height along the plane normal.
    const Body slant = extrudeProfile(square, {0.5f, 0.f, 2.f});
    EXPECT_TRUE(slant.checkIntegrity().ok);
    EXPECT_TRUE(slant.checkGeometry().ok);
    EXPECT_NEAR(slant.massProperties().volume, 2.f, 1e-4f);
}

TEST(BRepExtrude, DegenerateInputRejected)
{
    const std::vector<Vec3> square = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    // dir parallel to the profile plane → zero projected height.
    EXPECT_EQ(extrudeProfile(square, {1, 0, 0}).faceCount(), 0u);
    // Fewer than 3 points.
    EXPECT_EQ(extrudeProfile({{0, 0, 0}, {1, 0, 0}}, {0, 0, 1}).faceCount(), 0u);
    // Collinear (zero-area) profile.
    EXPECT_EQ(extrudeProfile({{0, 0, 0}, {1, 0, 0}, {2, 0, 0}}, {0, 0, 1}).faceCount(), 0u);
    // Non-finite coordinate (bit-pattern +Inf, reliable under -ffast-math).
    std::vector<Vec3> bad = square;
    bad[1].x = std::bit_cast<float>(0x7F800000u);
    EXPECT_EQ(extrudeProfile(bad, {0, 0, 1}).faceCount(), 0u);
}

TEST(BRepExtrude, Deterministic)
{
    const std::vector<Vec3> square = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    const Body a = extrudeProfile(square, {0, 0, 3});
    const Body b = extrudeProfile(square, {0, 0, 3});
    EXPECT_EQ(a.vertexCount(), b.vertexCount());
    EXPECT_EQ(a.faceCount(), b.faceCount());
    EXPECT_EQ(a.massProperties().volume, b.massProperties().volume);
}

}  // namespace nexus::geometry::brep::testing
