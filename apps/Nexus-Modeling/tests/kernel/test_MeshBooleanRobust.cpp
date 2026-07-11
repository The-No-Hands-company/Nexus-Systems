#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBooleanRobust.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/MeshTopologyValidation.h>

#include <gtest/gtest.h>

#include <cmath>
#include <utility>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

namespace {
// Two axis-non-coplanar overlapping unit-ish boxes: A=[-1,1]^3, B=[0,2]^3.
// Overlap = [0,1]^3 (volume 1). volA = volB = 8.
Mesh boxA() { return makeBox(2.f, 2.f, 2.f); }
Mesh boxB()
{
    Mesh b = makeBox(2.f, 2.f, 2.f);
    auto p = b.attributes().positions();
    for (auto& v : p) { v.x += 1.f; v.y += 1.f; v.z += 1.f; }
    b.attributes().setPositions(std::move(p));
    return b;
}
float vol(const Mesh& m) { return std::abs(MeshMassProperties::compute(m).volume); }
}  // namespace

// The whole point: a real boolean is watertight + 2-manifold on COARSE meshes.
// A closed genus-0 surface has Euler characteristic χ = V - E + F = 2.
TEST(MeshBooleanRobust, ResultsAreWatertightManifoldOnCoarseBoxes)
{
    for (auto op : {BooleanOperationType::Union, BooleanOperationType::Intersection,
                    BooleanOperationType::Difference}) {
        const Mesh m = robustMeshBoolean(boxA(), boxB(), op);
        const auto v = MeshTopologyValidation::validate(m);
        EXPECT_EQ(v.euler, 2) << "op=" << static_cast<int>(op);  // closed genus-0 shell
        EXPECT_TRUE(m.isValid()) << "op=" << static_cast<int>(op);
    }
}

TEST(MeshBooleanRobust, IntersectionVolumeIsOverlap)
{
    const Mesh i = robustMeshBoolean(boxA(), boxB(), BooleanOperationType::Intersection);
    EXPECT_NEAR(vol(i), 1.f, 0.05f);  // overlap cube [0,1]^3
    EXPECT_TRUE(i.isValid());
}

TEST(MeshBooleanRobust, UnionVolumeIsSum)
{
    const Mesh u = robustMeshBoolean(boxA(), boxB(), BooleanOperationType::Union);
    EXPECT_NEAR(vol(u), 15.f, 0.1f);  // 8 + 8 - 1
    EXPECT_TRUE(u.isValid());
}

TEST(MeshBooleanRobust, DifferenceVolumeIsAMinusOverlap)
{
    const Mesh d = robustMeshBoolean(boxA(), boxB(), BooleanOperationType::Difference);
    EXPECT_NEAR(vol(d), 7.f, 0.1f);  // 8 - 1
    EXPECT_TRUE(d.isValid());
}

}  // namespace nexus::geometry::testing
