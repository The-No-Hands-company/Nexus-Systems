#include <gtest/gtest.h>
#include "nexus/volume/VolumeTypes.h"
#include "nexus/volume/VolumeGrid.h"
#include "nexus/volume/VolumeSDF.h"
#include "nexus/geometry/Mesh.h"

#include <cmath>

using namespace nexus::volume;

TEST(VolumeGrid, EmptyHasBackgroundValue) {
    VolumeConfig config;
    config.resolutionX = 8;
    config.resolutionY = 8;
    config.resolutionZ = 8;

    VolumeGrid grid(config);
    EXPECT_FLOAT_EQ(grid.value(VoxelCoord{4, 4, 4}), 0.0f);
}

TEST(VolumeGrid, SetAndGetValue) {
    VolumeConfig config;
    config.resolutionX = 8;
    config.resolutionY = 8;
    config.resolutionZ = 8;

    VolumeGrid grid(config);
    grid.setValue(VoxelCoord{2, 3, 4}, 1.5f);
    EXPECT_FLOAT_EQ(grid.value(VoxelCoord{2, 3, 4}), 1.5f);
    EXPECT_EQ(grid.activeCount(), 1u);
}

TEST(VolumeGrid, FillSetsAllVoxels) {
    VolumeConfig config;
    config.resolutionX = 4;
    config.resolutionY = 4;
    config.resolutionZ = 4;
    config.backgroundValue = -1.0f;

    VolumeGrid grid(config);
    grid.fill(1.0f);
    EXPECT_EQ(grid.activeCount(), 64u);
    EXPECT_FLOAT_EQ(grid.value(VoxelCoord{0, 0, 0}), 1.0f);
}

TEST(VolumeGrid, HasVoxel) {
    VolumeConfig config;
    config.resolutionX = 8;
    config.resolutionY = 8;
    config.resolutionZ = 8;

    VolumeGrid grid(config);
    EXPECT_FALSE(grid.hasVoxel(VoxelCoord{0, 0, 0}));
    grid.setValue(VoxelCoord{0, 0, 0}, 1.0f);
    EXPECT_TRUE(grid.hasVoxel(VoxelCoord{0, 0, 0}));
}

TEST(VolumeGrid, SampleWorld) {
    VolumeConfig config;
    config.origin = Vec3{-1, -1, -1};
    config.extents = Vec3{2, 2, 2};
    config.resolutionX = 4;
    config.resolutionY = 4;
    config.resolutionZ = 4;

    VolumeGrid grid(config);
    grid.setValue(VoxelCoord{0, 0, 0}, 42.0f);
    EXPECT_FLOAT_EQ(grid.sampleWorld(Vec3{-1, -1, -1}), 42.0f);
}

TEST(VolumeGrid, StatsCorrect) {
    VolumeConfig config;
    config.resolutionX = 4;
    config.resolutionY = 4;
    config.resolutionZ = 4;

    VolumeGrid grid(config);
    grid.setValue(VoxelCoord{0, 0, 0}, 5.0f);
    grid.setValue(VoxelCoord{1, 1, 1}, -3.0f);

    auto s = grid.stats();
    EXPECT_EQ(s.activeVoxels, 2u);
    EXPECT_FLOAT_EQ(s.minValue, -3.0f);
    EXPECT_FLOAT_EQ(s.maxValue, 5.0f);
}

TEST(VolumeGrid, AddOperation) {
    VolumeConfig config;
    config.resolutionX = 4;
    config.resolutionY = 4;
    config.resolutionZ = 4;

    VolumeGrid a(config);
    a.setValue(VoxelCoord{0, 0, 0}, 3.0f);

    VolumeGrid b(config);
    b.setValue(VoxelCoord{0, 0, 0}, 2.0f);

    a.add(b);
    EXPECT_FLOAT_EQ(a.value(VoxelCoord{0, 0, 0}), 5.0f);
}

