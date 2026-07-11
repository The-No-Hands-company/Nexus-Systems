#include <nexus/geometry/TriangleRetriangulate.h>
#include <nexus/geometry/ConstrainedDelaunay2D.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace nexus::geometry::testing {

using V = nexus::render::Vec3;

namespace {
float triArea(const V& a, const V& b, const V& c)
{
    return 0.5f * std::sqrt((b - a).cross(c - a).lengthSq());
}
float totalArea(const RetriangulationResult& r)
{
    float s = 0.f;
    for (const auto& t : r.triangles) {
        s += triArea(r.points[t[0]], r.points[t[1]], r.points[t[2]]);
    }
    return s;
}
}  // namespace

// Regression: ConstrainedDelaunay2D::triangulate previously returned ZERO
// triangles for every input (a broken Bowyer-Watson bad-triangle test). It must
// actually produce a triangulation.
TEST(TriangleRetriangulate, CDTProducesTriangulation)
{
    using nexus::geometry::Vec2;
    // 3 hull verts + 1 interior point, no constraints → a valid fan (>= 3 tris).
    const auto r = ConstrainedDelaunay2D::triangulate(
        {Vec2{0, 0}, Vec2{4, 0}, Vec2{0, 4}, Vec2{1, 1}}, {});
    EXPECT_TRUE(r.ok);
    EXPECT_GE(r.triangles.size(), 3u);
    // With a constraint edge across the interior.
    const auto rc = ConstrainedDelaunay2D::triangulate(
        {Vec2{0, 0}, Vec2{4, 0}, Vec2{0, 4}, Vec2{1, 1}, Vec2{2, 0.5f}}, {ConstraintEdge{3, 4}});
    EXPECT_TRUE(rc.ok);
    EXPECT_GE(rc.triangles.size(), 3u);
}

TEST(TriangleRetriangulate, NoSegmentsReturnsSingleTriangle)
{
    const V a{0, 0, 0}, b{4, 0, 0}, c{0, 4, 0};
    const RetriangulationResult r = TriangleRetriangulate::apply(a, b, c, {});
    EXPECT_EQ(r.triangles.size(), 1u);
    EXPECT_EQ(r.points.size(), 3u);
    EXPECT_NEAR(totalArea(r), 8.f, 1e-3f);
}

TEST(TriangleRetriangulate, OneSegmentSplitsAndPreservesArea)
{
    const V a{0, 0, 0}, b{4, 0, 0}, c{0, 4, 0};  // area 8, in z=0
    std::vector<IntersectionSegment> segs{IntersectionSegment{V{2, 0, 0}, V{0, 2, 0}}};

    const RetriangulationResult r = TriangleRetriangulate::apply(a, b, c, segs);
    EXPECT_GE(r.triangles.size(), 2u);   // the segment forced a split
    EXPECT_GE(r.points.size(), 5u);      // 3 verts + 2 distinct segment endpoints
    EXPECT_NEAR(totalArea(r), 8.f, 0.05f);  // sub-triangles tile the original
    for (const auto& t : r.triangles) {
        EXPECT_GT(triArea(r.points[t[0]], r.points[t[1]], r.points[t[2]]), 1e-5f);  // non-degenerate
    }
    // All output points lie on the z=0 plane (lift is correct).
    for (const V& p : r.points) {
        EXPECT_NEAR(p.z, 0.f, 1e-4f);
    }
}

TEST(TriangleRetriangulate, InteriorSegmentSplits)
{
    const V a{0, 0, 0}, b{4, 0, 0}, c{0, 4, 0};
    // A segment fully inside the triangle.
    std::vector<IntersectionSegment> segs{IntersectionSegment{V{1, 1, 0}, V{2, 0.5f, 0}}};
    const RetriangulationResult r = TriangleRetriangulate::apply(a, b, c, segs);
    EXPECT_GE(r.triangles.size(), 3u);
    EXPECT_NEAR(totalArea(r), 8.f, 0.05f);
}

TEST(TriangleRetriangulate, DegenerateTriangleFallsBackToSingle)
{
    const V a{0, 0, 0}, b{4, 0, 0}, c{8, 0, 0};  // collinear -> zero area
    std::vector<IntersectionSegment> segs{IntersectionSegment{V{2, 0, 0}, V{6, 0, 0}}};
    const RetriangulationResult r = TriangleRetriangulate::apply(a, b, c, segs);
    EXPECT_EQ(r.triangles.size(), 1u);
}

TEST(TriangleRetriangulate, NonFiniteFallsBack)
{
    const V a{0, 0, 0}, b{4, 0, 0}, c{0, 4, 0};
    const V nan{std::numeric_limits<float>::quiet_NaN(), 0, 0};
    const RetriangulationResult r =
        TriangleRetriangulate::apply(nan, b, c, {IntersectionSegment{V{2, 0, 0}, V{0, 2, 0}}});
    EXPECT_EQ(r.triangles.size(), 1u);
}

}  // namespace nexus::geometry::testing
