#include <gtest/gtest.h>

#include <nexus/geometry/EdgeSlide.h>
#include <nexus/geometry/MeshVertexMerge.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <optional>
#include <vector>

namespace {

using namespace nexus::geometry;
using nexus::render::Vec3;

HalfEdgeMesh planeHem() {
    Mesh plane = primitives::makePlane(4.f, 4.f, 4, 4);  // 5x5 grid in the XZ plane, y = 0
    (void)plane.topology().triangulate();
    auto hem = HalfEdgeMesh::fromMesh(plane);
    EXPECT_TRUE(hem.has_value());
    return std::move(*hem);
}

// Index of an interior grid vertex (edges fan around it inside the XZ plane).
uint32_t interiorVertex(const HalfEdgeMesh& hem) {
    const auto& pos = hem.positions();
    for (uint32_t i = 0; i < pos.size(); ++i)
        if (std::abs(pos[i].x) < 1.9f && std::abs(pos[i].z) < 1.9f) return i;
    return HalfEdgeMesh::kInvalid;
}

} // namespace

// A slide delta perpendicular to every incident edge (here, along the surface normal Y for a
// flat XZ patch) projects to zero on each edge, so the vertex must not move — it stays in
// the surface. The old code walked the face loop and summed spurious directions, moving the
// vertex off the plane.
TEST(EdgeSlide, NormalDeltaDoesNotMoveAPlanarVertex) {
    HalfEdgeMesh hem = planeHem();
    const uint32_t v = interiorVertex(hem);
    ASSERT_NE(v, HalfEdgeMesh::kInvalid);
    const Vec3 before = hem.positions()[v];

    EdgeSlide::slideVertices(hem, {v}, {0.f, 1.f, 0.f});  // along the normal

    const Vec3 after = hem.positions()[v];
    EXPECT_NEAR(after.x, before.x, 1e-5f);
    EXPECT_NEAR(after.y, before.y, 1e-5f) << "a normal-direction slide must not leave the surface";
    EXPECT_NEAR(after.z, before.z, 1e-5f);
}

// An in-plane delta slides the vertex: the X component is the average of edgeDir.x^2 over the
// incident edges, which is strictly positive, so X always increases; Y is untouched because
// every edge lies in the plane; and the whole move is bounded by |delta|. (A triangulated
// fan's diagonal edges legitimately induce some Z motion, so Z is not pinned.)
TEST(EdgeSlide, InPlaneDeltaSlidesInThatDirection) {
    HalfEdgeMesh hem = planeHem();
    const uint32_t v = interiorVertex(hem);
    ASSERT_NE(v, HalfEdgeMesh::kInvalid);
    const Vec3 before = hem.positions()[v];

    EdgeSlide::slideVertices(hem, {v}, {1.f, 0.f, 0.f});  // in-plane, +X

    const Vec3 after = hem.positions()[v];
    EXPECT_GT(after.x, before.x + 1e-4f) << "vertex should slide toward +X";
    EXPECT_NEAR(after.y, before.y, 1e-5f) << "an in-plane slide must stay in the plane";

    const float move = std::sqrt((after.x - before.x) * (after.x - before.x)
                               + (after.y - before.y) * (after.y - before.y)
                               + (after.z - before.z) * (after.z - before.z));
    EXPECT_LE(move, 1.f + 1e-5f) << "slide must be bounded by |delta|";
}

TEST(EdgeSlide, OutOfRangeIndexIsIgnored) {
    HalfEdgeMesh hem = planeHem();
    const auto before = hem.positions();
    EdgeSlide::slideVertices(hem, {999999u}, {1.f, 0.f, 0.f});
    EXPECT_EQ(hem.positions().size(), before.size());  // no crash, no change
    for (size_t i = 0; i < before.size(); ++i)
        EXPECT_FLOAT_EQ(hem.positions()[i].x, before[i].x);
}

// mergeToVertex welds source onto target and rebuilds: one vertex is removed and the result
// is a valid manifold. The old code poked edge().src through a const accessor and never
// compiled; the rebuild path collapses the two faces that shared the (0,1) edge from quads
// to triangles and drops vertex 1.
TEST(MeshVertexMerge, MergeWeldsAndRemovesOneVertex) {
    auto hemOpt = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(hemOpt.has_value());
    HalfEdgeMesh hem = std::move(*hemOpt);
    const uint32_t vBefore = hem.vertexCount();

    MergeReport r = MeshVertexMerge::mergeToVertex(hem, /*source=*/1, /*target=*/0);
    EXPECT_TRUE(r.success);
    EXPECT_EQ(r.verticesRemoved, 1u);
    EXPECT_EQ(hem.vertexCount(), vBefore - 1) << "the merged-away vertex must be gone";
    EXPECT_TRUE(hem.checkIntegrity().ok) << hem.checkIntegrity().reason;
}

TEST(MeshVertexMerge, RejectsDegenerateArguments) {
    auto hemOpt = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(hemOpt.has_value());
    HalfEdgeMesh hem = std::move(*hemOpt);

    EXPECT_FALSE(MeshVertexMerge::mergeToVertex(hem, 2, 2).success);          // source == target
    EXPECT_FALSE(MeshVertexMerge::mergeToVertex(hem, 999999u, 0).success);    // out of range
    EXPECT_FALSE(MeshVertexMerge::mergeToVertex(hem, 0, 999999u).success);
}

// A clean box has no coincident vertices, so distance-merge must not fuse anything —
// guarding against spurious welds.
TEST(MeshVertexMerge, DistanceMergeDoesNotFuseDistinctVertices) {
    auto hemOpt = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(hemOpt.has_value());
    HalfEdgeMesh hem = std::move(*hemOpt);

    MergeReport r = MeshVertexMerge::mergeByDistance(hem, 1e-5f);
    EXPECT_EQ(r.verticesRemoved, 0u);
    EXPECT_FALSE(r.success);
}
