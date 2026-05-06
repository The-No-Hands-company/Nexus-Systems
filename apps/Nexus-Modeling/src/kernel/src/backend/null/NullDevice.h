#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  NullDevice — headless no-op device for testing and CI
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/CommandBuffer.h>
#include <cstring>
#include <vector>
namespace nexus::gfx {

// ── NullCommandBuffer — records nothing, returns immediately ──────────────────
class NullCommandBuffer final : public ICommandBuffer {
public:
    void begin() override {}
    void end()   override {}
    void reset() override {}

    void beginRenderPass(RenderPassHandle, std::span<const TextureHandle>,
                         TextureHandle, std::span<const ClearValue>, const Rect2D&) override {}
    void endRenderPass() override {}

    void bindPipeline  (PipelineHandle)                  override {}
    void setViewport   (const Viewport&)                 override {}
    void setScissor    (const Rect2D&)                   override {}

    void bindVertexBuffer(BufferHandle, uint64_t, uint32_t) override {}
    void bindIndexBuffer (BufferHandle, uint64_t, bool)     override {}
    void pushConstants   (ShaderStage, const void*, uint32_t, uint32_t) override {}

    void draw             (uint32_t, uint32_t, uint32_t, uint32_t) override {}
    void drawIndexed      (uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {}
    void drawIndirect     (BufferHandle, uint64_t, uint32_t, uint32_t) override {}
    void drawIndexedIndirect(BufferHandle, uint64_t, uint32_t, uint32_t) override {}

    void drawMeshTasks         (uint32_t, uint32_t, uint32_t)    override {}
    void drawMeshTasksIndirect (BufferHandle, uint64_t, uint32_t) override {}

    void dispatch        (uint32_t, uint32_t, uint32_t) override {}
    void dispatchIndirect(BufferHandle, uint64_t)        override {}

    void traceRays(uint32_t, uint32_t, uint32_t) override {}

    void copyBuffer (BufferHandle, BufferHandle,  uint64_t, uint64_t, uint64_t) override {}
    void copyTexture(TextureHandle, TextureHandle)                               override {}
    void blitTexture(TextureHandle, TextureHandle, const Rect2D&, const Rect2D&) override {}

    void globalBarrier(const GlobalMemoryBarrier&)                      override {}
    void textureBarrier (const TextureBarrier&)                         override {}
    void textureBarriers(std::span<const TextureBarrier>)               override {}
    void bufferBarrier  (const BufferBarrier&)                          override {}
    void bufferBarriers (std::span<const BufferBarrier>)                override {}

    void resetQueryPool (QueryPoolHandle, uint32_t, uint32_t) override {}
    void writeTimestamp (QueryPoolHandle, uint32_t)           override {}

    void beginDebugLabel (const char*, float, float, float) override {}
    void endDebugLabel   ()                                 override {}
    void insertDebugLabel(const char*)                       override {}
};

// ── NullDevice ────────────────────────────────────────────────────────────────
class NullDevice final : public IDevice {
public:
    [[nodiscard]] Backend                   backend()    const noexcept override { return Backend::Null; }
    [[nodiscard]] const DeviceCapabilities& caps()       const noexcept override { return m_caps; }
    [[nodiscard]] HardwareTier              tier()       const noexcept override { return HardwareTier::Low; }
    [[nodiscard]] std::string_view          deviceName() const noexcept override { return "NullDevice"; }

    [[nodiscard]] BufferHandle     createBuffer    (const BufferDesc&)      override { BufferHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] TextureHandle    createTexture   (const TextureDesc&)     override { TextureHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] ShaderHandle     createShader    (const ShaderDesc&)      override { ShaderHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] RenderPassHandle createRenderPass(const RenderPassDesc&)  override { RenderPassHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] PipelineHandle   createGraphicsPipeline   (const GraphicsPipelineDesc&)    override { PipelineHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] PipelineHandle   createMeshShaderPipeline (const MeshShaderPipelineDesc&)  override { PipelineHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] PipelineHandle   createComputePipeline    (const ComputePipelineDesc&)     override { PipelineHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] PipelineHandle   createRayTracingPipeline (const RayTracingPipelineDesc&)  override { PipelineHandle h{}; h.id = m_ctr++; return h; }

    void destroyBuffer     (BufferHandle)      override {}
    void destroyTexture    (TextureHandle)     override {}
    void destroyShader     (ShaderHandle)      override {}
    void destroyRenderPass (RenderPassHandle)  override {}
    void destroyPipeline   (PipelineHandle)    override {}
    void destroySampler    (SamplerHandle)     override {}
    void destroyQueryPool  (QueryPoolHandle)   override {}

    [[nodiscard]] CmdBufHandle allocateCommandBuffer(QueueType) override {
        CmdBufHandle h{}; h.id = m_ctr++; return h;
    }
    void freeCommandBuffer(CmdBufHandle) override {}
    [[nodiscard]] ICommandBuffer* getCommandBuffer(CmdBufHandle) override { return &m_nullCmdBuf; }

    [[nodiscard]] FenceHandle     createFence    (bool)   override { FenceHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] SemaphoreHandle createSemaphore()       override { SemaphoreHandle h{}; h.id = m_ctr++; return h; }
    void  destroyFence    (FenceHandle)                   override {}
    void  destroySemaphore(SemaphoreHandle)               override {}
    void  waitForFence    (FenceHandle, uint64_t)         override {}
    void  resetFence      (FenceHandle)                   override {}

    void uploadBuffer (BufferHandle, const void*, uint64_t, uint64_t) override {}
    void uploadTexture(TextureHandle, const void*, uint64_t)          override {}

    void submit(QueueType, std::span<const CmdBufHandle>,
                std::span<const SemaphoreHandle>, std::span<const SemaphoreHandle>,
                FenceHandle) override {}

    void waitIdle() override {}

    [[nodiscard]] AccelStructHandle buildBLAS(BufferHandle, BufferHandle, uint32_t, uint32_t) override { return {}; }
    [[nodiscard]] AccelStructHandle buildTLAS(std::span<const AccelStructHandle>)             override { return {}; }
    void destroyAccelStruct(AccelStructHandle) override {}

    [[nodiscard]] SamplerHandle   createSampler   (const SamplerDesc&)    override { SamplerHandle h{}; h.id = m_ctr++; return h; }
    [[nodiscard]] QueryPoolHandle createQueryPool (const QueryPoolDesc&)   override { QueryPoolHandle h{}; h.id = m_ctr++; return h; }
    void readbackTimestamps(QueryPoolHandle, uint32_t, uint32_t, uint64_t* out) override {
        if (out) std::memset(out, 0, sizeof(uint64_t)); // return 0 ns — no GPU in null backend
    }

    // ── Descriptor sets (valid handle so callers can proceed without guarding) ──
    [[nodiscard]] DescriptorSetHandle allocateDescriptorSet(const DescriptorSetDesc&) override {
        DescriptorSetHandle h{};
        if (!m_freeDescriptorSetIds.empty()) {
            h.id = m_freeDescriptorSetIds.back();
            m_freeDescriptorSetIds.pop_back();
            return h;
        }
        h.id = m_ctr++;
        return h;
    }
    void freeDescriptorSet(DescriptorSetHandle set) override {
        if (!set.valid()) {
            return;
        }
        m_freeDescriptorSetIds.push_back(set.id);
    }

private:
    DeviceCapabilities m_caps{};
    uint64_t           m_ctr = 0;
    std::vector<uint64_t> m_freeDescriptorSetIds;
    NullCommandBuffer  m_nullCmdBuf;
};

} // namespace nexus::gfx

