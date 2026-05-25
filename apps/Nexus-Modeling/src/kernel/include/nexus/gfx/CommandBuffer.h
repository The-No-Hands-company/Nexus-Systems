#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — ICommandBuffer
//
//  All draw and dispatch commands are recorded here and submitted via
//  IDevice::submit(). Command buffers are single-use by default.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <cstdint>
#include <span>

namespace nexus::gfx {

// ── Clear values ─────────────────────────────────────────────────────────────
struct ClearColor  { float r = 0.f, g = 0.f, b = 0.f, a = 1.f; };
struct ClearDepth  { float depth = 1.f; uint32_t stencil = 0; };

union ClearValue {
    ClearColor  color;
    ClearDepth  depth;
    ClearValue() : color{} {}   // default: transparent black colour attachment clear
};

// ── Draw indirect args (matches VkDrawIndirectCommand / D3D12 draw args) ──────
struct DrawIndirectArgs {
    uint32_t vertexCount   = 0;
    uint32_t instanceCount = 1;
    uint32_t firstVertex   = 0;
    uint32_t firstInstance = 0;
};

struct DrawIndexedIndirectArgs {
    uint32_t indexCount    = 0;
    uint32_t instanceCount = 1;
    uint32_t firstIndex    = 0;
    int32_t  vertexOffset  = 0;
    uint32_t firstInstance = 0;
};

// ── Dispatch indirect args ────────────────────────────────────────────────────
struct DispatchIndirectArgs {
    uint32_t groupCountX = 1;
    uint32_t groupCountY = 1;
    uint32_t groupCountZ = 1;
};

// ── Memory barrier (coarse, suitable for this abstraction level) ──────────────
struct GlobalMemoryBarrier {
    bool flushShaderWrites  = true;
    bool invalidateAll      = true;
};
// ── Fine-grained barrier types ──────────────────────────────────────────────────
struct TextureBarrier {
    TextureHandle texture;
    TextureLayout oldLayout      = TextureLayout::Undefined;
    TextureLayout newLayout      = TextureLayout::ShaderRead;
    uint32_t      baseMip        = 0;
    uint32_t      mipCount       = ~0u;   // all remaining mip levels
    uint32_t      baseLayer      = 0;
    uint32_t      layerCount     = ~0u;   // all remaining array layers
};

struct BufferBarrier {
    BufferHandle buffer;
    uint64_t     offsetBytes = 0;
    uint64_t     sizeBytes   = ~0ull;             // whole buffer
    BufferAccess srcAccess   = BufferAccess::ShaderWrite;
    BufferAccess dstAccess   = BufferAccess::ShaderRead;
};
// ── ICommandBuffer ────────────────────────────────────────────────────────────
class ICommandBuffer {
public:
    virtual ~ICommandBuffer() = default;

    // ── Lifecycle ──────────────────────────────────────────────────────────
    virtual void begin()  = 0;
    virtual void end()    = 0;
    virtual void reset()  = 0;

    // ── Render pass ────────────────────────────────────────────────────────
    virtual void beginRenderPass(
        RenderPassHandle         renderPass,
        std::span<const TextureHandle>  colorTargets,
        TextureHandle            depthTarget,
        std::span<const ClearValue>     clearValues,
        const Rect2D&            renderArea
    ) = 0;
    virtual void endRenderPass() = 0;

    // ── Pipeline binding ───────────────────────────────────────────────────
    virtual void bindPipeline(PipelineHandle pipeline) = 0;

    // ── Viewport / scissor ─────────────────────────────────────────────────
    virtual void setViewport(const Viewport& vp)  = 0;
    virtual void setScissor (const Rect2D&   rect) = 0;

    // ── Vertex / index buffers ─────────────────────────────────────────────
    virtual void bindVertexBuffer(BufferHandle buffer, uint64_t offset = 0, uint32_t binding = 0) = 0;
    virtual void bindIndexBuffer (BufferHandle buffer, uint64_t offset = 0, bool use16Bit = false) = 0;

