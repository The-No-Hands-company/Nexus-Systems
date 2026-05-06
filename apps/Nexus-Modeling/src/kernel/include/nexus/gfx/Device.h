#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — IDevice: logical GPU device interface
//
//  Every backend (Vulkan, Metal, DX12, Null) derives from this.
//  The application always programs against IDevice — never the concrete type.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Types.h>
#include <cstddef>
#include <span>
#include <string_view>
#include <memory>

namespace nexus::gfx {

class ICommandBuffer;  // forward declare — full type in CommandBuffer.h

// ── Descriptor structures (POD — trivially copyable) ─────────────────────────

struct BufferDesc {
    uint64_t    sizeBytes = 0;
    BufferUsage usage     = BufferUsage::None;
    MemoryHint  memory    = MemoryHint::GpuOnly;
    const char* debugName = nullptr;
};

struct TextureDesc {
    Extent3D    extent;
    uint32_t    mipLevels  = 1;
    uint32_t    arrayLayers= 1;
    Format      format     = Format::Undefined;
    TextureUsage usage     = TextureUsage::Sampled;
    SampleCount samples    = SampleCount::X1;
    MemoryHint  memory     = MemoryHint::GpuOnly;
    bool        isCubemap  = false;
    bool        isTiled    = false;  // sparse / reserved resource
    const char* debugName  = nullptr;
};

struct ShaderDesc {
    std::span<const uint32_t> spirvCode;   // SPIR-V bytecode (all backends transpile from it)
    std::string_view sourcePath{};          // Optional file path: .spv loads directly, .glsl compiles
    std::string_view glslSource{};          // Optional in-memory GLSL source (compiled via glslang)
    ShaderStage   stage      = ShaderStage::Vertex;
    const char*   entryPoint = "main";
    const char*   debugName  = nullptr;
};

struct ColorAttachmentDesc {
    Format   format  = Format::R8G8B8A8_Unorm;
    LoadOp   loadOp  = LoadOp::Clear;
    StoreOp  storeOp = StoreOp::Store;
};

struct DepthAttachmentDesc {
    Format   format       = Format::D32_Float;
    LoadOp   depthLoadOp  = LoadOp::Clear;
    StoreOp  depthStoreOp = StoreOp::Store;
    LoadOp   stencilLoadOp  = LoadOp::DontCare;
    StoreOp  stencilStoreOp = StoreOp::DontCare;
};

struct RenderPassDesc {
    std::span<const ColorAttachmentDesc> colorAttachments;
    DepthAttachmentDesc  depth;
    bool                 hasDepth    = false;
    SampleCount          sampleCount = SampleCount::X1;
    const char*          debugName   = nullptr;
};

struct SamplerDesc {
    SamplerFilter      magFilter         = SamplerFilter::Linear;
    SamplerFilter      minFilter         = SamplerFilter::Linear;
    SamplerMipmapMode  mipmapMode        = SamplerMipmapMode::Linear;
    SamplerAddressMode addressU          = SamplerAddressMode::Repeat;
    SamplerAddressMode addressV          = SamplerAddressMode::Repeat;
    SamplerAddressMode addressW          = SamplerAddressMode::Repeat;
    float              mipLodBias        = 0.f;
    float              minLod            = 0.f;
    float              maxLod            = 1000.f;  // VK_LOD_CLAMP_NONE
    bool               anisotropyEnable  = false;
    float              maxAnisotropy     = 1.f;
    bool               compareEnable     = false;
    CompareOp          compareOp         = CompareOp::Never;
    const char*        debugName         = nullptr;
};

struct QueryPoolDesc {
    uint32_t    count     = 64;  // number of timestamp slots
    const char* debugName = nullptr;
};

struct GraphicsPipelineDesc {
    ShaderHandle  vertexShader;
    ShaderHandle  fragmentShader;
    RenderPassHandle renderPass;
    // Primitive assembly
    Topology      topology              = Topology::TriangleList;
    // Rasterizer
    CullMode      cullMode              = CullMode::Back;
    FrontFace     frontFace             = FrontFace::CCW;
    PolygonMode   polygonMode           = PolygonMode::Fill;
    float         lineWidth             = 1.0f;
    // Depth / stencil
    bool          depthTest             = true;
    bool          depthWrite            = true;
    CompareOp     depthCompare          = CompareOp::GreaterOrEqual;  // reversed-Z default
    bool          stencilEnable         = false;
    // Color blend (per-attachment)
    bool          blendEnable           = false;
    BlendFactor   srcColorBlend         = BlendFactor::SrcAlpha;
    BlendFactor   dstColorBlend         = BlendFactor::OneMinusSrcAlpha;
    BlendOp       colorBlendOp          = BlendOp::Add;
    BlendFactor   srcAlphaBlend         = BlendFactor::One;
    BlendFactor   dstAlphaBlend         = BlendFactor::OneMinusSrcAlpha;
    BlendOp       alphaBlendOp          = BlendOp::Add;
    uint8_t       colorWriteMask        = 0xF;    // RGBA
    // Attachment formats (Undefined → use swapchain defaults B8G8R8A8_SRGB / D32_Float)
    Format        colorAttachmentFormat = Format::Undefined;
    Format        depthAttachmentFormat = Format::Undefined;
    const char*   debugName             = nullptr;
};

struct MeshShaderPipelineDesc {
    ShaderHandle  taskShader;    // optional — amplification stage
    ShaderHandle  meshShader;    // required
    ShaderHandle  fragmentShader;
    RenderPassHandle renderPass;
    bool          depthTest  = true;
    bool          depthWrite = true;
    const char*   debugName  = nullptr;
};

struct ComputePipelineDesc {
    ShaderHandle computeShader;
    const char*  debugName = nullptr;
};

struct RayTracingPipelineDesc {
    ShaderHandle rayGenShader;
    std::span<const ShaderHandle> missShaders;
    std::span<const ShaderHandle> closestHitShaders;
    std::span<const ShaderHandle> anyHitShaders;
    uint32_t     maxRecursionDepth = 2;
    const char*  debugName = nullptr;
};

// ── Descriptor set types ──────────────────────────────────────────────────────

// Binding types that a descriptor set slot can hold.
enum class DescriptorType : uint8_t {
    UniformBuffer        = 0,  // constant/uniform buffer view
    StorageBuffer        = 1,  // read-write storage buffer
    SampledTexture       = 2,  // shader-read texture
    StorageTexture       = 3,  // read-write storage image
    Sampler              = 4,  // standalone sampler
    CombinedImageSampler = 5,  // texture + sampler pair (Vulkan combined image sampler)
};

// Describes a single binding slot when allocating or updating a descriptor set.
// Only the field(s) appropriate for the given type need to be set.
struct DescriptorBindingDesc {
    uint32_t       binding = 0;
    DescriptorType type    = DescriptorType::UniformBuffer;
    BufferHandle   buffer  = {};   // valid for *Buffer types
    TextureHandle  texture = {};   // valid for *Texture types
    SamplerHandle  sampler = {};   // valid for Sampler / CombinedImageSampler
};

// Descriptor set allocation request.
struct DescriptorSetDesc {
    std::span<const DescriptorBindingDesc> bindings;
    const char* debugName = nullptr;
};

// ── IDevice ──────────────────────────────────────────────────────────────────

class IDevice {
public:
    virtual ~IDevice() = default;

