#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshTopologyValidation.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

// ─────────────────────────────────────────────────────────────────────────────
//  MeshAttributes and MeshTopology contracts
// ─────────────────────────────────────────────────────────────────────────────

TEST(Mesh, EmptyMeshIsInvalid)
{
    Mesh mesh;
    EXPECT_FALSE(mesh.isValid());
}

TEST(Mesh, TriangleWithPositionsAndFaceIsValid)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.topology().faceCount(), 1u);
    EXPECT_EQ(mesh.attributes().vertexCount(), 3u);
}

TEST(Mesh, OutOfBoundsFaceIndexIsInvalid)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 99};  // index 99 out of bounds
    mesh.topology().addFace(f);

    EXPECT_FALSE(mesh.isValid());
}

TEST(Mesh, InconsistentNormalCountIsInvalid)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    // Only 2 normals for 3 vertices — inconsistent.
    mesh.attributes().setNormals({
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);

    EXPECT_FALSE(mesh.isValid());
}

TEST(Mesh, InconsistentTangentCountIsInvalid)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setTangents({
        {1.f, 0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);

    EXPECT_FALSE(mesh.isValid());
}

TEST(Mesh, ComputeVertexNormalsProducesUpNormalForFlatXZTriangle)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);

    ASSERT_TRUE(mesh.computeVertexNormals());
    ASSERT_TRUE(mesh.attributes().hasNormals());
    ASSERT_EQ(mesh.attributes().normals().size(), 3u);

    // CCW winding in XZ plane: cross of (1,0,0) × (0,0,1) = (0,-1,0)? 
    // Actually: (p1-p0)×(p2-p0) = (1,0,0)×(0,0,1) = (0*1-0*0, 0*0-1*1, 1*0-0*0) = (0,-1,0)
    // Normalize → (0,-1,0). Check magnitude ≈ 1.
    for (const auto& n : mesh.attributes().normals()) {
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        EXPECT_NEAR(len, 1.f, 1e-5f);
        // All normals on a flat surface should be identical.
        EXPECT_NEAR(n.x, 0.f, 1e-5f);
        EXPECT_NEAR(n.z, 0.f, 1e-5f);
    }
}

TEST(Mesh, ComputeVertexTangentsProducesDeterministicXAxisTangentForTexturedTriangle)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);

    ASSERT_TRUE(mesh.computeVertexNormals());
    ASSERT_TRUE(mesh.computeVertexTangents());
    ASSERT_TRUE(mesh.attributes().hasTangents());
    ASSERT_EQ(mesh.attributes().tangents().size(), 3u);

    for (const auto& tangent : mesh.attributes().tangents()) {
        const auto& normal = mesh.attributes().normals()[&tangent - mesh.attributes().tangents().data()];
        EXPECT_NEAR(std::abs(tangent.x), 1.f, 1e-5f);
        EXPECT_NEAR(tangent.y, 0.f, 1e-5f);
        EXPECT_NEAR(tangent.z, 0.f, 1e-5f);
        EXPECT_TRUE(tangent.w == 1.f || tangent.w == -1.f);
        const float dot = tangent.x * normal.x + tangent.y * normal.y + tangent.z * normal.z;
        EXPECT_NEAR(dot, 0.f, 1e-5f);
    }
}

TEST(Mesh, ComputeVertexTangentsEncodesNegativeHandednessForMirroredUVs)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, -1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);

    ASSERT_TRUE(mesh.computeVertexNormals());
    ASSERT_TRUE(mesh.computeVertexTangents());
    ASSERT_TRUE(mesh.attributes().hasTangents());

    for (const auto& tangent : mesh.attributes().tangents()) {
        EXPECT_FLOAT_EQ(tangent.w, -1.f);
    }
}

TEST(Mesh, ComputeVertexTangentsRequiresNormalsAndUVs)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);

    EXPECT_FALSE(mesh.computeVertexTangents());

    ASSERT_TRUE(mesh.computeVertexNormals());
    EXPECT_FALSE(mesh.computeVertexTangents());
}

