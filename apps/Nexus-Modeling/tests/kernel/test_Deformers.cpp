#include <gtest/gtest.h>
#include "nexus/deform/DeformTypes.h"
#include "nexus/deform/LatticeDeformer.h"
#include "nexus/deform/ClusterDeformer.h"
#include "nexus/deform/WireDeformer.h"
#include "nexus/deform/DeformerStack.h"
#include "nexus/geometry/Mesh.h"

#include <cmath>

using namespace nexus::deform;

TEST(LatticeDeformer, DefaultCreatesControlPoints) {
    LatticeConfig config;
    config.divisionsX = 2;
    config.divisionsY = 2;
    config.divisionsZ = 2;
    config.extents = Vec3{2, 2, 2};

    LatticeDeformer lattice(config);
    auto cp = lattice.controlPoint(1, 1, 1);
    EXPECT_GT(cp.x, 0.0f);
    EXPECT_GT(cp.y, 0.0f);
    EXPECT_GT(cp.z, 0.0f);
}

TEST(LatticeDeformer, SetControlPointChangesValue) {
    LatticeConfig config;
    config.divisionsX = 1;
    config.divisionsY = 1;
    config.divisionsZ = 1;

    LatticeDeformer lattice(config);
    lattice.setControlPoint(1, 1, 1, Vec3{5, 5, 5});
    auto cp = lattice.controlPoint(1, 1, 1);
    EXPECT_FLOAT_EQ(cp.x, 5.0f);
    EXPECT_FLOAT_EQ(cp.y, 5.0f);
    EXPECT_FLOAT_EQ(cp.z, 5.0f);
}