    // ── Introspection ──────────────────────────────────────────────────────
    [[nodiscard]] virtual Backend            backend()      const noexcept = 0;
    [[nodiscard]] virtual const DeviceCapabilities& caps() const noexcept = 0;
    [[nodiscard]] virtual HardwareTier       tier()         const noexcept = 0;
    [[nodiscard]] virtual std::string_view   deviceName()   const noexcept = 0;

    // ── Resource creation ─────────────────────────────────────────────────
    [[nodiscard]] virtual BufferHandle     createBuffer    (const BufferDesc&     desc) = 0;
    [[nodiscard]] virtual TextureHandle    createTexture   (const TextureDesc&    desc) = 0;
    [[nodiscard]] virtual ShaderHandle     createShader    (const ShaderDesc&     desc) = 0;
    [[nodiscard]] virtual RenderPassHandle createRenderPass(const RenderPassDesc& desc) = 0;
    [[nodiscard]] virtual PipelineHandle   createGraphicsPipeline   (const GraphicsPipelineDesc&    desc) = 0;
    [[nodiscard]] virtual PipelineHandle   createMeshShaderPipeline (const MeshShaderPipelineDesc&  desc) = 0;
    [[nodiscard]] virtual PipelineHandle   createComputePipeline    (const ComputePipelineDesc&     desc) = 0;
    [[nodiscard]] virtual PipelineHandle   createRayTracingPipeline (const RayTracingPipelineDesc&  desc) = 0;
    [[nodiscard]] virtual SamplerHandle    createSampler            (const SamplerDesc&             desc) = 0;
    [[nodiscard]] virtual QueryPoolHandle  createQueryPool          (const QueryPoolDesc&           desc) = 0;
    // Readback timestamp query results.  outNanos is nanosecond GPU timestamps.
    virtual void readbackTimestamps(QueryPoolHandle pool, uint32_t first, uint32_t count,
                                    uint64_t* outNanos) = 0;

    // ── Resource destruction ──────────────────────────────────────────────
    virtual void destroyBuffer     (BufferHandle)      = 0;
    virtual void destroyTexture    (TextureHandle)     = 0;
    virtual void destroyShader     (ShaderHandle)      = 0;
    virtual void destroyRenderPass (RenderPassHandle)  = 0;
    virtual void destroyPipeline   (PipelineHandle)    = 0;
    virtual void destroySampler    (SamplerHandle)     = 0;
    virtual void destroyQueryPool  (QueryPoolHandle)   = 0;