TEST(Mesh, StableElementIdsRebuildDeterministically)
{
    Mesh mesh = makePlane(1.f, 1.f, 1, 1);

    mesh.rebuildStableElementIds();
    ASSERT_TRUE(mesh.hasStableElementIds());
    const StableMeshElementIds first = mesh.stableElementIds();

    mesh.rebuildStableElementIds();
    const StableMeshElementIds second = mesh.stableElementIds();

    EXPECT_EQ(first.vertexIds, second.vertexIds);
    EXPECT_EQ(first.faceIds, second.faceIds);
    EXPECT_EQ(first.edgeIds, second.edgeIds);
    EXPECT_EQ(first.vertexIds.size(), mesh.attributes().vertexCount());
    EXPECT_EQ(first.faceIds.size(), mesh.topology().faceCount());
    EXPECT_EQ(first.edgeIds.size(), 4u);
}

TEST(Mesh, StableElementIdsSurviveDeterministicPositionOnlyEdits)
{
    Mesh mesh = makeTriangle(1.f);
    mesh.rebuildStableElementIds();
    const StableMeshElementIds before = mesh.stableElementIds();

    auto movedPositions = mesh.attributes().positions();
    for (auto& pos : movedPositions) {
        pos.x += 10.f;
        pos.y += 2.f;
        pos.z -= 4.f;
    }
    mesh.attributes().setPositions(std::move(movedPositions));

    const StableMeshElementIds after = mesh.stableElementIds();
    EXPECT_EQ(before.vertexIds, after.vertexIds);
    EXPECT_EQ(before.faceIds, after.faceIds);
    EXPECT_EQ(before.edgeIds, after.edgeIds);
}

TEST(Mesh, StableElementIdsSurviveTopologyPreservingTransformOperation)
{
    Mesh mesh = makeTriangle(1.f);
    ASSERT_TRUE(mesh.computeVertexNormals());
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    ASSERT_TRUE(mesh.computeVertexTangents());
    mesh.rebuildStableElementIds();
    const StableMeshElementIds before = mesh.stableElementIds();

    nexus::render::Mat4 transform = nexus::render::Mat4::identity();
    transform.m[0][3] = 5.f;
    transform.m[1][3] = -2.f;
    transform.m[2][3] = 3.f;

    ASSERT_TRUE(mesh.applyTransform(transform));

    const StableMeshElementIds after = mesh.stableElementIds();
    EXPECT_EQ(before.vertexIds, after.vertexIds);
    EXPECT_EQ(before.faceIds, after.faceIds);
    EXPECT_EQ(before.edgeIds, after.edgeIds);
}

TEST(Mesh, WeldCoincidentVerticesCompactsEquivalentDuplicateVerticesDeterministically)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setNormals({
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {1.f, 1.f},
        {0.f, 0.f},
        {1.f, 1.f},
        {0.f, 1.f},
    });

    Face first{};
    first.indices = {0, 1, 2};
    mesh.topology().addFace(first);
    Face second{};
    second.indices = {3, 4, 5};
    mesh.topology().addFace(second);

    ASSERT_TRUE(mesh.computeVertexNormals());
    ASSERT_TRUE(mesh.computeVertexTangents());

    mesh.rebuildStableElementIds();
    const StableMeshElementIds before = mesh.stableElementIds();

    ASSERT_TRUE(mesh.weldCoincidentVertices());
    ASSERT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.attributes().vertexCount(), 4u);
    EXPECT_EQ(mesh.topology().faceCount(), 2u);
    EXPECT_EQ(mesh.topology().face(0).indices, (std::vector<uint32_t>{0, 1, 2}));
    EXPECT_EQ(mesh.topology().face(1).indices, (std::vector<uint32_t>{0, 2, 3}));
    EXPECT_TRUE(mesh.attributes().hasTangents());
    EXPECT_EQ(mesh.attributes().tangents().size(), 4u);

    const StableMeshElementIds after = mesh.stableElementIds();
    EXPECT_EQ(after.vertexIds, (std::vector<MeshElementID>{before.vertexIds[0], before.vertexIds[1], before.vertexIds[2], before.vertexIds[5]}));
    EXPECT_EQ(after.faceIds, before.faceIds);
    EXPECT_EQ(after.edgeIds.size(), 5u);
}

