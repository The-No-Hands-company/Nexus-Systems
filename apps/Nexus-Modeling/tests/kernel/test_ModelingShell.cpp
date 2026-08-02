#include <nexus/geometry/ModelingShell.h>

#include <gtest/gtest.h>

#include <limits>

namespace nexus::geometry::testing {

TEST(ModelingShell, GuidedIntroStepsAreStableAndNonEmpty)
{
    const auto steps = ModelingShell::guidedIntroSteps();
    ASSERT_GE(steps.size(), 4u);
    EXPECT_EQ(steps[0], "Pick a starter primitive for your blockout.");
    EXPECT_EQ(steps[1], "Use quick cleanup to smooth topology and edge flow.");
}

TEST(ModelingShell, StartStarterModelBuildsValidWorkflowAndOverlay)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Sphere, 2.0f);

    EXPECT_TRUE(shell.workflow().mesh().isValid());
    EXPECT_TRUE(shell.workflow().mesh().attributes().hasNormals());
    EXPECT_TRUE(shell.isReady());

    ASSERT_GE(shell.workflow().stepReports().size(), 2u);
    EXPECT_EQ(shell.workflow().stepReports()[0].kind, HardSurfaceStepKind::Init);
    EXPECT_EQ(shell.workflow().stepReports()[1].kind, HardSurfaceStepKind::RebuildNormals);
}

TEST(ModelingShell, QuickCleanupAppendsExpectedSteps)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Box, 1.0f);

    BevelChamferDesc bevel{};
    bevel.distance = 0.05f;

    RemeshDesc remesh{};
    remesh.targetEdgeLength = 0.8f;
    remesh.maxIterations = 1u;

    const size_t before = shell.workflow().stepReports().size();
    shell.quickCleanup(bevel, remesh);

    ASSERT_GE(shell.workflow().stepReports().size(), before + 3u);
    const auto& reports = shell.workflow().stepReports();
    EXPECT_EQ(reports[before + 0].kind, HardSurfaceStepKind::BevelChamfer);
    EXPECT_EQ(reports[before + 1].kind, HardSurfaceStepKind::Remesh);
    EXPECT_EQ(reports[before + 2].kind, HardSurfaceStepKind::RebuildNormals);
}

TEST(ModelingShell, SessionReportReflectsWorkflowState)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Capsule, 1.5f);

    const ModelingShellSessionReport report = shell.sessionReport();
    EXPECT_TRUE(report.success);
    EXPECT_FALSE(report.workflowSteps.empty());
    // A capsule is a CLOSED surface, so it has exactly zero boundary edges. The previous
    // assertion was >= 0 on an unsigned count, which every possible value satisfies —
    // including the wrong ones.
    EXPECT_EQ(report.overlay.boundaryEdgeCount, 0u)
        << "a closed starter primitive must report no boundary edges";
    EXPECT_FALSE(report.message.empty());

    // The contrast is what gives the number meaning: an OPEN starter primitive must
    // report boundary edges. A counter that is always zero would pass the check above.
    ModelingShell open;
    open.startStarterModel(StarterPrimitive::Plane, 1.0f);
    EXPECT_GT(open.sessionReport().overlay.boundaryEdgeCount, 0u)
        << "an open plane must report boundary edges";
}

TEST(ModelingShell, RefreshDiagnosticsHonorsModeFiltering)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Plane, 1.0f);

    MeshDiagnosticOverlayOptions opts{};
    opts.modes = MeshOverlayMode::BoundaryEdges;

    ASSERT_TRUE(shell.refreshDiagnostics(opts));
    EXPECT_EQ(shell.diagnostics().nonManifoldEdgeCount, 0u);
    EXPECT_EQ(shell.diagnostics().degenerateFaceCount, 0u);
    EXPECT_GT(shell.diagnostics().boundaryEdgeCount, 0u);
}

TEST(ModelingShell, NonFiniteInputsAreSanitizedToSafeDefaults)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Box, std::numeric_limits<float>::quiet_NaN());

    BevelChamferDesc bevel{};
    bevel.distance = std::numeric_limits<float>::infinity();

    RemeshDesc remesh{};
    remesh.targetEdgeLength = std::numeric_limits<float>::quiet_NaN();
    remesh.maxIterations = 0u;

    shell.quickCleanup(bevel, remesh);

    EXPECT_TRUE(shell.workflow().mesh().isValid());
    EXPECT_TRUE(shell.workflow().mesh().attributes().hasNormals());
    ASSERT_FALSE(shell.workflow().stepReports().empty());
    EXPECT_EQ(shell.workflow().stepReports().back().kind, HardSurfaceStepKind::RebuildNormals);
}

} // namespace nexus::geometry::testing