    // ── Command buffer lifecycle ──────────────────────────────────────────
    [[nodiscard]] virtual CmdBufHandle   allocateCommandBuffer(QueueType queue = QueueType::Graphics) = 0;
    virtual void                         freeCommandBuffer(CmdBufHandle) = 0;

    // Obtain an ICommandBuffer for recording into a previously allocated CmdBufHandle.
    // The returned pointer is valid until freeCommandBuffer() is called.
    [[nodiscard]] virtual ICommandBuffer* getCommandBuffer(CmdBufHandle) = 0;

    // ── Synchronisation primitives ────────────────────────────────────────
    [[nodiscard]] virtual FenceHandle     createFence    (bool signaled = false)        = 0;
    [[nodiscard]] virtual SemaphoreHandle createSemaphore()                             = 0;
    // Timeline semaphore helpers (backends may map to binary fallback when unsupported).
    [[nodiscard]] virtual SemaphoreHandle createTimelineSemaphore(uint64_t /*initialValue*/ = 0) {
        return createSemaphore();
    }
    [[nodiscard]] virtual uint64_t querySemaphoreValue(SemaphoreHandle /*semaphore*/) {
        return 0;
    }
    virtual void signalSemaphore(SemaphoreHandle /*semaphore*/, uint64_t /*value*/) {}
    virtual void waitSemaphore  (SemaphoreHandle /*semaphore*/, uint64_t /*value*/, uint64_t /*timeoutNs*/ = UINT64_MAX) {}
    virtual void  destroyFence    (FenceHandle)                                         = 0;
    virtual void  destroySemaphore(SemaphoreHandle)                                     = 0;
    virtual void  waitForFence    (FenceHandle, uint64_t timeoutNs = UINT64_MAX)        = 0;
    virtual void  resetFence      (FenceHandle)                                         = 0;

    // ── CPU ↔ GPU data transfer helpers ──────────────────────────────────
    virtual void uploadBuffer (BufferHandle dst, const void* data, uint64_t sizeBytes, uint64_t dstOffset = 0) = 0;
    virtual void uploadTexture(TextureHandle dst, const void* data, uint64_t sizeBytes) = 0;

    // ── Queue submission ──────────────────────────────────────────────────
    virtual void submit(
        QueueType          queue,
        std::span<const CmdBufHandle> cmds,
        std::span<const SemaphoreHandle> waitSemaphores   = {},
        std::span<const SemaphoreHandle> signalSemaphores = {},
        FenceHandle        signalFence = {}
    ) = 0;

    // Timeline-aware submit path.
    // waitValues/signalValues are positionally matched to wait/signal semaphores.
    // For binary semaphores, the corresponding value is ignored (must be 0 on Vulkan).
    virtual void submitWithTimeline(
        QueueType queue,
        std::span<const CmdBufHandle> cmds,
        std::span<const SemaphoreHandle> waitSemaphores,
        std::span<const uint64_t> waitValues,
        std::span<const SemaphoreHandle> signalSemaphores,
        std::span<const uint64_t> signalValues,
        FenceHandle signalFence = {}
    ) {
        (void)waitValues;
        (void)signalValues;
        submit(queue, cmds, waitSemaphores, signalSemaphores, signalFence);
    }

    virtual void waitIdle() = 0;

    // ── Descriptor set management ─────────────────────────────────────────
    // Default implementations are no-ops so backends that use push constants
    // or bindless do not need to override.  Backends with classic descriptor
    // pools (Vulkan VkDescriptorSet) should override all three.
    [[nodiscard]] virtual DescriptorSetHandle allocateDescriptorSet(const DescriptorSetDesc& desc) {
        (void)desc; return {};
    }
    virtual void updateDescriptorSet(DescriptorSetHandle set,
                                     std::span<const DescriptorBindingDesc> bindings) {
        (void)set; (void)bindings;
    }
    virtual void freeDescriptorSet(DescriptorSetHandle set) { (void)set; }

    // ── Acceleration structure (BVH) ──────────────────────────────────────
    // Only valid when caps().rayTracingTier >= 1
    [[nodiscard]] virtual AccelStructHandle buildBLAS(BufferHandle vertexBuffer, BufferHandle indexBuffer,
                                                      uint32_t vertexCount, uint32_t indexCount) = 0;
    [[nodiscard]] virtual AccelStructHandle buildTLAS(std::span<const AccelStructHandle> blases) = 0;
    virtual void destroyAccelStruct(AccelStructHandle) = 0;
};

// ── Factory ──────────────────────────────────────────────────────────────────
std::unique_ptr<IDevice> createDevice(Backend preferred = Backend::Vulkan);

} // namespace nexus::gfx