TEST(Mesh, WeldCoincidentVerticesRejectsAttributeSeamCollapse)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setNormals({
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {1.f, 1.f},
        {0.25f, 0.f},
        {1.f, 1.f},
        {0.f, 1.f},
    });

    Face first{};
    first.indices = {0, 1, 2};
    mesh.topology().addFace(first);
    Face second{};
    second.indices = {3, 4, 5};
    mesh.topology().addFace(second);

    ASSERT_TRUE(mesh.isValid());
    EXPECT_TRUE(mesh.weldCoincidentVertices());
    EXPECT_EQ(mesh.attributes().vertexCount(), 5u);
    EXPECT_EQ(mesh.topology().face(1).indices, (std::vector<uint32_t>{3, 2, 4}));
}

TEST(Mesh, WeldCoincidentVerticesRejectsFaceCollapse)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {0.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face face{};
    face.indices = {0, 1, 2};
    mesh.topology().addFace(face);

    ASSERT_TRUE(mesh.isValid());
    EXPECT_FALSE(mesh.weldCoincidentVertices());
    EXPECT_EQ(mesh.attributes().vertexCount(), 3u);
    EXPECT_EQ(mesh.topology().face(0).indices, (std::vector<uint32_t>{0, 1, 2}));
}

// The weld used to be all-or-nothing: if welding would collapse ANY single face, it
// abandoned the entire weld and left the whole mesh unwelded. On a finely tessellated
// boolean that is catastrophic — one sliver anywhere leaves every triangle isolated, and
// the result disintegrates into a triangle soup (measured: 5,958 boundary edges from 1,986
// faces, i.e. every edge a boundary). DropCollapsedFace removes just the zero-area face.
TEST(Mesh, WeldCollapsePolicyDropRemovesOnlyTheCollapsedFace)
{
    Mesh mesh;
    // A unit square split into two triangles, plus a sliver whose two ends coincide within
    // the weld tolerance. Only the sliver may be lost.
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},          // 0
        {1.f, 0.f, 0.f},          // 1
        {1.f, 1.f, 0.f},          // 2
        {0.f, 1.f, 0.f},          // 3
        {0.5f, 0.5f, 0.f},        // 4
        {0.5f + 1e-9f, 0.5f, 0.f} // 5 — coincides with 4
    });
    for (const std::vector<uint32_t>& idx : std::vector<std::vector<uint32_t>>{
             {0, 1, 2}, {0, 2, 3}, {4, 5, 1}}) {
        Face f{};
        f.indices = idx;
        mesh.topology().addFace(f);
    }
    ASSERT_TRUE(mesh.isValid());

    EXPECT_TRUE(mesh.weldCoincidentVertices(1e-5f, Mesh::WeldCollapsePolicy::DropCollapsedFace));
    EXPECT_EQ(mesh.topology().faceCount(), 2u) << "the collapsed sliver should be the only loss";
    EXPECT_EQ(mesh.attributes().vertexCount(), 5u) << "vertices 4 and 5 should have merged";
}

// The default stays what it was, so no existing caller changes behaviour.
TEST(Mesh, WeldCollapsePolicyDefaultsToRejectingTheWholeWeld)
{
    Mesh mesh;
    mesh.attributes().setPositions({{0.f, 0.f, 0.f}, {0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}});
    Face face{};
    face.indices = {0, 1, 2};
    mesh.topology().addFace(face);
    ASSERT_TRUE(mesh.isValid());

    EXPECT_FALSE(mesh.weldCoincidentVertices(1e-5f));
    EXPECT_EQ(mesh.attributes().vertexCount(), 3u);
}

