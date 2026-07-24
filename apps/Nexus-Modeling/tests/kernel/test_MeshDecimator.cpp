#include <nexus/geometry/MeshDecimator.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

namespace {

// Watertight: every live half-edge has a twin.
bool decimateOutputIsClosed(const HalfEdgeMesh& hem) {
    for (uint32_t e = 0; e < hem.edgeCount(); ++e) {
        const auto& he = hem.edge(e);
        if (he.src == HalfEdgeMesh::kInvalid) continue;
        if (he.twin == HalfEdgeMesh::kInvalid) return false;
    }
    return true;
}

} // namespace

TEST(MeshDecimator, DecimateReducesFaceCount)
{
    Mesh mesh = makePlane(2.f, 2.f, 4, 4);
    const size_t faceCountBefore = mesh.topology().faceCount();
    EXPECT_GT(faceCountBefore, 4u);

    Mesh simplified;
    for (size_t i = 0; i < faceCountBefore / 2; ++i) {
        if (mesh.extractFaceRange(i, 1u, simplified)) {
            EXPECT_TRUE(simplified.isValid());
        }
    }
}

TEST(MeshDecimator, TargetFaceCountZeroUsesRatio)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    const size_t totalFaces = mesh.topology().faceCount();

    EXPECT_GT(totalFaces, 0u);

    Mesh half;
    EXPECT_TRUE(mesh.extractFaceRange(0u, totalFaces / 2, half));
    EXPECT_EQ(half.topology().faceCount(), totalFaces / 2);
    EXPECT_TRUE(half.isValid());
}

TEST(MeshDecimator, InvalidInputHandled)
{
    Mesh empty;
    EXPECT_FALSE(empty.isValid());

    Mesh out;
    EXPECT_FALSE(empty.extractFaceRange(0u, 1u, out));
    EXPECT_FALSE(out.isValid());
}

TEST(MeshDecimator, TriangleMeshDecimatesToFewerVertices)
{
    Mesh tri = makeTriangle(1.f);
    const size_t vcBefore = tri.attributes().vertexCount();

    EXPECT_EQ(vcBefore, 3u);

    Mesh simplified;
    EXPECT_TRUE(tri.extractFaceRange(0u, 1u, simplified));
    EXPECT_EQ(simplified.attributes().vertexCount(), vcBefore);
    EXPECT_TRUE(simplified.isValid());
}

TEST(MeshDecimator, DecimatedMeshPreservesTopologyConsistency)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_TRUE(mesh.topology().allFacesArePoly3Plus());
    EXPECT_TRUE(mesh.topology().hasValidIndices(mesh.attributes().vertexCount()));

    const auto b = mesh.computeBounds();
    EXPECT_NE(b.min, b.max);
}

// -----------------------------------------------------------------------------
// The tests above operate on Mesh helpers and extractFaceRange; none of them call
// the production quadric decimator MeshDecimator::decimate(HalfEdgeMesh). That is
// what the editor's decimate command, the evaluation graph, and MeshSimplify all
// invoke, and it had no test verifying decimation QUALITY — only that the face
// count drops and the result is valid. The tests below pin the quadric-error
// behaviour to known-correct geometry.
// -----------------------------------------------------------------------------

// The defining property of the quadric error metric: a collapse within a flat region
// costs (almost) nothing, because every incident face lies in one plane and the
// squared distance of any on-plane point to that plane is zero. So decimating a planar
// grid must both keep the applied error at ~0 and leave every vertex on the plane.
TEST(MeshDecimator, CoplanarDecimationIsAlmostFree)
{
    // makePlane lays a flat grid in the XZ plane (y = 0).
    auto hem = HalfEdgeMesh::fromMesh(makePlane(4.f, 4.f, 6, 6));
    ASSERT_TRUE(hem.has_value());

    DecimationOptions opts;
    opts.targetRatio = 0.5f;
    auto res = MeshDecimator::decimate(*hem, opts);
    ASSERT_TRUE(res.has_value());

    EXPECT_GT(res->second.collapses, 0u) << "interior of a plane must be collapsible";
    EXPECT_LT(res->second.maxErrorApplied, 1e-6f)
        << "coplanar collapses must cost ~0 under a correct quadric";

    for (const auto& p : res->first.positions())
        EXPECT_NEAR(p.y, 0.f, 1e-5f) << "decimating a plane must keep vertices on the plane";
}

// Quadric decimation drives collapses to preserve the surface. On a sphere every
// vertex lies at radius R; after a moderate decimation the surviving vertices must
// still lie close to that sphere (a coarse but real Hausdorff-style bound).
TEST(MeshDecimator, DecimatedSphereStaysNearSurface)
{
    const float R = 1.f;
    auto hem = HalfEdgeMesh::fromMesh(makeSphere(R, 24, 24));
    ASSERT_TRUE(hem.has_value());

    DecimationOptions opts;
    opts.targetRatio = 0.4f;
    auto res = MeshDecimator::decimate(*hem, opts);
    ASSERT_TRUE(res.has_value());
    EXPECT_LT(res->second.facesOut, res->second.facesIn);

    for (const auto& p : res->first.positions()) {
        const float r = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
        EXPECT_NEAR(r, R, 0.06f) << "decimated sphere vertex drifted off the surface";
    }
}

// A tight maxError budget must be honoured: no collapse whose quadric cost exceeds the
// budget is applied, so the reported applied error stays within it (curved collapses
// that would exceed the budget are simply skipped, leaving more faces).
TEST(MeshDecimator, DecimationRespectsMaxErrorBudget)
{
    auto hem = HalfEdgeMesh::fromMesh(makeSphere(1.f, 20, 20));
    ASSERT_TRUE(hem.has_value());

    DecimationOptions opts;
    opts.targetFaceCount = 4;          // ask for aggressive reduction...
    opts.maxError = 1e-4f;             // ...but cap the per-collapse error hard
    auto res = MeshDecimator::decimate(*hem, opts);
    ASSERT_TRUE(res.has_value());

    EXPECT_LE(res->second.maxErrorApplied, opts.maxError)
        << "no collapse may exceed the error budget";
}

TEST(MeshDecimator, DecimatedBoxStaysWatertightAndReachesTarget)
{
    auto quadHem = HalfEdgeMesh::fromMesh(makeBox(2.f, 2.f, 2.f));
    ASSERT_TRUE(quadHem.has_value());
    auto hem = HalfEdgeMesh::fromMesh(quadHem->toMesh(true));  // triangulated box
    ASSERT_TRUE(hem.has_value());
    const uint32_t facesIn = hem->faceCount();

    DecimationOptions opts;
    opts.targetFaceCount = 8;
    auto res = MeshDecimator::decimate(*hem, opts);
    ASSERT_TRUE(res.has_value());

    EXPECT_LE(res->second.facesOut, 8u);
    EXPECT_LT(res->second.facesOut, facesIn);
    EXPECT_TRUE(decimateOutputIsClosed(res->first))
        << "closed input must stay watertight through decimation";
}
