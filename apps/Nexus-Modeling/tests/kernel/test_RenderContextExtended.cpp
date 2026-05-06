#include <nexus/gfx/RenderContext.h>
#include <gtest/gtest.h>

using namespace nexus::gfx;

TEST(RenderContextExtended, SetFrameTimingCallbackAcceptsLambda)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    ASSERT_NO_THROW(
        ctx->setFrameTimingCallback([](double, double) {})
    );
}

TEST(RenderContextExtended, SubmitComputeAcceptsCommandBuffer)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto h = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(h.valid());

    ASSERT_NO_THROW(
        ctx->submitCompute({&h, 1}, {})
    );

    ctx->device().freeCommandBuffer(h);
}

TEST(RenderContextExtended, SubmitComputeWithTimelineAcceptsTimelineArrays)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto h = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(h.valid());

    auto waitSem = ctx->device().createTimelineSemaphore(1);
    auto signalSem = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(waitSem.valid());
    ASSERT_TRUE(signalSem.valid());

    const std::array<CmdBufHandle, 1> cmds{h};
    const std::array<SemaphoreHandle, 1> waitSems{waitSem};
    const std::array<uint64_t, 1> waitVals{1};
    const std::array<SemaphoreHandle, 1> signalSems{signalSem};
    const std::array<uint64_t, 1> signalVals{2};

    ASSERT_NO_THROW(
        ctx->submitComputeWithTimeline(
            std::span<const CmdBufHandle>(cmds),
            std::span<const SemaphoreHandle>(waitSems),
            std::span<const uint64_t>(waitVals),
            std::span<const SemaphoreHandle>(signalSems),
            std::span<const uint64_t>(signalVals),
            {}
        )
    );

    ctx->device().destroySemaphore(signalSem);
    ctx->device().destroySemaphore(waitSem);
    ctx->device().freeCommandBuffer(h);
}

TEST(RenderContextExtended, SubmitCrossQueueWithTimelineSupportsComputeToGraphics)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd  = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(timeline.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};

    CrossQueueTimelineSubmitDesc submitDesc{};
    submitDesc.graphicsCmds      = std::span<const CmdBufHandle>(graphicsCmds);
    submitDesc.computeCmds       = std::span<const CmdBufHandle>(computeCmds);
    submitDesc.dependency        = CrossQueueTimelineDependency::ComputeToGraphics;
    submitDesc.timelineSemaphore = timeline;
    submitDesc.dependencyValue   = 1;

    ASSERT_NO_THROW(ctx->submitCrossQueueWithTimeline(submitDesc));

    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

TEST(RenderContextExtended, SubmitCrossQueueWithTimelineSupportsGraphicsToCompute)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd  = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(timeline.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};

    CrossQueueTimelineSubmitDesc submitDesc{};
    submitDesc.graphicsCmds      = std::span<const CmdBufHandle>(graphicsCmds);
    submitDesc.computeCmds       = std::span<const CmdBufHandle>(computeCmds);
    submitDesc.dependency        = CrossQueueTimelineDependency::GraphicsToCompute;
    submitDesc.timelineSemaphore = timeline;
    submitDesc.dependencyValue   = 2;

    ASSERT_NO_THROW(ctx->submitCrossQueueWithTimeline(submitDesc));

    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

TEST(RenderContextExtended, SubmitCrossQueueWithTimelineRejectsMissingTimelineSemaphore)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd  = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};

    CrossQueueTimelineSubmitDesc submitDesc{};
    submitDesc.graphicsCmds    = std::span<const CmdBufHandle>(graphicsCmds);
    submitDesc.computeCmds     = std::span<const CmdBufHandle>(computeCmds);
    submitDesc.dependency      = CrossQueueTimelineDependency::ComputeToGraphics;
    submitDesc.dependencyValue = 1;

    ASSERT_THROW(ctx->submitCrossQueueWithTimeline(submitDesc), std::invalid_argument);

    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

