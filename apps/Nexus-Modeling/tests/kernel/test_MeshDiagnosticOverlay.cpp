#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshDiagnosticOverlay.h>

#include <gtest/gtest.h>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

// ─────────────────────────────────────────────────────────────────────────────

TEST(MeshDiagnosticOverlay, ReturnsFalseForEmptyMesh)
{
    Mesh empty;
    MeshDiagnosticOverlayData data;
    const bool ok = MeshDiagnosticExporter::extract(empty, {}, data);
    EXPECT_FALSE(ok);
    EXPECT_EQ(data.nonManifoldEdgeCount, 0u);
    EXPECT_EQ(data.boundaryEdgeCount,    0u);
    EXPECT_EQ(data.degenerateFaceCount,  0u);
    EXPECT_EQ(data.isolatedVertexCount,  0u);
}

TEST(MeshDiagnosticOverlay, ClosedBoxHasNoBoundaryEdges)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    MeshDiagnosticOverlayData data;
    const bool ok = MeshDiagnosticExporter::extract(box, {}, data);
    EXPECT_TRUE(ok);
    EXPECT_EQ(data.boundaryEdgeCount, 0u);
    EXPECT_EQ(data.nonManifoldEdgeCount, 0u);
    EXPECT_EQ(data.degenerateFaceCount,  0u);
}

TEST(MeshDiagnosticOverlay, OpenPlaneHasBoundaryEdges)
{
    // A 1×1 plane has 4 boundary edges (open mesh)
    Mesh plane = makePlane(1.f, 1.f, 1, 1);
    MeshDiagnosticOverlayData data;
    const bool ok = MeshDiagnosticExporter::extract(plane, {}, data);
    EXPECT_TRUE(ok);
    EXPECT_GT(data.boundaryEdgeCount, 0u);
    EXPECT_EQ(data.nonManifoldEdgeCount, 0u);
}

TEST(MeshDiagnosticOverlay, DetectsDegenerateFaceWithZeroArea)
{
    // Triangle where all three positions are the same → zero area
    Mesh mesh;
    mesh.attributes().setPositions({{0.f, 0.f, 0.f},
                                    {0.f, 0.f, 0.f},
                                    {0.f, 0.f, 0.f}});
    mesh.topology().addFace(Face{{0u, 1u, 2u}});

    MeshDiagnosticOverlayData data;
    const bool ok = MeshDiagnosticExporter::extract(mesh, {}, data);
    EXPECT_TRUE(ok);
    EXPECT_EQ(data.degenerateFaceCount, 1u);
    ASSERT_EQ(data.faces.size(), 1u);
    EXPECT_EQ(data.faces[0].faceIndex, 0u);
    EXPECT_EQ(data.faces[0].kind, OverlayFaceKind::Degenerate);
}

TEST(MeshDiagnosticOverlay, DetectsIsolatedVertex)
{
    // Box positions plus one extra unreferenced vertex
    Mesh box = makeBox(1.f, 1.f, 1.f);
    auto positions = box.attributes().positions();
    positions.push_back({99.f, 99.f, 99.f});  // orphaned
    box.attributes().setPositions(std::move(positions));

    MeshDiagnosticOverlayData data;
    const bool ok = MeshDiagnosticExporter::extract(box, {}, data);
    EXPECT_TRUE(ok);
    EXPECT_EQ(data.isolatedVertexCount, 1u);
    ASSERT_EQ(data.vertices.size(), 1u);
    EXPECT_EQ(data.vertices[0].kind, OverlayVertexKind::Isolated);
}

TEST(MeshDiagnosticOverlay, ModeFlagFiltersOutput)
{
    // Open plane — has boundary edges and no degenerate faces
    Mesh plane = makePlane(1.f, 1.f, 1, 1);

    // Only request degenerate faces; boundary edges should not appear
    MeshDiagnosticOverlayOptions opts;
    opts.modes = MeshOverlayMode::DegenerateFaces;

    MeshDiagnosticOverlayData data;
    MeshDiagnosticExporter::extract(plane, opts, data);
    EXPECT_EQ(data.boundaryEdgeCount,   0u);
    EXPECT_EQ(data.nonManifoldEdgeCount, 0u);
    EXPECT_EQ(data.degenerateFaceCount,  0u);
    EXPECT_TRUE(data.edges.empty());
}

TEST(MeshDiagnosticOverlay, DetectsNonManifoldEdge)
{
    // Build a mesh where one edge is shared by 3 faces:
    //   - Triangle (0,1,2)
    //   - Triangle (0,2,3)  ← both use edge 0-2
    //   - Triangle (0,2,4)  ← third face on edge 0-2 → non-manifold
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},  // 0
        {1.f, 0.f, 0.f},  // 1
        {0.5f, 1.f, 0.f}, // 2
        {-0.5f, 1.f, 0.f},// 3
        {0.5f, -1.f, 0.f},// 4
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u}});
    mesh.topology().addFace(Face{{0u, 2u, 3u}});
    mesh.topology().addFace(Face{{0u, 2u, 4u}});  // third face on edge 0-2

    MeshDiagnosticOverlayData data;
    MeshDiagnosticExporter::extract(mesh, {}, data);
    EXPECT_GE(data.nonManifoldEdgeCount, 1u);
}

TEST(MeshDiagnosticOverlay, DeterministicAcrossRepeatedExtract)
{
    Mesh box = makeBox(2.f, 1.f, 0.5f);

    MeshDiagnosticOverlayData data1;
    MeshDiagnosticOverlayData data2;
    MeshDiagnosticExporter::extract(box, {}, data1);
    MeshDiagnosticExporter::extract(box, {}, data2);

    EXPECT_EQ(data1.nonManifoldEdgeCount, data2.nonManifoldEdgeCount);
    EXPECT_EQ(data1.boundaryEdgeCount,    data2.boundaryEdgeCount);
    EXPECT_EQ(data1.degenerateFaceCount,  data2.degenerateFaceCount);
    EXPECT_EQ(data1.isolatedVertexCount,  data2.isolatedVertexCount);
    EXPECT_EQ(data1.edges.size(),         data2.edges.size());
    EXPECT_EQ(data1.faces.size(),         data2.faces.size());
    EXPECT_EQ(data1.vertices.size(),      data2.vertices.size());
}

} // namespace nexus::geometry::testing
