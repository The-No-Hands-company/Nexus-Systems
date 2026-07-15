// Foundation — analytic B-rep revolve (solid of revolution). revolveProfile
// sweeps a closed planar profile 360° about an axis into a ring/torus-like solid
// (quad band per profile edge, no caps). Volume matches Pappus's theorem; being
// torus-like the boundary has euler characteristic 0 (genus 1).

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

TEST(BRepRevolve, SquareRevolveMatchesPappus)
{
    // Unit square at x∈[2,3], z∈[0,1] (in the xz plane) revolved about the z-axis.
    // Pappus: V = 2π·R̄·A, R̄ = 2.5 (centroid radius), A = 1.
    const std::vector<Vec3> sq = {{2, 0, 0}, {3, 0, 0}, {3, 0, 1}, {2, 0, 1}};
    const Body b = revolveProfile(sq, {0, 0, 0}, {0, 0, 1}, 128);

    const auto ig = b.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 0);           // torus surface: V−E+F = 0, genus 1
    EXPECT_EQ(ig.boundaryEdges, 0u);  // watertight (full revolution, no caps)
    EXPECT_TRUE(b.isClosed());
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;

    // Topology counts: V = m·s, E = 2·m·s, F = m·s.
    EXPECT_EQ(ig.faces, 4u * 128u);
    EXPECT_EQ(ig.vertices, 4u * 128u);
    EXPECT_EQ(ig.edges, 2u * 4u * 128u);

    const double pappus = 2.0 * M_PI * 2.5 * 1.0;
    EXPECT_NEAR(b.massProperties().volume, static_cast<float>(pappus), 0.03);
}

TEST(BRepRevolve, TorusVolumeMatchesPappus)
{
    // Small square cross-section (side 0.4, area 0.16) centred at radius 5.
    const std::vector<Vec3> t = {
        {4.8, 0, -0.2}, {5.2, 0, -0.2}, {5.2, 0, 0.2}, {4.8, 0, 0.2}};
    const Body b = revolveProfile(t, {0, 0, 0}, {0, 0, 1}, 128);
    EXPECT_TRUE(b.checkIntegrity().ok);
    EXPECT_EQ(b.checkIntegrity().euler, 0);
    EXPECT_TRUE(b.isClosed());
    EXPECT_TRUE(b.checkGeometry().ok);
    const double pappus = 2.0 * M_PI * 5.0 * 0.16;
    EXPECT_NEAR(b.massProperties().volume, static_cast<float>(pappus), 0.01);
}

TEST(BRepRevolve, DegenerateInputRejected)
{
    const std::vector<Vec3> sq = {{2, 0, 0}, {3, 0, 0}, {3, 0, 1}, {2, 0, 1}};
    // Profile crossing the axis (x from −1 to 1).
    const std::vector<Vec3> crossing = {{-1, 0, 0}, {1, 0, 0}, {1, 0, 1}, {-1, 0, 1}};
    EXPECT_EQ(revolveProfile(crossing, {0, 0, 0}, {0, 0, 1}, 16).faceCount(), 0u);
    EXPECT_EQ(revolveProfile(sq, {0, 0, 0}, {0, 0, 1}, 2).faceCount(), 0u);        // segments<3
    EXPECT_EQ(revolveProfile({{2, 0, 0}, {3, 0, 0}}, {0, 0, 0}, {0, 0, 1}, 16).faceCount(),
              0u);                                                                // <3 points
    EXPECT_EQ(revolveProfile(sq, {0, 0, 0}, {0, 0, 0}, 16).faceCount(), 0u);      // zero axis
    // Collinear (zero-area) profile.
    EXPECT_EQ(revolveProfile({{2, 0, 0}, {3, 0, 0}, {4, 0, 0}}, {0, 0, 0}, {0, 0, 1}, 16)
                  .faceCount(),
              0u);
    // Non-finite coordinate.
    std::vector<Vec3> bad = sq;
    bad[1].x = std::bit_cast<float>(0x7F800000u);  // +Inf
    EXPECT_EQ(revolveProfile(bad, {0, 0, 0}, {0, 0, 1}, 16).faceCount(), 0u);
}

TEST(BRepRevolve, Deterministic)
{
    const std::vector<Vec3> sq = {{2, 0, 0}, {3, 0, 0}, {3, 0, 1}, {2, 0, 1}};
    const Body a = revolveProfile(sq, {0, 0, 0}, {0, 0, 1}, 32);
    const Body b = revolveProfile(sq, {0, 0, 0}, {0, 0, 1}, 32);
    EXPECT_EQ(a.vertexCount(), b.vertexCount());
    EXPECT_EQ(a.faceCount(), b.faceCount());
    EXPECT_EQ(a.massProperties().volume, b.massProperties().volume);
}

}  // namespace nexus::geometry::brep::testing
