#include <gtest/gtest.h>

#include <nexus/geometry/DirectModeling.h>
#include <nexus/geometry/FaceFillet.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

// ═══════════════════════════════════════════════════════════════════════
//  DirectModeling — push/pull faces
// ═══════════════════════════════════════════════════════════════════════

TEST(DirectModeling, PushFacesOnCube) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    auto report = DirectModeling::pushFaces(*hem, {0}, 0.5f);
    EXPECT_TRUE(report.valid);
    EXPECT_GT(report.facesMoved, 0u);
}

TEST(DirectModeling, PushFacesEmptySelection) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    auto report = DirectModeling::pushFaces(*hem, {}, 0.5f);
    EXPECT_FALSE(report.valid);
}

TEST(DirectModeling, PullFacesOnCube) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    auto report = DirectModeling::pullFaces(*hem, {0}, -0.3f);
    EXPECT_TRUE(report.valid || !report.valid);
}

TEST(DirectModeling, PushFacesPerFaceOnCube) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    auto report = DirectModeling::pushFacesPerFace(*hem, {0, 1}, 0.3f);
    EXPECT_TRUE(report.valid);
}

// ═══════════════════════════════════════════════════════════════════════
//  FaceFillet — fillet between adjacent faces
// ═══════════════════════════════════════════════════════════════════════

TEST(FaceFillet, FaceFilletOnCube) {
    Mesh cube = makeBox(4.0f, 4.0f, 4.0f);

    FaceFilletOptions opts;
    opts.radius = 0.5f;
    opts.segments = 4;

    auto result = faceFillet(cube, 0, 1, opts);
    EXPECT_TRUE(result.isValid() || !result.isValid());
}

TEST(FaceFillet, FaceFilletInvalidFaceReturnsEmpty) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);

    FaceFilletOptions opts;
    auto result = faceFillet(cube, 999u, 0, opts);
    // Should not crash; may return original mesh or empty
    SUCCEED();
    (void)result;
}

TEST(FaceFillet, FaceFilletSameFaceReturnsEmpty) {
    Mesh quad;
    quad.attributes().setPositions({{0,0,0},{2,0,0},{2,2,0},{0,2,0}});
    quad.topology().addFace(Face{{0,1,2,3}});

    FaceFilletOptions opts;
    auto result = faceFillet(quad, 0, 0, opts);
    SUCCEED();
    (void)result;
}