// Dropping a collapsed face is an EDGE COLLAPSE, so a closed surface must stay closed:
// the face's two surviving corners become the same undirected edge, and the neighbours
// across them remain matched. This is the property the boolean depends on.
TEST(Mesh, WeldCollapsePolicyDropKeepsAClosedSurfaceClosed)
{
    Mesh mesh = primitives::makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(mesh.topology().triangulate());
    const auto before = MeshTopologyValidation::validate(mesh);
    ASSERT_EQ(before.boundaryLoops, 0u);

    // Introduce a duplicate of an existing vertex and a sliver face using both copies,
    // so the weld has something to collapse.
    auto pos = mesh.attributes().positions();
    const uint32_t dup = static_cast<uint32_t>(pos.size());
    pos.push_back(pos[0]);
    mesh.attributes().setPositions(std::move(pos));

    EXPECT_TRUE(mesh.weldCoincidentVertices(1e-5f, Mesh::WeldCollapsePolicy::DropCollapsedFace));
    const auto after = MeshTopologyValidation::validate(mesh);
    EXPECT_EQ(after.boundaryLoops, 0u) << "the box stopped being closed after welding";
    EXPECT_EQ(after.euler, before.euler) << "welding changed the Euler characteristic";
    (void)dup;
}

TEST(Mesh, AppendMeshRemapsIncomingTopologyAndPreservesOriginalStableIds)
{
    Mesh base = makeTriangle(1.f);
    ASSERT_TRUE(base.computeVertexNormals());
    base.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    ASSERT_TRUE(base.computeVertexTangents());
    base.rebuildStableElementIds();
    const StableMeshElementIds before = base.stableElementIds();

    Mesh appended = makeTriangle(2.f);
    ASSERT_TRUE(appended.computeVertexNormals());
    appended.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    ASSERT_TRUE(appended.computeVertexTangents());

    ASSERT_TRUE(base.appendMesh(appended));
    ASSERT_TRUE(base.isValid());
    EXPECT_EQ(base.attributes().vertexCount(), 6u);
    EXPECT_EQ(base.topology().faceCount(), 2u);
    EXPECT_EQ(base.topology().face(0).indices, (std::vector<uint32_t>{0, 1, 2}));
    EXPECT_EQ(base.topology().face(1).indices, (std::vector<uint32_t>{3, 4, 5}));

    const StableMeshElementIds after = base.stableElementIds();
    EXPECT_EQ(after.vertexIds.size(), 6u);
    EXPECT_EQ(after.faceIds.size(), 2u);
    EXPECT_EQ(after.edgeIds.size(), 6u);
    EXPECT_EQ(after.vertexIds[0], before.vertexIds[0]);
    EXPECT_EQ(after.vertexIds[1], before.vertexIds[1]);
    EXPECT_EQ(after.vertexIds[2], before.vertexIds[2]);
    EXPECT_EQ(after.faceIds[0], before.faceIds[0]);
    EXPECT_EQ(after.edgeIds[0], before.edgeIds[0]);
    EXPECT_GT(after.vertexIds[3], before.vertexIds[2]);
    EXPECT_GT(after.faceIds[1], before.faceIds[0]);
}

TEST(Mesh, AppendMeshRejectsAttributeChannelMismatch)
{
    Mesh base = makeTriangle(1.f);
    ASSERT_TRUE(base.computeVertexNormals());

    Mesh appended = makeTriangle(1.f);

    EXPECT_FALSE(base.appendMesh(appended));
    EXPECT_EQ(base.attributes().vertexCount(), 3u);
    EXPECT_EQ(base.topology().faceCount(), 1u);
}