TEST(VolumeGrid, MultiplyOperation) {
    VolumeConfig config;
    config.resolutionX = 4;
    config.resolutionY = 4;
    config.resolutionZ = 4;

    VolumeGrid a(config);
    a.setValue(VoxelCoord{0, 0, 0}, 3.0f);

    VolumeGrid b(config);
    b.setValue(VoxelCoord{0, 0, 0}, 4.0f);

    a.multiply(b);
    EXPECT_FLOAT_EQ(a.value(VoxelCoord{0, 0, 0}), 12.0f);
}

TEST(VolumeGrid, SmoothBlursValues) {
    VolumeConfig config;
    config.resolutionX = 8;
    config.resolutionY = 8;
    config.resolutionZ = 8;

    VolumeGrid grid(config);
    grid.setValue(VoxelCoord{4, 4, 4}, 1.0f);
    grid.smooth(2);
    EXPECT_GE(grid.activeCount(), 1u);
}

TEST(VolumeGrid, DilateExpands) {
    VolumeConfig config;
    config.resolutionX = 8;
    config.resolutionY = 8;
    config.resolutionZ = 8;

    VolumeGrid grid(config);
    grid.setValue(VoxelCoord{4, 4, 4}, 1.0f);
    grid.dilate(2);
    EXPECT_GT(grid.activeCount(), 1u);
}

TEST(VolumeSDF, SphereGeneratesSignedDistance) {
    VolumeConfig config;
    config.resolutionX = 16;
    config.resolutionY = 16;
    config.resolutionZ = 16;
    config.origin = Vec3{-1, -1, -1};
    config.extents = Vec3{2, 2, 2};

    auto grid = VolumeSDF::sphere(config, Vec3{0, 0, 0}, 0.5f);
    EXPECT_GT(grid.activeCount(), 0u);

    float centerVal = grid.sampleWorld(Vec3{0, 0, 0});
    EXPECT_LT(centerVal, 0.0f);

    float outsideVal = grid.sampleWorld(Vec3{0.9f, 0, 0});
    EXPECT_GT(outsideVal, 0.0f);
}

TEST(VolumeSDF, BoxGeneratesSignedDistance) {
    VolumeConfig config;
    config.resolutionX = 16;
    config.resolutionY = 16;
    config.resolutionZ = 16;
    config.origin = Vec3{-1, -1, -1};
    config.extents = Vec3{2, 2, 2};

    auto grid = VolumeSDF::box(config, Vec3{-0.5, -0.5, -0.5}, Vec3{0.5, 0.5, 0.5});
    EXPECT_GT(grid.activeCount(), 0u);
    EXPECT_LT(grid.sampleWorld(Vec3{0, 0, 0}), 0.0f);
}

TEST(VolumeSDF, UnionCombinesGrids) {
    VolumeConfig config;
    config.resolutionX = 16;
    config.resolutionY = 16;
    config.resolutionZ = 16;
    config.origin = Vec3{-2, -2, -2};
    config.extents = Vec3{4, 4, 4};

    auto s1 = VolumeSDF::sphere(config, Vec3{-1, 0, 0}, 1.0f);
    auto s2 = VolumeSDF::sphere(config, Vec3{1, 0, 0}, 1.0f);
    auto result = VolumeSDF::unionOp(s1, s2);

    EXPECT_LT(result.sampleWorld(Vec3{-1, 0, 0}), 0.0f);
    EXPECT_LT(result.sampleWorld(Vec3{1, 0, 0}), 0.0f);
}

TEST(VolumeSDF, Intersection) {
    VolumeConfig config;
    config.resolutionX = 16;
    config.resolutionY = 16;
    config.resolutionZ = 16;
    config.origin = Vec3{-2, -2, -2};
    config.extents = Vec3{4, 4, 4};

    auto s1 = VolumeSDF::sphere(config, Vec3{0, 0, 0}, 1.0f);
    auto s2 = VolumeSDF::sphere(config, Vec3{0, 0, 0}, 1.0f);
    auto result = VolumeSDF::intersectionOp(s1, s2);

    EXPECT_LT(result.sampleWorld(Vec3{0, 0, 0}), 0.0f);
}

