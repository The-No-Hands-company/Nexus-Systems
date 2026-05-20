#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/RemeshOperation.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

namespace {

float maxEdgeLength(const Mesh& mesh)
{
    const auto& positions = mesh.attributes().positions();
    float maxLen = 0.f;

    for (size_t fi = 0; fi < mesh.topology().faceCount(); ++fi) {
        const Face& f = mesh.topology().face(fi);
        if (f.indices.size() < 3u) {
            continue;
        }

        for (size_t i = 0; i < f.indices.size(); ++i) {
            const auto& a = positions[f.indices[i]];
            const auto& b = positions[f.indices[(i + 1) % f.indices.size()]];
            const float dx = a.x - b.x;
            const float dy = a.y - b.y;
            const float dz = a.z - b.z;
            maxLen = std::max(maxLen, std::sqrt(dx * dx + dy * dy + dz * dz));
        }
    }

    return maxLen;
}

} // namespace

TEST(RemeshOperation, RejectsInvalidTargetEdgeLength)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);

    RemeshDesc desc{};
    desc.targetEdgeLength = 0.f;

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(box, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, RemeshDiagnostic::InvalidTargetEdgeLength));
}

TEST(RemeshOperation, RejectsNonFiniteTargetEdgeLength)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);

    RemeshDesc desc{};
    desc.targetEdgeLength = std::numeric_limits<float>::quiet_NaN();

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(box, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, RemeshDiagnostic::InvalidTargetEdgeLength));
}

TEST(RemeshOperation, RejectsNonFiniteSplitThresholdMultiplier)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);

    RemeshDesc desc{};
    desc.targetEdgeLength = 1.f;
    desc.splitThresholdMultiplier = std::numeric_limits<float>::infinity();

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(box, desc, out);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, RemeshDiagnostic::InvalidTargetEdgeLength));
}

TEST(RemeshOperation, TriangulatesAndSplitsForShortTarget)
{
    Mesh plane = makePlane(1.f, 1.f, 1, 1);

    RemeshDesc desc{};
    desc.targetEdgeLength = 0.45f;
    desc.maxIterations = 3;

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(plane, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, RemeshDiagnostic::InputTriangulated));
    EXPECT_GT(report.splitCount, 0u);
    EXPECT_GT(out.topology().faceCount(), plane.topology().faceCount());
    EXPECT_GT(out.attributes().vertexCount(), plane.attributes().vertexCount());
    EXPECT_LE(maxEdgeLength(out), desc.targetEdgeLength * desc.splitThresholdMultiplier + 1e-4f);
}

TEST(RemeshOperation, NoChangeWhenTargetIsLarge)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);

    RemeshDesc desc{};
    desc.targetEdgeLength = 10.f;
    desc.maxIterations = 2;

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, RemeshDiagnostic::NoChangesApplied));
    EXPECT_EQ(report.splitCount, 0u);
    EXPECT_EQ(out.topology().faceCount(), 12u);
}

TEST(RemeshOperation, DeterministicAcrossRepeatedRuns)
{
    Mesh a = makePlane(1.f, 1.f, 2, 2);
    Mesh b = makePlane(1.f, 1.f, 2, 2);

    RemeshDesc desc{};
    desc.targetEdgeLength = 0.3f;
    desc.maxIterations = 2;

    Mesh outA;
    Mesh outB;
    const RemeshReport reportA = RemeshOperation::apply(a, desc, outA);
    const RemeshReport reportB = RemeshOperation::apply(b, desc, outB);

    ASSERT_TRUE(reportA.valid);
    ASSERT_TRUE(reportB.valid);
    ASSERT_EQ(outA.topology().faceCount(), outB.topology().faceCount());
    ASSERT_EQ(outA.attributes().vertexCount(), outB.attributes().vertexCount());

    const auto& pA = outA.attributes().positions();
    const auto& pB = outB.attributes().positions();
    for (size_t i = 0; i < pA.size(); ++i) {
        EXPECT_FLOAT_EQ(pA[i].x, pB[i].x);
        EXPECT_FLOAT_EQ(pA[i].y, pB[i].y);
        EXPECT_FLOAT_EQ(pA[i].z, pB[i].z);
    }

    ASSERT_TRUE(outA.hasStableElementIds());
    ASSERT_TRUE(outB.hasStableElementIds());
    EXPECT_EQ(outA.stableElementIds().vertexIds, outB.stableElementIds().vertexIds);
    EXPECT_EQ(outA.stableElementIds().faceIds, outB.stableElementIds().faceIds);
    EXPECT_EQ(outA.stableElementIds().edgeIds, outB.stableElementIds().edgeIds);
}