    // ── Push constants ─────────────────────────────────────────────────────
    virtual void pushConstants(ShaderStage stages, const void* data, uint32_t sizeBytes, uint32_t offset = 0) = 0;

    // ── Descriptor set binding ─────────────────────────────────────────────
    // Default is a no-op; backends with classic descriptor pools override this.
    // setIndex maps to the descriptor set slot index (Vulkan set=N, DX12 space N).
    virtual void bindDescriptorSet(DescriptorSetHandle set, uint32_t setIndex = 0) {
        (void)set; (void)setIndex;
    }

    // ── Draw calls ─────────────────────────────────────────────────────────
    virtual void draw       (uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount,  uint32_t instanceCount = 1, uint32_t firstIndex  = 0, int32_t  vertexOffset   = 0, uint32_t firstInstance = 0) = 0;
    virtual void drawIndirect       (BufferHandle argsBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride = sizeof(DrawIndirectArgs))        = 0;
    virtual void drawIndexedIndirect(BufferHandle argsBuffer, uint64_t offset, uint32_t drawCount, uint32_t stride = sizeof(DrawIndexedIndirectArgs)) = 0;

    // ── Mesh shader dispatch ───────────────────────────────────────────────
    // Only valid when caps().meshShaders == true
    virtual void drawMeshTasks(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) = 0;
    virtual void drawMeshTasksIndirect(BufferHandle argsBuffer, uint64_t offset, uint32_t drawCount) = 0;

    // ── Compute dispatch ───────────────────────────────────────────────────
    virtual void dispatch        (uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1) = 0;
    virtual void dispatchIndirect(BufferHandle argsBuffer, uint64_t offset) = 0;

    // ── Ray tracing dispatch ───────────────────────────────────────────────
    // Only valid when caps().rayTracingTier >= 2
    virtual void traceRays(uint32_t width, uint32_t height, uint32_t depth = 1) = 0;

    // ── Copies ────────────────────────────────────────────────────────────
    virtual void copyBuffer (BufferHandle  src, BufferHandle  dst, uint64_t sizeBytes, uint64_t srcOffset = 0, uint64_t dstOffset = 0) = 0;
    virtual void copyTexture(TextureHandle src, TextureHandle dst) = 0;
    // Copy a full color texture into a buffer for readback. The image must be in
    // TransferSrc layout and the buffer must have TransferDst usage. Tightly packed.
    // Default no-op for backends that don't support readback copies.
    virtual void copyTextureToBuffer(TextureHandle src, BufferHandle dst) { (void)src; (void)dst; }
    virtual void blitTexture(TextureHandle src, TextureHandle dst, const Rect2D& srcRect, const Rect2D& dstRect) = 0;

    // ── Barriers ──────────────────────────────────────────────────────────
    virtual void globalBarrier   (const GlobalMemoryBarrier& barrier)             = 0;
    virtual void textureBarrier  (const TextureBarrier& barrier)                  = 0;
    virtual void textureBarriers (std::span<const TextureBarrier> barriers)       = 0;
    virtual void bufferBarrier   (const BufferBarrier& barrier)                   = 0;
    virtual void bufferBarriers  (std::span<const BufferBarrier> barriers)        = 0;

    // ── GPU timestamps ─────────────────────────────────────────────────────
    virtual void resetQueryPool  (QueryPoolHandle pool, uint32_t first, uint32_t count) = 0;
    virtual void writeTimestamp  (QueryPoolHandle pool, uint32_t queryIndex)            = 0;

    // ── Debug labels (stripped in release) ───────────────────────────────
    virtual void beginDebugLabel(const char* name, float r = 1.f, float g = 1.f, float b = 1.f) = 0;
    virtual void endDebugLabel  ()                                                               = 0;
    virtual void insertDebugLabel(const char* name) = 0;
};

} // namespace nexus::gfx