TEST(RenderContextExtended, RenderPassGraphPlannerBuildsComputeToGraphicsDescriptor)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd  = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(timeline.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};

    RenderPassGraphPlanner planner{};
    const auto computePass = planner.addPass({QueueType::Compute, std::span<const CmdBufHandle>(computeCmds)});
    const auto graphicsPass = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmds)});
    planner.addDependency(computePass, graphicsPass);

    auto submitDesc = planner.buildCrossQueueSubmitDesc(timeline, 3);
    EXPECT_EQ(submitDesc.dependency, CrossQueueTimelineDependency::ComputeToGraphics);
    EXPECT_EQ(submitDesc.dependencyValue, 3u);
    EXPECT_EQ(submitDesc.computeCmds.size(), 1u);
    EXPECT_EQ(submitDesc.graphicsCmds.size(), 1u);

    ASSERT_NO_THROW(ctx->submitCrossQueueWithTimeline(submitDesc));

    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

TEST(RenderContextExtended, RenderPassGraphPlannerBuildsGraphicsToComputeDescriptor)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd  = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(timeline.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};

    RenderPassGraphPlanner planner{};
    const auto graphicsPass = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmds)});
    const auto computePass = planner.addPass({QueueType::Compute, std::span<const CmdBufHandle>(computeCmds)});
    planner.addDependency(graphicsPass, computePass);

    auto submitDesc = planner.buildCrossQueueSubmitDesc(timeline, 4);
    EXPECT_EQ(submitDesc.dependency, CrossQueueTimelineDependency::GraphicsToCompute);
    EXPECT_EQ(submitDesc.dependencyValue, 4u);
    EXPECT_EQ(submitDesc.computeCmds.size(), 1u);
    EXPECT_EQ(submitDesc.graphicsCmds.size(), 1u);

    ASSERT_NO_THROW(ctx->submitCrossQueueWithTimeline(submitDesc));

    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

TEST(RenderContextExtended, RenderPassGraphPlannerRejectsAmbiguousCrossQueueGraph)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmdA = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto gfxCmdB = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmdA.valid());
    ASSERT_TRUE(gfxCmdB.valid());
    ASSERT_TRUE(compCmd.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(timeline.valid());

    const std::array<CmdBufHandle, 1> graphicsCmdsA{gfxCmdA};
    const std::array<CmdBufHandle, 1> graphicsCmdsB{gfxCmdB};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};

    RenderPassGraphPlanner planner{};
    const auto graphicsPassA = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmdsA)});
    const auto computePass   = planner.addPass({QueueType::Compute, std::span<const CmdBufHandle>(computeCmds)});
    const auto graphicsPassB = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmdsB)});
    planner.addDependency(graphicsPassA, computePass);
    planner.addDependency(computePass, graphicsPassB);

    ASSERT_THROW(planner.buildCrossQueueSubmitDesc(timeline, 1), std::invalid_argument);

    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmdB);
    ctx->device().freeCommandBuffer(gfxCmdA);
}

TEST(RenderContextExtended, RenderPassGraphPlannerBuildSubmissionPlanForChainedCrossQueueEdges)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmdA = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    auto gfxCmdB = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    ASSERT_TRUE(gfxCmdA.valid());
    ASSERT_TRUE(compCmd.valid());
    ASSERT_TRUE(gfxCmdB.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(timeline.valid());

    const std::array<CmdBufHandle, 1> graphicsCmdsA{gfxCmdA};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};
    const std::array<CmdBufHandle, 1> graphicsCmdsB{gfxCmdB};

    RenderPassGraphPlanner planner{};
    const auto graphicsPassA = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmdsA)});
    const auto computePass   = planner.addPass({QueueType::Compute, std::span<const CmdBufHandle>(computeCmds)});
    const auto graphicsPassB = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmdsB)});
    planner.addDependency(graphicsPassA, computePass);
    planner.addDependency(computePass, graphicsPassB);

    auto plan = planner.buildSubmissionPlan(timeline, 7);
    ASSERT_EQ(plan.submits.size(), 3u);
    ASSERT_EQ(plan.terminalSubmitIndices.size(), 1u);
    EXPECT_EQ(plan.terminalSubmitIndices[0], 2u);

    EXPECT_EQ(plan.submits[0].queue, QueueType::Graphics);
    EXPECT_FALSE(plan.submits[0].waitOnTimeline);
    EXPECT_TRUE(plan.submits[0].signalTimeline);
    EXPECT_EQ(plan.submits[0].signalValue, 7u);

    EXPECT_EQ(plan.submits[1].queue, QueueType::Compute);
    EXPECT_TRUE(plan.submits[1].waitOnTimeline);
    EXPECT_EQ(plan.submits[1].waitValue, 7u);
    EXPECT_TRUE(plan.submits[1].signalTimeline);
    EXPECT_EQ(plan.submits[1].signalValue, 8u);

    EXPECT_EQ(plan.submits[2].queue, QueueType::Graphics);
    EXPECT_TRUE(plan.submits[2].waitOnTimeline);
    EXPECT_EQ(plan.submits[2].waitValue, 8u);
    EXPECT_FALSE(plan.submits[2].signalTimeline);

    ASSERT_NO_THROW(ctx->submitPlannedTimelineChain(plan));

    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(gfxCmdB);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmdA);
}

