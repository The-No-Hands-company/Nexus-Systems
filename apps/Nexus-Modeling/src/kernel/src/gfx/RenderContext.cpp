// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — RenderContext implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/RenderContext.h>

#ifdef NEXUS_BACKEND_VULKAN
#  include "backend/vulkan/VulkanDevice.h"
#  include "backend/vulkan/VulkanSwapchain.h"
#  include "backend/vulkan/VulkanAllocator.h"
#endif

#ifdef NEXUS_BACKEND_NULL
#  include "backend/null/NullDevice.h"
#  include "backend/null/NullSwapchain.h"
#endif

#include <stdexcept>
#include <array>
#include <queue>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace nexus::gfx {

RenderPassGraphPlanner::PassId RenderPassGraphPlanner::addPass(const PassDesc& desc)
{
    if (desc.cmds.empty()) {
        throw std::invalid_argument("RenderPassGraphPlanner::addPass requires at least one command buffer");
    }

    StoredPass pass{};
    pass.queue = desc.queue;
    pass.cmds.assign(desc.cmds.begin(), desc.cmds.end());
    m_passes.push_back(std::move(pass));
    return static_cast<PassId>(m_passes.size() - 1);
}

void RenderPassGraphPlanner::addDependency(PassId sourcePass, PassId destinationPass)
{
    if (sourcePass >= m_passes.size() || destinationPass >= m_passes.size()) {
        throw std::invalid_argument("RenderPassGraphPlanner::addDependency pass id out of range");
    }
    m_edges.emplace_back(sourcePass, destinationPass);
}

void RenderPassGraphPlanner::clear() noexcept
{
    m_passes.clear();
    m_edges.clear();
    m_plannedGraphicsCmds.clear();
    m_plannedComputeCmds.clear();
}

CrossQueueTimelineSubmitDesc RenderPassGraphPlanner::buildCrossQueueSubmitDesc(
    SemaphoreHandle timelineSemaphore,
    uint64_t dependencyValue)
{
    if (!timelineSemaphore.valid()) {
        throw std::invalid_argument("RenderPassGraphPlanner::buildCrossQueueSubmitDesc requires a valid timeline semaphore");
    }
    if (dependencyValue == 0) {
        throw std::invalid_argument("RenderPassGraphPlanner::buildCrossQueueSubmitDesc requires dependencyValue > 0");
    }
    if (m_passes.empty() || m_edges.empty()) {
        throw std::invalid_argument("RenderPassGraphPlanner::buildCrossQueueSubmitDesc requires passes and at least one edge");
    }

    const std::pair<PassId, PassId>* selectedEdge = nullptr;
    for (const auto& edge : m_edges) {
        if (edge.first >= m_passes.size() || edge.second >= m_passes.size()) {
            throw std::invalid_argument("RenderPassGraphPlanner::buildCrossQueueSubmitDesc found edge with invalid pass id");
        }

        const QueueType srcQueue = m_passes[edge.first].queue;
        const QueueType dstQueue = m_passes[edge.second].queue;
        if (srcQueue == dstQueue) {
            continue;
        }

        const bool supportedCrossQueue =
            (srcQueue == QueueType::Compute  && dstQueue == QueueType::Graphics) ||
            (srcQueue == QueueType::Graphics && dstQueue == QueueType::Compute);
        if (!supportedCrossQueue) {
            continue;
        }

        if (selectedEdge != nullptr) {
            throw std::invalid_argument("RenderPassGraphPlanner::buildCrossQueueSubmitDesc currently supports exactly one compute/graphics cross-queue edge");
        }
        selectedEdge = &edge;
    }

    if (selectedEdge == nullptr) {
        throw std::invalid_argument("RenderPassGraphPlanner::buildCrossQueueSubmitDesc found no compute/graphics cross-queue edge");
    }

    const auto& source = m_passes[selectedEdge->first];
    const auto& destination = m_passes[selectedEdge->second];

    m_plannedGraphicsCmds.clear();
    m_plannedComputeCmds.clear();

    if (source.queue == QueueType::Graphics) {
        m_plannedGraphicsCmds = source.cmds;
        m_plannedComputeCmds = destination.cmds;
    } else {
        m_plannedComputeCmds = source.cmds;
        m_plannedGraphicsCmds = destination.cmds;
    }

    if (m_plannedGraphicsCmds.empty() || m_plannedComputeCmds.empty()) {
        throw std::invalid_argument("RenderPassGraphPlanner::buildCrossQueueSubmitDesc produced an empty queue submission");
    }

    CrossQueueTimelineSubmitDesc result{};
    result.graphicsCmds      = std::span<const CmdBufHandle>(m_plannedGraphicsCmds);
    result.computeCmds       = std::span<const CmdBufHandle>(m_plannedComputeCmds);
    result.timelineSemaphore = timelineSemaphore;
    result.dependencyValue   = dependencyValue;
    result.dependency = (source.queue == QueueType::Compute)
        ? CrossQueueTimelineDependency::ComputeToGraphics
        : CrossQueueTimelineDependency::GraphicsToCompute;
    return result;
}

