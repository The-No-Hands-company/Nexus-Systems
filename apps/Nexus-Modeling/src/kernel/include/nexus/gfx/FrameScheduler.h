#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — IFrameScheduler
//
//  Drives the per-frame Acquire → Record → Submit → Present loop.
//  Manages per-frame command buffers, in-flight fences, and swapchain semaphore
//  coordination.
//
//  Usage (windowed rendering):
//
//      auto sched = std::make_unique<VulkanFrameScheduler>(vulkanDev, vulkanSc);
//
//      while (running) {
//          auto frame = sched->beginFrame();
//          if (!frame) { sched->onResize(newExtent); continue; }
//
//          ICommandBuffer& cb = *frame->cmd;
//          cb.setViewport(...);
//          cb.setScissor(...);
//          cb.beginRenderPass({}, {frame->colorTarget}, {}, clearValues, renderArea);
//          cb.bindPipeline(myPipeline);
//          cb.drawIndexed(idxCount);
//          cb.endRenderPass();
//
//          sched->endFrame();
//      }
//
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <nexus/gfx/CommandBuffer.h>
#include <optional>
#include <cstdint>

namespace nexus::gfx {

// ── FrameContext ──────────────────────────────────────────────────────────────
// Returned by IFrameScheduler::beginFrame().  Valid until endFrame() is called.
struct FrameContext {
    ICommandBuffer* cmd;          // never null; command buffer is already open (begun)
    TextureHandle   colorTarget;  // registered swapchain image for this frame
    Extent2D        extent;       // current swapchain extent
    uint32_t        frameSlot;    // 0..maxFramesInFlight-1  (which in-flight slot)
    uint32_t        imageIndex;   // swapchain image index (same as present index)
    TextureLayout   finalColorLayout = TextureLayout::Present;  // final layout expected for the color target
};

// ── IFrameScheduler ───────────────────────────────────────────────────────────
class IFrameScheduler {
public:
    virtual ~IFrameScheduler() = default;

    // Acquire the next frame.  Returns nullopt when the swapchain is out of date
    // (window resize); caller must call onResize() then retry on the next tick.
    // The returned FrameContext::cmd is already in recording state.
    [[nodiscard]] virtual std::optional<FrameContext> beginFrame() = 0;

    // End recording, submit, and present the current frame.
    // Must only be called after a successful beginFrame().
    virtual void endFrame() = 0;

    // Recreate swapchain for the given extent.  Call when beginFrame/endFrame
    // returns an OutOfDate signal, or on a window resize event.
    virtual void onResize(Extent2D newExtent) = 0;

    [[nodiscard]] virtual uint32_t maxFramesInFlight() const noexcept = 0;

    // Get the currently active FrameContext (after a successful beginFrame).
    // Returns nullptr if no frame is currently active.
    [[nodiscard]] virtual const FrameContext* getCurrentFrame() const noexcept {
        return nullptr;
    }
};

} // namespace nexus::gfx
