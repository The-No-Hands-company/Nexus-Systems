// Foundation — analytic B-rep tube / pipe (makeTube). An all-planar hollow
// cylinder built directly (outer + inner walls + annular caps, 4·n quads), robust
// at any segment count. The through-hole makes it genus 1 → euler 0; faceted
// material volume converges to π(R²−r²)h.

#include <nexus/geometry/AnalyticBRep.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace nexus::geometry::brep::testing {

TEST(BRepTube, WatertightGenus1ForAnySegmentCount)
{
    for (uint32_t n : {3u, 6u, 8u, 12u, 24u, 64u}) {
        const Body t = makeTube(2.f, 1.f, 3.f, n);
        const auto ig = t.checkIntegrity();
        ASSERT_TRUE(ig.ok) << "n=" << n << ": " << ig.reason;
        EXPECT_TRUE(t.isClosed()) << "n=" << n;
        EXPECT_EQ(ig.boundaryEdges, 0u) << "n=" << n;   // watertight
        EXPECT_EQ(ig.euler, 0) << "n=" << n;            // genus 1 (through-hole)
        EXPECT_EQ(ig.vertices, 4u * n) << "n=" << n;    // 4 rings of n
        EXPECT_EQ(ig.edges, 8u * n) << "n=" << n;
        EXPECT_EQ(ig.faces, 4u * n) << "n=" << n;       // outer+inner walls + 2 caps
        EXPECT_TRUE(t.checkGeometry().ok) << t.checkGeometry().reason;
    }
}

TEST(BRepTube, FacetedVolumeAndConvergenceToSmooth)
{
    const float R = 2.f, r = 1.f, h = 3.f;
    // Faceted n-gon annulus area = ½·n·(R²−r²)·sin(2π/n); volume = area·h.
    for (uint32_t n : {6u, 12u, 24u}) {
        const Body t = makeTube(R, r, h, n);
        const double s = std::sin(2.0 * M_PI / n);
        const double expected = 0.5 * n * (R * R - r * r) * s * h;
        EXPECT_GT(t.massProperties().volume, 0.f) << "n=" << n;  // outward winding
        EXPECT_NEAR(t.massProperties().volume, static_cast<float>(expected), 1e-3f) << "n=" << n;
    }
    // Converges to the smooth pipe volume π(R²−r²)h as segments grow.
    const double smooth = M_PI * (R * R - r * r) * h;  // = 9π ≈ 28.274
    const float v64 = makeTube(R, r, h, 64).massProperties().volume;
    const float v256 = makeTube(R, r, h, 256).massProperties().volume;
    EXPECT_LT(v64, v256);                                 // monotone increase
    EXPECT_LT(v256, static_cast<float>(smooth));          // still under the smooth limit
    EXPECT_NEAR(v256, static_cast<float>(smooth), 0.02f);  // close at 256
}

TEST(BRepTube, DegenerateInputRejected)
{
    EXPECT_EQ(makeTube(1.f, 2.f, 3.f, 8).faceCount(), 0u);   // outer ≤ inner
    EXPECT_EQ(makeTube(2.f, 2.f, 3.f, 8).faceCount(), 0u);   // outer == inner
    EXPECT_EQ(makeTube(2.f, 0.f, 3.f, 8).faceCount(), 0u);   // inner ≤ 0
    EXPECT_EQ(makeTube(2.f, -1.f, 3.f, 8).faceCount(), 0u);
    EXPECT_EQ(makeTube(2.f, 1.f, 0.f, 8).faceCount(), 0u);   // height ≤ 0
    const float inf = std::numeric_limits<float>::infinity();
    EXPECT_EQ(makeTube(inf, 1.f, 3.f, 8).faceCount(), 0u);   // non-finite
    // segments < 3 is clamped to 3 (a triangular tube), not rejected.
    EXPECT_EQ(makeTube(2.f, 1.f, 3.f, 1).checkIntegrity().faces, 12u);
}

TEST(BRepTube, Deterministic)
{
    EXPECT_EQ(makeTube(2.f, 1.f, 3.f, 16).serialize(), makeTube(2.f, 1.f, 3.f, 16).serialize());
}

}  // namespace nexus::geometry::brep::testing