RenderPassSubmissionPlan RenderPassGraphPlanner::buildSubmissionPlan(
    SemaphoreHandle timelineSemaphore,
    uint64_t firstSignalValue,
    FencePolicy fencePolicy,
    std::span<const FenceHandle> fences)
{
    if (m_passes.empty()) {
        throw std::invalid_argument("RenderPassGraphPlanner::buildSubmissionPlan requires at least one pass");
    }
    if (firstSignalValue == 0) {
        throw std::invalid_argument("RenderPassGraphPlanner::buildSubmissionPlan requires firstSignalValue > 0");
    }

    const size_t passCount = m_passes.size();
    std::vector<std::vector<PassId>> outgoing(passCount);
    std::vector<uint32_t> indegree(passCount, 0);

    for (const auto& edge : m_edges) {
        if (edge.first >= passCount || edge.second >= passCount) {
            throw std::invalid_argument("RenderPassGraphPlanner::buildSubmissionPlan found edge with invalid pass id");
        }
        outgoing[edge.first].push_back(edge.second);
        ++indegree[edge.second];
    }

    std::queue<PassId> ready;
    for (PassId i = 0; i < static_cast<PassId>(passCount); ++i) {
        if (indegree[i] == 0) {
            ready.push(i);
        }
    }

    std::vector<PassId> order;
    order.reserve(passCount);
    while (!ready.empty()) {
        const PassId current = ready.front();
        ready.pop();
        order.push_back(current);
        for (const PassId next : outgoing[current]) {
            if (--indegree[next] == 0) {
                ready.push(next);
            }
        }
    }

    if (order.size() != passCount) {
        throw std::invalid_argument("RenderPassGraphPlanner::buildSubmissionPlan detected cycle in pass graph");
    }

    // ── Fence policy: collect terminal passes in topological order ────────────
    // A terminal pass has no outgoing edges.
    std::vector<PassId> terminalPasses;
    for (const PassId passId : order) {
        if (outgoing[passId].empty()) {
            terminalPasses.push_back(passId);
        }
    }

    if (fencePolicy == FencePolicy::ExplicitFences &&
        fences.size() != terminalPasses.size())
    {
        throw std::invalid_argument(
            "RenderPassGraphPlanner::buildSubmissionPlan: ExplicitFences requires "
            "fences.size() == number of terminal passes");
    }

    // Build passId -> fenceHandle assignment map for terminal passes.
    std::unordered_map<PassId, FenceHandle> fenceMap;
    if (fencePolicy != FencePolicy::None) {
        const size_t assignCount = std::min(fences.size(), terminalPasses.size());
        for (size_t i = 0; i < assignCount; ++i) {
            fenceMap[terminalPasses[i]] = fences[i];
        }
    }

    std::vector<uint64_t> producedSignalValue(passCount, 0);
    uint64_t nextSignalValue = firstSignalValue;

    RenderPassSubmissionPlan plan{};
    plan.timelineSemaphore = timelineSemaphore;
    plan.submits.reserve(order.size());
    plan.terminalSubmitIndices.reserve(terminalPasses.size());

    std::unordered_set<PassId> terminalPassSet;
    terminalPassSet.reserve(terminalPasses.size());
    for (const PassId passId : terminalPasses) {
        terminalPassSet.insert(passId);
    }

    for (const PassId passId : order) {
        const auto& pass = m_passes[passId];

        PlannedQueueSubmit step{};
        step.queue = pass.queue;
        step.cmds = pass.cmds;

        uint64_t requiredWaitValue = 0;
        bool hasCrossQueueIncoming = false;
        for (const auto& edge : m_edges) {
            if (edge.second != passId) {
                continue;
            }
            const auto& source = m_passes[edge.first];
            if (source.queue == pass.queue) {
                continue;
            }
            hasCrossQueueIncoming = true;
            requiredWaitValue = std::max(requiredWaitValue, producedSignalValue[edge.first]);
        }

        if (hasCrossQueueIncoming) {
            if (!timelineSemaphore.valid()) {
                throw std::invalid_argument("RenderPassGraphPlanner::buildSubmissionPlan requires a valid timeline semaphore for cross-queue edges");
            }
            if (requiredWaitValue == 0) {
                throw std::invalid_argument("RenderPassGraphPlanner::buildSubmissionPlan found cross-queue dependency without producer signal");
            }
            step.waitOnTimeline = true;
            step.waitValue = requiredWaitValue;
        }

        bool hasCrossQueueOutgoing = false;
        for (const PassId destination : outgoing[passId]) {
            if (m_passes[destination].queue != pass.queue) {
                hasCrossQueueOutgoing = true;
                break;
            }
        }

        if (hasCrossQueueOutgoing) {
            if (!timelineSemaphore.valid()) {
                throw std::invalid_argument("RenderPassGraphPlanner::buildSubmissionPlan requires a valid timeline semaphore for cross-queue edges");
            }
            step.signalTimeline = true;
            step.signalValue = nextSignalValue;
            producedSignalValue[passId] = nextSignalValue;
            ++nextSignalValue;
        }

        if (const auto it = fenceMap.find(passId); it != fenceMap.end()) {
            step.signalFence = it->second;
        }

        if (terminalPassSet.contains(passId)) {
            plan.terminalSubmitIndices.push_back(static_cast<uint32_t>(plan.submits.size()));
        }

        plan.submits.push_back(std::move(step));
    }

    return plan;
}

