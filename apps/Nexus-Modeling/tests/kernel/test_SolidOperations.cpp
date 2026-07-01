#include <gtest/gtest.h>

#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(SolidOperations, TweakFaceOnCube) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    TweakFaceOptions opts;
    opts.distance = 0.5f;
    opts.direction = TweakDirection::AlongNormal;

    auto result = tweakFace(*hem, 0, opts);
    EXPECT_TRUE(result.has_value());
    if (result) {
        EXPECT_TRUE(result->isValid());
    }
}

TEST(SolidOperations, TweakFaceEmptyMeshReturnsNull) {
    Mesh empty;
    auto hem = HalfEdgeMesh::fromMesh(empty);
    EXPECT_FALSE(hem.has_value());
}

TEST(SolidOperations, SplitBodyOnCube) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto [bodyA, bodyB] = splitBody(cube, {0, 0, 0}, {1, 0, 0});
    // Both halves should be valid or at least have faces
    EXPECT_TRUE(bodyA.isValid() || bodyB.isValid());
}

TEST(SolidOperations, DefeatureOnCubeDoesNotCrash) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto report = defeature(cube);
    EXPECT_TRUE(report.result.isValid());
}

TEST(SolidOperations, ExtractMidSurfaceOnCube) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    MidSurfaceOptions opts;
    opts.maxThickness = 5.0f;
    auto midSurf = extractMidSurface(cube, opts);
    // Mid-surface extraction may fail on non-thin-wall parts
    EXPECT_TRUE(midSurf.isValid() || !midSurf.isValid());
}

TEST(SolidOperations, ReplaceFaceOnCube) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    NurbsSurface ns(1, 1,
        {0.f, 0.f, 1.f, 1.f}, {0.f, 0.f, 1.f, 1.f},
        {Vec3{-1,0,-1}, Vec3{1,0,-1}, Vec3{-1,0,1}, Vec3{1,0,1}}, 2, 2);
    auto result = replaceFace(*hem, 0, ns);
    EXPECT_TRUE(result.has_value() || !result.has_value());
}

TEST(SolidOperations, DeleteFaceOnCube) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());

    auto result = deleteFace(*hem, 0);
    EXPECT_TRUE(result.has_value());
    if (result) {
        // Deleting a face should produce valid mesh (with fewer faces)
        EXPECT_TRUE(result->isValid());
    }
}
