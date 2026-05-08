#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <algorithm>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

namespace {

float displacementMagnitudeSq(const nexus::render::Vec3& a,
                              const nexus::render::Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

TEST(BevelChamfer, RejectsInvalidDistance)
{
    Mesh box = makeBox(1.0f, 1.0f, 1.0f);

    BevelChamferDesc desc{};
    desc.distance = 0.0f;

    Mesh out;
    const BevelChamferReport report = BevelChamferOperation::apply(box, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, BevelChamferDiagnostic::InvalidDistance));
}

TEST(BevelChamfer, RejectsInvalidSharpAngle)
{
    Mesh box = makeBox(1.0f, 1.0f, 1.0f);

    BevelChamferDesc desc{};
    desc.sharpAngleDegrees = 180.0f;

    Mesh out;
    const BevelChamferReport report = BevelChamferOperation::apply(box, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, BevelChamferDiagnostic::InvalidSharpAngle));
}

TEST(BevelChamfer, BevelMovesVerticesDeterministically)
{
    Mesh box = makeBox(2.0f, 2.0f, 2.0f);
    const auto originalPositions = box.attributes().positions();

    BevelChamferDesc desc{};
    desc.mode = BevelChamferMode::Bevel;
    desc.distance = 0.1f;
    desc.sharpAngleDegrees = 40.0f;

    Mesh out;
    const BevelChamferReport report = BevelChamferOperation::apply(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_GT(report.sharpEdgeCount, 0u);
    EXPECT_GT(report.splitEdgeCount, 0u);
    EXPECT_GT(report.supportFaceCount, 0u);
    EXPECT_GT(report.movedVertexCount, 0u);
    EXPECT_GT(out.topology().faceCount(), box.topology().faceCount());
    EXPECT_GT(out.attributes().vertexCount(), box.attributes().vertexCount());

    const auto& newPositions = out.attributes().positions();
    uint32_t changedAppended = 0;
    for (size_t i = originalPositions.size(); i < newPositions.size(); ++i) {
        const auto& p = newPositions[i];
        bool matchesAnyInput = false;
        for (const auto& in : originalPositions) {
            if (displacementMagnitudeSq(in, p) <= 1e-10f) {
                matchesAnyInput = true;
                break;
            }
        }
        if (!matchesAnyInput) {
            ++changedAppended;
        }
    }
    EXPECT_GT(changedAppended, 0u);
}

TEST(BevelChamfer, ChamferMovesLessThanBevelForSameDistance)
{
    Mesh box = makeBox(2.0f, 2.0f, 2.0f);

    BevelChamferDesc bevelDesc{};
    bevelDesc.mode = BevelChamferMode::Bevel;
    bevelDesc.distance = 0.1f;
    bevelDesc.sharpAngleDegrees = 40.0f;

    BevelChamferDesc chamferDesc = bevelDesc;
    chamferDesc.mode = BevelChamferMode::Chamfer;

    Mesh bevelOut;
    Mesh chamferOut;

    const BevelChamferReport bevelReport = BevelChamferOperation::apply(box, bevelDesc, bevelOut);
    const BevelChamferReport chamferReport = BevelChamferOperation::apply(box, chamferDesc, chamferOut);

    ASSERT_TRUE(bevelReport.valid);
    ASSERT_TRUE(chamferReport.valid);
    ASSERT_EQ(bevelOut.attributes().vertexCount(), chamferOut.attributes().vertexCount());
    ASSERT_EQ(bevelOut.topology().faceCount(), chamferOut.topology().faceCount());

    float maxBevelRadiusSq = 0.f;
    float maxChamferRadiusSq = 0.f;
    const auto& bevelPos = bevelOut.attributes().positions();
    const auto& chamferPos = chamferOut.attributes().positions();
    for (size_t i = 0; i < bevelPos.size(); ++i) {
        const float br = bevelPos[i].x * bevelPos[i].x
                       + bevelPos[i].y * bevelPos[i].y
                       + bevelPos[i].z * bevelPos[i].z;
        const float cr = chamferPos[i].x * chamferPos[i].x
                       + chamferPos[i].y * chamferPos[i].y
                       + chamferPos[i].z * chamferPos[i].z;
        maxBevelRadiusSq = std::max(maxBevelRadiusSq, br);
        maxChamferRadiusSq = std::max(maxChamferRadiusSq, cr);
    }

    EXPECT_GT(maxBevelRadiusSq, maxChamferRadiusSq);
}

TEST(BevelChamfer, BoundaryToggleControlsSingleTriangleBehavior)
{
    Mesh tri = makeTriangle(1.0f);

    BevelChamferDesc disabled{};
    disabled.includeBoundaryEdges = false;
    disabled.distance = 0.1f;

    Mesh outDisabled;
    const BevelChamferReport disabledReport = BevelChamferOperation::apply(tri, disabled, outDisabled);

    ASSERT_TRUE(disabledReport.valid);
    EXPECT_TRUE(hasDiagnostic(disabledReport.diagnostic,
                              BevelChamferDiagnostic::NoSharpEdgesDetected));
    EXPECT_EQ(outDisabled.topology().faceCount(), tri.topology().faceCount());

    BevelChamferDesc enabled = disabled;
    enabled.includeBoundaryEdges = true;

    Mesh outEnabled;
    const BevelChamferReport enabledReport = BevelChamferOperation::apply(tri, enabled, outEnabled);

    ASSERT_TRUE(enabledReport.valid);
    EXPECT_GT(enabledReport.movedVertexCount, 0u);
    EXPECT_GT(enabledReport.splitEdgeCount, 0u);
    EXPECT_GT(enabledReport.supportFaceCount, 0u);
    EXPECT_GT(outEnabled.topology().faceCount(), tri.topology().faceCount());
}

TEST(BevelChamfer, DeterministicAcrossRepeatedRuns)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(2.0f, 2.0f, 2.0f);

    BevelChamferDesc desc{};
    desc.mode = BevelChamferMode::Bevel;
    desc.distance = 0.075f;
    desc.sharpAngleDegrees = 35.0f;

    Mesh outA;
    Mesh outB;

    const BevelChamferReport reportA = BevelChamferOperation::apply(boxA, desc, outA);
    const BevelChamferReport reportB = BevelChamferOperation::apply(boxB, desc, outB);

    ASSERT_TRUE(reportA.valid);
    ASSERT_TRUE(reportB.valid);
    ASSERT_EQ(outA.attributes().vertexCount(), outB.attributes().vertexCount());
    ASSERT_EQ(outA.topology().faceCount(), outB.topology().faceCount());

    const auto& posA = outA.attributes().positions();
    const auto& posB = outB.attributes().positions();
    for (size_t i = 0; i < posA.size(); ++i) {
        EXPECT_FLOAT_EQ(posA[i].x, posB[i].x);
        EXPECT_FLOAT_EQ(posA[i].y, posB[i].y);
        EXPECT_FLOAT_EQ(posA[i].z, posB[i].z);
    }

    ASSERT_TRUE(outA.hasStableElementIds());
    ASSERT_TRUE(outB.hasStableElementIds());
    EXPECT_EQ(outA.stableElementIds().vertexIds, outB.stableElementIds().vertexIds);
    EXPECT_EQ(outA.stableElementIds().faceIds, outB.stableElementIds().faceIds);
    EXPECT_EQ(outA.stableElementIds().edgeIds, outB.stableElementIds().edgeIds);
}