RenderPassCompletionWaitDesc RenderPassGraphPlanner::buildCompletionWaitDesc(
    const RenderPassSubmissionPlan& plan)
{
    RenderPassCompletionWaitDesc desc{};
    desc.timelineSemaphore = plan.timelineSemaphore;

    if (plan.submits.empty()) {
        return desc;
    }

    std::unordered_set<uint64_t> seenFenceIds;
    std::unordered_set<uint64_t> seenTimelineValues;

    for (const uint32_t submitIndex : plan.terminalSubmitIndices) {
        if (submitIndex >= plan.submits.size()) {
            throw std::invalid_argument("RenderPassGraphPlanner::buildCompletionWaitDesc found terminal submit index out of range");
        }

        const auto& step = plan.submits[submitIndex];

        if (step.signalFence.valid() && seenFenceIds.insert(step.signalFence.id).second) {
            desc.fences.push_back(step.signalFence);
        }

        if (step.signalTimeline) {
            if (!plan.timelineSemaphore.valid()) {
                throw std::invalid_argument("RenderPassGraphPlanner::buildCompletionWaitDesc requires valid timeline semaphore when terminal step signals timeline");
            }
            if (step.signalValue == 0) {
                throw std::invalid_argument("RenderPassGraphPlanner::buildCompletionWaitDesc requires non-zero timeline value on terminal timeline signal");
            }
            if (seenTimelineValues.insert(step.signalValue).second) {
                desc.timelineWaitValues.push_back(step.signalValue);
            }
        }
    }

    return desc;
}

// ── Pimpl ─────────────────────────────────────────────────────────────────────
struct RenderContext::Impl {
    std::unique_ptr<IDevice>       device;
    std::unique_ptr<IGPUAllocator> allocator;
    FrameTimingCallback            timingCb;
    Backend                        activeBackend = Backend::Null;
};

// ── Construction ──────────────────────────────────────────────────────────────
RenderContext::RenderContext()
    : m_impl(std::make_unique<Impl>())
{}

