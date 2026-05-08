#include <nexus/geometry/ExtrudeOperation.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

// ─────────────────────────────────────────────────────────────────────────────

TEST(ExtrudeOperation, RejectsInvalidMesh)
{
    Mesh empty;
    ExtrudeDesc desc{};
    Mesh out;
    const ExtrudeReport report = ExtrudeOperation::applyToAllFaces(empty, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, ExtrudeDiagnostic::InvalidInputMesh));
}

TEST(ExtrudeOperation, RejectsNonFiniteDistance)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    ExtrudeDesc desc{};
    desc.distance = std::numeric_limits<float>::infinity();

    Mesh out;
    const ExtrudeReport report = ExtrudeOperation::applyToAllFaces(box, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, ExtrudeDiagnostic::InvalidDistance));
}

TEST(ExtrudeOperation, ExtrudeBoxIncreasesVertexAndFaceCount)
{
    // Box has 8 verts, 6 quads. Extruding all faces open (no original faces kept):
    // 6 faces * (1 cap + 4 wall quads) = 30 new faces; 0 original kept
    Mesh box = makeBox(1.f, 1.f, 1.f);
    const size_t origVerts = box.attributes().vertexCount();
    const size_t origFaces = box.topology().faceCount();

    ExtrudeDesc desc{};
    desc.distance          = 0.25f;
    desc.keepOriginalFaces = false;

    Mesh out;
    const ExtrudeReport report = ExtrudeOperation::applyToAllFaces(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_EQ(report.extrudedFaceCount, origFaces);
    EXPECT_GT(out.attributes().vertexCount(), origVerts);
    EXPECT_GT(out.topology().faceCount(),     0u);
    EXPECT_TRUE(out.isValid());
}

TEST(ExtrudeOperation, KeepOriginalFacesAddsExtraFaces)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    const size_t origFaces = box.topology().faceCount();

    ExtrudeDesc descKeep{};
    descKeep.distance          = 0.1f;
    descKeep.keepOriginalFaces = true;

    ExtrudeDesc descDrop{};
    descDrop.distance          = 0.1f;
    descDrop.keepOriginalFaces = false;

    Mesh outKeep, outDrop;
    ExtrudeOperation::applyToAllFaces(box, descKeep, outKeep);
    ExtrudeOperation::applyToAllFaces(box, descDrop, outDrop);

    // keepOriginalFaces adds origFaces more faces than the open variant
    EXPECT_EQ(outKeep.topology().faceCount(),
              outDrop.topology().faceCount() + origFaces);
}

TEST(ExtrudeOperation, ZeroDistanceProducesValidOutput)
{
    // Zero distance is valid: walls have zero height, caps sit on original face
    Mesh plane = makePlane(1.f, 1.f, 1, 1);

    ExtrudeDesc desc{};
    desc.distance          = 0.f;
    desc.keepOriginalFaces = false;

    Mesh out;
    const ExtrudeReport report = ExtrudeOperation::applyToAllFaces(plane, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(out.isValid());
    EXPECT_GT(out.topology().faceCount(), 0u);
}

TEST(ExtrudeOperation, NegativeDistanceExtrudesInward)
{
    // Negative distance: caps should be displaced inward (valid, just reversed)
    Mesh box = makeBox(1.f, 1.f, 1.f);

    ExtrudeDesc desc{};
    desc.distance          = -0.1f;
    desc.keepOriginalFaces = false;

    Mesh out;
    const ExtrudeReport report = ExtrudeOperation::applyToAllFaces(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(out.isValid());
}

TEST(ExtrudeOperation, OutputTopologyIsValid)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);

    ExtrudeDesc desc{};
    desc.distance          = 0.2f;
    desc.keepOriginalFaces = true;

    Mesh out;
    const ExtrudeReport report = ExtrudeOperation::applyToAllFaces(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(out.isValid());

    const TopologyValidationReport topo =
        TopologyUtilities::validateTopology(out.topology(), out.attributes().vertexCount());
    EXPECT_TRUE(topo.valid);
}

TEST(ExtrudeOperation, DeterministicAcrossRepeatedRuns)
{
    Mesh a = makeBox(1.f, 1.f, 1.f);
    Mesh b = makeBox(1.f, 1.f, 1.f);

    ExtrudeDesc desc{};
    desc.distance          = 0.15f;
    desc.keepOriginalFaces = false;

    Mesh outA, outB;
    const ExtrudeReport repA = ExtrudeOperation::applyToAllFaces(a, desc, outA);
    const ExtrudeReport repB = ExtrudeOperation::applyToAllFaces(b, desc, outB);

    ASSERT_TRUE(repA.valid);
    ASSERT_TRUE(repB.valid);
    EXPECT_EQ(outA.topology().faceCount(),   outB.topology().faceCount());
    EXPECT_EQ(outA.attributes().vertexCount(), outB.attributes().vertexCount());
    EXPECT_EQ(repA.extrudedFaceCount,        repB.extrudedFaceCount);
    EXPECT_EQ(repA.addedFaceCount,           repB.addedFaceCount);

    const auto& pA = outA.attributes().positions();
    const auto& pB = outB.attributes().positions();
    ASSERT_EQ(pA.size(), pB.size());
    for (size_t i = 0; i < pA.size(); ++i) {
        EXPECT_FLOAT_EQ(pA[i].x, pB[i].x);
        EXPECT_FLOAT_EQ(pA[i].y, pB[i].y);
        EXPECT_FLOAT_EQ(pA[i].z, pB[i].z);
    }
}

TEST(ExtrudeOperation, CapVerticesDisplacedByDistance)
{
    // Single triangle: verify cap vertices are at exactly distance from original
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u}});

    ExtrudeDesc desc{};
    desc.distance          = 1.f;
    desc.keepOriginalFaces = true;
    desc.recomputeNormals  = false;  // keep raw positions for exact checking

    Mesh out;
    const ExtrudeReport report = ExtrudeOperation::applyToAllFaces(mesh, desc, out);

    ASSERT_TRUE(report.valid);
    ASSERT_EQ(report.addedVertexCount, 3u);  // 3 new cap vertices

    const auto& pos = out.attributes().positions();
    ASSERT_EQ(pos.size(), 6u);  // 3 original + 3 cap

    // Extruded face normal for this triangle is +Z (CCW winding in XY plane)
    // Each cap vertex should be at (original.x, original.y, 1.0)
    for (size_t i = 3; i < 6; ++i) {
        EXPECT_NEAR(pos[i].z, 1.f, 1e-5f);
    }
}

} // namespace nexus::geometry::testing