TEST(BevelChamfer, TopologyOutputRemainsManifoldForClosedBoxCase)
{
    Mesh box = makeBox(2.0f, 2.0f, 2.0f);

    BevelChamferDesc desc{};
    desc.mode = BevelChamferMode::Bevel;
    desc.distance = 0.05f;
    desc.sharpAngleDegrees = 35.0f;
    desc.includeBoundaryEdges = false;

    Mesh out;
    const BevelChamferReport report = BevelChamferOperation::apply(box, desc, out);
    ASSERT_TRUE(report.valid);

    const TopologyValidationReport topoReport =
        TopologyUtilities::validateTopology(out.topology(), out.attributes().vertexCount());

    EXPECT_TRUE(topoReport.valid);
    EXPECT_EQ(topoReport.nonManifoldEdgeCount, 0u);
    EXPECT_FALSE(hasDiagnostic(report.diagnostic, BevelChamferDiagnostic::OutputNonManifold));
}

TEST(BevelChamfer, DegeneracyRegressionWithTinyDistanceProducesValidTopology)
{
    Mesh box = makeBox(2.0f, 2.0f, 2.0f);

    BevelChamferDesc desc{};
    desc.mode = BevelChamferMode::Chamfer;
    desc.distance = 1e-4f;
    desc.sharpAngleDegrees = 20.0f;

    Mesh out;
    const BevelChamferReport report = BevelChamferOperation::apply(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(out.isValid());
    EXPECT_FALSE(hasDiagnostic(report.diagnostic, BevelChamferDiagnostic::GeneratedDegenerateFace));

    const TopologyValidationReport topoReport =
        TopologyUtilities::validateTopology(out.topology(), out.attributes().vertexCount());
    EXPECT_TRUE(topoReport.valid);
}

} // namespace nexus::geometry::testing
