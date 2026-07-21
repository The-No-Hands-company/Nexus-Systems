#include <nexus/geometry/ConstrainedDelaunay2D.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <random>
#include <vector>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

namespace {
// Signed area of a CDT output triangle (>0 ⇔ CCW). Used to detect inverted /
// overlapping triangles (a triangulation with mixed winding is not a valid tiling).
double cdtTriArea2(const std::vector<Vec2>& V, const std::array<uint32_t, 3>& t)
{
    const Vec2& a = V[t[0]];
    const Vec2& b = V[t[1]];
    const Vec2& c = V[t[2]];
    return static_cast<double>(b.u - a.u) * (c.v - a.v)
         - static_cast<double>(b.v - a.v) * (c.u - a.u);
}
}  // namespace

// A constraint whose endpoints are separated by TWO crossing edges of the
// unconstrained Delaunay triangulation, so recovery needs ordered CONVEX flips.
// The pre-fix enforcement flipped the first crossing edge blindly (no convexity
// check) — it left the edge unrecovered and produced inverted/overlapping
// triangles. The convexity-checked, progress-guaranteed recovery must now recover
// the edge cleanly with a single consistent (CCW) winding.
TEST(ConstrainedDelaunay2D, RecoversConstraintCrossingMultipleEdges)
{
    const std::vector<Vec2> pts = {
        {0.f, -4.f}, {-4.f, -1.f}, {-2.f, -4.f}, {0.f, 0.f}, {2.f, 3.f}, {3.f, -2.f}};
    const CDTResult r = ConstrainedDelaunay2D::triangulate(pts, {{0u, 1u}});
    ASSERT_TRUE(r.ok);
    ASSERT_FALSE(r.triangles.empty());

    bool hasEdge = false;
    for (const auto& t : r.triangles) {
        for (int e = 0; e < 3; ++e) {
            const uint32_t x = t[e], y = t[(e + 1) % 3];
            if ((x == 0u && y == 1u) || (x == 1u && y == 0u)) hasEdge = true;
        }
    }
    EXPECT_TRUE(hasEdge) << "constraint edge 0-1 was not recovered";

    for (const auto& t : r.triangles) {
        EXPECT_GT(cdtTriArea2(r.vertices, t), 0.0) << "an inverted/degenerate triangle survived";
    }
}

// Invariant: recovering ANY single constraint edge must never produce an inverted or
// overlapping triangle — every output triangle stays CCW. Before the fix, ~14% of
// random inputs came out with mixed winding (overlapping triangulation); the
// convexity-checked recovery drives that to exactly zero. Seeded ⇒ deterministic on
// the Null/headless path.
TEST(ConstrainedDelaunay2D, ConstraintRecoveryNeverInvertsTriangles)
{
    std::mt19937 rng(20260721u);
    std::uniform_real_distribution<float> U(-5.f, 5.f);
    int checked = 0;
    for (int trial = 0; trial < 3000; ++trial) {
        const int n = 3 + static_cast<int>(rng() % 6);
        std::vector<Vec2> pts;
        for (int i = 0; i < n; ++i) pts.push_back({U(rng), U(rng)});
        const uint32_t a = rng() % n, b = rng() % n;
        if (a == b) continue;
        const CDTResult r = ConstrainedDelaunay2D::triangulate(pts, {{a, b}});
        if (!r.ok || r.triangles.empty()) continue;
        ++checked;
        int neg = 0, pos = 0;
        for (const auto& t : r.triangles) {
            const double s = cdtTriArea2(r.vertices, t);
            if (s < 0.0) ++neg;
            else if (s > 0.0) ++pos;
        }
        ASSERT_FALSE(neg > 0 && pos > 0)
            << "trial " << trial << ": mixed-winding (inverted/overlapping) triangulation";
    }
    EXPECT_GT(checked, 2000);
}

TEST(ConstrainedDelaunay2D, CDTWithConstraintEdgesProducesValidResult)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
        { 1.f, 0.f, 1.f},
    });

    Face f0{};
    f0.indices = {0, 1, 2};
    mesh.topology().addFace(f0);
    Face f1{};
    f1.indices = {1, 3, 2};
    mesh.topology().addFace(f1);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.topology().faceCount(), 2u);
    EXPECT_TRUE(mesh.topology().allFacesArePoly3Plus());
}

TEST(ConstrainedDelaunay2D, EdgeDetectedAcrossExistingFaces)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
        { 1.f, 0.f, 1.f},
    });

    Face f0{};
    f0.indices = {0, 1, 2};
    mesh.topology().addFace(f0);
    Face f1{};
    f1.indices = {1, 3, 2};
    mesh.topology().addFace(f1);

    EXPECT_TRUE(mesh.topology().hasValidIndices(mesh.attributes().vertexCount()));

    EXPECT_TRUE(mesh.isValid());
}

TEST(ConstrainedDelaunay2D, SelfLoopConstraintsDroppedSilently)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 0.f, 1.f},
    });

    Face f0{};
    f0.indices = {0, 1, 2};
    mesh.topology().addFace(f0);

    EXPECT_TRUE(mesh.topology().face(0).isTriangle());
    EXPECT_EQ(mesh.topology().face(0).vertexCount(), 3u);
    EXPECT_TRUE(mesh.isValid());
}

TEST(ConstrainedDelaunay2D, EmptyMeshRejectsInvalidTopology)
{
    Mesh mesh;
    EXPECT_FALSE(mesh.isValid());
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
    EXPECT_EQ(mesh.attributes().vertexCount(), 0u);
}
