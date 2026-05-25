#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  VulkanCommandBuffer — maps ICommandBuffer to vkCmd* calls.
//  Uses Vulkan 1.3 dynamic rendering (no VkRenderPass objects needed at runtime).
//  Uses VK_KHR_synchronization2 for barriers.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/CommandBuffer.h>
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "VulkanRayTracing.h"   // VulkanAccelStruct for the resource pool
#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

namespace nexus::gfx {

// Forward-declare the pool struct so we can resolve handles → Vk objects
struct VulkanResourcePool;

class VulkanCommandBuffer final : public ICommandBuffer {
public:
    explicit VulkanCommandBuffer(VkCommandBuffer cmd, VkDevice device,
                                  VulkanResourcePool* pool);

    // ── Lifecycle ────────────────────────────────────────────────────────────
    void begin()  override;
    void end()    override;
    void reset()  override;

    // ── Render pass (dynamic rendering) ─────────────────────────────────────
    void beginRenderPass(RenderPassHandle, std::span<const TextureHandle> colorTargets,
                         TextureHandle depthTarget, std::span<const ClearValue> clearValues,
                         const Rect2D& renderArea) override;
    void endRenderPass() override;

    // ── Pipeline ─────────────────────────────────────────────────────────────
    void bindPipeline(PipelineHandle pipeline) override;

    // ── Viewport / scissor ────────────────────────────────────────────────────
    void setViewport(const Viewport& vp)   override;
    void setScissor (const Rect2D&   rect) override;

    // ── Buffers ───────────────────────────────────────────────────────────────
    void bindVertexBuffer(BufferHandle, uint64_t offset, uint32_t binding) override;
    void bindIndexBuffer (BufferHandle, uint64_t offset, bool use16Bit)    override;

    // ── Push constants ────────────────────────────────────────────────────────
    void pushConstants(ShaderStage stages, const void* data, uint32_t sizeBytes,
                       uint32_t offset) override;

    // ── Descriptor sets ───────────────────────────────────────────────────────
    void bindDescriptorSet(DescriptorSetHandle set, uint32_t setIndex) override;

    // ── Draw ──────────────────────────────────────────────────────────────────
    void draw       (uint32_t vtxCount, uint32_t instCount, uint32_t firstVtx, uint32_t firstInst) override;
    void drawIndexed(uint32_t idxCount, uint32_t instCount, uint32_t firstIdx, int32_t  vtxOffset, uint32_t firstInst) override;
    void drawIndirect       (BufferHandle, uint64_t offset, uint32_t drawCount, uint32_t stride) override;
    void drawIndexedIndirect(BufferHandle, uint64_t offset, uint32_t drawCount, uint32_t stride) override;

    // ── Mesh shaders ─────────────────────────────────────────────────────────
    void drawMeshTasks        (uint32_t gx, uint32_t gy, uint32_t gz)             override;
    void drawMeshTasksIndirect(BufferHandle, uint64_t offset, uint32_t drawCount) override;

    // ── Compute ───────────────────────────────────────────────────────────────
    void dispatch        (uint32_t gx, uint32_t gy, uint32_t gz)   override;
    void dispatchIndirect(BufferHandle, uint64_t offset)            override;

    // ── Ray tracing ───────────────────────────────────────────────────────────
    void traceRays(uint32_t width, uint32_t height, uint32_t depth) override;
    void copyTextureToBuffer(TextureHandle src, BufferHandle dst) override;

    // ── Copies ────────────────────────────────────────────────────────────────
    void copyBuffer (BufferHandle src, BufferHandle  dst, uint64_t size, uint64_t srcOff, uint64_t dstOff) override;
    void copyTexture(TextureHandle src, TextureHandle dst)                                                  override;
    void blitTexture(TextureHandle src, TextureHandle dst, const Rect2D& sr, const Rect2D& dr)             override;

    // ── Barriers ─────────────────────────────────────────────────────────────
    void globalBarrier(const GlobalMemoryBarrier& barrier) override;    void textureBarrier (const TextureBarrier& barrier)              override;
    void textureBarriers(std::span<const TextureBarrier> barriers)   override;
    void bufferBarrier  (const BufferBarrier& barrier)               override;
    void bufferBarriers (std::span<const BufferBarrier> barriers)    override;

    // ── GPU timestamps ─────────────────────────────────────────────────────────────────────
    void resetQueryPool (QueryPoolHandle pool, uint32_t first, uint32_t count) override;
    void writeTimestamp (QueryPoolHandle pool, uint32_t queryIndex)            override;
    // ── Debug ─────────────────────────────────────────────────────────────────
    void beginDebugLabel (const char* name, float r, float g, float b) override;
    void endDebugLabel   ()                                             override;
    void insertDebugLabel(const char* name)                             override;

    // ── Image layout transition helper (public for VulkanSwapchain use) ──────
    void transitionImage(VkImage image,
                         VkImageAspectFlags aspect,
                         VkImageLayout oldLayout,
                         VkImageLayout newLayout,
                         VkPipelineStageFlags2 srcStage,
                         VkAccessFlags2        srcAccess,
                         VkPipelineStageFlags2 dstStage,
                         VkAccessFlags2        dstAccess);

