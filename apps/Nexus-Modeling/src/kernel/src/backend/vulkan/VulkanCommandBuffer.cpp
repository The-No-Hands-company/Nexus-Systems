// ─────────────────────────────────────────────────────────────────────────────
//  VulkanCommandBuffer — full vkCmd* implementation
//  Uses Vulkan 1.3 dynamic rendering + synchronization2
// ─────────────────────────────────────────────────────────────────────────────
#include "VulkanCommandBuffer.h"
#include "VulkanUtils.h"
#include <stdexcept>

namespace nexus::gfx {

// ── Helper to load a device function pointer ─────────────────────────────────
template<typename PFN>
static PFN loadFn(VkDevice dev, const char* name) {
    return reinterpret_cast<PFN>(vkGetDeviceProcAddr(dev, name));
}

// ── Constructor ───────────────────────────────────────────────────────────────
VulkanCommandBuffer::VulkanCommandBuffer(VkCommandBuffer cmd, VkDevice device,
                                          VulkanResourcePool* pool)
    : m_cmd(cmd), m_device(device), m_pool(pool)
{
    m_pfnBeginRendering        = loadFn<PFN_vkCmdBeginRenderingKHR>      (device, "vkCmdBeginRenderingKHR");
    m_pfnEndRendering          = loadFn<PFN_vkCmdEndRenderingKHR>        (device, "vkCmdEndRenderingKHR");
    m_pfnDrawMeshTasks         = loadFn<PFN_vkCmdDrawMeshTasksEXT>       (device, "vkCmdDrawMeshTasksEXT");
    m_pfnDrawMeshTasksIndirect = loadFn<PFN_vkCmdDrawMeshTasksIndirectEXT>(device, "vkCmdDrawMeshTasksIndirectEXT");
    m_pfnTraceRays             = loadFn<PFN_vkCmdTraceRaysKHR>           (device, "vkCmdTraceRaysKHR");
    m_pfnPipelineBarrier2      = loadFn<PFN_vkCmdPipelineBarrier2KHR>    (device, "vkCmdPipelineBarrier2KHR");
    m_pfnBeginLabel            = loadFn<PFN_vkCmdBeginDebugUtilsLabelEXT>(device, "vkCmdBeginDebugUtilsLabelEXT");
    m_pfnEndLabel              = loadFn<PFN_vkCmdEndDebugUtilsLabelEXT>  (device, "vkCmdEndDebugUtilsLabelEXT");
    m_pfnInsertLabel           = loadFn<PFN_vkCmdInsertDebugUtilsLabelEXT>(device,"vkCmdInsertDebugUtilsLabelEXT");
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────
void VulkanCommandBuffer::begin()
{
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(m_cmd, &bi);
}

void VulkanCommandBuffer::end()   { vkEndCommandBuffer(m_cmd); }
void VulkanCommandBuffer::reset() { vkResetCommandBuffer(m_cmd, 0); }

// ── Render pass (dynamic rendering) ──────────────────────────────────────────
void VulkanCommandBuffer::beginRenderPass(
    RenderPassHandle /*renderPass*/,
    std::span<const TextureHandle>  colorTargets,
    TextureHandle                   depthTarget,
    std::span<const ClearValue>     clearValues,
    const Rect2D&                   renderArea)
{
    std::vector<VkRenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(colorTargets.size());

    for (size_t i = 0; i < colorTargets.size(); ++i) {
        const auto& tex = m_pool->textures[colorTargets[i].id];

        // Transition to color attachment optimal
        transitionImage(tex.image, VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,    0,
                        VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

        VkRenderingAttachmentInfo att{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
        att.imageView   = tex.defaultView;
        att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        att.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        if (i < clearValues.size()) {
            att.clearValue.color = { clearValues[i].color.r, clearValues[i].color.g,
                                     clearValues[i].color.b, clearValues[i].color.a };
        }
        colorAttachments.push_back(att);
    }

    VkRenderingAttachmentInfo depthAtt{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    bool hasDepth = depthTarget.valid() && depthTarget.id < m_pool->textures.size();
    if (hasDepth) {
        const auto& dtex = m_pool->textures[depthTarget.id];
        transitionImage(dtex.image, VK_IMAGE_ASPECT_DEPTH_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
                        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

        depthAtt.imageView   = dtex.defaultView;
        depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAtt.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
        depthAtt.clearValue.depthStencil = { 0.f, 0 };  // reversed-Z: 0 = far
        // Override from clearValues if provided after color clear values
        if (colorTargets.size() < clearValues.size()) {
            depthAtt.clearValue.depthStencil = {
                clearValues[colorTargets.size()].depth.depth,
                clearValues[colorTargets.size()].depth.stencil
            };
        }
    }

    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea.offset    = { renderArea.offset.x, renderArea.offset.y };
    ri.renderArea.extent    = { renderArea.extent.width, renderArea.extent.height };
    ri.layerCount           = 1;
    ri.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
    ri.pColorAttachments    = colorAttachments.data();
    if (hasDepth) ri.pDepthAttachment = &depthAtt;

    if (m_pfnBeginRendering)
        m_pfnBeginRendering(m_cmd, &ri);
    else
        vkCmdBeginRendering(m_cmd, &ri);  // Vulkan 1.3 core
}

void VulkanCommandBuffer::endRenderPass()
{
    if (m_pfnEndRendering)
        m_pfnEndRendering(m_cmd);
    else
        vkCmdEndRendering(m_cmd);
}

// ── Pipeline ──────────────────────────────────────────────────────────────────
void VulkanCommandBuffer::bindPipeline(PipelineHandle pipeline)
{
    if (!pipeline.valid() || pipeline.id >= m_pool->pipelines.size()) return;
    const auto& entry = m_pool->pipelines[pipeline.id];
    m_currentPipelineLayout = entry.layout;
    m_currentBindPoint      = entry.bindPoint;
    vkCmdBindPipeline(m_cmd, entry.bindPoint, entry.pipeline);
}

// ── Viewport / scissor ────────────────────────────────────────────────────────
void VulkanCommandBuffer::setViewport(const Viewport& vp)
{
    VkViewport v{ vp.x, vp.y, vp.width, vp.height, vp.minDepth, vp.maxDepth };
    vkCmdSetViewport(m_cmd, 0, 1, &v);
}

void VulkanCommandBuffer::setScissor(const Rect2D& rect)
{
    VkRect2D r{{ rect.offset.x, rect.offset.y }, { rect.extent.width, rect.extent.height }};
    vkCmdSetScissor(m_cmd, 0, 1, &r);
}

// ── Vertex / index buffers ────────────────────────────────────────────────────
void VulkanCommandBuffer::bindVertexBuffer(BufferHandle h, uint64_t offset, uint32_t binding)
{
    if (!h.valid() || h.id >= m_pool->buffers.size()) return;
    VkBuffer buf = m_pool->buffers[h.id].handle;
    VkDeviceSize off = offset;
    vkCmdBindVertexBuffers(m_cmd, binding, 1, &buf, &off);
}

void VulkanCommandBuffer::bindIndexBuffer(BufferHandle h, uint64_t offset, bool use16Bit)
{
    if (!h.valid() || h.id >= m_pool->buffers.size()) return;
    vkCmdBindIndexBuffer(m_cmd, m_pool->buffers[h.id].handle, offset,
                         use16Bit ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);
}

// ── Push constants ────────────────────────────────────────────────────────────
void VulkanCommandBuffer::pushConstants(ShaderStage stages, const void* data,
                                         uint32_t sizeBytes, uint32_t offset)
{
    if (!m_currentPipelineLayout) return;
    vkCmdPushConstants(m_cmd, m_currentPipelineLayout,
                       vkutil::toVkShaderStage(stages),
                       offset, sizeBytes, data);
}

// ── Descriptor sets ───────────────────────────────────────────────────────────
void VulkanCommandBuffer::bindDescriptorSet(DescriptorSetHandle handle, uint32_t setIndex)
{
    if (!handle.valid() || handle.id >= m_pool->descSets.size()) return;
    if (!m_currentPipelineLayout) return;
    VkDescriptorSet vkSet = m_pool->descSets[handle.id].set;
    if (!vkSet) return;
    vkCmdBindDescriptorSets(m_cmd, m_currentBindPoint, m_currentPipelineLayout,
                             setIndex, 1, &vkSet, 0, nullptr);
}

// ── Draw calls ────────────────────────────────────────────────────────────────
void VulkanCommandBuffer::draw(uint32_t vtxCount, uint32_t instCount,
                                uint32_t firstVtx, uint32_t firstInst)
{
    vkCmdDraw(m_cmd, vtxCount, instCount, firstVtx, firstInst);
}

void VulkanCommandBuffer::drawIndexed(uint32_t idxCount, uint32_t instCount,
                                       uint32_t firstIdx, int32_t vtxOffset,
                                       uint32_t firstInst)
{
    vkCmdDrawIndexed(m_cmd, idxCount, instCount, firstIdx, vtxOffset, firstInst);
}

void VulkanCommandBuffer::drawIndirect(BufferHandle h, uint64_t offset,
                                        uint32_t drawCount, uint32_t stride)
{
    if (!h.valid() || h.id >= m_pool->buffers.size()) return;
    vkCmdDrawIndirect(m_cmd, m_pool->buffers[h.id].handle, offset, drawCount, stride);
}

void VulkanCommandBuffer::drawIndexedIndirect(BufferHandle h, uint64_t offset,
                                               uint32_t drawCount, uint32_t stride)
{
    if (!h.valid() || h.id >= m_pool->buffers.size()) return;
    vkCmdDrawIndexedIndirect(m_cmd, m_pool->buffers[h.id].handle, offset, drawCount, stride);
}

// ── Mesh shaders ──────────────────────────────────────────────────────────────
void VulkanCommandBuffer::drawMeshTasks(uint32_t gx, uint32_t gy, uint32_t gz)
{
    if (m_pfnDrawMeshTasks) m_pfnDrawMeshTasks(m_cmd, gx, gy, gz);
}

void VulkanCommandBuffer::drawMeshTasksIndirect(BufferHandle h, uint64_t offset,
                                                 uint32_t drawCount)
{
    if (!m_pfnDrawMeshTasksIndirect || !h.valid() || h.id >= m_pool->buffers.size()) return;
    m_pfnDrawMeshTasksIndirect(m_cmd, m_pool->buffers[h.id].handle,
                               offset, drawCount, sizeof(DrawIndirectArgs));
}

// ── Compute ───────────────────────────────────────────────────────────────────
void VulkanCommandBuffer::dispatch(uint32_t gx, uint32_t gy, uint32_t gz)
{
    vkCmdDispatch(m_cmd, gx, gy, gz);
}

void VulkanCommandBuffer::dispatchIndirect(BufferHandle h, uint64_t offset)
{
    if (!h.valid() || h.id >= m_pool->buffers.size()) return;
    vkCmdDispatchIndirect(m_cmd, m_pool->buffers[h.id].handle, offset);
}

// ── Ray tracing ───────────────────────────────────────────────────────────────
void VulkanCommandBuffer::traceRays(uint32_t width, uint32_t height, uint32_t depth)
{
    if (!m_pfnTraceRays) return;
    m_pfnTraceRays(m_cmd, &m_sbtRayGen, &m_sbtMiss, &m_sbtHitGroup, &m_sbtCallable,
                   width, height, depth);
}

void VulkanCommandBuffer::bindRayTracingSBT(VkStridedDeviceAddressRegionKHR rg,
                                             VkStridedDeviceAddressRegionKHR miss,
                                             VkStridedDeviceAddressRegionKHR hit,
                                             VkStridedDeviceAddressRegionKHR callable)
{
    m_sbtRayGen   = rg;
    m_sbtMiss     = miss;
    m_sbtHitGroup = hit;
    m_sbtCallable = callable;
}

// ── Copies ────────────────────────────────────────────────────────────────────
void VulkanCommandBuffer::copyBuffer(BufferHandle src, BufferHandle dst,
                                      uint64_t size, uint64_t srcOff, uint64_t dstOff)
{
    if (!src.valid() || !dst.valid()) return;
    VkBufferCopy region{ srcOff, dstOff, size };
    vkCmdCopyBuffer(m_cmd, m_pool->buffers[src.id].handle,
                            m_pool->buffers[dst.id].handle, 1, &region);
}

void VulkanCommandBuffer::copyTexture(TextureHandle src, TextureHandle dst)
{
    if (!src.valid() || !dst.valid()) return;
    const auto& s = m_pool->textures[src.id];
    const auto& d = m_pool->textures[dst.id];

    VkImageCopy region{};
    region.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    region.extent         = s.extent;
    vkCmdCopyImage(m_cmd,
                   s.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   d.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &region);
}

void VulkanCommandBuffer::blitTexture(TextureHandle src, TextureHandle dst,
                                       const Rect2D& sr, const Rect2D& dr)
{
    if (!src.valid() || !dst.valid()) return;
    const auto& s = m_pool->textures[src.id];
    const auto& d = m_pool->textures[dst.id];

    VkImageBlit blit{};
    blit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.srcOffsets[0]  = { sr.offset.x, sr.offset.y, 0 };
    blit.srcOffsets[1]  = { (int32_t)(sr.offset.x + sr.extent.width),
                             (int32_t)(sr.offset.y + sr.extent.height), 1 };
    blit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    blit.dstOffsets[0]  = { dr.offset.x, dr.offset.y, 0 };
    blit.dstOffsets[1]  = { (int32_t)(dr.offset.x + dr.extent.width),
                             (int32_t)(dr.offset.y + dr.extent.height), 1 };
    vkCmdBlitImage(m_cmd,
                   s.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   d.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_LINEAR);
}

// ── Barriers ──────────────────────────────────────────────────────────────────
void VulkanCommandBuffer::globalBarrier(const GlobalMemoryBarrier& b)
{
    VkMemoryBarrier2 mb{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    if (b.flushShaderWrites)
        mb.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        mb.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    if (b.invalidateAll)
        mb.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        mb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;

    VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.memoryBarrierCount = 1;
    di.pMemoryBarriers    = &mb;

    if (m_pfnPipelineBarrier2)
        m_pfnPipelineBarrier2(m_cmd, &di);
    else
        vkCmdPipelineBarrier2(m_cmd, &di);  // Vulkan 1.3 core
}

void VulkanCommandBuffer::transitionImage(VkImage image, VkImageAspectFlags aspect,
                                           VkImageLayout oldLayout, VkImageLayout newLayout,
                                           VkPipelineStageFlags2 srcStage,
                                           VkAccessFlags2        srcAccess,
                                           VkPipelineStageFlags2 dstStage,
                                           VkAccessFlags2        dstAccess)
{
    VkImageMemoryBarrier2 imb{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imb.srcStageMask                 = srcStage;
    imb.srcAccessMask                = srcAccess;
    imb.dstStageMask                 = dstStage;
    imb.dstAccessMask                = dstAccess;
    imb.oldLayout                    = oldLayout;
    imb.newLayout                    = newLayout;
    imb.srcQueueFamilyIndex          = VK_QUEUE_FAMILY_IGNORED;
    imb.dstQueueFamilyIndex          = VK_QUEUE_FAMILY_IGNORED;
    imb.image                        = image;
    imb.subresourceRange.aspectMask  = aspect;
    imb.subresourceRange.levelCount  = VK_REMAINING_MIP_LEVELS;
    imb.subresourceRange.layerCount  = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers    = &imb;

    if (m_pfnPipelineBarrier2)
        m_pfnPipelineBarrier2(m_cmd, &di);
    else
        vkCmdPipelineBarrier2(m_cmd, &di);
}

// ── Fine-grained texture barriers ────────────────────────────────────────────
void VulkanCommandBuffer::textureBarrier(const TextureBarrier& b)
{
    if (!b.texture.valid() || b.texture.id >= m_pool->textures.size()) return;
    const auto& tex = m_pool->textures[b.texture.id];
    VkImageAspectFlags aspect = vkutil::isDepthFormat(tex.format)
        ? (VK_IMAGE_ASPECT_DEPTH_BIT | (vkutil::hasStencil(tex.format) ? VK_IMAGE_ASPECT_STENCIL_BIT : 0))
        : VK_IMAGE_ASPECT_COLOR_BIT;

    VkImageMemoryBarrier2 imb{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    imb.oldLayout                       = vkutil::toVkImageLayout(b.oldLayout);
    imb.newLayout                       = vkutil::toVkImageLayout(b.newLayout);
    imb.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    imb.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    imb.image                           = tex.image;
    imb.subresourceRange.aspectMask     = aspect;
    imb.subresourceRange.baseMipLevel   = b.baseMip;
    imb.subresourceRange.levelCount     = b.mipCount;
    imb.subresourceRange.baseArrayLayer = b.baseLayer;
    imb.subresourceRange.layerCount     = b.layerCount;

    switch (b.oldLayout) {
    case TextureLayout::ColorAttachment:
        imb.srcStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imb.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; break;
    case TextureLayout::DepthWrite:
        imb.srcStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        imb.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; break;
    case TextureLayout::ShaderReadWrite: case TextureLayout::General:
        imb.srcStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imb.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT; break;
    case TextureLayout::TransferDst:
        imb.srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
        imb.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT; break;
    default:
        imb.srcStageMask  = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        imb.srcAccessMask = 0; break;
    }
    switch (b.newLayout) {
    case TextureLayout::ColorAttachment:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        imb.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT; break;
    case TextureLayout::DepthWrite:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        imb.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT; break;
    case TextureLayout::DepthRead:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        imb.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT; break;
    case TextureLayout::ShaderRead:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
        imb.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT; break;
    case TextureLayout::ShaderReadWrite: case TextureLayout::General:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imb.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT; break;
    case TextureLayout::TransferSrc:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
        imb.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT; break;
    case TextureLayout::TransferDst:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
        imb.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT; break;
    case TextureLayout::Present:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
        imb.dstAccessMask = 0; break;
    default:
        imb.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imb.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT; break;
    }

    VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers    = &imb;
    if (m_pfnPipelineBarrier2) m_pfnPipelineBarrier2(m_cmd, &di);
    else                       vkCmdPipelineBarrier2(m_cmd, &di);
}

void VulkanCommandBuffer::textureBarriers(std::span<const TextureBarrier> barriers)
{
    for (const auto& b : barriers) textureBarrier(b);
}

void VulkanCommandBuffer::bufferBarrier(const BufferBarrier& b)
{
    if (!b.buffer.valid() || b.buffer.id >= m_pool->buffers.size()) return;
    VkBuffer vkBuf = m_pool->buffers[b.buffer.id].handle;
    if (!vkBuf) return;

    auto src = vkutil::toVkBufferBarrierMask(b.srcAccess);
    auto dst = vkutil::toVkBufferBarrierMask(b.dstAccess);

    VkBufferMemoryBarrier2 bmb{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2};
    bmb.srcStageMask        = src.stage;
    bmb.srcAccessMask       = src.access;
    bmb.dstStageMask        = dst.stage;
    bmb.dstAccessMask       = dst.access;
    bmb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    bmb.buffer              = vkBuf;
    bmb.offset              = b.offsetBytes;
    bmb.size                = b.sizeBytes;  // VK_WHOLE_SIZE == ~0ull

    VkDependencyInfo di{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.bufferMemoryBarrierCount = 1;
    di.pBufferMemoryBarriers    = &bmb;
    if (m_pfnPipelineBarrier2) m_pfnPipelineBarrier2(m_cmd, &di);
    else                       vkCmdPipelineBarrier2(m_cmd, &di);
}

void VulkanCommandBuffer::bufferBarriers(std::span<const BufferBarrier> barriers)
{
    for (const auto& b : barriers) bufferBarrier(b);
}

// ── GPU timestamps ────────────────────────────────────────────────────────────
void VulkanCommandBuffer::resetQueryPool(QueryPoolHandle pool, uint32_t first, uint32_t count)
{
    if (!pool.valid() || pool.id >= m_pool->queryPools.size()) return;
    VkQueryPool qp = m_pool->queryPools[pool.id];
    if (qp) vkCmdResetQueryPool(m_cmd, qp, first, count);
}

void VulkanCommandBuffer::writeTimestamp(QueryPoolHandle pool, uint32_t queryIndex)
{
    if (!pool.valid() || pool.id >= m_pool->queryPools.size()) return;
    VkQueryPool qp = m_pool->queryPools[pool.id];
    if (qp) vkCmdWriteTimestamp2(m_cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, qp, queryIndex);
}

// ── Debug labels ─────────────────────────────────────────────────────────────
void VulkanCommandBuffer::beginDebugLabel(const char* name, float r, float g, float b)
{
    if (!m_pfnBeginLabel) return;
    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    label.color[0] = r; label.color[1] = g;
    label.color[2] = b; label.color[3] = 1.f;
    m_pfnBeginLabel(m_cmd, &label);
}

void VulkanCommandBuffer::endDebugLabel()
{
    if (m_pfnEndLabel) m_pfnEndLabel(m_cmd);
}

void VulkanCommandBuffer::insertDebugLabel(const char* name)
{
    if (!m_pfnInsertLabel) return;
    VkDebugUtilsLabelEXT label{VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT};
    label.pLabelName = name;
    m_pfnInsertLabel(m_cmd, &label);
}

} // namespace nexus::gfx