TEST(RemeshOperation, OutputTopologyStaysValid)
{
    Mesh box = makeBox(2.f, 1.f, 1.f);

    RemeshDesc desc{};
    desc.targetEdgeLength = 0.5f;
    desc.maxIterations = 2;

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(out.isValid());

    const TopologyValidationReport topo =
        TopologyUtilities::validateTopology(out.topology(), out.attributes().vertexCount());
    EXPECT_TRUE(topo.valid);

    for (size_t fi = 0; fi < out.topology().faceCount(); ++fi) {
        EXPECT_EQ(out.topology().face(fi).indices.size(), 3u);
    }
}

// ─── Edge-collapse pass ───────────────────────────────────────────────────────

TEST(RemeshOperation, CollapseIsNoOpWhenDisabled)
{
    // collapseEdgesBelow defaults to 0 → no collapses, same as before
    Mesh box = makeBox(1.f, 1.f, 1.f);
    RemeshDesc desc{};
    desc.targetEdgeLength = 10.f;  // no splits either
    desc.maxIterations = 1;
    desc.collapseEdgesBelow = 0.f;

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(box, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_EQ(report.splitCount,   0u);
    EXPECT_EQ(report.collapseCount, 0u);
}

TEST(RemeshOperation, CollapseReducesFaceCountForHighlySubdividedMesh)
{
    // Split a plane down to very small edges, then collapse to recover some faces
    Mesh plane = makePlane(1.f, 1.f, 1, 1);

    RemeshDesc descSplit{};
    descSplit.targetEdgeLength    = 0.1f;
    descSplit.maxIterations       = 4;
    descSplit.collapseEdgesBelow  = 0.f;

    Mesh split;
    const RemeshReport splitReport = RemeshOperation::apply(plane, descSplit, split);
    ASSERT_TRUE(splitReport.valid);
    const size_t splitFaceCount = split.topology().faceCount();

    // Now run with collapse threshold that covers the post-split edge range.
    // After 4 split iterations on a 1x1 plane, edges are ~0.25-0.35.
    // Use collapseEdgesBelow=3.5 → threshold = 3.5 * 0.1 = 0.35, catching those edges.
    RemeshDesc descCollapse{};
    descCollapse.targetEdgeLength         = 0.1f;
    descCollapse.maxIterations            = 1;
    descCollapse.splitThresholdMultiplier = 100.f; // suppress splits
    descCollapse.collapseEdgesBelow       = 3.5f;  // collapse edges < 0.35
    descCollapse.maxCollapseIterations    = 3;

    Mesh collapsed;
    const RemeshReport collapseReport = RemeshOperation::apply(split, descCollapse, collapsed);
    ASSERT_TRUE(collapseReport.valid);
    EXPECT_GT(collapseReport.collapseCount, 0u);
    EXPECT_LT(collapsed.topology().faceCount(), splitFaceCount);
}

TEST(RemeshOperation, CollapseOutputIsValidTopology)
{
    Mesh plane = makePlane(1.f, 1.f, 2, 2);

    RemeshDesc desc{};
    desc.targetEdgeLength     = 0.5f;
    desc.maxIterations        = 2;
    desc.collapseEdgesBelow   = 0.5f;
    desc.maxCollapseIterations = 2;

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(plane, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(out.isValid());

    // Every face must be a proper triangle (3 unique indices)
    for (size_t fi = 0; fi < out.topology().faceCount(); ++fi) {
        const Face& f = out.topology().face(fi);
        ASSERT_EQ(f.indices.size(), 3u);
        EXPECT_NE(f.indices[0], f.indices[1]);
        EXPECT_NE(f.indices[1], f.indices[2]);
        EXPECT_NE(f.indices[0], f.indices[2]);
    }
}

TEST(RemeshOperation, CollapseDeterministicAcrossRuns)
{
    Mesh a = makePlane(1.f, 1.f, 2, 2);
    Mesh b = makePlane(1.f, 1.f, 2, 2);

    RemeshDesc desc{};
    desc.targetEdgeLength     = 0.3f;
    desc.maxIterations        = 3;
    desc.collapseEdgesBelow   = 0.6f;
    desc.maxCollapseIterations = 2;

    Mesh outA, outB;
    const RemeshReport reportA = RemeshOperation::apply(a, desc, outA);
    const RemeshReport reportB = RemeshOperation::apply(b, desc, outB);

    ASSERT_TRUE(reportA.valid);
    ASSERT_TRUE(reportB.valid);
    EXPECT_EQ(reportA.collapseCount, reportB.collapseCount);
    EXPECT_EQ(outA.topology().faceCount(), outB.topology().faceCount());
}

TEST(RemeshOperation, UploadContractMultipleWarningsAreLexicographicallySorted)
{
    // Non-triangulated input plane will trigger InputTriangulated.
    // Large target edge length will trigger NoChangesApplied (no edges subdivided).
    // The messages vector must be lexicographically sorted.
    Mesh plane = makePlane(1.f, 1.f, 2, 2);

    RemeshDesc desc{};
    desc.targetEdgeLength = 100.f;  // Large target — no changes applied
    desc.maxIterations = 2;
    desc.recomputeNormals = false;

    Mesh out;
    const RemeshReport report = RemeshOperation::apply(plane, desc, out);

    ASSERT_TRUE(report.valid);
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, RemeshDiagnostic::InputTriangulated));
    EXPECT_TRUE(hasDiagnostic(report.diagnostic, RemeshDiagnostic::NoChangesApplied));
    EXPECT_EQ(report.messages.size(), 2);

    // Expected messages in sorted order:
    std::vector<std::string> expected = {
        "Input contained non-triangle faces and was triangulated before remeshing",
        "No edges exceeded remesh threshold; output remains unchanged"
    };
    EXPECT_EQ(report.messages, expected);
}

