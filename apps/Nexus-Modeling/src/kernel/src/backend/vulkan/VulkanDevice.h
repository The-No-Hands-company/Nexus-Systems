#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — VulkanDevice
//
//  Implements IDevice on top of Vulkan 1.4.
//  Requires Vulkan SDK headers via find_package(Vulkan REQUIRED).
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/gfx/Allocator.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>

struct VmaAllocator_T;
using VmaAllocator = VmaAllocator_T*;

namespace nexus::gfx {

struct VulkanResourcePool;  // defined in VulkanCommandBuffer.h

class VulkanDevice final : public IDevice {
public:
    VulkanDevice();
    ~VulkanDevice() override;

    // Called once by RenderContext::create()
    void init(const RenderContextDesc& desc);

    // IDevice overrides
    [[nodiscard]] Backend                     backend()    const noexcept override { return Backend::Vulkan; }
    [[nodiscard]] const DeviceCapabilities&   caps()       const noexcept override { return m_caps; }
    [[nodiscard]] HardwareTier                tier()       const noexcept override { return m_tier; }
    [[nodiscard]] std::string_view            deviceName() const noexcept override { return m_deviceName; }

    [[nodiscard]] BufferHandle     createBuffer    (const BufferDesc&)      override;
    [[nodiscard]] TextureHandle    createTexture   (const TextureDesc&)     override;
    [[nodiscard]] ShaderHandle     createShader    (const ShaderDesc&)      override;
    [[nodiscard]] RenderPassHandle createRenderPass(const RenderPassDesc&)  override;
    [[nodiscard]] PipelineHandle   createGraphicsPipeline   (const GraphicsPipelineDesc&)    override;
    [[nodiscard]] PipelineHandle   createMeshShaderPipeline (const MeshShaderPipelineDesc&)  override;
    [[nodiscard]] PipelineHandle   createComputePipeline    (const ComputePipelineDesc&)     override;
    [[nodiscard]] PipelineHandle   createRayTracingPipeline (const RayTracingPipelineDesc&)  override;
    [[nodiscard]] SamplerHandle    createSampler            (const SamplerDesc&)             override;
    [[nodiscard]] QueryPoolHandle  createQueryPool          (const QueryPoolDesc&)           override;
    void readbackTimestamps(QueryPoolHandle, uint32_t, uint32_t, uint64_t*) override;

    void destroyBuffer     (BufferHandle)      override;
    void destroyTexture    (TextureHandle)     override;
    void destroyShader     (ShaderHandle)      override;
    void destroyRenderPass (RenderPassHandle)  override;
    void destroyPipeline   (PipelineHandle)    override;
    void destroySampler    (SamplerHandle)     override;
    void destroyQueryPool  (QueryPoolHandle)   override;

    [[nodiscard]] CmdBufHandle   allocateCommandBuffer(QueueType) override;
    void                         freeCommandBuffer(CmdBufHandle) override;
    [[nodiscard]] ICommandBuffer* getCommandBuffer(CmdBufHandle) override;

    [[nodiscard]] FenceHandle     createFence    (bool signaled)     override;
    [[nodiscard]] SemaphoreHandle createSemaphore()                  override;
    [[nodiscard]] SemaphoreHandle createTimelineSemaphore(uint64_t initialValue) override;
    [[nodiscard]] uint64_t querySemaphoreValue(SemaphoreHandle semaphore) override;
    void  signalSemaphore(SemaphoreHandle semaphore, uint64_t value) override;
    void  waitSemaphore  (SemaphoreHandle semaphore, uint64_t value, uint64_t timeoutNs) override;
    void  destroyFence    (FenceHandle)                              override;
    void  destroySemaphore(SemaphoreHandle)                          override;
    void  waitForFence    (FenceHandle, uint64_t timeoutNs)          override;
    void  resetFence      (FenceHandle)                              override;

    void uploadBuffer (BufferHandle, const void*, uint64_t, uint64_t) override;
    void uploadTexture(TextureHandle, const void*, uint64_t)          override;

