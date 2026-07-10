#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanFrameScheduler — Vulkan implementation of IFrameScheduler.
//
//  Manages kMaxFramesInFlight in-flight frames, each with:
//    - A dedicated VkCommandPool (reset per frame — fastest reuse)
//    - A VkCommandBuffer allocated from that pool
//    - A VkFence (CPU wait for this frame slot to finish)
//
//  Swapchain semaphores (imageAvailable / renderFinished) are owned by
//  VulkanSwapchain and indexed by frame slot.
//
//  Swapchain images are registered in VulkanDevice's resource pool as
//  external (isExternalImage=true) TextureHandles so callers can use
//  ICommandBuffer::beginRenderPass() with them directly.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/FrameScheduler.h>
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanCommandBuffer.h"
#include <array>
#include <optional>

namespace nexus::gfx {

class VulkanFrameScheduler final : public IFrameScheduler {
public:
    static constexpr uint32_t kMaxFramesInFlight = 2;  // double-buffer default

    VulkanFrameScheduler(VulkanDevice& device, VulkanSwapchain& swapchain,
                         uint32_t maxInFlight = kMaxFramesInFlight);
    ~VulkanFrameScheduler() override;

    // IFrameScheduler
    [[nodiscard]] std::optional<FrameContext> beginFrame() override;
    void endFrame()                    override;
    void onResize(Extent2D newExtent)  override;
    [[nodiscard]] uint32_t maxFramesInFlight() const noexcept override { return m_maxInFlight; }
    [[nodiscard]] const FrameContext* getCurrentFrame() const noexcept override;

private:
    void createPerFrameResources();
    void destroyPerFrameResources();

    // Register/update swapchain images in the device pool as external textures.
    void registerSwapchainImages();

    VulkanDevice&    m_dev;
    VulkanSwapchain& m_sc;
    uint32_t         m_maxInFlight;

    // Per-frame resources (indexed by frameSlot % m_maxInFlight)
    struct PerFrame {
        VkCommandPool   pool  = VK_NULL_HANDLE;
        VkCommandBuffer cmd   = VK_NULL_HANDLE;
        VkFence         fence = VK_NULL_HANDLE;
        std::unique_ptr<VulkanCommandBuffer> cmdObj;
    };
    std::vector<PerFrame> m_frames;

    // Registered TextureHandles for swapchain images (indexed by imageIndex).
    // These are external — NOT freed by VulkanDevice on shutdown.
    std::vector<TextureHandle> m_swapchainTextures;
    // If the underlying swapchain images are not storage-writable, we
    // keep the raw VkImage handles here (owned by VulkanSwapchain) so we can
    // copy into them from an intermediate storage image at endFrame.
    std::vector<VkImage> m_externalSwapchainImages;

    // When set, m_swapchainTextures point to intermediate storage-backed
    // textures (created by the device). This flag marks that fallback mode.
    bool m_usingIntermediateStorage = false;

    uint32_t m_frameSlot  = 0;  // advances every frame
    uint32_t m_imageIndex = 0;  // set by acquire()

    // Current frame context (valid after beginFrame() returns successfully)
    std::optional<FrameContext> m_currentFrame;

    // vkQueueSubmit2 function pointer (loaded once)
    PFN_vkQueueSubmit2KHR m_pfnSubmit2 = nullptr;
};

} // namespace nexus::gfx