TEST(RenderContextExtended, RenderPassGraphPlannerBuildSubmissionPlanRejectsCycle)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(timeline.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};

    RenderPassGraphPlanner planner{};
    const auto graphicsPass = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmds)});
    const auto computePass  = planner.addPass({QueueType::Compute, std::span<const CmdBufHandle>(computeCmds)});
    planner.addDependency(graphicsPass, computePass);
    planner.addDependency(computePass, graphicsPass);

    ASSERT_THROW(planner.buildSubmissionPlan(timeline, 1), std::invalid_argument);

    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

// ── Fence policy tests ─────────────────────────────────────────────────────

TEST(RenderContextExtended, FencePolicyAutoFenceAssignsFenceToSingleTerminalPass)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmdA = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    auto gfxCmdB = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    ASSERT_TRUE(gfxCmdA.valid());
    ASSERT_TRUE(compCmd.valid());
    ASSERT_TRUE(gfxCmdB.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    auto fence    = ctx->device().createFence(false);
    ASSERT_TRUE(timeline.valid());
    ASSERT_TRUE(fence.valid());

    const std::array<CmdBufHandle, 1> graphicsCmdsA{gfxCmdA};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};
    const std::array<CmdBufHandle, 1> graphicsCmdsB{gfxCmdB};
    const std::array<FenceHandle,  1> fences{fence};

    // Chain: graphicsA -> compute -> graphicsB  (graphicsB is the sole terminal)
    RenderPassGraphPlanner planner{};
    const auto graphicsPassA = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmdsA)});
    const auto computePass   = planner.addPass({QueueType::Compute,  std::span<const CmdBufHandle>(computeCmds)});
    const auto graphicsPassB = planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmdsB)});
    planner.addDependency(graphicsPassA, computePass);
    planner.addDependency(computePass, graphicsPassB);

    auto plan = planner.buildSubmissionPlan(timeline, 1,
        FencePolicy::AutoFenceTerminalPasses,
        std::span<const FenceHandle>(fences));
    ASSERT_EQ(plan.submits.size(), 3u);
    ASSERT_EQ(plan.terminalSubmitIndices.size(), 1u);
    EXPECT_EQ(plan.terminalSubmitIndices[0], 2u);

    // Only the terminal pass (graphicsB, index 2) should have the fence.
    EXPECT_FALSE(plan.submits[0].signalFence.valid());
    EXPECT_FALSE(plan.submits[1].signalFence.valid());
    EXPECT_TRUE(plan.submits[2].signalFence.valid());
    EXPECT_EQ(plan.submits[2].signalFence.id, fence.id);

    ASSERT_NO_THROW(ctx->submitPlannedTimelineChain(plan));

    ctx->device().destroyFence(fence);
    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(gfxCmdB);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmdA);
}

TEST(RenderContextExtended, FencePolicyExplicitFencesRejectsMismatchedFenceCount)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd  = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    auto timeline = ctx->device().createTimelineSemaphore(0);
    ASSERT_TRUE(timeline.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};

    // Two independent passes — no dependency — both are terminal.
    RenderPassGraphPlanner planner{};
    planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmds)});
    planner.addPass({QueueType::Compute,  std::span<const CmdBufHandle>(computeCmds)});

    // Provide zero fences for ExplicitFences — should reject (2 terminals, 0 fences).
    EXPECT_THROW(
        planner.buildSubmissionPlan(timeline, 1, FencePolicy::ExplicitFences, {}),
        std::invalid_argument);

    ctx->device().destroySemaphore(timeline);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

