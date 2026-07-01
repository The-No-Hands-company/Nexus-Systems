#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshTopologyValidation.h>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshTopologyValidation, EulerCharacteristicOfCubeIsTwo) {
    EXPECT_EQ(MeshTopologyValidation::computeEulerCharacteristic(8, 12, 6), 2);
}

TEST(MeshTopologyValidation, EulerCharacteristicOfTetrahedron) {
    EXPECT_EQ(MeshTopologyValidation::computeEulerCharacteristic(4, 6, 4), 2);
}

TEST(MeshTopologyValidation, GenusOfSphereIsZero) {
    EXPECT_EQ(MeshTopologyValidation::computeGenus(2, 0), 0);
}

TEST(MeshTopologyValidation, GenusOfTorusIsOne) {
    EXPECT_EQ(MeshTopologyValidation::computeGenus(0, 0), 1);
}

TEST(MeshTopologyValidation, CubeValidationPasses) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto result = MeshTopologyValidation::validate(cube);
    EXPECT_TRUE(result.valid);
}

TEST(MeshTopologyValidation, EmptyMeshFailsValidation) {
    Mesh empty;
    auto result = MeshTopologyValidation::validate(empty);
    EXPECT_FALSE(result.valid);
}

TEST(MeshTopologyValidation, EulerOnCubeIsTwo) {
    Mesh cube = makeBox(2.0f, 2.0f, 2.0f);
    auto result = MeshTopologyValidation::validate(cube);
    EXPECT_EQ(result.euler, 2);
}