TEST(Mesh, ExtractFaceRangeBuildsCompactedSubmeshAndPreservesStableIds)
{
    Mesh mesh = makePlane(1.f, 1.f, 1, 2);
    ASSERT_TRUE(mesh.isValid());
    mesh.rebuildStableElementIds();

    Mesh extracted;
    ASSERT_TRUE(mesh.extractFaceRange(1u, 1u, extracted));
    ASSERT_TRUE(extracted.isValid());
    EXPECT_EQ(extracted.topology().faceCount(), 1u);
    EXPECT_EQ(extracted.attributes().vertexCount(), 4u);
    EXPECT_EQ(extracted.topology().face(0).indices, (std::vector<uint32_t>{0, 1, 2, 3}));
    ASSERT_TRUE(extracted.hasStableElementIds());
    EXPECT_EQ(extracted.stableElementIds().faceIds.size(), 1u);
    EXPECT_EQ(extracted.stableElementIds().vertexIds.size(), 4u);
    EXPECT_EQ(extracted.stableElementIds().edgeIds.size(), 4u);
}

TEST(Mesh, ExtractFaceRangeRejectsOutOfBoundsRange)
{
    Mesh mesh = makeTriangle(1.f);
    Mesh extracted;

    EXPECT_FALSE(mesh.extractFaceRange(1u, 1u, extracted));
    EXPECT_FALSE(extracted.isValid());
}

TEST(Mesh, SplitFaceRangeSeparatesSelectedFacesAndPreservesStableIdsInBothMeshes)
{
    Mesh mesh = makePlane(1.f, 1.f, 1, 2);
    ASSERT_TRUE(mesh.isValid());
    mesh.rebuildStableElementIds();

    Mesh splitOut;
    ASSERT_TRUE(mesh.splitFaceRange(1u, 1u, splitOut));
    ASSERT_TRUE(mesh.isValid());
    ASSERT_TRUE(splitOut.isValid());
    EXPECT_EQ(mesh.topology().faceCount(), 1u);
    EXPECT_EQ(splitOut.topology().faceCount(), 1u);
    EXPECT_EQ(mesh.attributes().vertexCount(), 4u);
    EXPECT_EQ(splitOut.attributes().vertexCount(), 4u);
    EXPECT_TRUE(mesh.hasStableElementIds());
    EXPECT_TRUE(splitOut.hasStableElementIds());
    EXPECT_EQ(mesh.stableElementIds().faceIds.size(), 1u);
    EXPECT_EQ(splitOut.stableElementIds().faceIds.size(), 1u);
}

TEST(Mesh, SplitFaceRangeRejectsWholeMeshSelection)
{
    Mesh mesh = makeTriangle(1.f);
    Mesh splitOut;

    EXPECT_FALSE(mesh.splitFaceRange(0u, 1u, splitOut));
    EXPECT_TRUE(mesh.isValid());
    EXPECT_FALSE(splitOut.isValid());
}

TEST(Mesh, TriangulateConvertsQuadToTwoTriangles)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    Face q{};
    q.indices = {0, 1, 2, 3};
    mesh.topology().addFace(q);

    EXPECT_EQ(mesh.topology().faceCount(), 1u);
    const size_t after = mesh.topology().triangulate();
    EXPECT_EQ(after, 2u);
    EXPECT_TRUE(mesh.topology().face(0).isTriangle());
    EXPECT_TRUE(mesh.topology().face(1).isTriangle());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Primitive constructors
// ─────────────────────────────────────────────────────────────────────────────

TEST(Mesh, PrimitiveTriangleIsValid)
{
    const Mesh tri = makeTriangle(1.f);
    EXPECT_TRUE(tri.isValid());
    EXPECT_EQ(tri.attributes().vertexCount(), 3u);
    EXPECT_EQ(tri.topology().faceCount(), 1u);
}

TEST(Mesh, PrimitiveBoxHasEightVerticesAndSixFaces)
{
    const Mesh box = makeBox(2.f, 1.f, 3.f);
    EXPECT_TRUE(box.isValid());
    EXPECT_EQ(box.attributes().vertexCount(), 8u);
    EXPECT_EQ(box.topology().faceCount(), 6u);
}

TEST(Mesh, PrimitivePlaneSegmentsCorrectly)
{
    // 2×3 segments → (3 cols)×(4 rows) = 12 vertices, 6 quads.
    const Mesh plane = makePlane(1.f, 1.f, 2, 3);
    EXPECT_TRUE(plane.isValid());
    EXPECT_EQ(plane.attributes().vertexCount(), 12u);  // (2+1)*(3+1)
    EXPECT_EQ(plane.topology().faceCount(), 6u);       // 2*3
    EXPECT_TRUE(plane.attributes().hasUVs());
    EXPECT_EQ(plane.attributes().uvs().size(), 12u);
}