TEST(RemeshOperation, UploadContractWarningOrderIsIndependentOfConfigVariation)
{
    // Apply remesh with two slightly different descriptors that don't affect warnings.
    // Both should produce InputTriangulated + NoChangesApplied in identical order.
    Mesh a = makePlane(1.f, 1.f, 2, 2);
    Mesh b = makePlane(1.f, 1.f, 2, 2);

    RemeshDesc descA{};
    descA.targetEdgeLength = 100.f;
    descA.maxIterations = 2;
    descA.recomputeNormals = false;
    descA.collapseEdgesBelow = 0.f;

    RemeshDesc descB{};
    descB.targetEdgeLength = 100.f;
    descB.maxIterations = 2;
    descB.recomputeNormals = false;
    descB.collapseEdgesBelow = 0.f;
    descB.maxCollapseIterations = 5;  // Different collapse setting, but no collapse since collapseEdgesBelow=0

    Mesh outA, outB;
    const RemeshReport reportA = RemeshOperation::apply(a, descA, outA);
    const RemeshReport reportB = RemeshOperation::apply(b, descB, outB);

    ASSERT_TRUE(reportA.valid);
    ASSERT_TRUE(reportB.valid);
    EXPECT_EQ(reportA.messages.size(), reportB.messages.size());
    EXPECT_EQ(reportA.messages, reportB.messages);
}

} // namespace nexus::geometry::testing
