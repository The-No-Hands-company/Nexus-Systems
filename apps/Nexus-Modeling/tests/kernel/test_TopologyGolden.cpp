// Golden topology regression set — G2.6
//
// These tests lock in the canonical topology of each built-in primitive and
// verify that round-trip operations (extract, split, weld, append) produce
// known-good outputs.  A failure here indicates an unintentional change to
// primitive construction, topology utilities, or mesh operations.
//
// Values are derived directly from the Mesh.cpp primitive implementations and
// must be updated deliberately if the topology contract intentionally changes.

#include <nexus/geometry/GeometryKernel.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/// Sort and return the face index set for a given face.
std::vector<uint32_t> sortedFaceIndices(const Face& f)
{
    auto v = f.indices;
    std::sort(v.begin(), v.end());
    return v;
}

/// Total unique edges in the edge list.
size_t edgeCount(const MeshTopology& topo)
{
    return TopologyUtilities::extractEdgeList(topo).size();
}

/// Boundary loop count.
size_t boundaryLoopCount(const MeshTopology& topo)
{
    return TopologyUtilities::extractBoundaryLoops(topo).size();
}

/// Returns sizes of all boundary loops, sorted ascending.
std::vector<size_t> boundaryLoopSizes(const MeshTopology& topo)
{
    auto loops = TopologyUtilities::extractBoundaryLoops(topo);
    std::vector<size_t> sizes;
    sizes.reserve(loops.size());
    for (const auto& l : loops) {
        sizes.push_back(l.size());
    }
    std::sort(sizes.begin(), sizes.end());
    return sizes;
}

/// Connected component count.
size_t componentCount(const MeshTopology& topo)
{
    return TopologyUtilities::connectedFaceComponents(topo).size();
}

} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  Triangle primitive
// ─────────────────────────────────────────────────────────────────────────────

TEST(TopologyGolden, TriangleHasThreeVerticesOneFace)
{
    Mesh m = makeTriangle(1.f);
    EXPECT_EQ(m.attributes().vertexCount(), 3u);
    EXPECT_EQ(m.topology().faceCount(), 1u);
    EXPECT_TRUE(m.isValid());
}

TEST(TopologyGolden, TriangleFaceIndicesAreZeroOneTwo)
{
    Mesh m = makeTriangle(1.f);
    ASSERT_EQ(m.topology().faceCount(), 1u);
    const auto& f = m.topology().face(0);
    ASSERT_EQ(f.indices.size(), 3u);
    EXPECT_EQ(f.indices[0], 0u);
    EXPECT_EQ(f.indices[1], 1u);
    EXPECT_EQ(f.indices[2], 2u);
}

TEST(TopologyGolden, TriangleValidationReportIsCanonical)
{
    Mesh m = makeTriangle(1.f);
    const auto report =
        TopologyUtilities::validateTopology(m.topology(), m.attributes().vertexCount());

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.faceCount, 1u);
    EXPECT_EQ(report.boundaryEdgeCount, 3u);
    EXPECT_EQ(report.nonManifoldEdgeCount, 0u);
    EXPECT_TRUE(report.issues.empty());
}

TEST(TopologyGolden, TriangleEdgeListHasThreeSortedEdges)
{
    Mesh m = makeTriangle(1.f);
    const auto edges = TopologyUtilities::extractEdgeList(m.topology());

    ASSERT_EQ(edges.size(), 3u);
    // Expected sorted undirected edges: (0,1), (0,2), (1,2)
    EXPECT_EQ(edges[0].v0, 0u);  EXPECT_EQ(edges[0].v1, 1u);
    EXPECT_EQ(edges[1].v0, 0u);  EXPECT_EQ(edges[1].v1, 2u);
    EXPECT_EQ(edges[2].v0, 1u);  EXPECT_EQ(edges[2].v1, 2u);
}

TEST(TopologyGolden, TriangleHasOneBoundaryLoopOfSizeFour)
{
    Mesh m = makeTriangle(1.f);
    EXPECT_EQ(boundaryLoopCount(m.topology()), 1u);
    const auto sizes = boundaryLoopSizes(m.topology());
    ASSERT_EQ(sizes.size(), 1u);
    // Closed loop: [v0, v1, v2, v0] — 4 entries
    EXPECT_EQ(sizes[0], 4u);
}