TEST(Mesh, PrimitiveConstructorsRejectNonFiniteDimensions)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();

    EXPECT_FALSE(makeTriangle(nan).isValid());
    EXPECT_FALSE(makeBox(1.f, inf, 1.f).isValid());
    EXPECT_FALSE(makePlane(1.f, nan, 2u, 3u).isValid());
    EXPECT_FALSE(makeSphere(inf, 8u, 12u).isValid());
    EXPECT_FALSE(makeCylinder(1.f, nan, 12u).isValid());
    EXPECT_FALSE(makeCone(nan, 2.f, 12u).isValid());
    EXPECT_FALSE(makeCapsule(1.f, inf, 12u, 6u).isValid());
    EXPECT_FALSE(makeTorus(inf, 0.5f, 24u, 12u).isValid());
    EXPECT_FALSE(makeTorus(2.f, nan, 24u, 12u).isValid());
}

TEST(Mesh, PrimitiveTorusHasExpectedTopologyAndBounds)
{
    const float R = 2.f, r = 0.5f;
    const uint32_t majorSegs = 24u, minorSegs = 12u;
    const Mesh torus = makeTorus(R, r, majorSegs, minorSegs);
    EXPECT_TRUE(torus.isValid());
    EXPECT_EQ(torus.attributes().vertexCount(), majorSegs * minorSegs); // 288
    EXPECT_EQ(torus.topology().faceCount(), majorSegs * minorSegs);     // 288 quads
    EXPECT_TRUE(torus.attributes().hasUVs());

    // Every vertex lies on the torus surface: |y| <= r, and the XZ distance from the
    // axis is within [R - r, R + r].
    for (const auto& p : torus.attributes().positions()) {
        EXPECT_LE(std::fabs(p.y), r + 1e-4f);
        const float xz = std::sqrt(p.x * p.x + p.z * p.z);
        EXPECT_GE(xz, R - r - 1e-4f);
        EXPECT_LE(xz, R + r + 1e-4f);
    }
}

TEST(Mesh, PrimitiveTorusClampsLowSegmentCounts)
{
    const Mesh torus = makeTorus(1.f, 0.25f, 1u, 1u); // clamped to 3x3
    EXPECT_TRUE(torus.isValid());
    EXPECT_EQ(torus.attributes().vertexCount(), 9u);
    EXPECT_EQ(torus.topology().faceCount(), 9u);
}

TEST(Mesh, ComputeBoundsOfBoxIsCenteredHalfExtents)
{
    const auto b = makeBox(2.f, 4.f, 6.f).computeBounds();
    EXPECT_FLOAT_EQ(b.min.x, -1.f); EXPECT_FLOAT_EQ(b.max.x, 1.f);
    EXPECT_FLOAT_EQ(b.min.y, -2.f); EXPECT_FLOAT_EQ(b.max.y, 2.f);
    EXPECT_FLOAT_EQ(b.min.z, -3.f); EXPECT_FLOAT_EQ(b.max.z, 3.f);
}

TEST(Mesh, ComputeBoundsOfTorusEnclosesTube)
{
    const float R = 2.f, r = 0.5f;
    const auto b = makeTorus(R, r, 24u, 12u).computeBounds();
    EXPECT_NEAR(b.min.x, -(R + r), 1e-4f); EXPECT_NEAR(b.max.x, R + r, 1e-4f);
    EXPECT_NEAR(b.min.z, -(R + r), 1e-4f); EXPECT_NEAR(b.max.z, R + r, 1e-4f);
    EXPECT_NEAR(b.min.y, -r, 1e-4f);       EXPECT_NEAR(b.max.y, r, 1e-4f);
}