RenderContext::~RenderContext() = default;

std::unique_ptr<RenderContext> RenderContext::create(const RenderContextDesc& desc)
{
    auto ctx = std::unique_ptr<RenderContext>(new RenderContext());
    auto& impl = *ctx->m_impl;

    Backend chosen = desc.preferredBackend;
    (void)impl;
    (void)chosen;

#ifdef NEXUS_BACKEND_VULKAN
    if (chosen == Backend::Vulkan) {
        auto vkDev = std::make_unique<VulkanDevice>();
        vkDev->init(desc);
        impl.allocator    = vkDev->createAllocator();
        impl.activeBackend = Backend::Vulkan;
        impl.device       = std::move(vkDev);
        return ctx;
    }
#endif

#ifdef NEXUS_BACKEND_NULL
    if (chosen == Backend::Null) {
        impl.device        = std::make_unique<NullDevice>();
        impl.allocator     = std::make_unique<NullAllocator>();
        impl.activeBackend = Backend::Null;
        return ctx;
    }
#endif

    throw std::runtime_error("No suitable graphics backend available");
}

// ── Accessors ─────────────────────────────────────────────────────────────────
IDevice&       RenderContext::device()    noexcept { return *m_impl->device; }
IGPUAllocator& RenderContext::allocator() noexcept { return *m_impl->allocator; }
Backend        RenderContext::activeBackend() const noexcept { return m_impl->activeBackend; }

HardwareTier RenderContext::hardwareTier() const noexcept {
    return m_impl->device ? m_impl->device->tier() : HardwareTier::Low;
}

const DeviceCapabilities& RenderContext::caps() const noexcept {
    return m_impl->device->caps();
}

// ── Swapchain factory ─────────────────────────────────────────────────────────
std::unique_ptr<ISwapchain> RenderContext::createSwapchain(const SwapchainDesc& desc)
{
#ifdef NEXUS_BACKEND_VULKAN
    if (m_impl->activeBackend == Backend::Vulkan) {
        auto* vkDev = static_cast<VulkanDevice*>(m_impl->device.get());
        return vkDev->createSwapchain(desc);
    }
#endif
#ifdef NEXUS_BACKEND_NULL
    if (m_impl->activeBackend == Backend::Null) {
        return std::make_unique<NullSwapchain>(desc.extent);
    }
#endif
    (void)desc;  // silence -Wunused-parameter when all backends are guarded
    throw std::runtime_error("createSwapchain: no backend active");
}

// ── Frame timing ──────────────────────────────────────────────────────────────
void RenderContext::setFrameTimingCallback(FrameTimingCallback cb) {
    m_impl->timingCb = std::move(cb);
}

// ── Async compute submit ──────────────────────────────────────────────────────
void RenderContext::submitCompute(std::span<const CmdBufHandle> cmds, FenceHandle signalFence)
{
    m_impl->device->submit(QueueType::Compute, cmds, {}, {}, signalFence);
}

void RenderContext::submitComputeWithTimeline(
    std::span<const CmdBufHandle> cmds,
    std::span<const SemaphoreHandle> waitSemaphores,
    std::span<const uint64_t> waitValues,
    std::span<const SemaphoreHandle> signalSemaphores,
    std::span<const uint64_t> signalValues,
    FenceHandle signalFence)
{
    m_impl->device->submitWithTimeline(
        QueueType::Compute,
        cmds,
        waitSemaphores,
        waitValues,
        signalSemaphores,
        signalValues,
        signalFence);
}

