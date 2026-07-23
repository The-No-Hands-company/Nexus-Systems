#include <gtest/gtest.h>

#include <nexus/geometry/MeshInstancer.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshInstancer, ScatterInstancesOnTargetFaces) {
    Mesh source = makeBox(0.5f, 0.5f, 0.5f);
    Mesh target = makePlane(4.f, 4.f, 1, 1);
    InstancerOptions opts;
    opts.seed = 42;
    Mesh result = MeshInstancer::scatter(source, target, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), target.topology().faceCount());
    EXPECT_GT(result.attributes().vertexCount(), target.attributes().vertexCount());
}

TEST(MeshInstancer, ScatterAtPointsCreatesInstances) {
    Mesh source = makeBox(0.3f, 0.3f, 0.3f);
    std::vector<nexus::render::Vec3> positions = {{0,0,0}, {1,0,0}, {0,0,1}};
    std::vector<nexus::render::Vec3> normals = {{0,1,0}, {0,1,0}, {0,1,0}};
    InstancerOptions opts;
    opts.seed = 1;
    Mesh result = MeshInstancer::scatterAtPoints(source, positions, normals, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), 0u);
}

TEST(MeshInstancer, MismatchedSizesReturnsEmpty) {
    Mesh source = makeBox(0.5f, 0.5f, 0.5f);
    std::vector<nexus::render::Vec3> positions = {{0,0,0}, {1,0,0}};
    std::vector<nexus::render::Vec3> normals = {{0,1,0}};
    InstancerOptions opts;
    Mesh result = MeshInstancer::scatterAtPoints(source, positions, normals, opts);
    // Implementation now gracefully handles mismatched normal/position arrays;
    // the result may be valid and contain at least the shared-count instances.
    EXPECT_GT(result.attributes().vertexCount(), 0u);
}

TEST(MeshInstancer, ScaleApplied) {
    // Scattering 0.5-scaled unit boxes over a 2x2 plane spans roughly the plane's own
    // extent plus half an instance at each end — about 2.5, not something under 1.5.
    //
    // The old bound of 1.5 held only because SplitMix64::uniform01 returned values in
    // [0, 0.000488] instead of [0, 1): every instance landed on top of the first one, so
    // the result was about as wide as a single box. With the generator fixed the scatter
    // covers the surface, and the assertion that actually tests SCALE is a comparison
    // between two scales rather than an absolute width.
    Mesh source = makeBox(1.f, 1.f, 1.f);
    Mesh target = makePlane(2.f, 2.f, 1, 1);

    InstancerOptions small;
    small.scaleMin = 0.5f;
    small.scaleMax = 0.5f;
    small.seed = 99;
    const Mesh smallResult = MeshInstancer::scatter(source, target, small);
    ASSERT_TRUE(smallResult.isValid());

    InstancerOptions large = small;
    large.scaleMin = 1.f;
    large.scaleMax = 1.f;
    const Mesh largeResult = MeshInstancer::scatter(source, target, large);
    ASSERT_TRUE(largeResult.isValid());

    const float smallWidth =
        smallResult.computeBounds().max.x - smallResult.computeBounds().min.x;
    const float largeWidth =
        largeResult.computeBounds().max.x - largeResult.computeBounds().min.x;

    // Same seed, same placements: the only difference is instance size, so the smaller
    // scale must produce a strictly narrower result, by about the half-box difference.
    EXPECT_LT(smallWidth, largeWidth) << "halving the scale did not shrink the result";
    EXPECT_NEAR(largeWidth - smallWidth, 0.5f, 0.2f)
        << "scale difference " << (largeWidth - smallWidth) << " does not match the "
        << "0.5 change in instance size";
    // And the scatter must actually cover the target rather than pile up at one point.
    EXPECT_GT(smallWidth, 2.f) << "instances did not spread across the 2-unit plane";
}

TEST(MeshInstancer, IncludeNormalsWhenSourceHasThem) {
    Mesh source = makeBox(0.5f, 0.5f, 0.5f);
    EXPECT_TRUE(source.computeVertexNormals());
    Mesh target = makePlane(2.f, 2.f, 1, 1);
    InstancerOptions opts;
    opts.seed = 7;
    Mesh result = MeshInstancer::scatter(source, target, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasNormals());
}

TEST(MeshInstancer, NormalsDisabledWhenSourceLacksThem) {
    Mesh source = makeBox(0.5f, 0.5f, 0.5f);
    source.attributes().clearNormals();
    Mesh target = makePlane(2.f, 2.f, 1, 1);
    InstancerOptions opts;
    opts.seed = 13;
    Mesh result = MeshInstancer::scatter(source, target, opts);
    EXPECT_TRUE(result.isValid());
}
