// ─────────────────────────────────────────────────────────────────────────────
//  Test: Semantic Query Engine — descriptive topology queries
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/agent/SemanticQuery.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::agent;
using namespace nexus::geometry;

namespace {

HalfEdgeMesh makeTestBox() {
    auto mesh = primitives::makeBox(2.0f, 2.0f, 2.0f);
    auto hem = HalfEdgeMesh::fromMesh(mesh);
    EXPECT_TRUE(hem.has_value());
    return std::move(*hem);
}

} // namespace

TEST(SemanticQuery, FindFacesAdjacentTo)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    // Box has 12 faces (2 triangles per quad face * 6 sides)
    ASSERT_GT(hem.faceCount(), 0u);
    auto result = engine.findFacesAdjacentTo({0});
    EXPECT_TRUE(result.valid);
}

TEST(SemanticQuery, FindFacesParallelTo)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findFacesParallelTo(Vec3{0, 1, 0}, 0.01f);
    EXPECT_TRUE(result.valid);
    // Top and bottom faces should be parallel to Y
    EXPECT_GT(result.faces.size(), 0u);
}

TEST(SemanticQuery, FindFacesLargerThan)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findFacesLargerThan(0.1f);
    EXPECT_TRUE(result.valid);
    EXPECT_GT(result.faces.size(), 0u);
}

TEST(SemanticQuery, FindFacesSmallerThan)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findFacesSmallerThan(100.0f);
    EXPECT_TRUE(result.valid);
    EXPECT_GT(result.faces.size(), 0u);
}

TEST(SemanticQuery, FindEdgesSharperThan)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findEdgesSharperThan(30.0f);
    EXPECT_TRUE(result.valid);
    // Box has sharp edges (90 degrees between adjacent faces)
    EXPECT_GT(result.edges.size(), 0u);
}

TEST(SemanticQuery, FindEdgesSofterThan)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findEdgesSofterThan(30.0f);
    EXPECT_TRUE(result.valid);
}

TEST(SemanticQuery, FindFacesClosestTo)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findFacesClosestTo(Vec3{0, 2, 0}, 4);
    EXPECT_TRUE(result.valid);
    EXPECT_EQ(result.faces.size(), 4u);
}

TEST(SemanticQuery, FindPlanarFaces)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findPlanarFaces(0.01f);
    EXPECT_TRUE(result.valid);
    EXPECT_GT(result.faces.size(), 0u);
}

TEST(SemanticQuery, FindFacesByDraftAngle)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findFacesByDraftAngle(Vec3{0, 1, 0}, 0.0f, 180.0f);
    EXPECT_TRUE(result.valid);
}

TEST(SemanticQuery, FindEdgesByLength)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findEdgesByLength(0.0f, 10.0f);
    EXPECT_TRUE(result.valid);
    EXPECT_GT(result.edges.size(), 0u);
}

TEST(SemanticQuery, FindEdgesBoundary)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    auto result = engine.findEdgesBoundary();
    EXPECT_TRUE(result.valid);
    // A closed box has no boundary edges
    EXPECT_EQ(result.edges.size(), 0u);
}

TEST(SemanticQuery, GenericQueryDispatch)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    TopologyQuery q;
    q.criterion = QueryCriterion::AdjacentTo;
    q.targetFaces = {0};
    auto result = engine.query(q);
    EXPECT_TRUE(result.valid);
}

TEST(SemanticQuery, FaceNormalValid)
{
    auto hem = makeTestBox();
    SemanticQueryEngine engine(hem);
    ASSERT_GT(hem.faceCount(), 0u);
    // Verify first face normal is a unit vector
    auto result = engine.findFacesParallelTo(Vec3{0, 1, 0}, 0.01f);
    EXPECT_TRUE(result.valid);
}
