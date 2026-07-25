#include <gtest/gtest.h>

#include <nexus/geometry/FeatureRecognition.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

#include <utility>

namespace {

using namespace nexus::geometry;

// FeatureRecognition is a deliberately coarse heuristic pass (interior face -> "hole",
// quad -> "fillet", boundary face -> "pocket"); there is no ground truth for the labels, so
// these tests pin the structural invariants that must hold regardless of heuristic quality:
// self-consistent counts, no pockets on a closed solid, pockets on an open mesh, and no
// crash on empty input.

TEST(FeatureRecognition, CountsAreSelfConsistent) {
    auto hem = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(hem.has_value());

    FeatureRecognitionResult r = FeatureRecognition::recognize(*hem);
    EXPECT_EQ(r.features.size(),
              static_cast<size_t>(r.holesDetected) + r.filletsDetected + r.pocketsDetected)
        << "the feature list must account for exactly the reported hole/fillet/pocket counts";
}

TEST(FeatureRecognition, ClosedSolidHasNoPockets) {
    // A pocket is flagged from a boundary edge; a closed manifold has none.
    auto hem = HalfEdgeMesh::fromMesh(primitives::makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(hem.has_value());
    ASSERT_TRUE(hem->isClosed());

    FeatureRecognitionResult r = FeatureRecognition::recognize(*hem);
    EXPECT_EQ(r.pocketsDetected, 0u) << "a watertight solid exposes no boundary faces";
}

TEST(FeatureRecognition, OpenMeshExposesPockets) {
    // A flat plane is bounded on all sides, so its boundary faces register as pockets.
    auto hem = HalfEdgeMesh::fromMesh(primitives::makePlane(4.f, 4.f, 3, 3));
    ASSERT_TRUE(hem.has_value());
    ASSERT_FALSE(hem->isClosed());

    FeatureRecognitionResult r = FeatureRecognition::recognize(*hem);
    EXPECT_GT(r.pocketsDetected, 0u) << "an open mesh has boundary faces";
}

TEST(FeatureRecognition, EmptyMeshRecognizesNothing) {
    HalfEdgeMesh empty;
    FeatureRecognitionResult r = FeatureRecognition::recognize(empty);
    EXPECT_TRUE(r.features.empty());
    EXPECT_EQ(r.holesDetected, 0u);
    EXPECT_EQ(r.filletsDetected, 0u);
    EXPECT_EQ(r.pocketsDetected, 0u);
}

TEST(FeatureRecognition, GpuAccelerationOptionsRoundTrip) {
    EXPECT_FALSE(GpuAcceleration::isAvailable());  // placeholder until a Vulkan compute query
    GpuAccelOptions opts = GpuAcceleration::options();
    GpuAcceleration::setOptions(opts);  // round-trips without altering state
    EXPECT_EQ(&GpuAcceleration::options(), &GpuAcceleration::options());
}

TEST(FeatureRecognition, AutoHealDelegatesToMeshRepair) {
    Mesh box = primitives::makeBox(2.f, 2.f, 2.f);
    AutoHealReport r = MeshAutoHeal::heal(box);
    EXPECT_TRUE(r.valid) << "a clean box should heal to a valid mesh";
    EXPECT_TRUE(box.isValid());
}

} // namespace