    [[nodiscard]] VkCommandBuffer handle() const noexcept { return m_cmd; }

    // ── RT SBT (set before traceRays) ─────────────────────────────────────────
    void bindRayTracingSBT(VkStridedDeviceAddressRegionKHR rayGen,
                           VkStridedDeviceAddressRegionKHR miss,
                           VkStridedDeviceAddressRegionKHR hitGroup,
                           VkStridedDeviceAddressRegionKHR callable);

private:
    VkCommandBuffer     m_cmd    = VK_NULL_HANDLE;
    VkDevice            m_device = VK_NULL_HANDLE;
    VulkanResourcePool* m_pool   = nullptr;

    // Cached current pipeline layout and bind point (needed for push constants and descriptor sets)
    VkPipelineLayout    m_currentPipelineLayout = VK_NULL_HANDLE;
    VkPipelineBindPoint m_currentBindPoint      = VK_PIPELINE_BIND_POINT_GRAPHICS;

    // RT SBT regions (set by bindRayTracingSBT)
    VkStridedDeviceAddressRegionKHR m_sbtRayGen   {};
    VkStridedDeviceAddressRegionKHR m_sbtMiss      {};
    VkStridedDeviceAddressRegionKHR m_sbtHitGroup  {};
    VkStridedDeviceAddressRegionKHR m_sbtCallable  {};

    // Function pointers for extensions (loaded once at construction)
    PFN_vkCmdBeginRenderingKHR       m_pfnBeginRendering    = nullptr;
    PFN_vkCmdEndRenderingKHR         m_pfnEndRendering       = nullptr;
    PFN_vkCmdDrawMeshTasksEXT        m_pfnDrawMeshTasks      = nullptr;
    PFN_vkCmdDrawMeshTasksIndirectEXT m_pfnDrawMeshTasksIndirect = nullptr;
    PFN_vkCmdTraceRaysKHR            m_pfnTraceRays          = nullptr;
    PFN_vkCmdPipelineBarrier2KHR     m_pfnPipelineBarrier2   = nullptr;
    PFN_vkCmdBeginDebugUtilsLabelEXT m_pfnBeginLabel         = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT   m_pfnEndLabel           = nullptr;
    PFN_vkCmdInsertDebugUtilsLabelEXT m_pfnInsertLabel        = nullptr;
};

// ── Shared resource pool (defined in VulkanDevice.cpp, exposed here) ─────────
struct VulkanResourcePool {
    std::vector<VulkanBuffer>   buffers;
    std::vector<VulkanTexture>  textures;
    std::vector<VkShaderModule> shaders;
    std::vector<VkRenderPass>   renderPasses;

    struct PipelineEntry {
        VkPipeline         pipeline  = VK_NULL_HANDLE;
        VkPipelineLayout   layout    = VK_NULL_HANDLE;
        VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        VulkanShaderBindingTableGpu sbt{}; // valid only for ray-tracing pipelines
        std::vector<VkDescriptorSetLayout> ownedSetLayouts; // per-set layouts owned by this pipeline
    };
    std::vector<PipelineEntry>   pipelines;
    std::vector<VkCommandBuffer> cmdBufs;
    std::vector<VkCommandPool>   cmdPools;
    std::vector<VkFence>         fences;
    std::vector<VkSemaphore>     semaphores;
    std::vector<uint8_t>         semaphoreKinds;   // 0=binary, 1=timeline
    std::vector<uint64_t>        semaphoreValues;  // cached timeline values
    std::vector<VulkanAccelStruct> accelStructs;  // full AS + backing buffer

    std::vector<VkSampler>    samplers;
    std::vector<VkQueryPool>  queryPools;

    // Descriptor sets (allocated from shared per-signature pool buckets)
    struct DescSetEntry {
        VkDescriptorPool      pool   = VK_NULL_HANDLE;
        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VkDescriptorSet       set    = VK_NULL_HANDLE;
    };
    std::vector<DescSetEntry> descSets;
    uint64_t nextDescSetId = 0;

    // Pipeline state object (PSO) cache: descriptor hash → PipelineHandle
    // Avoids redundant VkCreateGraphicsPipelines calls for identical descriptors
    std::unordered_map<uint64_t, uint64_t> psoCacheGraphics;     // hash → PipelineHandle.id
    std::unordered_map<uint64_t, uint64_t> psoCacheMeshShader;
    std::unordered_map<uint64_t, uint64_t> psoCacheCompute;
    std::unordered_map<uint64_t, uint64_t> psoCacheRayTracing;

    uint64_t nextBufferId  = 0;
    uint64_t nextTextureId = 0;
    uint64_t nextShaderId  = 0;
    uint64_t nextPassId    = 0;
    uint64_t nextPipeId    = 0;
    uint64_t nextCmdId     = 0;
    uint64_t nextFenceId   = 0;
    uint64_t nextSemId     = 0;
    uint64_t nextASId      = 0;
    uint64_t nextSamplerId = 0;
    uint64_t nextQPoolId   = 0;
};

} // namespace nexus::gfx