TEST(VolumeSDF, Offset ) {
    VolumeConfig config;
    config.resolutionX = 8;
    config.resolutionY = 8;
    config.resolutionZ = 8;
    config.origin = Vec3{-1, -1, -1};
    config.extents = Vec3{2, 2, 2};

    auto sphere = VolumeSDF::sphere(config, Vec3{0, 0, 0}, 0.5f);
    float before = sphere.sampleWorld(Vec3{0, 0, 0});
    auto expanded = VolumeSDF::offset(sphere, 0.2f);
    float after = expanded.sampleWorld(Vec3{0, 0, 0});
    EXPECT_LT(after, before);
}

TEST(VolumeSDF, ToMeshProducesGeometry) {
    VolumeConfig config;
    config.resolutionX = 16;
    config.resolutionY = 16;
    config.resolutionZ = 16;
    config.origin = Vec3{-1, -1, -1};
    config.extents = Vec3{2, 2, 2};
    config.voxelSize = 0.125f;

    auto sphere = VolumeSDF::sphere(config, Vec3{0, 0, 0}, 0.5f);
    auto mesh = VolumeSDF::toMesh(sphere);
    EXPECT_GT(mesh.topology().faceCount(), 0u);
}

TEST(VolumeSDF, CylinderSDF) {
    VolumeConfig config;
    config.resolutionX = 16;
    config.resolutionY = 16;
    config.resolutionZ = 16;
    config.origin = Vec3{-2, -2, -2};
    config.extents = Vec3{4, 4, 4};

    auto cyl = VolumeSDF::cylinder(config, Vec3{0, -1, 0}, Vec3{0, 1, 0}, 0.5f);
    EXPECT_LT(cyl.sampleWorld(Vec3{0, 0, 0}), 0.0f);
    EXPECT_GT(cyl.sampleWorld(Vec3{1, 0, 0}), 0.0f);
}

TEST(VolumeGrid, MaxWithOperation) {
    VolumeConfig config;
    config.resolutionX = 4;
    config.resolutionY = 4;
    config.resolutionZ = 4;

    VolumeGrid a(config);
    a.setValue(VoxelCoord{0, 0, 0}, 3.0f);
    VolumeGrid b(config);
    b.setValue(VoxelCoord{0, 0, 0}, 7.0f);

    a.maxWith(b);
    EXPECT_FLOAT_EQ(a.value(VoxelCoord{0, 0, 0}), 7.0f);
}

TEST(VolumeGrid, ClearEmptiesData) {
    VolumeConfig config;
    config.resolutionX = 4;
    config.resolutionY = 4;
    config.resolutionZ = 4;

    VolumeGrid grid(config);
    grid.setValue(VoxelCoord{0, 0, 0}, 1.0f);
    EXPECT_EQ(grid.activeCount(), 1u);
    grid.clear();
    EXPECT_EQ(grid.activeCount(), 0u);
}

TEST(VolumeGrid, ResampleChangesResolution) {
    VolumeConfig srcConfig;
    srcConfig.resolutionX = 8;
    srcConfig.resolutionY = 8;
    srcConfig.resolutionZ = 8;
    srcConfig.origin = Vec3{0, 0, 0};
    srcConfig.extents = Vec3{1, 1, 1};

    VolumeGrid src(srcConfig);
    src.setValue(VoxelCoord{4, 4, 4}, 1.0f);

    VolumeConfig dstConfig;
    dstConfig.resolutionX = 4;
    dstConfig.resolutionY = 4;
    dstConfig.resolutionZ = 4;
    dstConfig.origin = Vec3{0, 0, 0};
    dstConfig.extents = Vec3{1, 1, 1};

    auto resampled = src.resample(dstConfig);
    EXPECT_LE(resampled.activeCount(), 8u * 8u * 8u);
}