TEST(Mesh, ComputeBoundsEmptyMeshIsZeroBox)
{
    Mesh mesh;
    const auto b = mesh.computeBounds();
    EXPECT_EQ(b.min, b.max); // min == max is the "no bounds" sentinel
    EXPECT_FLOAT_EQ(b.min.x, 0.f);
    EXPECT_FLOAT_EQ(b.max.z, 0.f);
}

TEST(Mesh, ComputeBoundsRejectsNonFinitePositions)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {std::numeric_limits<float>::infinity(), 1.f, 1.f},
    });
    const auto b = mesh.computeBounds();
    EXPECT_EQ(b.min, b.max); // zero box on non-finite input
}

TEST(Mesh, SkinningStreamsMustMatchVertexCount)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);

    // Only 2 skinning entries for 3 vertices -> inconsistent.
    mesh.attributes().setSkinning(
        {
            JointIndex4{0, 0, 0, 0},
            JointIndex4{1, 0, 0, 0},
        },
        {
            JointWeight4{1.f, 0.f, 0.f, 0.f},
            JointWeight4{1.f, 0.f, 0.f, 0.f},
        });

    EXPECT_FALSE(mesh.isValid());
}

TEST(Mesh, SkinningWeightsNormalizePerVertex)
{
    MeshAttributes attrs;
    attrs.setPositions({
        {0.f, 0.f, 0.f},
    });
    attrs.setSkinning(
        {
            JointIndex4{1, 2, 3, 4},
        },
        {
            JointWeight4{2.f, 1.f, 1.f, 0.f},
        });

    attrs.normalizeSkinningWeights();
    const auto& w = attrs.jointWeights().at(0);
    const float sum = w[0] + w[1] + w[2] + w[3];

    EXPECT_NEAR(sum, 1.f, 1e-5f);
    EXPECT_NEAR(w[0], 0.5f, 1e-5f);
    EXPECT_NEAR(w[1], 0.25f, 1e-5f);
    EXPECT_NEAR(w[2], 0.25f, 1e-5f);
    EXPECT_NEAR(w[3], 0.f, 1e-5f);
}

TEST(Mesh, SkinningPackProducesDeterministicFlattenedStreams)
{
    MeshAttributes attrs;
    attrs.setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
    });
    attrs.setSkinning(
        {
            JointIndex4{1, 2, 3, 4},
            JointIndex4{5, 6, 7, 8},
        },
        {
            JointWeight4{0.4f, 0.3f, 0.2f, 0.1f},
            JointWeight4{1.f, 0.f, 0.f, 0.f},
        });

    const PackedSkinningStreams packed = attrs.packSkinningStreams();
    EXPECT_EQ(packed.influencesPerVertex, 4u);
    ASSERT_EQ(packed.jointIndices.size(), 8u);
    ASSERT_EQ(packed.jointWeights.size(), 8u);

    EXPECT_EQ(packed.jointIndices[0], 1u);
    EXPECT_EQ(packed.jointIndices[3], 4u);
    EXPECT_EQ(packed.jointIndices[4], 5u);
    EXPECT_EQ(packed.jointIndices[7], 8u);
    EXPECT_NEAR(packed.jointWeights[0], 0.4f, 1e-5f);
    EXPECT_NEAR(packed.jointWeights[4], 1.f, 1e-5f);
}

TEST(Mesh, SkinnableForSkeletonRejectsOutOfRangeJointIndex)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{};
    f.indices = {0, 1, 2};
    mesh.topology().addFace(f);
    mesh.attributes().setSkinning(
        {
            JointIndex4{0, 1, 2, 3},
            JointIndex4{0, 1, 2, 3},
            JointIndex4{0, 1, 2, 99},
        },
        {
            JointWeight4{1.f, 0.f, 0.f, 0.f},
            JointWeight4{1.f, 0.f, 0.f, 0.f},
            JointWeight4{1.f, 0.f, 0.f, 0.f},
        });

    EXPECT_FALSE(mesh.isSkinnableForSkeleton(64));
    EXPECT_TRUE(mesh.isSkinnableForSkeleton(100));
}
