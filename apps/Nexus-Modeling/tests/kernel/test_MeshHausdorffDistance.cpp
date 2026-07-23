#include <gtest/gtest.h>

#include <nexus/geometry/MeshHausdorffDistance.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;
using nexus::render::Mat4;

TEST(MeshHausdorffDistance, IdenticalMeshesNearZero) {
    Mesh a = makeBox(1.f, 1.f, 1.f);
    Mesh b = makeBox(1.f, 1.f, 1.f);
    HausdorffOptions opts;
    opts.seed = 42;
    HausdorffResult r = MeshHausdorffDistance::compute(a, b, opts);
    EXPECT_NEAR(r.forwardMax, 0.f, 1e-4f);
    EXPECT_NEAR(r.backwardMax, 0.f, 1e-4f);
    EXPECT_NEAR(r.symmetricMax, 0.f, 1e-4f);
}

TEST(MeshHausdorffDistance, ParallelOffsetEqualsGap) {
    Mesh a = makeBox(1.f, 1.f, 1.f);
    Mesh b = a;
    Mat4 t = Mat4::identity();
    t.m[0][3] = 3.f;
    (void)b.applyTransform(t);
    HausdorffOptions opts;
    opts.seed = 12345;
    HausdorffResult r = MeshHausdorffDistance::compute(a, b, opts);
    // A 1x1x1 box spans -0.5..0.5, so translating it by +3 gives 2.5..3.5. The
    // configuration is SYMMETRIC: A's far face (x=-0.5) is 3.0 from B's nearest (x=2.5),
    // and B's far face (x=3.5) is 3.0 from A's nearest (x=0.5). Both directions are 3.
    //
    // This previously expected 2.0 backward, with a comment about a "nearest-face gap".
    // That was never geometrically true — it passed because SplitMix64::uniform01 returned
    // values in [0, 0.000488] instead of [0, 1), so sampling never reached the far face.
    // With the generator fixed, the measurement matches the geometry.
    EXPECT_NEAR(r.forwardMax, 3.f, 0.15f);
    EXPECT_NEAR(r.backwardMax, 3.f, 0.15f);
}

TEST(MeshHausdorffDistance, EmptyMeshReturnsZero) {
    Mesh a = makeBox(1.f, 1.f, 1.f);
    Mesh b;
    HausdorffOptions opts;
    HausdorffResult r = MeshHausdorffDistance::compute(a, b, opts);
    EXPECT_FLOAT_EQ(r.forwardMax, 0.f);
    EXPECT_FLOAT_EQ(r.backwardMax, 0.f);
    EXPECT_FLOAT_EQ(r.symmetricMax, 0.f);
}

TEST(MeshHausdorffDistance, SymmetricMaxIsMaxOfDirected) {
    Mesh a = makeSphere(1.f, 8, 8);
    Mesh b = makeBox(1.5f, 1.5f, 1.5f);
    HausdorffOptions opts;
    opts.seed = 777;
    HausdorffResult r = MeshHausdorffDistance::compute(a, b, opts);
    float maxDir = std::max(r.forwardMax, r.backwardMax);
    EXPECT_NEAR(r.symmetricMax, maxDir, 1e-5f);
}

TEST(MeshHausdorffDistance, DistancesNonNegative) {
    Mesh a = makeBox(2.f, 2.f, 2.f);
    Mesh b = makeSphere(1.f, 8, 8);
    HausdorffOptions opts;
    opts.seed = 11;
    HausdorffResult r = MeshHausdorffDistance::compute(a, b, opts);
    EXPECT_GE(r.forwardMax, 0.f);
    EXPECT_GE(r.backwardMax, 0.f);
    EXPECT_GE(r.symmetricMax, 0.f);
    EXPECT_GE(r.forwardMean, 0.f);
    EXPECT_GE(r.backwardMean, 0.f);
    EXPECT_GE(r.symmetricMean, 0.f);
}
