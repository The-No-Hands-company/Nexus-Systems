#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus GFX — Common types, handles, enumerations
//  Used by every layer of the graphics kernel.
// ─────────────────────────────────────────────────────────────────────────────
#include <cstdint>
#include <limits>
#include <functional>

namespace nexus::gfx {

// ── Null handle sentinel ──────────────────────────────────────────────────────
inline constexpr uint64_t kNullHandle = std::numeric_limits<uint64_t>::max();

// ── Opaque typed handles (avoids void* casts) ─────────────────────────────────
template<typename Tag>
struct Handle {
    uint64_t id = kNullHandle;
    [[nodiscard]] bool valid() const noexcept { return id != kNullHandle; }
    bool operator==(const Handle&) const = default;
};

struct BufferTag    {};
struct TextureTag   {};
struct ShaderTag    {};
struct PipelineTag  {};
struct CmdBufTag    {};
struct RenderPassTag{};
struct SamplerTag   {};
struct AccelStructTag{};  // BVH for ray tracing
struct FenceTag        {};
struct SemaphoreTag    {};
struct QueryPoolTag    {};
struct DescriptorSetTag{};

using BufferHandle        = Handle<BufferTag>;
using TextureHandle       = Handle<TextureTag>;
using ShaderHandle        = Handle<ShaderTag>;
using PipelineHandle      = Handle<PipelineTag>;
using CmdBufHandle        = Handle<CmdBufTag>;
using RenderPassHandle    = Handle<RenderPassTag>;
using SamplerHandle       = Handle<SamplerTag>;
using AccelStructHandle   = Handle<AccelStructTag>;
using FenceHandle         = Handle<FenceTag>;
using SemaphoreHandle     = Handle<SemaphoreTag>;
using QueryPoolHandle     = Handle<QueryPoolTag>;
using DescriptorSetHandle = Handle<DescriptorSetTag>;

// ── Backend enumeration ───────────────────────────────────────────────────────
enum class Backend : uint8_t {
    Null    = 0,   // headless / testing
    Vulkan  = 1,
    Metal   = 2,
    DX12    = 3,
};

// ── Queue types ───────────────────────────────────────────────────────────────
enum class QueueType : uint8_t {
    Graphics = 0,
    Compute  = 1,   // async compute — isolated from graphics queue
    Transfer = 2,
    Count
};

// ── Texture format (subset of Vulkan VkFormat, maps 1:1) ─────────────────────
enum class Format : uint32_t {
    Undefined = 0,
    // 8-bit
    R8_Unorm,  R8_Uint, R8_Sint,
    R8G8_Unorm, R8G8B8A8_Unorm, R8G8B8A8_Srgb,
    B8G8R8A8_Unorm, B8G8R8A8_Srgb,
    // 16-bit float
    R16_Float, R16G16_Float, R16G16B16A16_Float,
    // 32-bit float
    R32_Float, R32G32_Float, R32G32B32_Float, R32G32B32A32_Float,
    // Packed
    A2R10G10B10_Unorm,
    // Depth / stencil
    D16_Unorm, D32_Float, D24_Unorm_S8_Uint, D32_Float_S8_Uint,
    // Block-compressed
    BC1_Unorm, BC3_Unorm, BC5_Unorm, BC7_Unorm,
    // HDR presentation
    A2R10G10B10_Unorm_Pack32,
    R16G16B16A16_Sfloat,
};

// ── Sample count ─────────────────────────────────────────────────────────────
enum class SampleCount : uint8_t {
    X1  = 1,
    X2  = 2,
    X4  = 4,
    X8  = 8,
    X16 = 16,
};

// ── Usage flags ───────────────────────────────────────────────────────────────
enum class BufferUsage : uint32_t {
    None          = 0,
    VertexBuffer  = 1 << 0,
    IndexBuffer   = 1 << 1,
    UniformBuffer = 1 << 2,
    StorageBuffer = 1 << 3,
    IndirectArgs  = 1 << 4,
    TransferSrc   = 1 << 5,
    TransferDst   = 1 << 6,
    RayTracingAS  = 1 << 7,   // acceleration structure build input
    MeshletBuffer = 1 << 8,   // mesh-shader meshlet descriptors
};
inline BufferUsage operator|(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool hasFlag(BufferUsage val, BufferUsage flag) {
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}
inline BufferUsage operator&(BufferUsage a, BufferUsage b) {
    return static_cast<BufferUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline BufferUsage operator~(BufferUsage a) {
    return static_cast<BufferUsage>(~static_cast<uint32_t>(a));
}

enum class TextureUsage : uint32_t {
    None            = 0,
    Sampled         = 1 << 0,
    Storage         = 1 << 1,
    ColorAttachment = 1 << 2,
    DepthAttachment = 1 << 3,
    TransferSrc     = 1 << 4,
    TransferDst     = 1 << 5,
    ShadingRate     = 1 << 6,
};
inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline TextureUsage operator&(TextureUsage a, TextureUsage b) {
    return static_cast<TextureUsage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline TextureUsage operator~(TextureUsage a) {
    return static_cast<TextureUsage>(~static_cast<uint32_t>(a));
}

// ── Memory visibility ─────────────────────────────────────────────────────────
enum class MemoryHint : uint8_t {
    GpuOnly,    // fastest VRAM — no CPU access
    CpuToGpu,   // upload / staging
    GpuToCpu,   // readback
    Unified,    // shared GPU/CPU (APUs, integrated graphics, Vulkan device-local+host-visible)
};

// ── Pipeline stage / shader stage flags ──────────────────────────────────────
enum class ShaderStage : uint32_t {
    Vertex   = 1 << 0,
    Fragment = 1 << 1,
    Compute  = 1 << 2,
    Task     = 1 << 3,   // mesh shader task/amplification stage
    Mesh     = 1 << 4,   // mesh shader
    RayGen   = 1 << 5,
    ClosestHit = 1 << 6,
    AnyHit   = 1 << 7,
    Miss     = 1 << 8,
    Intersect= 1 << 9,
};
inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
    return static_cast<ShaderStage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline ShaderStage operator&(ShaderStage a, ShaderStage b) {
    return static_cast<ShaderStage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline ShaderStage operator~(ShaderStage a) {
    return static_cast<ShaderStage>(~static_cast<uint32_t>(a));
}

// ── Topology ─────────────────────────────────────────────────────────────────
enum class Topology : uint8_t {
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList,
};

// ── Load / store ops ─────────────────────────────────────────────────────────
enum class LoadOp  : uint8_t { Load, Clear, DontCare };
enum class StoreOp : uint8_t { Store, DontCare };

// ── Common 2D/3D extents ─────────────────────────────────────────────────────
struct Extent2D { uint32_t width = 0; uint32_t height = 0; };
struct Extent3D { uint32_t width = 0; uint32_t height = 0; uint32_t depth = 1; };
struct Offset2D { int32_t  x = 0; int32_t y = 0; };
struct Rect2D   { Offset2D offset; Extent2D extent; };
struct Viewport {
    float x = 0.f, y = 0.f;
    float width  = 0.f, height = 0.f;
    float minDepth = 0.f, maxDepth = 1.f;
};

// ── Capability flags (queried at device init) ─────────────────────────────────
struct DeviceCapabilities {
    bool meshShaders          = false;
    bool rayTracingPipeline   = false;
    bool rayQuery             = false;  // inline ray queries in any stage
    bool timelineSemaphores   = false;
    bool variableRateShading  = false;
    bool conservativeRaster   = false;
    bool tiledResources       = false;  // sparse binding / reserved resources
    bool asyncCompute         = false;
    bool unifiedMemory        = false;  // device-local + host-visible on same heap
    bool directStorage        = false;  // platform DirectStorage or equivalent
    bool aftermath            = false;  // NVIDIA Aftermath present
    uint32_t maxMeshletVerts  = 0;
    uint32_t maxMeshletPrims  = 0;
    uint32_t minSubgroupSize  = 0;
    uint32_t maxSubgroupSize  = 0;
    // Tier: 0 = no RT, 1 = ray queries only, 2 = full RT pipeline
    uint8_t  rayTracingTier   = 0;
};

// ── Texture layout (for barrier transitions) ─────────────────────────────────
enum class TextureLayout : uint8_t {
    Undefined = 0,
    General,            // UAV / general purpose
    ColorAttachment,    // render target write
    DepthWrite,         // depth+stencil write
    DepthRead,          // depth read-only (shadow map sampling)
    ShaderRead,         // sampled texture
    ShaderReadWrite,    // storage image read+write
    TransferSrc,        // copy source
    TransferDst,        // copy destination
    Present,            // ready for presentation
};

// ── Buffer access flags (for buffer barriers) ─────────────────────────────────
enum class BufferAccess : uint32_t {
    None            = 0,
    IndirectCommand = 1 << 0,
    IndexRead       = 1 << 1,
    VertexRead      = 1 << 2,
    UniformRead     = 1 << 3,
    ShaderRead      = 1 << 4,
    ShaderWrite     = 1 << 5,
    TransferRead    = 1 << 6,
    TransferWrite   = 1 << 7,
};
inline BufferAccess operator|(BufferAccess a, BufferAccess b) {
    return static_cast<BufferAccess>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline BufferAccess operator&(BufferAccess a, BufferAccess b) {
    return static_cast<BufferAccess>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// ── Rasterizer state ──────────────────────────────────────────────────────────
enum class CullMode    : uint8_t { None = 0, Front, Back };
enum class FrontFace   : uint8_t { CCW  = 0, CW };
enum class PolygonMode : uint8_t { Fill = 0, Line, Point };

// ── Depth / stencil ───────────────────────────────────────────────────────────
enum class CompareOp : uint8_t {
    Never = 0, Less, Equal, LessOrEqual, Greater, NotEqual, GreaterOrEqual, Always
};
enum class StencilOp : uint8_t {
    Keep = 0, Zero, Replace, IncrClamp, DecrClamp, Invert, IncrWrap, DecrWrap
};

// ── Blend ─────────────────────────────────────────────────────────────────────
enum class BlendFactor : uint8_t {
    Zero = 0, One,
    SrcColor, OneMinusSrcColor,
    DstColor, OneMinusDstColor,
    SrcAlpha, OneMinusSrcAlpha,
    DstAlpha, OneMinusDstAlpha,
    ConstantColor, OneMinusConstantColor,
    SrcAlphaSaturate,
};
enum class BlendOp : uint8_t { Add = 0, Subtract, ReverseSubtract, Min, Max };

// ── Sampler ───────────────────────────────────────────────────────────────────
enum class SamplerFilter      : uint8_t { Nearest = 0, Linear };
enum class SamplerAddressMode : uint8_t {
    Repeat = 0, MirroredRepeat, ClampToEdge, ClampToBorder, MirrorClampToEdge
};
enum class SamplerMipmapMode  : uint8_t { Nearest = 0, Linear };

// ── Hardware tier (used to select code paths for low/high-end scaling) ────────
enum class HardwareTier : uint8_t {
    Low    = 0,   // integrated / iGPU (< 4 GB VRAM, no RT)
    Mid    = 1,   // discrete mid-range (4–8 GB, RT tier 1)
    High   = 2,   // high-end (8–16 GB, mesh shaders, RT tier 2)
    Ultra  = 3,   // flagship (16 GB+, full feature set, neural)
};

} // namespace nexus::gfx

// ── std::hash specialisation for Handle<T> ───────────────────────────────────
namespace std {
template<typename Tag>
struct hash<nexus::gfx::Handle<Tag>> {
    std::size_t operator()(const nexus::gfx::Handle<Tag>& h) const noexcept {
        return std::hash<uint64_t>{}(h.id);
    }
};
} // namespace std
