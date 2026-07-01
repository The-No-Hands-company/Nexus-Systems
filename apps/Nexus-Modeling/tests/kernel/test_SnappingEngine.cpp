#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/SnappingEngine.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(SnappingEngine, BuildFromCubeDoesNotCrash) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    SnappingEngine engine;
    engine.build(cube);
    EXPECT_TRUE(engine.isBuilt());
}

TEST(SnappingEngine, SnapToGridRoundsPosition) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    SnappingEngine engine;
    engine.build(cube);

    SnappingConfig config;
    config.enableGrid = true;
    config.gridSpacing = 1.0f;
    engine.setConfig(config);

    Vec3 result = engine.snapToGrid({0.6f, -0.4f, 2.1f});
    EXPECT_NEAR(result.x, 1.0f, 0.01f);
    EXPECT_NEAR(result.y, 0.0f, 0.01f);
    EXPECT_NEAR(result.z, 2.0f, 0.01f);
}

TEST(SnappingEngine, SnapWithoutBuildReturnsNone) {
    SnappingEngine engine;
    auto result = engine.snap({1, 2, 3});
    EXPECT_EQ(result.target, SnapTarget::None);
}

TEST(SnappingEngine, RebuildUpdatesBVH) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    SnappingEngine engine;
    engine.build(cube);
    EXPECT_TRUE(engine.isBuilt());
    engine.rebuild(cube);
    EXPECT_TRUE(engine.isBuilt());
}
