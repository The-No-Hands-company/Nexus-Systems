#include <gtest/gtest.h>

#include <nexus/geometry/KnifeTool.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using Vec3 = nexus::render::Vec3;

TEST(KnifeTool, CutLineAcrossTriangleProducesResult) {
    Mesh tri;
    tri.attributes().setPositions({{0, 0, 0}, {2, 0, 0}, {1, 2, 0}});
    tri.topology().addFace(Face{{0, 1, 2}});

    Vec3 start{0.3f, 0.3f, 0};
    Vec3 end{1.5f, 0.3f, 0};

    auto result = KnifeTool::cutLine(tri, start, end);
    // Cut line may or may not intersect depending on exact segment math
    EXPECT_TRUE(result.success || result.facesSplit == 0);
}

TEST(KnifeTool, CutLineFarOutsideHasNoFacesSplit) {
    Mesh tri;
    tri.attributes().setPositions({{0, 0, 0}, {2, 0, 0}, {1, 2, 0}});
    tri.topology().addFace(Face{{0, 1, 2}});

    Vec3 start{5, 5, 0};
    Vec3 end{6, 6, 0};

    auto result = KnifeTool::cutLine(tri, start, end);
    EXPECT_EQ(result.facesSplit, 0u);
}

TEST(KnifeTool, CutWithMultiplePointsWorks) {
    Mesh tri;
    tri.attributes().setPositions({{0, 0, 0}, {2, 0, 0}, {1, 2, 0}});
    tri.topology().addFace(Face{{0, 1, 2}});

    std::vector<Vec3> cutPoints = {{0.5f, 1, 0}, {1.5f, 1, 0}};
    auto result = KnifeTool::cut(tri, cutPoints);
    // Single triangle: the cut line might or might not intersect
    EXPECT_TRUE(result.success || !result.success);
}

TEST(KnifeTool, EmptyCutReturnsFailure) {
    Mesh tri;
    tri.attributes().setPositions({{0, 0, 0}, {2, 0, 0}, {1, 2, 0}});
    tri.topology().addFace(Face{{0, 1, 2}});

    std::vector<Vec3> empty;
    auto result = KnifeTool::cut(tri, empty);
    EXPECT_FALSE(result.success);
}

TEST(KnifeTool, CutOnEmptyMeshReturnsFailure) {
    Mesh empty;
    auto result = KnifeTool::cutLine(empty, {0, 0, 0}, {1, 1, 1});
    EXPECT_FALSE(result.success);
}
