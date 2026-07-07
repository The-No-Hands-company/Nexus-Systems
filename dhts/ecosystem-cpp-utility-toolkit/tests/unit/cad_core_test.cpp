#include <gtest/gtest.h>
#include <cmath>
#include <vector>

#include "nexus/utility/graphics/half_edge_mesh.h"
#include "nexus/utility/graphics/octree.h"
#include "nexus/utility/graphics/exact_predicates.h"
#include "nexus/utility/graphics/curve_utils.h"

using namespace nexus::utility::graphics;

// ── HalfEdgeMesh ────────────────────────────────────────────────────────────

TEST(HalfEdgeMeshTest, AddTriangle) {
    HalfEdgeMesh m;
    Vector3 v0{0,0,0}, v1{1,0,0}, v2{0,1,0};
    uint32_t f = m.addFace(v0, v1, v2);
    EXPECT_EQ(m.faceCount(), 1);
    EXPECT_EQ(m.vertexCount(), 3);
    EXPECT_EQ(m.edgeCount(), 3);
}

TEST(HalfEdgeMeshTest, SharedEdge) {
    HalfEdgeMesh m;
    // Two triangles sharing an edge
    m.addFace({0,0,0}, {1,0,0}, {0,1,0});
    m.addFace({1,0,0}, {1,1,0}, {0,1,0});
    EXPECT_EQ(m.faceCount(), 2);
    EXPECT_EQ(m.vertexCount(), 4);
    EXPECT_EQ(m.edgeCount(), 5); // 6 half-edges, 5 unique edges (1 shared)
}

TEST(HalfEdgeMeshTest, VertexOneRing) {
    HalfEdgeMesh m;
    m.addFace({0,0,0}, {1,0,0}, {0,1,0});
    m.addFace({1,0,0}, {1,1,0}, {0,1,0});
    m.addFace({0,0,0}, {0,1,0}, {-1,0,0});
    auto ring = m.vertexOneRing(0); // Center vertex
    EXPECT_GE(ring.size(), 2);
}

TEST(HalfEdgeMeshTest, BoundaryDetection) {
    HalfEdgeMesh m;
    m.addFace({0,0,0}, {1,0,0}, {0,1,0});
    auto boundaries = m.boundaryEdges();
    EXPECT_EQ(boundaries.size(), 3); // All edges are boundary on a single triangle
}

// ── Octree ──────────────────────────────────────────────────────────────────

TEST(OctreeTest, InsertAndSearch) {
    Octree<int> tree({0,0,0}, {10,10,10}, 4);
    tree.insert({1,1,1}, 42);
    tree.insert({2,2,2}, 100);
    tree.insert({5,5,5}, 200);

    auto results = tree.radiusSearch({1.5,1.5,1.5}, 2.0f);
    EXPECT_EQ(results.size(), 2);

    auto box = tree.boxQuery({0,0,0}, {2,2,2});
    EXPECT_EQ(box.size(), 2);
}

TEST(OctreeTest, LargeDataset) {
    Octree<float> tree({0,0,0}, {100,100,100}, 8);
    for (int i = 0; i < 500; ++i)
        tree.insert({static_cast<float>(i % 100), static_cast<float>((i / 10) % 100), 0}, static_cast<float>(i));
    EXPECT_GT(tree.size(), 400);
    auto results = tree.radiusSearch({50,50,0}, 10.0f);
    EXPECT_GT(results.size(), 0);
}

// ── ExactPredicates ─────────────────────────────────────────────────────────

TEST(ExactPredicatesTest, Orient2D) {
    EXPECT_GT(ExactPredicates::orient2D(0,0, 1,0, 0,1), 0);   // CCW
    EXPECT_LT(ExactPredicates::orient2D(0,0, 0,1, 1,0), 0);   // CW
    EXPECT_NEAR(ExactPredicates::orient2D(0,0, 0.5,0, 1,0), 0.0, 1e-10); // Collinear
}

TEST(ExactPredicatesTest, SegmentsIntersect) {
    EXPECT_TRUE(ExactPredicates::segmentsIntersect(0,0, 1,1, 0,1, 1,0));
    EXPECT_FALSE(ExactPredicates::segmentsIntersect(0,0, 0.5,0.5, 0.6,0.6, 1,1));
}

TEST(ExactPredicatesTest, PointInTriangle) {
    EXPECT_TRUE(ExactPredicates::pointInTriangle(0.25, 0.25, 0,0, 1,0, 0,1));
    EXPECT_FALSE(ExactPredicates::pointInTriangle(1, 1, 0,0, 1,0, 0,1));
}

// ── CurveUtils ──────────────────────────────────────────────────────────────

TEST(CurveUtilsTest, BezierLinear) {
    std::vector<Vector3> pts = {{0,0,0}, {10,0,0}};
    Vector3 mid = CurveUtils::bezierEval(pts, 0.5f);
    EXPECT_NEAR(mid.x, 5.0f, 0.01f);
    EXPECT_NEAR(mid.y, 0.0f, 0.01f);
}

TEST(CurveUtilsTest, BezierCubic) {
    std::vector<Vector3> pts = {{0,0,0}, {1,2,0}, {2,2,0}, {3,0,0}};
    Vector3 start = CurveUtils::bezierEval(pts, 0.0f);
    Vector3 end = CurveUtils::bezierEval(pts, 1.0f);
    EXPECT_NEAR(start.x, 0.0f, 0.01f);
    EXPECT_NEAR(end.x, 3.0f, 0.01f);
}

TEST(CurveUtilsTest, BezierDerivative) {
    std::vector<Vector3> pts = {{0,0,0}, {10,0,0}};
    Vector3 d = CurveUtils::bezierDerivative(pts, 0.5f);
    EXPECT_NEAR(d.x, 10.0f, 0.01f);
}

TEST(CurveUtilsTest, BezierSubdivide) {
    std::vector<Vector3> pts = {{0,0,0}, {1,1,0}, {2,0,0}};
    auto [left, right] = CurveUtils::bezierSubdivide(pts, 0.5f);
    EXPECT_EQ(left.size(), 3);
    EXPECT_EQ(right.size(), 3);
    EXPECT_NEAR(left[2].x, right[0].x, 0.01f); // Continuity
}

TEST(CurveUtilsTest, BezierLength) {
    std::vector<Vector3> pts = {{0,0,0}, {10,0,0}};
    float len = CurveUtils::bezierLength(pts);
    EXPECT_NEAR(len, 10.0f, 0.1f);
}