TEST(RenderContextExtended, FencePolicyAutoFenceMultiTerminalAssignsFencesInOrder)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd  = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    auto fence0 = ctx->device().createFence(false);
    auto fence1 = ctx->device().createFence(false);
    ASSERT_TRUE(fence0.valid());
    ASSERT_TRUE(fence1.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};
    const std::array<FenceHandle,  2> fences{fence0, fence1};

    // Two independent passes — no edges — both are terminal.
    RenderPassGraphPlanner planner{};
    planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmds)});
    planner.addPass({QueueType::Compute,  std::span<const CmdBufHandle>(computeCmds)});

    auto plan = planner.buildSubmissionPlan({}, 1,
        FencePolicy::AutoFenceTerminalPasses,
        std::span<const FenceHandle>(fences));
    ASSERT_EQ(plan.submits.size(), 2u);
    ASSERT_EQ(plan.terminalSubmitIndices.size(), 2u);
    EXPECT_EQ(plan.terminalSubmitIndices[0], 0u);
    EXPECT_EQ(plan.terminalSubmitIndices[1], 1u);

    // Both terminal passes receive their respective fences in topological order.
    EXPECT_TRUE(plan.submits[0].signalFence.valid());
    EXPECT_TRUE(plan.submits[1].signalFence.valid());
    EXPECT_EQ(plan.submits[0].signalFence.id, fence0.id);
    EXPECT_EQ(plan.submits[1].signalFence.id, fence1.id);

    ctx->device().destroyFence(fence1);
    ctx->device().destroyFence(fence0);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

TEST(RenderContextExtended, CompletionWaitDescCollectsTerminalFences)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    auto gfxCmd  = ctx->device().allocateCommandBuffer(QueueType::Graphics);
    auto compCmd = ctx->device().allocateCommandBuffer(QueueType::Compute);
    ASSERT_TRUE(gfxCmd.valid());
    ASSERT_TRUE(compCmd.valid());

    auto fence0 = ctx->device().createFence(false);
    auto fence1 = ctx->device().createFence(false);
    ASSERT_TRUE(fence0.valid());
    ASSERT_TRUE(fence1.valid());

    const std::array<CmdBufHandle, 1> graphicsCmds{gfxCmd};
    const std::array<CmdBufHandle, 1> computeCmds{compCmd};
    const std::array<FenceHandle,  2> fences{fence0, fence1};

    RenderPassGraphPlanner planner{};
    planner.addPass({QueueType::Graphics, std::span<const CmdBufHandle>(graphicsCmds)});
    planner.addPass({QueueType::Compute,  std::span<const CmdBufHandle>(computeCmds)});

    auto plan = planner.buildSubmissionPlan(
        {},
        1,
        FencePolicy::AutoFenceTerminalPasses,
        std::span<const FenceHandle>(fences));

    auto waits = RenderPassGraphPlanner::buildCompletionWaitDesc(plan);
    ASSERT_EQ(waits.fences.size(), 2u);
    EXPECT_EQ(waits.fences[0].id, fence0.id);
    EXPECT_EQ(waits.fences[1].id, fence1.id);
    EXPECT_TRUE(waits.timelineWaitValues.empty());

    ctx->device().destroyFence(fence1);
    ctx->device().destroyFence(fence0);
    ctx->device().freeCommandBuffer(compCmd);
    ctx->device().freeCommandBuffer(gfxCmd);
}

TEST(RenderContextExtended, CompletionWaitDescCollectsTerminalTimelineValues)
{
    RenderPassSubmissionPlan plan{};
    plan.timelineSemaphore.id = 77;
    plan.submits = {
        PlannedQueueSubmit{
            .queue = QueueType::Graphics,
            .cmds = {},
            .waitOnTimeline = false,
            .signalTimeline = true,
            .waitValue = 0,
            .signalValue = 9,
            .signalFence = {}
        }
    };
    plan.terminalSubmitIndices = {0};

    auto waits = RenderPassGraphPlanner::buildCompletionWaitDesc(plan);
    ASSERT_EQ(waits.timelineWaitValues.size(), 1u);
    EXPECT_EQ(waits.timelineSemaphore.id, 77u);
    EXPECT_EQ(waits.timelineWaitValues[0], 9u);
    EXPECT_TRUE(waits.fences.empty());
}

TEST(RenderContextExtended, CompletionWaitDescRejectsTerminalIndexOutOfRange)
{
    RenderPassSubmissionPlan plan{};
    plan.submits = {PlannedQueueSubmit{}};
    plan.terminalSubmitIndices = {1};

    EXPECT_THROW(
        RenderPassGraphPlanner::buildCompletionWaitDesc(plan),
        std::invalid_argument);
}

