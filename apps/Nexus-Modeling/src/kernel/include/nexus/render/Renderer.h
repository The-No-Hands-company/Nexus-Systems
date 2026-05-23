#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Renderer
//
//  Orchestrates per-frame rendering.  Selects rasterization vs path tracing
//  vs mesh-shader path based on HardwareTier and user preference.
//
//  Frame pipeline (high-end):
//    1. Frustum cull (CPU / GPU indirect)
//    2. Mesh shader geometry amplification (LOD / meshlets)
//    3. Rasterize GBuffer (or path-trace directly)
//    4. Async compute: denoising / up-scaling (DLSS / XeSS / OIDN)
//    5. TAA / temporal accumulation
//    6. Tone mapping → swapchain present
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/gfx/FrameScheduler.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/render/RenderGraphValidator.h>
#include <nexus/render/FrameCaptureExporter.h>
#include <array>
#include <cstdint>
#include <memory>

namespace nexus::render {

// ── Render mode ───────────────────────────────────────────────────────────────
enum class RenderMode : uint8_t {
    Rasterize,        // classic rasterisation (all hardware tiers)
    PathTrace,        // hardware RT path tracing (High / Ultra only)
    HybridRT,         // raster primary rays + RT secondary (Mid+)
    Wireframe,        // debug: mesh edges only
    Normals,          // debug: world-space normals
    Overdraw,         // debug: fragment overdraw heat-map
};

// ── Up-scaling mode ───────────────────────────────────────────────────────────
enum class UpscaleMode : uint8_t {
    Off,
    DLSS4,    // NVIDIA
    XeSS,     // Intel
    FSR3,     // AMD
    Bilinear, // software fallback
};

// ── Renderer settings ─────────────────────────────────────────────────────────
struct RendererSettings {
    RenderMode  mode            = RenderMode::Rasterize;
    UpscaleMode upscale         = UpscaleMode::Off;
    float       renderScale     = 1.0f;  // <1.0 = render at lower res, then upscale
    uint32_t    maxBounces      = 4;     // path tracing bounce limit
    bool        enableTAA       = true;
    bool        enableShadows   = true;
    bool        enableAO        = true;
    bool        enableBloom     = true;
    bool        enableSSR       = false; // screen-space reflections
    bool        enableRTReflect = false; // RT reflections (High+ tier)
    bool        enableRayTracingStub = false; // allow deterministic stub RT on non-RT backends
};

// ── Per-frame stats ───────────────────────────────────────────────────────────
struct FrameStats {
    uint32_t totalNodes       = 0;
    uint32_t visibleNodes     = 0;
    uint32_t drawCalls        = 0;
    uint32_t triangles        = 0;
    uint32_t rayTracingDrawCalls = 0;
    uint32_t rayPayloads      = 0;
    uint32_t rayHits          = 0;
    uint32_t meshlets         = 0;
    double   gpuTimeMs        = 0.0;
    double   cpuCullTimeMs    = 0.0;
};

// ── Composite input diagnostic ────────────────────────────────────────────────
// Named flags for each reason a composite or shadow input may be absent.
// Multiple flags may be set simultaneously; None means all required fields present.
enum class CompositeInputMissing : uint32_t {
    None                 = 0,
    GBufferAlbedo        = 1 << 0,
    GBufferNormal        = 1 << 1,
    GBufferVelocity      = 1 << 2,
    GBufferDepth         = 1 << 3,
    SamplerAlbedo        = 1 << 4,
    SamplerNormal        = 1 << 5,
    SamplerVelocity      = 1 << 6,
    SamplerDepth         = 1 << 7,
    MaterialTable        = 1 << 8,  // only flagged when materialTableOffsetBytes > 0
    ShadowDepthTexture   = 1 << 9,
    ShadowDepthSampler   = 1 << 10,
    ShadowLightingBuffer = 1 << 11,
    ShadowCascadeCount   = 1 << 12,
};
inline CompositeInputMissing operator|(CompositeInputMissing a, CompositeInputMissing b) noexcept {
    return static_cast<CompositeInputMissing>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline CompositeInputMissing operator&(CompositeInputMissing a, CompositeInputMissing b) noexcept {
    return static_cast<CompositeInputMissing>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
inline bool hasFlag(CompositeInputMissing val, CompositeInputMissing flag) noexcept {
    return (static_cast<uint32_t>(val) & static_cast<uint32_t>(flag)) != 0;
}

struct CompositeMaterialBindings {
    nexus::gfx::SamplerHandle albedoMaterialSampler;
    nexus::gfx::SamplerHandle normalSampler;
    nexus::gfx::SamplerHandle velocitySampler;
    nexus::gfx::SamplerHandle depthSampler;
    nexus::gfx::SamplerHandle shadowDepthSampler;
    nexus::gfx::BufferHandle  materialTable;
    uint32_t                  materialTableOffsetBytes = 0;
};

struct CompositePassBindingDesc {
    nexus::gfx::TextureHandle albedoTexture;
    nexus::gfx::TextureHandle normalTexture;
    nexus::gfx::TextureHandle velocityTexture;
    nexus::gfx::TextureHandle depthTexture;
    nexus::gfx::SamplerHandle albedoSampler;
    nexus::gfx::SamplerHandle normalSampler;
    nexus::gfx::SamplerHandle velocitySampler;
    nexus::gfx::SamplerHandle depthSampler;
    nexus::gfx::BufferHandle  materialTable;
    uint32_t                  materialTableOffsetBytes = 0;
    nexus::gfx::TextureHandle shadowDepthTexture;
    nexus::gfx::SamplerHandle shadowDepthSampler;
    nexus::gfx::BufferHandle  shadowLightingBuffer;
    uint32_t                  shadowCascadeCount = 0;

    [[nodiscard]] bool hasCoreInputs() const noexcept
    {
        return albedoTexture.valid()
            && normalTexture.valid()
            && velocityTexture.valid()
            && depthTexture.valid()
            && albedoSampler.valid()
            && normalSampler.valid()
            && velocitySampler.valid()
            && depthSampler.valid();
    }

    [[nodiscard]] bool hasMaterialTableInput() const noexcept
    {
        return materialTable.valid();
    }

    [[nodiscard]] bool hasShadowInputs() const noexcept
    {
        return shadowDepthTexture.valid()
            && shadowDepthSampler.valid()
            && shadowLightingBuffer.valid()
            && shadowCascadeCount > 0;
    }

    [[nodiscard]] bool readyForComposite() const noexcept
    {
        return hasCoreInputs()
            && (materialTableOffsetBytes == 0 || hasMaterialTableInput());
    }

    // Returns a bitmask of every field that is absent or invalid.
    // None means all fields checked by this call are present; the composite pass
    // is safe to run when the returned value contains no flags that block it
    // (i.e. GBuffer*, Sampler*, and MaterialTable when offset is set).
    [[nodiscard]] CompositeInputMissing diagnose() const noexcept
    {
        CompositeInputMissing missing = CompositeInputMissing::None;
        if (!albedoTexture.valid())
            missing = missing | CompositeInputMissing::GBufferAlbedo;
        if (!normalTexture.valid())
            missing = missing | CompositeInputMissing::GBufferNormal;
        if (!velocityTexture.valid())
            missing = missing | CompositeInputMissing::GBufferVelocity;
        if (!depthTexture.valid())
            missing = missing | CompositeInputMissing::GBufferDepth;
        if (!albedoSampler.valid())
            missing = missing | CompositeInputMissing::SamplerAlbedo;
        if (!normalSampler.valid())
            missing = missing | CompositeInputMissing::SamplerNormal;
        if (!velocitySampler.valid())
            missing = missing | CompositeInputMissing::SamplerVelocity;
        if (!depthSampler.valid())
            missing = missing | CompositeInputMissing::SamplerDepth;
        if (materialTableOffsetBytes > 0 && !materialTable.valid())
            missing = missing | CompositeInputMissing::MaterialTable;
        if (!shadowDepthTexture.valid())
            missing = missing | CompositeInputMissing::ShadowDepthTexture;
        if (!shadowDepthSampler.valid())
            missing = missing | CompositeInputMissing::ShadowDepthSampler;
        if (!shadowLightingBuffer.valid())
            missing = missing | CompositeInputMissing::ShadowLightingBuffer;
        if (shadowCascadeCount == 0)
            missing = missing | CompositeInputMissing::ShadowCascadeCount;
        return missing;
    }
};

struct ShadowLightingContract {
    static constexpr uint32_t kMaxCascades = 4;
    Mat4     lightViewProj[kMaxCascades]{};
    float    cascadeSplits[kMaxCascades]{};
    uint32_t cascadeCount = 0;
    float    shadowBias = 0.0005f;
    float    normalBias = 0.001f;
};

struct ShadowCascadePassDesc {
    uint32_t            cascadeIndex = 0;
    nexus::gfx::Rect2D  atlasRect{};
    uint32_t            drawCalls = 0;
    uint32_t            triangles = 0;

    [[nodiscard]] bool isValid() const noexcept
    {
        return atlasRect.extent.width > 0 && atlasRect.extent.height > 0;
    }
};

// ── Shadow pass chain descriptor ─────────────────────────────────────────────
// Exposes the owned shadow atlas and per-cascade pass descriptors so tests and
// backend binding code can validate atlas lifetime, cascade packing, and
// sampling readiness independently of composite inputs.
struct ShadowPassBindingDesc {
    static constexpr uint32_t kMaxCascades = ShadowLightingContract::kMaxCascades;

    nexus::gfx::TextureHandle                  atlasDepthTexture;
    nexus::gfx::Extent2D                       atlasExtent{};
    nexus::gfx::Extent2D                       cascadeExtent{};
    nexus::gfx::TextureLayout                  atlasDepthLayout = nexus::gfx::TextureLayout::Undefined;
    uint32_t                  cascadeCount = 0;
    uint32_t                  atlasColumns = 0;
    uint32_t                  atlasRows = 0;
    std::array<ShadowCascadePassDesc, kMaxCascades> cascadePasses{};

    [[nodiscard]] bool hasAtlasTarget() const noexcept
    {
        return atlasDepthTexture.valid() && atlasExtent.width > 0 && atlasExtent.height > 0;
    }

    [[nodiscard]] bool hasDepthTarget() const noexcept
    {
        return hasAtlasTarget();
    }

    [[nodiscard]] bool hasPassDescriptors() const noexcept
    {
        if (!hasAtlasTarget() || cascadeCount == 0 || cascadeCount > kMaxCascades) {
            return false;
        }
        for (uint32_t i = 0; i < cascadeCount; ++i) {
            if (!cascadePasses[i].isValid() || cascadePasses[i].cascadeIndex != i) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool readyForDepthPass() const noexcept
    {
        return hasPassDescriptors();
    }

    [[nodiscard]] bool readyForLightingSampling() const noexcept
    {
        return hasPassDescriptors()
            && atlasDepthLayout == nexus::gfx::TextureLayout::DepthRead;
    }
};

// ── Dedicated shadow lighting binding descriptor ───────────────────────────────
// Carries all shadow-specific fields needed to bind a shadow lighting descriptor
// set, including per-cascade split values and bias parameters from the contract.
// Populated by Renderer::buildShadowLightingBindingDesc() and validated
// independently of the composite GBuffer inputs.
struct ShadowLightingBindingDesc {
    static constexpr uint32_t kMaxCascades = ShadowLightingContract::kMaxCascades;

    nexus::gfx::TextureHandle shadowDepthTexture;
    nexus::gfx::SamplerHandle shadowDepthSampler;
    nexus::gfx::BufferHandle  shadowLightingBuffer;
    uint32_t                  shadowCascadeCount = 0;
    Mat4                      lightViewProj[kMaxCascades]{};
    float                     cascadeSplits[kMaxCascades]{};
    float                     shadowBias = 0.0f;
    float                     normalBias = 0.0f;

    // All four required fields are valid/non-zero.
    [[nodiscard]] bool isComplete() const noexcept
    {
        return shadowDepthTexture.valid()
            && shadowDepthSampler.valid()
            && shadowLightingBuffer.valid()
            && shadowCascadeCount > 0;
    }
};

// ── Renderer ──────────────────────────────────────────────────────────────────
class Renderer {
public:
    explicit Renderer(nexus::gfx::RenderContext& ctx, nexus::gfx::ISwapchain& swapchain);
    // Optional: wire an IFrameScheduler for production Vulkan rendering.
    // When set, beginFrame/endFrame delegate to the scheduler.
    void setFrameScheduler(nexus::gfx::IFrameScheduler* scheduler) noexcept;
    ~Renderer();

    // ── Per-frame entry points ─────────────────────────────────────────────
    void beginFrame();
    void render(const Camera& camera, SceneGraph& scene);
    void endFrame();

    // ── Configuration ──────────────────────────────────────────────────────
    void applySettings(const RendererSettings& s);
    void setFallbackGeometryPipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    void setShadowPipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    void setLightingCompositePipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    void setRayTracingPipeline(nexus::gfx::PipelineHandle pipeline) noexcept;
    void setMaterialPipeline(MaterialID material, nexus::gfx::PipelineHandle pipeline) noexcept;
    void clearMaterialPipelines() noexcept;
    void setCompositeMaterialBindings(const CompositeMaterialBindings& bindings) noexcept;
    void clearCompositeMaterialBindings() noexcept;
    void setShadowLightingContract(const ShadowLightingContract& contract) noexcept;
    void clearShadowLightingContract() noexcept;
    [[nodiscard]] ShadowPassBindingDesc     buildShadowPassBindingDesc()      const noexcept;
    [[nodiscard]] CompositePassBindingDesc   buildCompositePassBindingDesc()   const noexcept;
    [[nodiscard]] ShadowLightingBindingDesc  buildShadowLightingBindingDesc()  const noexcept;
    void resetScene(SceneGraph& scene);
    void resetSceneAndDestroyTLAS(SceneGraph& scene);
    [[nodiscard]] const RendererSettings& settings() const noexcept { return m_settings; }

    // ── Render graph validation hook ───────────────────────────────────────
    // When set, Renderer::render() will validate and store the last frame's
    // render graph report.  Pass nullptr to disable.
    void setRenderGraphValidationEnabled(bool enabled) noexcept;
    [[nodiscard]] bool isRenderGraphValidationEnabled() const noexcept;
    [[nodiscard]] const RenderGraphValidationReport& lastRenderGraphReport() const noexcept;

    // ── Frame capture hook ────────────────────────────────────────────────
    // Optional exporter called from beginFrame / recordPass / endFrame.
    // Pass nullptr to detach.
    void setFrameCaptureExporter(IFrameCaptureExporter* exporter) noexcept;
    [[nodiscard]] IFrameCaptureExporter* frameCaptureExporter() const noexcept;

    // ── Stats ──────────────────────────────────────────────────────────────
    [[nodiscard]] const FrameStats& lastFrameStats() const noexcept { return m_stats; }

    // ── Resize ────────────────────────────────────────────────────────────
    void onResize(nexus::gfx::Extent2D newExtent);

private:
    void selectRenderPath();   // updates m_activePath based on settings + caps
    void ensureGBuffer(nexus::gfx::Extent2D extent);
    void destroyGBuffer();
    void ensureShadowTargets(nexus::gfx::Extent2D extent, uint32_t cascadeCount);
    void destroyShadowTargets();
    void uploadShadowLightingContract();

    nexus::gfx::RenderContext& m_ctx;
    nexus::gfx::ISwapchain&    m_swapchain;
    RendererSettings           m_settings;
    FrameStats                 m_stats;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace nexus::render
