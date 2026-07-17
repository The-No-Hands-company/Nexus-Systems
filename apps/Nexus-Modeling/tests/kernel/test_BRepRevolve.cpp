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

TEST(BRepRevolve, SemicircleRevolvesToSphere)
{
    // Polygonal semicircle (radius 1): the two ends sit on the z-axis (poles),
    // the arc bulges to +x; the closing diameter edge lies on the axis. Revolved
    // 360° → a faceted sphere (a FILLED, genus-0 solid → euler 2).
    const float r = 1.f;
    const int K = 48;
    std::vector<Vec3> semi;
    semi.push_back({0, 0, -r});  // bottom pole (on axis)
    for (int k = 1; k < K; ++k) {
        const float a = -static_cast<float>(M_PI) / 2.f + static_cast<float>(M_PI) * k / K;
        semi.push_back({r * std::cos(a), 0.f, r * std::sin(a)});
    }
    semi.push_back({0, 0, r});  // top pole (on axis)

    const Body b = revolveProfile(semi, {0, 0, 0}, {0, 0, 1}, 64);
    const auto ig = b.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);  // sphere boundary: genus 0
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_TRUE(b.isClosed());
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
    EXPECT_NEAR(b.massProperties().volume, static_cast<float>(4.0 / 3.0 * M_PI), 0.05f);
}

TEST(BRepRevolve, TriangleRevolvesToCone)
{
    // Triangle with two vertices on the axis (apex-line) → a cone r=1, h=2.
    const std::vector<Vec3> tri = {{0, 0, 0}, {1, 0, 0}, {0, 0, 2}};
    const Body b = revolveProfile(tri, {0, 0, 0}, {0, 0, 1}, 64);
    const auto ig = b.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.euler, 2);
    EXPECT_TRUE(b.isClosed());
    EXPECT_TRUE(b.checkGeometry().ok);
    EXPECT_NEAR(b.massProperties().volume, static_cast<float>(M_PI / 3.0 * 1.0 * 1.0 * 2.0),
                0.02f);  // ⅓ π r² h
}

TEST(BRepRevolve, NonTouchingRingStaysEuler0)
{
    // The pole handling must not regress the non-axis-touching ring (torus).
    const std::vector<Vec3> ring = {{2, 0, 0}, {3, 0, 0}, {3, 0, 1}, {2, 0, 1}};
    const Body b = revolveProfile(ring, {0, 0, 0}, {0, 0, 1}, 32);
    EXPECT_TRUE(b.checkIntegrity().ok);
    EXPECT_EQ(b.checkIntegrity().euler, 0);  // genus 1
    EXPECT_TRUE(b.isClosed());
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

// ──────────── Partial revolve (arc solid with two end caps) ───────────────────

// A partial revolve is a CAPPED arc: genus 0 (euler 2, watertight), with volume
// = (θ/2π)·(full Pappus volume) = θ·R̄·A.  Unit square at x∈[1.5,2.5] (R̄=2, A=1).
TEST(BRepRevolvePartial, CappedArcMatchesPartialPappus)
{
    const std::vector<Vec3> sq = {{1.5, 0, -0.5}, {2.5, 0, -0.5}, {2.5, 0, 0.5}, {1.5, 0, 0.5}};
    const uint32_t seg = 128;
    for (float frac : {0.25f, 0.5f, 0.75f}) {
        const float theta = static_cast<float>(2.0 * M_PI) * frac;
        const Body b = revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 1}, seg, theta);
        const auto ig = b.checkIntegrity();
        ASSERT_TRUE(ig.ok) << "frac=" << frac << ": " << ig.reason;
        EXPECT_EQ(ig.euler, 2) << "frac=" << frac;          // genus 0, NOT the torus
        EXPECT_EQ(ig.boundaryEdges, 0u) << "frac=" << frac;  // watertight (caps close it)
        EXPECT_TRUE(b.isClosed()) << "frac=" << frac;
        EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
        // Topology: V = m·(seg+1), F = m·seg + 2 caps.
        EXPECT_EQ(ig.vertices, 4u * (seg + 1u)) << "frac=" << frac;
        EXPECT_EQ(ig.faces, 4u * seg + 2u) << "frac=" << frac;
        // Volume = θ·R̄·A (winding must be OUTWARD → positive, not clamped to 0).
        const double expected = static_cast<double>(theta) * 2.0 * 1.0;
        EXPECT_GT(b.massProperties().volume, 0.f) << "frac=" << frac;
        EXPECT_NEAR(b.massProperties().volume, static_cast<float>(expected), 0.02)
            << "frac=" << frac;
    }
}

// θ ≥ 2π delegates to the full revolve → the genus-1 ring (euler 0), byte-identical.
TEST(BRepRevolvePartial, FullSweepDelegatesToRing)
{
    const std::vector<Vec3> sq = {{2, 0, 0}, {3, 0, 0}, {3, 0, 1}, {2, 0, 1}};
    const Body partial = revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 1}, 64,
                                               static_cast<float>(2.0 * M_PI));
    const Body full = revolveProfile(sq, {0, 0, 0}, {0, 0, 1}, 64);
    EXPECT_EQ(partial.checkIntegrity().euler, 0);  // torus
    EXPECT_EQ(partial.serialize(), full.serialize());  // byte-identical to the full path
}

TEST(BRepRevolvePartial, DegenerateInputRejected)
{
    const std::vector<Vec3> sq = {{1.5, 0, -0.5}, {2.5, 0, -0.5}, {2.5, 0, 0.5}, {1.5, 0, 0.5}};
    const float half = static_cast<float>(M_PI);
    EXPECT_EQ(revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 1}, 16, 0.f).faceCount(), 0u);    // θ=0
    EXPECT_EQ(revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 1}, 16, -1.f).faceCount(), 0u);   // θ<0
    EXPECT_EQ(revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 1}, 2, half).faceCount(), 0u);    // seg<3
    EXPECT_EQ(revolveProfilePartial({{2, 0, 0}, {3, 0, 0}}, {0, 0, 0}, {0, 0, 1}, 16, half)
                  .faceCount(), 0u);                                                        // <3 pts
    EXPECT_EQ(revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 0}, 16, half).faceCount(), 0u);   // zero axis
    // Axis-touching profile (a vertex on the axis) is unsupported for partial.
    const std::vector<Vec3> touch = {{0, 0, 0}, {1, 0, 0}, {1, 0, 1}};
    EXPECT_EQ(revolveProfilePartial(touch, {0, 0, 0}, {0, 0, 1}, 16, half).faceCount(), 0u);
    // Non-finite sweep angle.
    const float inf = std::bit_cast<float>(0x7F800000u);
    EXPECT_EQ(revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 1}, 16, inf).faceCount(), 0u);
}

TEST(BRepRevolvePartial, Deterministic)
{
    const std::vector<Vec3> sq = {{1.5, 0, -0.5}, {2.5, 0, -0.5}, {2.5, 0, 0.5}, {1.5, 0, 0.5}};
    const float t = static_cast<float>(M_PI) * 0.5f;
    const Body a = revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 1}, 40, t);
    const Body b = revolveProfilePartial(sq, {0, 0, 0}, {0, 0, 1}, 40, t);
    EXPECT_EQ(a.serialize(), b.serialize());
}

}  // namespace nexus::geometry::brep::testing
