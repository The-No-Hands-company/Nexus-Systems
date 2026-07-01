#include <gtest/gtest.h>

#include <nexus/geometry/EdgeBridge.h>
#include <nexus/geometry/Mesh.h>

using namespace nexus::geometry;
using Vec3 = nexus::render::Vec3;

TEST(EdgeBridge, BridgeTwoEqualLoopsCreatesQuads) {
    std::vector<uint32_t> loopA = {0, 1, 2, 3};
    std::vector<uint32_t> loopB = {4, 5, 6, 7};

    std::vector<Vec3> positions = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
        {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1},
    };

    Mesh output;
    BridgeOptions opts;
    opts.segments = 1;
    opts.closed = false;

    auto report = EdgeBridge::bridge(loopA, loopB, positions, output, opts);
    EXPECT_TRUE(report.success);
    EXPECT_GT(report.facesCreated, 0u);
    EXPECT_GT(output.topology().faceCount(), 0u);
}

TEST(EdgeBridge, MismatchedLoopsInterpolates) {
    std::vector<uint32_t> loopA = {0, 1, 2, 3}; // 4 vertices
    std::vector<uint32_t> loopB = {4, 5, 6};    // 3 vertices

    std::vector<Vec3> positions = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
        {0, 1, 0}, {1, 1, 0}, {1, 1, 1},
    };

    Mesh output;
    BridgeOptions opts;
    auto report = EdgeBridge::bridge(loopA, loopB, positions, output, opts);
    EXPECT_TRUE(report.success);
    EXPECT_GT(output.topology().faceCount(), 0u);
}

TEST(EdgeBridge, ClosedOptionIsAccepted) {
    std::vector<uint32_t> loopA = {0, 1, 2, 3};
    std::vector<uint32_t> loopB = {4, 5, 6, 7};

    std::vector<Vec3> positions = {
        {0, 0, 0}, {1, 0, 0}, {1, 0, 1}, {0, 0, 1},
        {0, 1, 0}, {1, 1, 0}, {1, 1, 1}, {0, 1, 1},
    };

    Mesh output;
    BridgeOptions opts;
    opts.closed = true;
    auto report = EdgeBridge::bridge(loopA, loopB, positions, output, opts);
    EXPECT_TRUE(report.success);
}

TEST(EdgeBridge, EmptyLoopsReturnFailure) {
    std::vector<uint32_t> emptyLoop;
    std::vector<uint32_t> loopB = {0, 1, 2};
    std::vector<Vec3> positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}};

    Mesh output;
    auto report = EdgeBridge::bridge(emptyLoop, loopB, positions, output);
    EXPECT_FALSE(report.success);
}
