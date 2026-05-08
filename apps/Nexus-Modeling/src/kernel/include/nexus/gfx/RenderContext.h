#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — RenderContext
//
//  Top-level kernel entry point.  Owns the IDevice, allocator,
//  async-compute queue manager, and feature-capability negotiation.
//
//  Usage:
//      auto ctx = nexus::gfx::RenderContext::create(desc);
//      IDevice& dev = ctx->device();
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <nexus/gfx/Device.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/gfx/Allocator.h>
#include <memory>
#include <string_view>
#include <functional>
#include <vector>
#include <utility>

namespace nexus::gfx {

// ── Validation / debug tier ────────────────────────────────────────────────
enum class ValidationLevel : uint8_t {
    Off,       // release — zero overhead
    Core,      // API validation only
    Full,      // + GPU-based validation, synchronisation checks
    Aftermath, // Full + NVIDIA Aftermath breadcrumbs (requires NEXUS_ENABLE_AFTERMATH)
};

// ── Frame timing callback ──────────────────────────────────────────────────
using FrameTimingCallback = std::function<void(double gpuMs, double cpuMs)>;

enum class CrossQueueTimelineDependency : uint8_t {
    None = 0,
    ComputeToGraphics,
    GraphicsToCompute,
};

// Render-graph friendly cross-queue submission descriptor.
// dependency + timelineSemaphore + dependencyValue define the queue edge.
struct CrossQueueTimelineSubmitDesc {
    std::span<const CmdBufHandle> graphicsCmds{};
    std::span<const CmdBufHandle> computeCmds{};
    CrossQueueTimelineDependency  dependency      = CrossQueueTimelineDependency::None;
    SemaphoreHandle               timelineSemaphore{};
    uint64_t                      dependencyValue = 1;
    FenceHandle                   graphicsFence{};
    FenceHandle                   computeFence{};
};

// Controls which submission steps in a planned queue chain receive a fence.
enum class FencePolicy : uint8_t {
    None,                    ///< No fences assigned (default).
    AutoFenceTerminalPasses, ///< Assign provided fences[] to terminal passes in topological order.
                             ///  Tolerated if fences.size() < terminal count; extra terminals left unfenced.
    ExplicitFences,          ///< Like AutoFenceTerminalPasses but strictly requires
                             ///  fences.size() == number of terminal passes.
};

struct PlannedQueueSubmit {
    QueueType                 queue = QueueType::Graphics;
    std::vector<CmdBufHandle> cmds;
    bool                      waitOnTimeline   = false;
    bool                      signalTimeline   = false;
    uint64_t                  waitValue        = 0;
    uint64_t                  signalValue      = 0;
    FenceHandle               signalFence{};
};

struct RenderPassSubmissionPlan {
    SemaphoreHandle                timelineSemaphore{};
    std::vector<PlannedQueueSubmit> submits;
    std::vector<uint32_t>           terminalSubmitIndices;
};

// Compact, frame-orchestration friendly completion waits derived from
// planner output.
struct RenderPassCompletionWaitDesc {
    SemaphoreHandle       timelineSemaphore{};
    std::vector<uint64_t> timelineWaitValues;
    std::vector<FenceHandle> fences;
};

class RenderPassGraphPlanner {
public:
    using PassId = uint32_t;

    struct PassDesc {
        QueueType                   queue = QueueType::Graphics;
        std::span<const CmdBufHandle> cmds{};
    };

    PassId addPass(const PassDesc& desc);
    void addDependency(PassId sourcePass, PassId destinationPass);
    void clear() noexcept;

    // Builds a cross-queue descriptor from declared pass edges.
    // Current planner scope supports one compute<->graphics edge per build.
    [[nodiscard]] CrossQueueTimelineSubmitDesc buildCrossQueueSubmitDesc(
        SemaphoreHandle timelineSemaphore,
        uint64_t dependencyValue = 1);

    // Builds an ordered queue submission chain from pass edges.
    // Cross-queue edges are converted into timeline wait/signal values.
    // fencePolicy controls which passes receive a fence handle from fences[].
    [[nodiscard]] RenderPassSubmissionPlan buildSubmissionPlan(
        SemaphoreHandle timelineSemaphore,
        uint64_t firstSignalValue = 1,
        FencePolicy fencePolicy = FencePolicy::None,
        std::span<const FenceHandle> fences = {});

    // Builds a compact completion-wait descriptor from a submission plan.
    // completion values/fences are derived from terminal submit steps.
    [[nodiscard]] static RenderPassCompletionWaitDesc buildCompletionWaitDesc(
        const RenderPassSubmissionPlan& plan);

private:
    struct StoredPass {
        QueueType queue = QueueType::Graphics;
        std::vector<CmdBufHandle> cmds;
    };

    std::vector<StoredPass>           m_passes;
    std::vector<std::pair<PassId, PassId>> m_edges;
    std::vector<CmdBufHandle>         m_plannedGraphicsCmds;
    std::vector<CmdBufHandle>         m_plannedComputeCmds;
};

// ── RenderContext descriptor ───────────────────────────────────────────────
struct RenderContextDesc {
    Backend         preferredBackend = Backend::Vulkan;
    ValidationLevel validation       = ValidationLevel::Core;
    bool            enableHDR        = false;
    bool            enableAsyncCompute = true;
    bool            enableMeshShaders  = true;
    bool            enableRayTracing   = true;
    bool            enableNeuralHints  = false;  // reserved for neural texture/denoising APIs
    std::string_view appName          = "NexusModeling";
    uint32_t         appVersion       = 1;
};

// ── RenderContext ──────────────────────────────────────────────────────────
class RenderContext {
public:
    ~RenderContext();

    [[nodiscard]] static std::unique_ptr<RenderContext> create(const RenderContextDesc& desc);

    // ── Access ────────────────────────────────────────────────────────────
    [[nodiscard]] IDevice&        device()    noexcept;
    [[nodiscard]] IGPUAllocator&  allocator() noexcept;
    [[nodiscard]] Backend         activeBackend()  const noexcept;
    [[nodiscard]] HardwareTier    hardwareTier()   const noexcept;
    [[nodiscard]] const DeviceCapabilities& caps() const noexcept;

    // ── Swapchain factory ─────────────────────────────────────────────────
    [[nodiscard]] std::unique_ptr<ISwapchain> createSwapchain(const SwapchainDesc& desc);

    // ── Frame budget monitoring ────────────────────────────────────────────
    void setFrameTimingCallback(FrameTimingCallback cb);

    // ── Async compute queue helper ─────────────────────────────────────────
    // Submits work to the dedicated compute queue (non-blocking to graphics).
    void submitCompute(std::span<const CmdBufHandle> cmds,
                       FenceHandle signalFence = {});

    // Timeline-aware async compute helper.
    // wait/signal values are positionally matched with wait/signal semaphores.
    void submitComputeWithTimeline(
        std::span<const CmdBufHandle> cmds,
        std::span<const SemaphoreHandle> waitSemaphores,
        std::span<const uint64_t> waitValues,
        std::span<const SemaphoreHandle> signalSemaphores,
        std::span<const uint64_t> signalValues,
        FenceHandle signalFence = {});

    // Single-call cross-queue timeline wiring for render-graph style execution.
    // Supports compute->graphics and graphics->compute dependency chains.
    void submitCrossQueueWithTimeline(const CrossQueueTimelineSubmitDesc& desc);

    // Execute a planner-produced queue submission chain.
    void submitPlannedTimelineChain(const RenderPassSubmissionPlan& plan);

private:
    RenderContext(); // use create()

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace nexus::gfx