TEST(TopologyGolden, TriangleHasOneConnectedComponent)
{
    Mesh m = makeTriangle(1.f);
    const auto components = TopologyUtilities::connectedFaceComponents(m.topology());
    ASSERT_EQ(components.size(), 1u);
    ASSERT_EQ(components[0].size(), 1u);
    EXPECT_EQ(components[0][0], 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Box primitive
// ─────────────────────────────────────────────────────────────────────────────

TEST(TopologyGolden, BoxHasEightVerticesSixFaces)
{
    Mesh m = makeBox(1.f, 1.f, 1.f);
    EXPECT_EQ(m.attributes().vertexCount(), 8u);
    EXPECT_EQ(m.topology().faceCount(), 6u);
    EXPECT_TRUE(m.isValid());
}

TEST(TopologyGolden, BoxValidationReportIsCanonical)
{
    Mesh m = makeBox(1.f, 1.f, 1.f);
    const auto report =
        TopologyUtilities::validateTopology(m.topology(), m.attributes().vertexCount());

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.faceCount, 6u);
    EXPECT_EQ(report.boundaryEdgeCount, 0u);
    EXPECT_EQ(report.nonManifoldEdgeCount, 0u);
    EXPECT_TRUE(report.issues.empty());
}

TEST(TopologyGolden, BoxHasTwelveUniqueEdges)
{
    Mesh m = makeBox(1.f, 1.f, 1.f);
    EXPECT_EQ(edgeCount(m.topology()), 12u);
}

TEST(TopologyGolden, BoxHasNoOpenBoundaryLoops)
{
    Mesh m = makeBox(1.f, 1.f, 1.f);
    EXPECT_EQ(boundaryLoopCount(m.topology()), 0u);
}

TEST(TopologyGolden, BoxHasOneConnectedComponent)
{
    Mesh m = makeBox(1.f, 1.f, 1.f);
    const auto components = TopologyUtilities::connectedFaceComponents(m.topology());
    ASSERT_EQ(components.size(), 1u);
    EXPECT_EQ(components[0].size(), 6u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plane 1×1 (single quad)
// ─────────────────────────────────────────────────────────────────────────────

TEST(TopologyGolden, Plane1x1HasFourVerticesOneFace)
{
    Mesh m = makePlane(1.f, 1.f, 1, 1);
    EXPECT_EQ(m.attributes().vertexCount(), 4u);
    EXPECT_EQ(m.topology().faceCount(), 1u);
    EXPECT_TRUE(m.isValid());
}

TEST(TopologyGolden, Plane1x1FaceIndicesAreCanonical)
{
    Mesh m = makePlane(1.f, 1.f, 1, 1);
    ASSERT_EQ(m.topology().faceCount(), 1u);
    const auto& f = m.topology().face(0);
    ASSERT_EQ(f.indices.size(), 4u);
    // makePlane emits {tl, bl, br, tr} = {0, 2, 3, 1} for the single cell
    EXPECT_EQ(f.indices[0], 0u);
    EXPECT_EQ(f.indices[1], 2u);
    EXPECT_EQ(f.indices[2], 3u);
    EXPECT_EQ(f.indices[3], 1u);
}

TEST(TopologyGolden, Plane1x1ValidationReportIsCanonical)
{
    Mesh m = makePlane(1.f, 1.f, 1, 1);
    const auto report =
        TopologyUtilities::validateTopology(m.topology(), m.attributes().vertexCount());

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.faceCount, 1u);
    EXPECT_EQ(report.boundaryEdgeCount, 4u);
    EXPECT_EQ(report.nonManifoldEdgeCount, 0u);
    EXPECT_TRUE(report.issues.empty());
}

TEST(TopologyGolden, Plane1x1EdgeListHasFourSortedEdges)
{
    Mesh m = makePlane(1.f, 1.f, 1, 1);
    const auto edges = TopologyUtilities::extractEdgeList(m.topology());

    ASSERT_EQ(edges.size(), 4u);
    // Face {0,2,3,1} → undirected: (0,2),(2,3),(1,3),(0,1) → sorted: (0,1),(0,2),(1,3),(2,3)
    EXPECT_EQ(edges[0].v0, 0u);  EXPECT_EQ(edges[0].v1, 1u);
    EXPECT_EQ(edges[1].v0, 0u);  EXPECT_EQ(edges[1].v1, 2u);
    EXPECT_EQ(edges[2].v0, 1u);  EXPECT_EQ(edges[2].v1, 3u);
    EXPECT_EQ(edges[3].v0, 2u);  EXPECT_EQ(edges[3].v1, 3u);
}

TEST(TopologyGolden, Plane1x1HasOneBoundaryLoopOfSizeFive)
{
    Mesh m = makePlane(1.f, 1.f, 1, 1);
    EXPECT_EQ(boundaryLoopCount(m.topology()), 1u);
    const auto sizes = boundaryLoopSizes(m.topology());
    ASSERT_EQ(sizes.size(), 1u);
    // Closed loop: 4 perimeter vertices + repeated start = 5 entries
    EXPECT_EQ(sizes[0], 5u);
}

TEST(TopologyGolden, Plane1x1HasOneConnectedComponent)
{
    Mesh m = makePlane(1.f, 1.f, 1, 1);
    const auto components = TopologyUtilities::connectedFaceComponents(m.topology());
    ASSERT_EQ(components.size(), 1u);
    EXPECT_EQ(components[0].size(), 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Plane 2×2 (2×2 quad grid)
// ─────────────────────────────────────────────────────────────────────────────

TEST(TopologyGolden, Plane2x2HasNineVerticesFourFaces)
{
    Mesh m = makePlane(1.f, 1.f, 2, 2);
    EXPECT_EQ(m.attributes().vertexCount(), 9u);
    EXPECT_EQ(m.topology().faceCount(), 4u);
    EXPECT_TRUE(m.isValid());
}

TEST(TopologyGolden, Plane2x2FaceIndicesAreCanonical)
{
    Mesh m = makePlane(1.f, 1.f, 2, 2);
    ASSERT_EQ(m.topology().faceCount(), 4u);

    // Row-major, CCW quads: {tl, bl, br, tr} for each cell.
    // numCols = 3; cell (row,col):
    //   (0,0): tl=0, bl=3, br=4, tr=1  → {0,3,4,1}
    //   (0,1): tl=1, bl=4, br=5, tr=2  → {1,4,5,2}
    //   (1,0): tl=3, bl=6, br=7, tr=4  → {3,6,7,4}
    //   (1,1): tl=4, bl=7, br=8, tr=5  → {4,7,8,5}
    const std::array<std::array<uint32_t, 4>, 4> expected = {{
        {0u, 3u, 4u, 1u},
        {1u, 4u, 5u, 2u},
        {3u, 6u, 7u, 4u},
        {4u, 7u, 8u, 5u},
    }};

    for (size_t fi = 0; fi < 4; ++fi) {
        const auto& f = m.topology().face(fi);
        ASSERT_EQ(f.indices.size(), 4u) << "face " << fi;
        for (size_t k = 0; k < 4; ++k) {
            EXPECT_EQ(f.indices[k], expected[fi][k]) << "face " << fi << " idx " << k;
        }
    }
}

TEST(TopologyGolden, Plane2x2ValidationReportIsCanonical)
{
    Mesh m = makePlane(1.f, 1.f, 2, 2);
    const auto report =
        TopologyUtilities::validateTopology(m.topology(), m.attributes().vertexCount());

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.faceCount, 4u);
    EXPECT_EQ(report.boundaryEdgeCount, 8u);
    EXPECT_EQ(report.nonManifoldEdgeCount, 0u);
    EXPECT_TRUE(report.issues.empty());
}

TEST(TopologyGolden, Plane2x2EdgeListHasTwelveEdges)
{
    Mesh m = makePlane(1.f, 1.f, 2, 2);
    // 4 quads × 4 edges = 16 directed, but 4 interior edges shared → 12 unique undirected
    EXPECT_EQ(edgeCount(m.topology()), 12u);
}

TEST(TopologyGolden, Plane2x2EdgeListBoundaryAndInteriorCounts)
{
    Mesh m = makePlane(1.f, 1.f, 2, 2);
    // 12 total edges: 8 boundary (perimeter) + 4 interior (shared)
    const auto report =
        TopologyUtilities::validateTopology(m.topology(), m.attributes().vertexCount());
    const size_t interiorEdges = edgeCount(m.topology()) - report.boundaryEdgeCount;
    EXPECT_EQ(report.boundaryEdgeCount, 8u);
    EXPECT_EQ(interiorEdges, 4u);
}

TEST(TopologyGolden, Plane2x2HasOneBoundaryLoopOfSizeNine)
{
    Mesh m = makePlane(1.f, 1.f, 2, 2);
    EXPECT_EQ(boundaryLoopCount(m.topology()), 1u);
    const auto sizes = boundaryLoopSizes(m.topology());
    ASSERT_EQ(sizes.size(), 1u);
    // 8 perimeter vertices + repeated start vertex = 9 entries
    EXPECT_EQ(sizes[0], 9u);
}

TEST(TopologyGolden, Plane2x2HasOneConnectedComponent)
{
    Mesh m = makePlane(1.f, 1.f, 2, 2);
    const auto components = TopologyUtilities::connectedFaceComponents(m.topology());
    ASSERT_EQ(components.size(), 1u);
    EXPECT_EQ(components[0].size(), 4u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Operation round-trip golden tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(TopologyGolden, ExtractFaceRangeFromPlane2x2ProducesGoldenSubset)
{
    // Extract face 0 from a 4-face plane; result should be a single valid quad.
    Mesh src = makePlane(1.f, 1.f, 2, 2);
    Mesh out;
    ASSERT_TRUE(src.extractFaceRange(0, 1, out));

    EXPECT_EQ(out.topology().faceCount(), 1u);
    EXPECT_EQ(out.attributes().vertexCount(), 4u);
    EXPECT_TRUE(out.isValid());

    // Source must be unchanged.
    EXPECT_EQ(src.topology().faceCount(), 4u);
    EXPECT_EQ(src.attributes().vertexCount(), 9u);

    // Extracted quad has 4 boundary edges.
    const auto report =
        TopologyUtilities::validateTopology(out.topology(), out.attributes().vertexCount());
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.boundaryEdgeCount, 4u);
    EXPECT_EQ(report.nonManifoldEdgeCount, 0u);
}

TEST(TopologyGolden, ExtractFaceRangeLastTwoFacesFromPlane2x2IsGolden)
{
    Mesh src = makePlane(1.f, 1.f, 2, 2);
    Mesh out;
    ASSERT_TRUE(src.extractFaceRange(2, 2, out));

    EXPECT_EQ(out.topology().faceCount(), 2u);
    EXPECT_TRUE(out.isValid());

    // Source must be unchanged.
    EXPECT_EQ(src.topology().faceCount(), 4u);

    // Two quads sharing an edge → 7 total unique edges
    const auto report =
        TopologyUtilities::validateTopology(out.topology(), out.attributes().vertexCount());
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.faceCount, 2u);
    // 2 quads sharing 1 edge: 4+4-1 = 7 unique edges, 6 boundary, 1 interior
    EXPECT_EQ(report.boundaryEdgeCount, 6u);
    EXPECT_EQ(report.nonManifoldEdgeCount, 0u);
}

TEST(TopologyGolden, SplitFaceRangeFromPlane2x2ProducesGoldenHalves)
{
    // Split last face off a 2-face plane.
    Mesh src = makePlane(1.f, 1.f, 1, 2);  // 2 quads side by side
    ASSERT_EQ(src.topology().faceCount(), 2u);

    Mesh splitOut;
    ASSERT_TRUE(src.splitFaceRange(1, 1, splitOut));

    // Remaining (src) has 1 face.
    EXPECT_EQ(src.topology().faceCount(), 1u);
    EXPECT_TRUE(src.isValid());

    // Split-off mesh has 1 face.
    EXPECT_EQ(splitOut.topology().faceCount(), 1u);
    EXPECT_TRUE(splitOut.isValid());

    // Both are single quads → 4 boundary edges each.
    const auto reportSrc =
        TopologyUtilities::validateTopology(src.topology(), src.attributes().vertexCount());
    EXPECT_EQ(reportSrc.boundaryEdgeCount, 4u);

    const auto reportOut =
        TopologyUtilities::validateTopology(splitOut.topology(), splitOut.attributes().vertexCount());
    EXPECT_EQ(reportOut.boundaryEdgeCount, 4u);
}

TEST(TopologyGolden, WeldIsNoopOnCleanPlane)
{
    // A freshly built plane has no coincident vertices; weld should not compact.
    Mesh m = makePlane(1.f, 1.f, 2, 2);
    const size_t vertsBefore = m.attributes().vertexCount();
    ASSERT_EQ(vertsBefore, 9u);

    ASSERT_TRUE(m.weldCoincidentVertices(1e-5f));

    EXPECT_EQ(m.attributes().vertexCount(), 9u);
    EXPECT_EQ(m.topology().faceCount(), 4u);
    EXPECT_TRUE(m.isValid());
}

TEST(TopologyGolden, AppendMergeProducesTwoDisconnectedComponents)
{
    // Appending two triangles with no shared vertices must yield 2 components.
    Mesh base = makeTriangle(1.f);
    Mesh other = makeTriangle(2.f);

    ASSERT_TRUE(base.appendMesh(other));

    EXPECT_EQ(base.topology().faceCount(), 2u);
    EXPECT_EQ(base.attributes().vertexCount(), 6u);
    EXPECT_TRUE(base.isValid());

    const auto components = TopologyUtilities::connectedFaceComponents(base.topology());
    EXPECT_EQ(components.size(), 2u);
}

TEST(TopologyGolden, AppendMergeValidationReportIsCorrect)
{
    Mesh base = makeTriangle(1.f);
    Mesh other = makeTriangle(2.f);
    ASSERT_TRUE(base.appendMesh(other));

    const auto report =
        TopologyUtilities::validateTopology(base.topology(), base.attributes().vertexCount());

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.faceCount, 2u);
    // Each triangle contributes 3 boundary edges; no shared edges → 6 total boundary
    EXPECT_EQ(report.boundaryEdgeCount, 6u);
    EXPECT_EQ(report.nonManifoldEdgeCount, 0u);
}

TEST(TopologyGolden, AppendMergeEdgeListHasSixEdges)
{
    Mesh base = makeTriangle(1.f);
    Mesh other = makeTriangle(2.f);
    ASSERT_TRUE(base.appendMesh(other));

    // Two disjoint triangles → 3 + 3 = 6 unique undirected edges
    EXPECT_EQ(edgeCount(base.topology()), 6u);
}

TEST(TopologyGolden, AppendMergeHasTwoBoundaryLoops)
{
    Mesh base = makeTriangle(1.f);
    Mesh other = makeTriangle(2.f);
    ASSERT_TRUE(base.appendMesh(other));

    // Each triangle's perimeter is a separate closed loop.
    EXPECT_EQ(boundaryLoopCount(base.topology()), 2u);
    const auto sizes = boundaryLoopSizes(base.topology());
    ASSERT_EQ(sizes.size(), 2u);
    EXPECT_EQ(sizes[0], 4u);
    EXPECT_EQ(sizes[1], 4u);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Stable element ID survival through operations
// ─────────────────────────────────────────────────────────────────────────────

TEST(TopologyGolden, ExtractPreservesStableIdsInBothMeshes)
{
    Mesh src = makePlane(1.f, 1.f, 2, 2);
    src.rebuildStableElementIds();
    ASSERT_TRUE(src.hasStableElementIds());

    Mesh out;
    ASSERT_TRUE(src.extractFaceRange(0, 2, out));

    EXPECT_TRUE(src.hasStableElementIds());
    EXPECT_TRUE(out.hasStableElementIds());
    EXPECT_EQ(src.stableElementIds().faceIds.size(), 4u);
    EXPECT_EQ(out.stableElementIds().faceIds.size(), 2u);
}

TEST(TopologyGolden, SplitPreservesStableIdsInBothMeshes)
{
    Mesh src = makePlane(1.f, 1.f, 2, 2);
    src.rebuildStableElementIds();
    ASSERT_TRUE(src.hasStableElementIds());

    Mesh out;
    ASSERT_TRUE(src.splitFaceRange(0, 2, out));

    EXPECT_TRUE(src.hasStableElementIds());
    EXPECT_TRUE(out.hasStableElementIds());
    EXPECT_EQ(src.stableElementIds().faceIds.size(), 2u);
    EXPECT_EQ(out.stableElementIds().faceIds.size(), 2u);
}