TEST(LatticeDeformer, DeformPointsInterpolates) {
    LatticeConfig config;
    config.divisionsX = 1;
    config.divisionsY = 1;
    config.divisionsZ = 1;
    config.extents = Vec3{2, 2, 2};
    config.origin = Vec3{-1, -1, -1};

    LatticeDeformer lattice(config);

    std::vector<Vec3> points = {Vec3{0, 0, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    weights.envelope = 1.0f;

    auto deformed = lattice.deformPoints(points, weights);
    EXPECT_NEAR(deformed[0].x, 0.0f, 0.1f);
    EXPECT_NEAR(deformed[0].y, 0.0f, 0.1f);
}

TEST(LatticeDeformer, ControlPointMovesVertex) {
    LatticeConfig config;
    config.divisionsX = 2;
    config.divisionsY = 2;
    config.divisionsZ = 2;
    config.extents = Vec3{4, 4, 4};
    config.origin = Vec3{-2, -2, -2};

    LatticeDeformer lattice(config);
    auto orig = lattice.controlPoint(2, 2, 2);
    lattice.setControlPoint(2, 2, 2, Vec3{orig.x + 5, orig.y, orig.z});

    std::vector<Vec3> points = {Vec3{2, 2, 2}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    auto deformed = lattice.deformPoints(points, weights);
    EXPECT_GT(deformed[0].x, points[0].x);
}

TEST(ClusterDeformer, PointsNearClusterAreAffected) {
    ClusterConfig config;
    config.position = Vec3{0, 0, 0};
    config.radius = 5.0f;
    config.strength = 1.0f;

    ClusterDeformer cluster(config);

    std::vector<Vec3> points = {Vec3{1, 0, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    weights.envelope = 1.0f;

    auto deformed = cluster.deformPoints(points, weights);
    EXPECT_LT(deformed[0].x, points[0].x);
}

TEST(ClusterDeformer, PointsOutsideRadiusUnaffected) {
    ClusterConfig config;
    config.position = Vec3{0, 0, 0};
    config.radius = 1.0f;
    config.strength = 1.0f;

    ClusterDeformer cluster(config);

    std::vector<Vec3> points = {Vec3{10, 0, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);

    auto deformed = cluster.deformPoints(points, weights);
    EXPECT_FLOAT_EQ(deformed[0].x, points[0].x);
    EXPECT_FLOAT_EQ(deformed[0].y, points[0].y);
}

TEST(WireDeformer, PointsNearCurveAffected) {
    WireConfig config;
    config.curvePoints = {
        Vec3{0, 0, 0},
        Vec3{5, 0, 0},
        Vec3{10, 0, 0},
    };
    config.radius = 2.0f;
    config.strength = 1.0f;
    config.dropoff = 2.0f;

    WireDeformer wire(config);

    std::vector<Vec3> points = {Vec3{5, 1, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    weights.envelope = 1.0f;

    auto deformed = wire.deformPoints(points, weights);
    EXPECT_LT(deformed[0].y, points[0].y);
}

TEST(WireDeformer, DistantPointsUnaffected) {
    WireConfig config;
    config.curvePoints = {
        Vec3{0, 0, 0},
        Vec3{1, 0, 0},
    };
    config.radius = 1.0f;
    config.strength = 1.0f;

    WireDeformer wire(config);

    std::vector<Vec3> points = {Vec3{100, 100, 100}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);

    auto deformed = wire.deformPoints(points, weights);
    EXPECT_FLOAT_EQ(deformed[0].x, points[0].x);
}

TEST(DeformerStack, AddDeformerIncreasesCount) {
    DeformerStack stack;
    EXPECT_EQ(stack.count(), 0u);

    stack.addBend(0.5f, Vec3{1, 0, 0});
    EXPECT_EQ(stack.count(), 1u);

    stack.addTwist(0.3f, Vec3{0, 1, 0});
    EXPECT_EQ(stack.count(), 2u);
}

TEST(DeformerStack, RemoveDeformer) {
    DeformerStack stack;
    stack.addBend(0.5f, Vec3{1, 0, 0});
    stack.addTwist(0.3f, Vec3{0, 1, 0});
    EXPECT_EQ(stack.count(), 2u);

    stack.removeDeformer(0);
    EXPECT_EQ(stack.count(), 1u);
}

TEST(DeformerStack, DisabledDeformerSkipped) {
    DeformerStack stack;
    stack.addTwist(1.0f, Vec3{0, 1, 0});
    stack.setEnabled(0, false);

    std::vector<Vec3> points = {Vec3{1, 1, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);

    auto deformed = stack.deformPoints(points, weights);
    EXPECT_FLOAT_EQ(deformed[0].x, points[0].x);
}

TEST(DeformerStack, TwistDeformsCorrectly) {
    DeformerStack stack;
    stack.addTwist(1.57f, Vec3{0, 1, 0});

    std::vector<Vec3> points = {Vec3{1, 1, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    weights.envelope = 1.0f;

    auto deformed = stack.deformPoints(points, weights);
    EXPECT_NEAR(deformed[0].z, points[0].x, 0.1f);
}

TEST(DeformerStack, TaperDeformsCorrectly) {
    DeformerStack stack;
    stack.addTaper(0.5f, Vec3{0, 1, 0});

    std::vector<Vec3> points = {Vec3{2, 2, 2}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    weights.envelope = 1.0f;

    auto deformed = stack.deformPoints(points, weights);
    EXPECT_GT(deformed[0].x, points[0].x);
}

TEST(DeformerStack, BendDeformsMesh) {
    DeformerStack stack;
    stack.addBend(0.8f, Vec3{1, 0, 0});

    std::vector<Vec3> points = {Vec3{0, 1, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    weights.envelope = 1.0f;

    auto deformed = stack.deformPoints(points, weights);
    EXPECT_GT(deformed[0].x, points[0].x);
}

TEST(DeformerStack, SineDeformerAppliesWave) {
    DeformerStack stack;
    stack.addSine(2.0f, 1.0f, Vec3{0, 1, 0});

    std::vector<Vec3> points = {Vec3{1, 0, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    weights.envelope = 1.0f;

    auto deformed = stack.deformPoints(points, weights);
    EXPECT_NE(deformed[0].y, points[0].y);
}

TEST(DeformerWeights, UniformWeights) {
    DeformerWeights weights;
    weights.setUniform(100, 0.5f);
    EXPECT_FLOAT_EQ(weights.get(0), 0.5f);
    EXPECT_FLOAT_EQ(weights.get(50), 0.5f);
    EXPECT_FLOAT_EQ(weights.get(99), 0.5f);
    EXPECT_FLOAT_EQ(weights.get(100), 1.0f);
}

TEST(DeformerStack, AddLatticeToStack) {
    DeformerStack stack;

    LatticeConfig config;
    config.divisionsX = 2;
    config.divisionsY = 2;
    config.divisionsZ = 2;
    config.extents = Vec3{2, 2, 2};
    stack.addLattice(config, "Test");

    EXPECT_EQ(stack.count(), 1u);
    auto* entry = stack.entry(0);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->method, DeformMethod::Lattice);
}

TEST(DeformerStack, ClearRemovesAll) {
    DeformerStack stack;
    stack.addBend(0.5f, Vec3{1, 0, 0});
    stack.addTwist(0.3f, Vec3{0, 1, 0});
    stack.addTaper(0.5f, Vec3{0, 1, 0});

    EXPECT_EQ(stack.count(), 3u);
    stack.clear();
    EXPECT_EQ(stack.count(), 0u);
}

TEST(DeformerStack, SetEnvelope) {
    DeformerStack stack;
    stack.addBend(1.0f, Vec3{1, 0, 0});
    stack.setEnvelope(0, 0.0f);

    std::vector<Vec3> points = {Vec3{0, 1, 0}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);

    auto deformed = stack.deformPoints(points, weights);
    EXPECT_FLOAT_EQ(deformed[0].x, points[0].x);
}

TEST(DeformerStack, ChainedDeformersCompose) {
    DeformerStack stack;
    stack.addTaper(0.1f, Vec3{0, 1, 0});
    stack.addSine(1.0f, 0.5f, Vec3{0, 1, 0});

    std::vector<Vec3> points = {Vec3{1, 1, 1}};
    DeformerWeights weights;
    weights.setUniform(1, 1.0f);
    weights.envelope = 1.0f;

    auto deformed = stack.deformPoints(points, weights);
    EXPECT_NE(deformed[0].y, points[0].y);
}