void RenderContext::submitCrossQueueWithTimeline(const CrossQueueTimelineSubmitDesc& desc)
{
    if (desc.dependency == CrossQueueTimelineDependency::None) {
        if (!desc.computeCmds.empty()) {
            m_impl->device->submit(QueueType::Compute, desc.computeCmds, {}, {}, desc.computeFence);
        }
        if (!desc.graphicsCmds.empty()) {
            m_impl->device->submit(QueueType::Graphics, desc.graphicsCmds, {}, {}, desc.graphicsFence);
        }
        return;
    }

    if (!desc.timelineSemaphore.valid()) {
        throw std::invalid_argument("submitCrossQueueWithTimeline: timelineSemaphore is required when dependency is set");
    }
    if (desc.dependencyValue == 0) {
        throw std::invalid_argument("submitCrossQueueWithTimeline: dependencyValue must be non-zero");
    }
    if (desc.computeCmds.empty() || desc.graphicsCmds.empty()) {
        throw std::invalid_argument("submitCrossQueueWithTimeline: both computeCmds and graphicsCmds are required for cross-queue dependency");
    }

    const std::array<SemaphoreHandle, 1> semaphores{desc.timelineSemaphore};
    const std::array<uint64_t, 1> values{desc.dependencyValue};

    if (desc.dependency == CrossQueueTimelineDependency::ComputeToGraphics) {
        m_impl->device->submitWithTimeline(
            QueueType::Compute,
            desc.computeCmds,
            {},
            {},
            std::span<const SemaphoreHandle>(semaphores),
            std::span<const uint64_t>(values),
            desc.computeFence);

        m_impl->device->submitWithTimeline(
            QueueType::Graphics,
            desc.graphicsCmds,
            std::span<const SemaphoreHandle>(semaphores),
            std::span<const uint64_t>(values),
            {},
            {},
            desc.graphicsFence);
        return;
    }

    m_impl->device->submitWithTimeline(
        QueueType::Graphics,
        desc.graphicsCmds,
        {},
        {},
        std::span<const SemaphoreHandle>(semaphores),
        std::span<const uint64_t>(values),
        desc.graphicsFence);

    m_impl->device->submitWithTimeline(
        QueueType::Compute,
        desc.computeCmds,
        std::span<const SemaphoreHandle>(semaphores),
        std::span<const uint64_t>(values),
        {},
        {},
        desc.computeFence);
}

void RenderContext::submitPlannedTimelineChain(const RenderPassSubmissionPlan& plan)
{
    if (plan.submits.empty()) {
        return;
    }

    for (const auto& step : plan.submits) {
        if (step.cmds.empty()) {
            throw std::invalid_argument("submitPlannedTimelineChain: submission step contains no command buffers");
        }

        const bool timelineRequired = step.waitOnTimeline || step.signalTimeline;
        if (!timelineRequired) {
            m_impl->device->submit(step.queue, std::span<const CmdBufHandle>(step.cmds), {}, {}, step.signalFence);
            continue;
        }

        if (!plan.timelineSemaphore.valid()) {
            throw std::invalid_argument("submitPlannedTimelineChain: timeline semaphore required by planned step");
        }

        std::array<SemaphoreHandle, 1> waitSemaphores{};
        std::array<uint64_t, 1> waitValues{};
        std::array<SemaphoreHandle, 1> signalSemaphores{};
        std::array<uint64_t, 1> signalValues{};

        std::span<const SemaphoreHandle> waitSpan{};
        std::span<const uint64_t> waitValueSpan{};
        std::span<const SemaphoreHandle> signalSpan{};
        std::span<const uint64_t> signalValueSpan{};

        if (step.waitOnTimeline) {
            if (step.waitValue == 0) {
                throw std::invalid_argument("submitPlannedTimelineChain: wait value must be non-zero when waitOnTimeline is set");
            }
            waitSemaphores[0] = plan.timelineSemaphore;
            waitValues[0] = step.waitValue;
            waitSpan = std::span<const SemaphoreHandle>(waitSemaphores);
            waitValueSpan = std::span<const uint64_t>(waitValues);
        }

        if (step.signalTimeline) {
            if (step.signalValue == 0) {
                throw std::invalid_argument("submitPlannedTimelineChain: signal value must be non-zero when signalTimeline is set");
            }
            signalSemaphores[0] = plan.timelineSemaphore;
            signalValues[0] = step.signalValue;
            signalSpan = std::span<const SemaphoreHandle>(signalSemaphores);
            signalValueSpan = std::span<const uint64_t>(signalValues);
        }

        m_impl->device->submitWithTimeline(
            step.queue,
            std::span<const CmdBufHandle>(step.cmds),
            waitSpan,
            waitValueSpan,
            signalSpan,
            signalValueSpan,
            step.signalFence);
    }
}

} // namespace nexus::gfx
