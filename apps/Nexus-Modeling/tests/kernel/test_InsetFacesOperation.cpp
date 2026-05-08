#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/InsetFacesOperation.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

// ─────────────────────────────────────────────────────────────────────────────

TEST(InsetFacesOperation, RejectsInvalidMesh)
{
    Mesh empty;
    InsetDesc desc{};
    Mesh out;
    const InsetReport report = InsetFacesOperation::applyToAllFaces(empty, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, InsetDiagnostic::InvalidInputMesh));
}

TEST(InsetFacesOperation, RejectsNonFiniteAmount)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    InsetDesc desc{};
    desc.amount = std::numeric_limits<float>::quiet_NaN();

    Mesh out;
    const InsetReport report = InsetFacesOperation::applyToAllFaces(box, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, InsetDiagnostic::InvalidAmount));
}

TEST(InsetFacesOperation, RejectsNegativeDistance)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    InsetDesc desc{};
    desc.mode   = InsetMode::Distance;
    desc.amount = -0.1f;

    Mesh out;
    const InsetReport report = InsetFacesOperation::applyToAllFaces(box, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, InsetDiagnostic::InvalidAmount));
}

TEST(InsetFacesOperation, FactorModeAddsBorderAndInnerFaces)
{
    // Box: 6 quads. Each inset produces 1 inner quad + 4 border quads = 5 new faces.
    // With replaceOriginal = true: no originals kept → 6 * 5 = 30 total.
    Mesh box = makeBox(1.f, 1.f, 1.f);
    const size_t origFaces = box.topology().faceCount();

    InsetDesc desc{};
    desc.amount          = 0.2f;
    desc.mode            = InsetMode::Factor;
    desc.replaceOriginal = true;

    Mesh out;
    const InsetReport report = InsetFacesOperation::applyToAllFaces(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_EQ(report.insetFaceCount, origFaces);
    EXPECT_GT(out.topology().faceCount(), origFaces);
    EXPECT_GT(out.attributes().vertexCount(), box.attributes().vertexCount());
    EXPECT_TRUE(out.isValid());
}

TEST(InsetFacesOperation, KeepOriginalFacesAddsMore)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    const size_t origFaces = box.topology().faceCount();

    InsetDesc descReplace{};
    descReplace.amount          = 0.2f;
    descReplace.replaceOriginal = true;

    InsetDesc descKeep{};
    descKeep.amount          = 0.2f;
    descKeep.replaceOriginal = false;

    Mesh outReplace, outKeep;
    InsetFacesOperation::applyToAllFaces(box, descReplace, outReplace);
    InsetFacesOperation::applyToAllFaces(box, descKeep,    outKeep);

    EXPECT_EQ(outKeep.topology().faceCount(),
              outReplace.topology().faceCount() + origFaces);
}

TEST(InsetFacesOperation, ZeroFactorProducesNoInset)
{
    // factor = 0 → inner ring at original vertex positions → zero-area inner faces
    // but the operation should complete successfully
    Mesh plane = makePlane(1.f, 1.f, 1, 1);

    InsetDesc desc{};
    desc.amount          = 0.f;
    desc.mode            = InsetMode::Factor;
    desc.replaceOriginal = true;

    Mesh out;
    const InsetReport report = InsetFacesOperation::applyToAllFaces(plane, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_GT(out.topology().faceCount(), 0u);
}

TEST(InsetFacesOperation, DistanceModeProducesValidOutput)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);

    InsetDesc desc{};
    desc.amount          = 0.1f;
    desc.mode            = InsetMode::Distance;
    desc.replaceOriginal = true;

    Mesh out;
    const InsetReport report = InsetFacesOperation::applyToAllFaces(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(out.isValid());
}

TEST(InsetFacesOperation, OutputTopologyIsValid)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);

    InsetDesc desc{};
    desc.amount          = 0.25f;
    desc.mode            = InsetMode::Factor;
    desc.replaceOriginal = true;

    Mesh out;
    const InsetReport report = InsetFacesOperation::applyToAllFaces(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(out.isValid());

    const TopologyValidationReport topo =
        TopologyUtilities::validateTopology(out.topology(), out.attributes().vertexCount());
    EXPECT_TRUE(topo.valid);
}

TEST(InsetFacesOperation, DeterministicAcrossRepeatedRuns)
{
    Mesh a = makeBox(1.f, 1.f, 1.f);
    Mesh b = makeBox(1.f, 1.f, 1.f);

    InsetDesc desc{};
    desc.amount          = 0.3f;
    desc.mode            = InsetMode::Factor;
    desc.replaceOriginal = true;

    Mesh outA, outB;
    const InsetReport repA = InsetFacesOperation::applyToAllFaces(a, desc, outA);
    const InsetReport repB = InsetFacesOperation::applyToAllFaces(b, desc, outB);

    ASSERT_TRUE(repA.valid);
    ASSERT_TRUE(repB.valid);
    EXPECT_EQ(outA.topology().faceCount(),     outB.topology().faceCount());
    EXPECT_EQ(outA.attributes().vertexCount(), outB.attributes().vertexCount());
    EXPECT_EQ(repA.insetFaceCount,             repB.insetFaceCount);
    EXPECT_EQ(repA.addedFaceCount,             repB.addedFaceCount);

    const auto& pA = outA.attributes().positions();
    const auto& pB = outB.attributes().positions();
    ASSERT_EQ(pA.size(), pB.size());
    for (size_t i = 0; i < pA.size(); ++i) {
        EXPECT_FLOAT_EQ(pA[i].x, pB[i].x);
        EXPECT_FLOAT_EQ(pA[i].y, pB[i].y);
        EXPECT_FLOAT_EQ(pA[i].z, pB[i].z);
    }
}

TEST(InsetFacesOperation, InnerVerticesAreCloserToCentroid)
{
    // For a single quad face, inner vertices should be closer to the centroid
    // than the original vertices.
    Mesh mesh;
    mesh.attributes().setPositions({
        {-1.f, 0.f, -1.f},
        { 1.f, 0.f, -1.f},
        { 1.f, 0.f,  1.f},
        {-1.f, 0.f,  1.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u, 3u}});

    InsetDesc desc{};
    desc.amount          = 0.5f;
    desc.mode            = InsetMode::Factor;
    desc.replaceOriginal = true;
    desc.recomputeNormals = false;

    Mesh out;
    const InsetReport report = InsetFacesOperation::applyToAllFaces(mesh, desc, out);
    ASSERT_TRUE(report.valid);

    // Centroid of original quad is (0, 0, 0)
    // Original vertices are at distance sqrt(2) ≈ 1.414 from centroid
    // Inner vertices at factor 0.5 should be at distance 0.5 * sqrt(2) ≈ 0.707
    const auto& pos = out.attributes().positions();
    // Inner ring vertices are the last 4 added (indices 4..7)
    ASSERT_EQ(pos.size(), 8u);
    for (size_t i = 4; i < 8; ++i) {
        const float dist = std::sqrt(pos[i].x * pos[i].x + pos[i].z * pos[i].z);
        EXPECT_LT(dist, 1.0f);  // closer to centroid than original (~1.414)
    }
}

} // namespace nexus::geometry::testing
