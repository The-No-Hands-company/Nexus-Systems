// ─────────────────────────────────────────────────────────────────────────────
//  VulkanFrameScheduler — Acquire → Record → Submit → Present loop
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanFrameScheduler.h"
#include "VulkanTexture.h"
#include "VulkanUtils.h"
#include <stdexcept>
#include <cstdio>
#include <algorithm>

namespace nexus::gfx {

// ── Constructor / Destructor ──────────────────────────────────────────────────

VulkanFrameScheduler::VulkanFrameScheduler(VulkanDevice& device,
                                            VulkanSwapchain& swapchain,
                                            uint32_t maxInFlight)
    : m_dev(device)
    , m_sc(swapchain)
    , m_maxInFlight(std::clamp(maxInFlight, 1u, kMaxFramesInFlight))
{
    // Load vkQueueSubmit2 — core in Vulkan 1.3, also available as KHR extension
    m_pfnSubmit2 = reinterpret_cast<PFN_vkQueueSubmit2KHR>(
        vkGetDeviceProcAddr(m_dev.logical(), "vkQueueSubmit2KHR"));
    if (!m_pfnSubmit2) {
        // Try the non-KHR name (Vulkan 1.3 core)
        m_pfnSubmit2 = reinterpret_cast<PFN_vkQueueSubmit2KHR>(
            vkGetDeviceProcAddr(m_dev.logical(), "vkQueueSubmit2"));
    }
    if (!m_pfnSubmit2)
        throw std::runtime_error("VulkanFrameScheduler: vkQueueSubmit2 not available");

    createPerFrameResources();
    registerSwapchainImages();
}

VulkanFrameScheduler::~VulkanFrameScheduler()
{
    m_dev.waitIdle();
    destroyPerFrameResources();
    // Note: swapchain TextureHandles remain in the pool as tombstones with
    // isExternalImage=true — VulkanDevice destructor skips them safely.
}

// ── Per-frame resource management ────────────────────────────────────────────

void VulkanFrameScheduler::createPerFrameResources()
{
    m_frames.resize(m_maxInFlight);
    const uint32_t qfamily = m_dev.queueFamily(QueueType::Graphics);

    for (auto& f : m_frames) {
        // Command pool (transient, reset per frame — no individual buffer resets)
        VkCommandPoolCreateInfo pci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.queueFamilyIndex = qfamily;
        pci.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        if (vkCreateCommandPool(m_dev.logical(), &pci, nullptr, &f.pool) != VK_SUCCESS)
            throw std::runtime_error("VulkanFrameScheduler: vkCreateCommandPool failed");

        // Primary command buffer
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool        = f.pool;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (vkAllocateCommandBuffers(m_dev.logical(), &ai, &f.cmd) != VK_SUCCESS)
            throw std::runtime_error("VulkanFrameScheduler: vkAllocateCommandBuffers failed");

        // In-flight fence (start signaled so the first frame doesn't block)
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (vkCreateFence(m_dev.logical(), &fi, nullptr, &f.fence) != VK_SUCCESS)
            throw std::runtime_error("VulkanFrameScheduler: vkCreateFence failed");

        // VulkanCommandBuffer wrapper (needs the device's resource pool for handle
        // resolution when callers call bindPipeline / bindVertexBuffer / etc.)
        f.cmdObj = std::make_unique<VulkanCommandBuffer>(f.cmd, m_dev.logical(),
                                                          m_dev.resourcePool());
    }
}

void VulkanFrameScheduler::destroyPerFrameResources()
{
    for (auto& f : m_frames) {
        f.cmdObj.reset();
        if (f.fence) vkDestroyFence      (m_dev.logical(), f.fence, nullptr);
        if (f.pool)  vkDestroyCommandPool(m_dev.logical(), f.pool,  nullptr);
        f.fence = VK_NULL_HANDLE;
        f.pool  = VK_NULL_HANDLE;
        f.cmd   = VK_NULL_HANDLE;
    }
    m_frames.clear();
}

// ── Swapchain image registration ──────────────────────────────────────────────

void VulkanFrameScheduler::registerSwapchainImages()
{
    const uint32_t count = m_sc.imageCount();
    VulkanResourcePool* pool = m_dev.resourcePool();
    m_externalSwapchainImages.resize(count);
    if (m_swapchainTextures.empty()) {
        // First time — allocate new slots
        m_swapchainTextures.resize(count);
        const bool imagesRequestedStorage = (m_sc.vkImageUsageFlags() & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
        m_usingIntermediateStorage = !imagesRequestedStorage;

        for (uint32_t i = 0; i < count; ++i) {
            m_externalSwapchainImages[i] = m_sc.image(i);

            VulkanTexture tex{};
            tex.image            = m_sc.image(i);
            tex.defaultView      = m_sc.imageView(i);
            tex.allocation       = VK_NULL_HANDLE;  // not VMA-managed
            tex.isExternalImage  = true;
            tex.vkFormat         = m_sc.vkColorFormat();
            tex.format           = m_sc.colorFormat();
            tex.extent           = { m_sc.extent().width, m_sc.extent().height, 1 };
            tex.mipLevels        = 1;
            tex.arrayLayers      = 1;
            tex.usage            = TextureUsage::ColorAttachment;

            TextureHandle h{};
            h.id = pool->nextTextureId;
            if (pool->nextTextureId >= pool->textures.size())
                pool->textures.resize(pool->nextTextureId + 1);
            pool->textures[pool->nextTextureId++] = tex;
            m_swapchainTextures[i] = h;

            if (!imagesRequestedStorage) {
                TextureDesc sd{};
                sd.extent.width  = m_sc.extent().width;
                sd.extent.height = m_sc.extent().height;
                sd.extent.depth  = 1;
                sd.format        = m_sc.colorFormat();
                sd.usage         = TextureUsage::ColorAttachment
                                 | TextureUsage::Storage
                                 | TextureUsage::TransferSrc;
                sd.mipLevels     = 1;
                sd.arrayLayers   = 1;
                sd.debugName     = "Swapchain.IntermediateStorage";

                TextureHandle storageHandle = m_dev.createTexture(sd);
                if (storageHandle.valid()) {
                    m_swapchainTextures[i] = storageHandle;
                }
            }
        }
    } else {
        // Swapchain was recreated — update existing slots in-place (handles stay valid)
        for (uint32_t i = 0; i < count; ++i) {
            m_externalSwapchainImages[i] = m_sc.image(i);
            if (i < static_cast<uint32_t>(m_swapchainTextures.size())) {
                auto& currentHandle = m_swapchainTextures[i];
                if (!m_usingIntermediateStorage) {
                    auto& entry = pool->textures[currentHandle.id];
                    entry.image       = m_sc.image(i);
                    entry.defaultView = m_sc.imageView(i);
                    entry.vkFormat    = m_sc.vkColorFormat();
                    entry.format      = m_sc.colorFormat();
                    entry.extent      = { m_sc.extent().width, m_sc.extent().height, 1 };
                } else {
                    // Keep the existing intermediate storage texture handles intact.
                    // They are separate from the external swapchain VkImage handles.
                }
            } else {
                if (m_usingIntermediateStorage) {
                    // New swapchain image count requires new intermediate storage textures.
                    TextureDesc sd{};
                    sd.extent.width  = m_sc.extent().width;
                    sd.extent.height = m_sc.extent().height;
                    sd.extent.depth  = 1;
                    sd.format        = m_sc.colorFormat();
                    sd.usage         = TextureUsage::ColorAttachment
                                     | TextureUsage::Storage
                                     | TextureUsage::TransferSrc;
                    sd.mipLevels     = 1;
                    sd.arrayLayers   = 1;
                    sd.debugName     = "Swapchain.IntermediateStorage";

                    TextureHandle storageHandle = m_dev.createTexture(sd);
                    if (storageHandle.valid()) {
                        m_swapchainTextures.push_back(storageHandle);
                    }
                } else {
                    VulkanTexture tex{};
                    tex.image            = m_sc.image(i);
                    tex.defaultView      = m_sc.imageView(i);
                    tex.allocation       = VK_NULL_HANDLE;
                    tex.isExternalImage  = true;
                    tex.vkFormat         = m_sc.vkColorFormat();
                    tex.format           = m_sc.colorFormat();
                    tex.extent           = { m_sc.extent().width, m_sc.extent().height, 1 };
                    tex.mipLevels        = 1;
                    tex.arrayLayers      = 1;
                    tex.usage            = TextureUsage::ColorAttachment;

                    TextureHandle h{};
                    h.id = pool->nextTextureId;
                    if (pool->nextTextureId >= pool->textures.size())
                        pool->textures.resize(pool->nextTextureId + 1);
                    pool->textures[pool->nextTextureId++] = tex;
                    m_swapchainTextures.push_back(h);
                }
            }
        }
    }
}

// ── beginFrame ────────────────────────────────────────────────────────────────

std::optional<FrameContext> VulkanFrameScheduler::beginFrame()
{
    const uint32_t slot = m_frameSlot % m_maxInFlight;
    PerFrame& f = m_frames[slot];

    // 1. Wait for this slot's previous frame to finish on GPU
    vkWaitForFences(m_dev.logical(), 1, &f.fence, VK_TRUE, UINT64_MAX);

    // 2. Acquire next swapchain image
    AcquiredFrame acquired = m_sc.acquire();
    if (acquired.result == AcquireResult::OutOfDate) {
        return std::nullopt;  // caller must call onResize()
    }
    m_imageIndex = acquired.imageIndex;

    // 3. Reset fence only after acquire succeeds (spec: fence must not be
    //    reset before it's safe to reuse the slot)
    vkResetFences(m_dev.logical(), 1, &f.fence);

    // 4. Reset the command pool — reclaims all allocation memory at once
    //    (faster than resetting individual command buffers)
    vkResetCommandPool(m_dev.logical(), f.pool, 0);

    // 5. Begin recording
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(f.cmd, &bi);
    // (f.cmdObj wraps the same VkCommandBuffer — it's now in recording state)

    // 6. Resolve color target handle
    TextureHandle colorTarget{};
    if (m_imageIndex < static_cast<uint32_t>(m_swapchainTextures.size()))
        colorTarget = m_swapchainTextures[m_imageIndex];

    FrameContext ctx{};
    ctx.cmd              = f.cmdObj.get();
    ctx.colorTarget      = colorTarget;
    ctx.extent           = m_sc.extent();
    ctx.frameSlot        = slot;
    ctx.imageIndex       = m_imageIndex;
    ctx.finalColorLayout = m_usingIntermediateStorage ? TextureLayout::TransferSrc
                                                      : TextureLayout::Present;
    return ctx;
    m_currentFrame = ctx;  // Store for later access
    return ctx;
}

// ── endFrame ─────────────────────────────────────────────────────────────────

void VulkanFrameScheduler::endFrame()
{
    const uint32_t slot = m_frameSlot % m_maxInFlight;
    PerFrame& f = m_frames[slot];

    // If the swapchain images were created without STORAGE usage, copy the
    // intermediate storage-backed color result into the actual swapchain image.
    if (m_usingIntermediateStorage && m_imageIndex < static_cast<uint32_t>(m_externalSwapchainImages.size())) {
        TextureHandle sourceColor = m_swapchainTextures[m_imageIndex];
        if (sourceColor.valid()) {
            const auto& tex = m_dev.resourcePool()->textures[sourceColor.id];
            VkImage targetSwapchainImage = m_externalSwapchainImages[m_imageIndex];
            if (tex.image != VK_NULL_HANDLE && targetSwapchainImage != VK_NULL_HANDLE) {
                VkImageMemoryBarrier2 transferBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                transferBarrier.srcStageMask    = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                transferBarrier.srcAccessMask   = 0;
                transferBarrier.dstStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT;
                transferBarrier.dstAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                transferBarrier.oldLayout       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                transferBarrier.newLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                transferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                transferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                transferBarrier.image           = targetSwapchainImage;
                transferBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                transferBarrier.subresourceRange.baseMipLevel = 0;
                transferBarrier.subresourceRange.levelCount = 1;
                transferBarrier.subresourceRange.baseArrayLayer = 0;
                transferBarrier.subresourceRange.layerCount = 1;

                VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
                di.imageMemoryBarrierCount = 1;
                di.pImageMemoryBarriers    = &transferBarrier;
                vkCmdPipelineBarrier2(f.cmd, &di);

                VkImageCopy copyRegion{};
                copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
                copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
                copyRegion.extent         = tex.extent;

                vkCmdCopyImage(f.cmd,
                               tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               targetSwapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1, &copyRegion);

                VkImageMemoryBarrier2 presentBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
                presentBarrier.srcStageMask    = VK_PIPELINE_STAGE_2_COPY_BIT;
                presentBarrier.srcAccessMask   = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                presentBarrier.dstStageMask    = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
                presentBarrier.dstAccessMask   = 0;
                presentBarrier.oldLayout       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                presentBarrier.newLayout       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                presentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                presentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                presentBarrier.image           = targetSwapchainImage;
                presentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                presentBarrier.subresourceRange.baseMipLevel = 0;
                presentBarrier.subresourceRange.levelCount = 1;
                presentBarrier.subresourceRange.baseArrayLayer = 0;
                presentBarrier.subresourceRange.layerCount = 1;

                di.imageMemoryBarrierCount = 1;
                di.pImageMemoryBarriers    = &presentBarrier;
                vkCmdPipelineBarrier2(f.cmd, &di);
            }
        }
    }

    // 1. End command buffer recording
    vkEndCommandBuffer(f.cmd);

    // 2. Submit with sync2
    //    wait:   imageAvailSem[slot] at COLOR_ATTACHMENT_OUTPUT stage
    //    signal: renderDoneSem[slot]
    //    signal: f.fence (CPU notification that this slot is free)
    VkSemaphore imgAvail   = m_sc.imageAvailSem(slot);
    VkSemaphore renderDone = m_sc.renderDoneSem(slot);

    VkCommandBufferSubmitInfo cbsi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cbsi.commandBuffer = f.cmd;
    cbsi.deviceMask    = 0;

    VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waitInfo.semaphore = imgAvail;
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;
    waitInfo.value     = 0;  // binary semaphore

    VkSemaphoreSubmitInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signalInfo.semaphore = renderDone;
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;
    signalInfo.value     = 0;  // binary semaphore

    VkSubmitInfo2KHR si2{VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR};
    si2.waitSemaphoreInfoCount   = (imgAvail   != VK_NULL_HANDLE) ? 1u : 0u;
    si2.pWaitSemaphoreInfos      = &waitInfo;
    si2.commandBufferInfoCount   = 1;
    si2.pCommandBufferInfos      = &cbsi;
    si2.signalSemaphoreInfoCount = (renderDone != VK_NULL_HANDLE) ? 1u : 0u;
    si2.pSignalSemaphoreInfos    = &signalInfo;

    m_pfnSubmit2(m_dev.queue(QueueType::Graphics), 1, &si2, f.fence);

    // 3. Present (wait on renderDone semaphore, by slot index — swapchain decodes it)
    SemaphoreHandle renderDoneHandle{};
    renderDoneHandle.id = slot;
    PresentResult pr = m_sc.present(m_imageIndex, renderDoneHandle);
    if (pr == PresentResult::OutOfDate) {
        // Will be caught on next beginFrame() as AcquireResult::OutOfDate
        // and the caller will call onResize().  No action needed here.
    }

// 4. Advance frame slot
    ++m_frameSlot;
}

// ── getCurrentFrame ─────────────────────────────────────────────────────────────
const FrameContext* VulkanFrameScheduler::getCurrentFrame() const noexcept {
    // We need to return the current active frame context
    // The active frame is at slot (m_frameSlot - 1) % m_maxInFlight
    if (m_frameSlot == 0 || m_frames.empty()) return nullptr;
    
    const uint32_t slot = (m_frameSlot - 1) % m_maxInFlight;
    const PerFrame& f = m_frames[slot];
    if (!f.cmdObj) return nullptr;

    static thread_local FrameContext cachedCtx;
    cachedCtx.cmd          = f.cmdObj.get();
    cachedCtx.colorTarget  = TextureHandle{}; // Would need to be filled from swapchain
    cachedCtx.extent       = m_sc.extent();
    cachedCtx.frameSlot    = slot % m_maxInFlight;
    cachedCtx.imageIndex   = m_imageIndex;
    cachedCtx.finalColorLayout = TextureLayout::Present;
    return &cachedCtx;
}

void nexus::gfx::VulkanFrameScheduler::onResize(nexus::gfx::Extent2D newExtent) {
    m_sc.resize(newExtent);
    registerSwapchainImages();
    m_frameSlot = 0;
}

} // namespace nexus::gfx