    void submit(QueueType, std::span<const CmdBufHandle>,
                std::span<const SemaphoreHandle>, std::span<const SemaphoreHandle>,
                FenceHandle) override;
    void submitWithTimeline(QueueType,
                            std::span<const CmdBufHandle>,
                            std::span<const SemaphoreHandle>,
                            std::span<const uint64_t>,
                            std::span<const SemaphoreHandle>,
                            std::span<const uint64_t>,
                            FenceHandle) override;

    void waitIdle() override;

    [[nodiscard]] AccelStructHandle buildBLAS(BufferHandle, BufferHandle, uint32_t, uint32_t) override;
    [[nodiscard]] AccelStructHandle buildTLAS(std::span<const AccelStructHandle>)             override;
    void destroyAccelStruct(AccelStructHandle) override;

    // ── Descriptor set management ─────────────────────────────────────────
    [[nodiscard]] DescriptorSetHandle allocateDescriptorSet(const DescriptorSetDesc& desc)                               override;
    void updateDescriptorSet(DescriptorSetHandle set, std::span<const DescriptorBindingDesc> bindings)                  override;
    void freeDescriptorSet   (DescriptorSetHandle set)                                                                   override;

    // ── VulkanDevice-specific extras ──────────────────────────────────────
    [[nodiscard]] std::unique_ptr<IGPUAllocator> createAllocator();
    [[nodiscard]] std::unique_ptr<ISwapchain>    createSwapchain(const SwapchainDesc&);

    // Raw Vulkan handles (used internally by other Vulkan objects)
    [[nodiscard]] VkInstance       instance()       const noexcept { return m_instance; }
    [[nodiscard]] VkDevice         logical()        const noexcept { return m_device; }
    [[nodiscard]] VkPhysicalDevice physical()       const noexcept { return m_physDevice; }
    [[nodiscard]] VmaAllocator     vma()            const noexcept; // VMA allocator (defined in .cpp where Impl is complete)
    [[nodiscard]] VkQueue          queue(QueueType) const noexcept;

    // Resource pool access (for VulkanFrameScheduler and other internal Vulkan subsystems)
    [[nodiscard]] VulkanResourcePool* resourcePool() noexcept;
    [[nodiscard]] uint32_t         queueFamily(QueueType) const noexcept;

    // Ray-tracing pipeline shader-group sizing for SBT construction.
    // All fields are zero when the device does not support RT pipelines.
    struct RtPipelineProps {
        uint32_t shaderGroupHandleSize      = 0;
        uint32_t shaderGroupBaseAlignment   = 0;
        uint32_t shaderGroupHandleAlignment = 0;
    };
    [[nodiscard]] const RtPipelineProps& rtPipelineProps() const noexcept { return m_rtPipelineProps; }

private:
    void createInstance(const RenderContextDesc& desc);
    void selectPhysicalDevice();
    void createLogicalDevice(bool enableMesh, bool enableRT);
    void queryCapabilities();
    HardwareTier classifyTier() const noexcept;
    void setupDebugMessenger(ValidationLevel level);

    // Single-use command buffer helpers (used internally + by RT builder)
    static VkCommandBuffer beginSingleUse(VkDevice device, VkCommandPool pool);
    static void endAndSubmitSingleUse(VkDevice device, VkCommandPool pool,
                                       VkQueue queue, VkCommandBuffer cmd);

    RtPipelineProps      m_rtPipelineProps{};

    VkInstance           m_instance   = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkPhysicalDevice     m_physDevice = VK_NULL_HANDLE;
    VkDevice             m_device     = VK_NULL_HANDLE;

    struct QueueData { VkQueue handle = VK_NULL_HANDLE; uint32_t family = UINT32_MAX; };
    QueueData m_queues[static_cast<size_t>(QueueType::Count)];

    DeviceCapabilities m_caps;
    HardwareTier       m_tier       = HardwareTier::Low;
    std::string        m_deviceName;
    bool               m_meshShadersRequested = false;
    bool               m_rayTracingRequested  = false;

    // Handle pool tables (slot-map pattern — no heap per handle)
    struct Impl;
    std::unique_ptr<Impl> m_pool;
};

} // namespace nexus::gfx
