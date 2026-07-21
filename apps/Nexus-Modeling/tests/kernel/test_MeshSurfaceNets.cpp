#include <gtest/gtest.h>

#include <nexus/geometry/MeshSurfaceNets.h>
#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

using namespace nexus::geometry;

static SurfaceNetsGrid makeSphereGrid(int32_t size)
{
    SurfaceNetsGrid grid;
    grid.nx = size;
    grid.ny = size;
    grid.nz = size;
    grid.data.resize(static_cast<size_t>(size) * size * size, 0);

    float radius = static_cast<float>(size) * 0.35f;
    float cx = static_cast<float>(size - 1) * 0.5f;
    float cy = static_cast<float>(size - 1) * 0.5f;
    float cz = static_cast<float>(size - 1) * 0.5f;

    for (int32_t z = 0; z < size; ++z) {
        for (int32_t y = 0; y < size; ++y) {
            for (int32_t x = 0; x < size; ++x) {
                float dx = static_cast<float>(x) - cx;
                float dy = static_cast<float>(y) - cy;
                float dz = static_cast<float>(z) - cz;
                if (dx * dx + dy * dy + dz * dz <= radius * radius) {
                    size_t idx = static_cast<size_t>(x + size * (y + size * z));
                    grid.data[idx] = 1;
                }
            }
        }
    }
    return grid;
}

TEST(MeshSurfaceNets, ProducesMeshFromSphereGrid)
{
    auto grid = makeSphereGrid(16);
    ASSERT_GT(grid.occupiedCount(), 0u);

    SurfaceNetsOptions opts;
    opts.computeNormals = true;

    Mesh result = MeshSurfaceNets::extract(grid, opts);
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.attributes().vertexCount(), 0u);
    EXPECT_GT(result.topology().faceCount(), 0u);
    EXPECT_TRUE(result.attributes().hasNormals());
}

TEST(MeshSurfaceNets, AllInsideGridNoSurface)
{
    SurfaceNetsGrid grid;
    grid.nx = 8;
    grid.ny = 8;
    grid.nz = 8;
    grid.data.resize(8 * 8 * 8, 1);

    Mesh result = MeshSurfaceNets::extract(grid);
    EXPECT_EQ(result.topology().faceCount(), 0u);
}

TEST(MeshSurfaceNets, AllOutsideGridNoSurface)
{
    SurfaceNetsGrid grid;
    grid.nx = 8;
    grid.ny = 8;
    grid.nz = 8;
    grid.data.resize(8 * 8 * 8, 0);

    Mesh result = MeshSurfaceNets::extract(grid);
    EXPECT_EQ(result.topology().faceCount(), 0u);
}

TEST(MeshSurfaceNets, TooSmallGridReturnsEmpty)
{
    SurfaceNetsGrid grid;
    grid.nx = 1;
    grid.ny = 1;
    grid.nz = 1;
    grid.data = {1};

    Mesh result = MeshSurfaceNets::extract(grid);
    EXPECT_EQ(result.topology().faceCount(), 0u);
}

TEST(MeshSurfaceNets, OccupiedCountReturnsCorrectCount)
{
    SurfaceNetsGrid grid;
    grid.nx = 4;
    grid.ny = 4;
    grid.nz = 4;
    grid.data.resize(64, 0);

    for (size_t i = 0; i < 20; ++i) {
        grid.data[i] = 1;
    }

    EXPECT_EQ(grid.occupiedCount(), 20u);
}

} // namespace
