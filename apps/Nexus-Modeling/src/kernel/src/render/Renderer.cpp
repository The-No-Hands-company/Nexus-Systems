// ─────────────────────────────────────────────────────────────────────────────
//  Renderer — frame orchestration
// ────────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Renderer.h>
#include <nexus/gfx/CommandBuffer.h>
#include <nexus/render/RenderGraphValidator.h>
#include <nexus/render/FrameCaptureExporter.h>
#include <algorithm>
#include <array>
#include <vector>
#include <optional>
#include <unordered_map>

namespace nexus::render {

struct Renderer::Impl {
    std::vector<nexus::gfx::CmdBufHandle>   cmdBufs;      // graphics[0], compute[1]
    nexus::gfx::FenceHandle                 frameFence;
    nexus::gfx::SemaphoreHandle             imageAvailable;
    nexus::gfx::SemaphoreHandle             renderFinished;
    nexus::gfx::IFrameScheduler*            scheduler     = nullptr;
    std::optional<nexus::gfx::FrameContext> currentFrame;
    uint32_t                                currentImageIndex = 0;
    struct GBuffer {
        nexus::gfx::TextureHandle albedoMaterial;
        nexus::gfx::TextureHandle normal;
        nexus::gfx::TextureHandle velocity;
        nexus::gfx::TextureHandle depth;
        nexus::gfx::Extent2D      extent{};
    } gbuffer;
    struct ShadowTargets {
        nexus::gfx::TextureHandle depthAtlas;
        nexus::gfx::Extent2D      atlasExtent{};
        nexus::gfx::Extent2D      cascadeExtent{};
        uint32_t                  cascadeCount = 0;
        uint32_t                  atlasColumns = 0;
        uint32_t                  atlasRows = 0;
        std::array<nexus::gfx::Rect2D, ShadowLightingContract::kMaxCascades> cascadeViewports{};
        std::array<uint32_t, ShadowLightingContract::kMaxCascades> cascadeDrawCalls{};
        std::array<uint32_t, ShadowLightingContract::kMaxCascades> cascadeTriangles{};
    } shadow;
    nexus::gfx::TextureLayout                albedoLayout   = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                normalLayout   = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                velocityLayout = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                depthLayout    = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                shadowDepthLayout = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                swapchainLayout= nexus::gfx::TextureLayout::Present;
    nexus::gfx::PipelineHandle               fallbackGeometryPipeline;
    nexus::gfx::PipelineHandle               shadowPipeline;
    nexus::gfx::PipelineHandle               lightingCompositePipeline;
    nexus::gfx::PipelineHandle               rayTracingPipeline;
    nexus::gfx::PipelineHandle               rayTracingMergePipeline;
    std::unordered_map<MaterialID, nexus::gfx::PipelineHandle> materialPipelines;
    std::vector<std::vector<nexus::gfx::DescriptorSetHandle>> deferredDescriptorSetsByFrame;
    CompositeMaterialBindings                compositeBindings;
    ShadowLightingContract                   shadowLightingContract{};
    nexus::gfx::BufferHandle                 shadowLightingBuffer;
    bool                                     shadowLightingDirty = false;
    nexus::gfx::BufferHandle                 cameraBuffer; // per-frame CameraUBO (geometry set 0)
    uint32_t                                frameIndex    = 0;

    // Render graph validation
    bool                        renderGraphValidationEnabled = false;
    RenderGraphValidationReport lastRenderGraphReport;

    // Frame capture exporter
    IFrameCaptureExporter*      captureExporter = nullptr;
};

namespace {

// Authoritative descriptor layouts for the lighting/composite pass. Both the
// composite pipeline's layout (built by the pipeline creator) and the renderer's
// runtime descriptor bindings derive from these, so they cannot drift. Only
// binding + type are meaningful here.
constexpr nexus::gfx::DescriptorBindingDesc kCompositeCoreLayout[] = {
    {0, nexus::gfx::DescriptorType::SampledTexture, {}, {}, {}, {}}, // albedo
    {1, nexus::gfx::DescriptorType::SampledTexture, {}, {}, {}, {}}, // normal
    {2, nexus::gfx::DescriptorType::SampledTexture, {}, {}, {}, {}}, // velocity
    {3, nexus::gfx::DescriptorType::SampledTexture, {}, {}, {}, {}}, // depth
    {4, nexus::gfx::DescriptorType::Sampler,        {}, {}, {}, {}}, // albedo sampler
    {5, nexus::gfx::DescriptorType::Sampler,        {}, {}, {}, {}}, // normal sampler
    {6, nexus::gfx::DescriptorType::Sampler,        {}, {}, {}, {}}, // velocity sampler
    {7, nexus::gfx::DescriptorType::Sampler,        {}, {}, {}, {}}, // depth sampler
    {8, nexus::gfx::DescriptorType::StorageBuffer,  {}, {}, {}, {}}, // material table (optional)
};
constexpr nexus::gfx::DescriptorBindingDesc kCompositeShadowLayout[] = {
    {0, nexus::gfx::DescriptorType::SampledTexture, {}, {}, {}, {}}, // shadow depth
    {1, nexus::gfx::DescriptorType::Sampler,        {}, {}, {}, {}}, // shadow sampler
    {2, nexus::gfx::DescriptorType::StorageBuffer,  {}, {}, {}, {}}, // shadow lighting
};
// Geometry/GBuffer pass: set 0 carries the per-frame CameraUBO. Geometry
// pipelines build their set-0 layout from this; render() uploads camera.ubo()
// and binds the matching set, so layout and runtime binding cannot drift.
constexpr nexus::gfx::DescriptorBindingDesc kGeometryCameraLayout[] = {
    {0, nexus::gfx::DescriptorType::UniformBuffer, {}, {}, {}, {}}, // CameraUBO
};

// Authoritative GBuffer attachment formats. ensureGBuffer creates the targets
// from these, and geometry pipeline creators build their colorAttachmentFormats /
// depthAttachmentFormat from the published accessors — so the pipeline's render
// target formats and the actual GBuffer cannot drift.
constexpr nexus::gfx::Format kGBufferColorFormats[] = {
    nexus::gfx::Format::R16G16B16A16_Float, // 0: albedo + material
    nexus::gfx::Format::R16G16B16A16_Float, // 1: normal
    nexus::gfx::Format::R16G16_Float,       // 2: velocity
};
constexpr nexus::gfx::Format kGBufferDepthFormat = nexus::gfx::Format::D32_Float;

struct SceneDrawPacket {
    nexus::gfx::BufferHandle vertexBuffer;
    nexus::gfx::BufferHandle indexBuffer;
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    Vec3 worldCenter{}; // world-space node center for light-space frustum culling
    MaterialID material = kInvalidMaterial;
    bool castShadow = true;
};

std::vector<SceneDrawPacket> buildSceneDrawPackets(std::span<Node* const> visible)
{
    std::vector<SceneDrawPacket> packets;
    packets.reserve(visible.size());

    for (Node* node : visible) {
        if (!node) {
            continue;
        }

        const auto& mesh = node->mesh;
        if (!mesh.vertexBuffer.valid() || !mesh.indexBuffer.valid() || mesh.indexCount == 0) {
            continue;
        }
        const Mat4 world = node->worldMatrix();
        const Vec3 worldCenter{world.m[0][3], world.m[1][3], world.m[2][3]};

        if (!node->sectionDrawPackets.empty()) {
            for (const auto& sec : node->sectionDrawPackets) {
                if (sec.indexCount == 0) {
                    continue;
                }
                const uint64_t begin = sec.firstIndex;
                const uint64_t end = static_cast<uint64_t>(sec.firstIndex) + sec.indexCount;
                if (begin >= mesh.indexCount || end > mesh.indexCount) {
                    continue;
                }

                SceneDrawPacket packet{};
                packet.vertexBuffer = mesh.vertexBuffer;
                packet.indexBuffer = mesh.indexBuffer;
                packet.firstIndex = sec.firstIndex;
                packet.indexCount = sec.indexCount;
                packet.worldCenter = worldCenter;
                packet.material = sec.material;
                packet.castShadow = node->castShadow;
                packets.push_back(packet);
            }
            continue;
        }

        SceneDrawPacket packet{};
        packet.vertexBuffer = mesh.vertexBuffer;
        packet.indexBuffer = mesh.indexBuffer;
        packet.firstIndex = 0;
        packet.indexCount = mesh.indexCount;
        packet.worldCenter = worldCenter;
        packet.material = node->material;
        packet.castShadow = node->castShadow;
        packets.push_back(packet);
    }

    return packets;
}

uint32_t submitShadowPackets(nexus::gfx::ICommandBuffer& cmd,
                             std::span<const SceneDrawPacket> packets)
{
    uint32_t submitted = 0;
    for (const auto& packet : packets) {
        if (!packet.castShadow || packet.indexCount == 0) {
            continue;
        }

        cmd.bindVertexBuffer(packet.vertexBuffer, 0, 0);
        cmd.bindIndexBuffer(packet.indexBuffer, 0, false);
        cmd.drawIndexed(packet.indexCount, 1, packet.firstIndex, 0, 0);
        ++submitted;
    }
    return submitted;
}

uint32_t shadowTriangleCount(std::span<const SceneDrawPacket> packets)
{
    uint32_t triangles = 0;
    for (const auto& packet : packets) {
        if (!packet.castShadow || packet.indexCount == 0) {
            continue;
        }
        triangles += packet.indexCount / 3;
    }
    return triangles;
}

// NDC slack applied to the point-based frustum test to conservatively include
// nodes whose mesh geometry extends slightly beyond the node's world center.
static constexpr float kCascadeFrustumMargin = 0.1f;

// Projects worldCenter into light clip space and tests inclusion within the
// cascade frustum, with a small margin for mesh-extent conservatism.
// Returns false for degenerate projections (w <= 0) and for points clearly
// outside the light NDC cube.
bool isNodeVisibleInCascadeFrustum(const Vec3& worldCenter, const Mat4& lightViewProj)
{
    const Vec4 clip = lightViewProj * Vec4{worldCenter.x, worldCenter.y, worldCenter.z, 1.0f};
    if (clip.w < 1e-6f) {
        return false;
    }
    const float rcpW = 1.0f / clip.w;
    const float ndcX = clip.x * rcpW;
    const float ndcY = clip.y * rcpW;
    const float ndcZ = clip.z * rcpW;
    const float m = kCascadeFrustumMargin;
    return ndcX >= -(1.0f + m) && ndcX <= (1.0f + m)
        && ndcY >= -(1.0f + m) && ndcY <= (1.0f + m)
        && ndcZ >= -m           && ndcZ <= (1.0f + m);
}

std::vector<SceneDrawPacket> buildCascadeShadowPackets(std::span<const SceneDrawPacket> packets,
                                                       uint32_t cascadeIndex,
                                                       const ShadowLightingContract& contract)
{
    std::vector<SceneDrawPacket> result;
    if (cascadeIndex >= contract.cascadeCount || cascadeIndex >= ShadowLightingContract::kMaxCascades) {
        return result;
    }

    const Mat4& lvp = contract.lightViewProj[cascadeIndex];
    result.reserve(packets.size());
    for (const auto& packet : packets) {
        if (!packet.castShadow || packet.indexCount == 0) {
            continue;
        }
        if (isNodeVisibleInCascadeFrustum(packet.worldCenter, lvp)) {
            result.push_back(packet);
        }
    }

    return result;
}

struct ShadowAtlasLayout {
    nexus::gfx::Extent2D atlasExtent{};
    uint32_t cascadeCount = 0;
    uint32_t atlasColumns = 0;
    uint32_t atlasRows = 0;
    std::array<nexus::gfx::Rect2D, ShadowLightingContract::kMaxCascades> cascadeViewports{};
};

ShadowAtlasLayout buildShadowAtlasLayout(nexus::gfx::Extent2D cascadeExtent,
                                         uint32_t cascadeCount)
{
    ShadowAtlasLayout layout{};
    if (cascadeExtent.width == 0 || cascadeExtent.height == 0 || cascadeCount == 0) {
        return layout;
    }

    const uint32_t clampedCount = cascadeCount <= ShadowLightingContract::kMaxCascades
        ? cascadeCount
        : ShadowLightingContract::kMaxCascades;
    const uint32_t columns = clampedCount > 2 ? 2u : clampedCount;
    const uint32_t rows = (clampedCount + columns - 1) / columns;

    layout.cascadeCount = clampedCount;
    layout.atlasColumns = columns;
    layout.atlasRows = rows;
    layout.atlasExtent = {
        cascadeExtent.width * columns,
        cascadeExtent.height * rows
    };

    for (uint32_t cascadeIndex = 0; cascadeIndex < clampedCount; ++cascadeIndex) {
        const uint32_t col = cascadeIndex % columns;
        const uint32_t row = cascadeIndex / columns;
        layout.cascadeViewports[cascadeIndex] = {
            {
                static_cast<int32_t>(col * cascadeExtent.width),
                static_cast<int32_t>(row * cascadeExtent.height)
            },
            cascadeExtent
        };
    }

    return layout;
}

uint32_t submitGeometryPackets(nexus::gfx::ICommandBuffer& cmd,
                               std::span<const SceneDrawPacket> packets,
                               nexus::gfx::PipelineHandle fallbackPipeline,
                               const std::unordered_map<MaterialID, nexus::gfx::PipelineHandle>& materialPipelines,
                               nexus::gfx::DescriptorSetHandle cameraSet,
                               uint32_t& outTriangles)
{
    uint32_t submitted = 0;
    outTriangles = 0;
    nexus::gfx::PipelineHandle boundPipeline{};

    for (const auto& packet : packets) {
        if (packet.indexCount == 0) {
            continue;
        }

        nexus::gfx::PipelineHandle packetPipeline = fallbackPipeline;
        if (packet.material != kInvalidMaterial) {
            const auto it = materialPipelines.find(packet.material);
            if (it != materialPipelines.end()) {
                packetPipeline = it->second;
            }
        }
        if (!packetPipeline.valid()) {
            continue;
        }

        if (!(boundPipeline == packetPipeline)) {
            cmd.bindPipeline(packetPipeline);
            boundPipeline = packetPipeline;
            // Per-frame camera UBO at set 0; rebind on every pipeline change since
            // an incompatible layout may disturb previously bound sets.
            if (cameraSet.valid()) {
                cmd.bindDescriptorSet(cameraSet, 0);
            }
        }

        cmd.bindVertexBuffer(packet.vertexBuffer, 0, 0);
        cmd.bindIndexBuffer(packet.indexBuffer, 0, false);
        cmd.drawIndexed(packet.indexCount, 1, packet.firstIndex, 0, 0);
        ++submitted;
        outTriangles += packet.indexCount / 3;
    }

    return submitted;
}

template <typename ImplT>
void invalidateCachedDrawState(ImplT& impl)
{
    impl.albedoLayout = nexus::gfx::TextureLayout::Undefined;
    impl.normalLayout = nexus::gfx::TextureLayout::Undefined;
    impl.velocityLayout = nexus::gfx::TextureLayout::Undefined;
    impl.depthLayout = nexus::gfx::TextureLayout::Undefined;
    impl.shadowDepthLayout = nexus::gfx::TextureLayout::Undefined;
    impl.swapchainLayout = nexus::gfx::TextureLayout::Present;
}

template <typename ImplT>
void deferDescriptorSetFree(ImplT& impl,
                            uint32_t frameSlot,
                            nexus::gfx::DescriptorSetHandle set)
{
    if (!set.valid()) {
        return;
    }
    if (frameSlot >= impl.deferredDescriptorSetsByFrame.size()) {
        impl.deferredDescriptorSetsByFrame.resize(static_cast<size_t>(frameSlot) + 1);
    }
    impl.deferredDescriptorSetsByFrame[frameSlot].push_back(set);
}

template <typename ImplT>
void releaseDeferredDescriptorSetsForFrame(ImplT& impl,
                                           nexus::gfx::IDevice& dev,
                                           uint32_t frameSlot)
{
    if (frameSlot >= impl.deferredDescriptorSetsByFrame.size()) {
        return;
    }

    auto& deferredSets = impl.deferredDescriptorSetsByFrame[frameSlot];
    for (const auto set : deferredSets) {
        dev.freeDescriptorSet(set);
    }
    deferredSets.clear();
}

template <typename ImplT>
void releaseAllDeferredDescriptorSets(ImplT& impl, nexus::gfx::IDevice& dev)
{
    for (uint32_t frameSlot = 0;
         frameSlot < static_cast<uint32_t>(impl.deferredDescriptorSetsByFrame.size());
         ++frameSlot) {
        releaseDeferredDescriptorSetsForFrame(impl, dev, frameSlot);
    }
}

template <typename ImplT>
void resetRendererSceneCaches(ImplT& impl,
                              nexus::gfx::IDevice& dev,
                              FrameStats& stats)
{
    releaseAllDeferredDescriptorSets(impl, dev);
    impl.materialPipelines.clear();
    impl.compositeBindings = {};
    if (impl.shadowLightingBuffer.valid()) {
        dev.destroyBuffer(impl.shadowLightingBuffer);
        impl.shadowLightingBuffer = {};
    }
    impl.shadowLightingContract = {};
    impl.shadowLightingDirty = false;
    if (impl.shadow.depthAtlas.valid()) {
        dev.destroyTexture(impl.shadow.depthAtlas);
        impl.shadow = {};
    }
    impl.currentFrame.reset();
    stats = {};
    invalidateCachedDrawState(impl);
}

} // namespace

Renderer::Renderer(nexus::gfx::RenderContext& ctx, nexus::gfx::ISwapchain& swapchain)
    : m_ctx(ctx), m_swapchain(swapchain), m_impl(std::make_unique<Impl>())
{
    auto& dev = ctx.device();
    m_impl->cmdBufs.push_back(dev.allocateCommandBuffer(nexus::gfx::QueueType::Graphics));
    m_impl->cmdBufs.push_back(dev.allocateCommandBuffer(nexus::gfx::QueueType::Compute));
    m_impl->frameFence      = dev.createFence(false);
    m_impl->imageAvailable  = dev.createSemaphore();
    m_impl->renderFinished  = dev.createSemaphore();
    selectRenderPath();
}

void Renderer::setFrameScheduler(nexus::gfx::IFrameScheduler* scheduler) noexcept
{
    m_impl->scheduler = scheduler;
    if (!scheduler) {
        m_impl->deferredDescriptorSetsByFrame.clear();
        return;
    }

    m_impl->deferredDescriptorSetsByFrame.clear();
    m_impl->deferredDescriptorSetsByFrame.resize(scheduler->maxFramesInFlight());
}

Renderer::~Renderer()
{
    auto& dev = m_ctx.device();
    dev.waitIdle();
    releaseAllDeferredDescriptorSets(*m_impl, dev);
    if (m_impl->shadowLightingBuffer.valid()) {
        dev.destroyBuffer(m_impl->shadowLightingBuffer);
        m_impl->shadowLightingBuffer = {};
    }
    if (m_impl->cameraBuffer.valid()) {
        dev.destroyBuffer(m_impl->cameraBuffer);
        m_impl->cameraBuffer = {};
    }
    destroyShadowTargets();
    destroyGBuffer();
    for (auto h : m_impl->cmdBufs) dev.freeCommandBuffer(h);
    dev.destroyFence    (m_impl->frameFence);
    dev.destroySemaphore(m_impl->imageAvailable);
    dev.destroySemaphore(m_impl->renderFinished);
}

void Renderer::uploadShadowLightingContract()
{
    if (!m_impl->shadowLightingDirty) return;

    auto& dev = m_ctx.device();
    if (!m_impl->shadowLightingBuffer.valid()) {
        nexus::gfx::BufferDesc bd{};
        bd.sizeBytes = sizeof(ShadowLightingContract);
        bd.usage = nexus::gfx::BufferUsage::StorageBuffer | nexus::gfx::BufferUsage::TransferDst;
        bd.memory = nexus::gfx::MemoryHint::GpuOnly;
        bd.debugName = "ShadowLighting.Contract";
        m_impl->shadowLightingBuffer = dev.createBuffer(bd);
    }

    if (m_impl->shadowLightingBuffer.valid()) {
        dev.uploadBuffer(m_impl->shadowLightingBuffer,
                         &m_impl->shadowLightingContract,
                         sizeof(ShadowLightingContract),
                         0);
        m_impl->shadowLightingDirty = false;
    }
}

void Renderer::destroyShadowTargets()
{
    auto& dev = m_ctx.device();
    if (m_impl->shadow.depthAtlas.valid()) {
        dev.destroyTexture(m_impl->shadow.depthAtlas);
    }
    m_impl->shadow = {};
    m_impl->shadowDepthLayout = nexus::gfx::TextureLayout::Undefined;
}

void Renderer::ensureShadowTargets(nexus::gfx::Extent2D extent, uint32_t cascadeCount)
{
    if (extent.width == 0 || extent.height == 0 || cascadeCount == 0) return;

    const ShadowAtlasLayout atlasLayout = buildShadowAtlasLayout(extent, cascadeCount);
    if (atlasLayout.cascadeCount == 0) {
        return;
    }

    if (m_impl->shadow.depthAtlas.valid()
        && m_impl->shadow.atlasExtent.width == atlasLayout.atlasExtent.width
        && m_impl->shadow.atlasExtent.height == atlasLayout.atlasExtent.height
        && m_impl->shadow.cascadeExtent.width == extent.width
        && m_impl->shadow.cascadeExtent.height == extent.height
        && m_impl->shadow.cascadeCount == atlasLayout.cascadeCount) {
        return;
    }

    destroyShadowTargets();

    const nexus::gfx::TextureDesc shadowDepthDesc{
        { atlasLayout.atlasExtent.width, atlasLayout.atlasExtent.height, 1 },
        1,
        1,
        nexus::gfx::Format::D32_Float,
        nexus::gfx::TextureUsage::DepthAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "Shadow.DepthAtlas"
    };

    auto& dev = m_ctx.device();
    m_impl->shadow.depthAtlas = dev.createTexture(shadowDepthDesc);
    m_impl->shadow.atlasExtent = atlasLayout.atlasExtent;
    m_impl->shadow.cascadeExtent = extent;
    m_impl->shadow.cascadeCount = atlasLayout.cascadeCount;
    m_impl->shadow.atlasColumns = atlasLayout.atlasColumns;
    m_impl->shadow.atlasRows = atlasLayout.atlasRows;
    m_impl->shadow.cascadeViewports = atlasLayout.cascadeViewports;
    m_impl->shadowDepthLayout = nexus::gfx::TextureLayout::Undefined;
}

void Renderer::destroyGBuffer()
{
    auto& dev = m_ctx.device();
    if (m_impl->gbuffer.albedoMaterial.valid()) dev.destroyTexture(m_impl->gbuffer.albedoMaterial);
    if (m_impl->gbuffer.normal.valid())         dev.destroyTexture(m_impl->gbuffer.normal);
    if (m_impl->gbuffer.velocity.valid())       dev.destroyTexture(m_impl->gbuffer.velocity);
    if (m_impl->gbuffer.depth.valid())          dev.destroyTexture(m_impl->gbuffer.depth);

    m_impl->gbuffer = {};
    invalidateCachedDrawState(*m_impl);
}

void Renderer::ensureGBuffer(nexus::gfx::Extent2D extent)
{
    if (extent.width == 0 || extent.height == 0) return;

    if (m_impl->gbuffer.albedoMaterial.valid()
        && m_impl->gbuffer.normal.valid()
        && m_impl->gbuffer.velocity.valid()
        && m_impl->gbuffer.depth.valid()
        && m_impl->gbuffer.extent.width == extent.width
        && m_impl->gbuffer.extent.height == extent.height) {
        return;
    }

    destroyGBuffer();

    auto& dev = m_ctx.device();
    // Formats sourced from the published GBuffer contract (single source of truth).
    const nexus::gfx::TextureDesc colorDesc{
        { extent.width, extent.height, 1 },
        1,
        1,
        kGBufferColorFormats[0],
        nexus::gfx::TextureUsage::ColorAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "GBuffer.AlbedoMaterial"
    };
    const nexus::gfx::TextureDesc normalDesc{
        { extent.width, extent.height, 1 },
        1,
        1,
        kGBufferColorFormats[1],
        nexus::gfx::TextureUsage::ColorAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "GBuffer.Normal"
    };
    const nexus::gfx::TextureDesc velocityDesc{
        { extent.width, extent.height, 1 },
        1,
        1,
        kGBufferColorFormats[2],
        nexus::gfx::TextureUsage::ColorAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "GBuffer.Velocity"
    };
    const nexus::gfx::TextureDesc depthDesc{
        { extent.width, extent.height, 1 },
        1,
        1,
        kGBufferDepthFormat,
        nexus::gfx::TextureUsage::DepthAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "GBuffer.Depth"
    };

    m_impl->gbuffer.albedoMaterial = dev.createTexture(colorDesc);
    m_impl->gbuffer.normal         = dev.createTexture(normalDesc);
    m_impl->gbuffer.velocity       = dev.createTexture(velocityDesc);
    m_impl->gbuffer.depth          = dev.createTexture(depthDesc);
    m_impl->gbuffer.extent         = extent;
    m_impl->albedoLayout           = nexus::gfx::TextureLayout::Undefined;
    m_impl->normalLayout           = nexus::gfx::TextureLayout::Undefined;
    m_impl->velocityLayout         = nexus::gfx::TextureLayout::Undefined;
    m_impl->depthLayout            = nexus::gfx::TextureLayout::Undefined;
}

void Renderer::beginFrame()
{
    m_stats = {};

    if (m_impl->captureExporter) {
        m_impl->captureExporter->beginCapture(m_impl->frameIndex);
    }

    if (m_impl->scheduler) {
        // ── Frame scheduler path (VulkanFrameScheduler) ──────────────────────────────────
        m_impl->currentFrame = m_impl->scheduler->beginFrame();
        if (m_impl->currentFrame) {
            releaseDeferredDescriptorSetsForFrame(*m_impl,
                                                 m_ctx.device(),
                                                 m_impl->currentFrame->frameSlot);
        }
    } else {
        // ── Direct swapchain path ────────────────────────────────────────────────────
        auto& dev = m_ctx.device();
        dev.waitForFence(m_impl->frameFence, UINT64_MAX);
        dev.resetFence  (m_impl->frameFence);
        // Acquire swapchain image BEFORE recording commands
        auto frame = m_swapchain.acquire();
        m_impl->currentImageIndex = frame.imageIndex;
        m_impl->currentFrame.reset();
    }
}

void Renderer::render(const Camera& camera, SceneGraph& scene)
{
    // 1. CPU frustum cull
    std::vector<Node*> visible;
    visible.reserve(256);
    scene.collectVisible(camera.frustum(), visible);

    m_stats.totalNodes   = static_cast<uint32_t>(visible.size());
    m_stats.visibleNodes = static_cast<uint32_t>(visible.size());
    m_stats.drawCalls    = 0;
    m_stats.triangles    = 0;

    const std::vector<SceneDrawPacket> drawPackets = buildSceneDrawPackets(visible);

    if (m_impl->scheduler) {
        // ── Frame scheduler path ───────────────────────────────────────────────────
        if (!m_impl->currentFrame) return;  // out-of-date — skip this frame
        auto& fc  = *m_impl->currentFrame;
        auto& cmd = *fc.cmd;
        const uint32_t shadowCascadeCount = m_impl->shadowLightingContract.cascadeCount;
        const bool runShadowPass = m_settings.enableShadows
            && m_impl->shadowPipeline.valid()
            && shadowCascadeCount > 0;
        const bool runRayTracingPass = (m_settings.mode == RenderMode::PathTrace
                                   || m_settings.mode == RenderMode::HybridRT)
            && m_impl->rayTracingPipeline.valid()
            && (m_settings.enableRayTracingStub || m_ctx.caps().rayTracingTier >= 2);
        // RT output is merged into the composite color via a compute post-pass
        // when a merge pipeline is bound. A render pass is unusable here (it would
        // clear the composite), so the merge runs as a dispatch over the color image.
        const bool runRayTracingMerge = runRayTracingPass
            && m_impl->rayTracingMergePipeline.valid();

        if (m_settings.enableShadows) {
            uploadShadowLightingContract();
        }
        if (runShadowPass) {
            ensureShadowTargets(fc.extent, shadowCascadeCount);
        }
        ensureGBuffer(fc.extent);
        if (!m_impl->gbuffer.albedoMaterial.valid() || !m_impl->gbuffer.depth.valid()) return;

        if (runShadowPass && m_impl->shadow.depthAtlas.valid()) {
            std::array<nexus::gfx::TextureBarrier, 1> toShadowWrite = {{
                { m_impl->shadow.depthAtlas, m_impl->shadowDepthLayout, nexus::gfx::TextureLayout::DepthWrite }
            }};
            cmd.textureBarriers(toShadowWrite);
            m_impl->shadowDepthLayout = nexus::gfx::TextureLayout::DepthWrite;

            m_impl->shadow.cascadeDrawCalls.fill(0);
            m_impl->shadow.cascadeTriangles.fill(0);
            for (uint32_t cascadeIndex = 0; cascadeIndex < m_impl->shadow.cascadeCount; ++cascadeIndex) {
                const std::vector<SceneDrawPacket> cascadePackets = buildCascadeShadowPackets(
                    drawPackets,
                    cascadeIndex,
                    m_impl->shadowLightingContract);
                const nexus::gfx::Rect2D passRect = m_impl->shadow.cascadeViewports[cascadeIndex];

                std::array<nexus::gfx::ClearValue, 1> shadowClears{};
                shadowClears[0].depth = {0.f, 0};
                cmd.beginRenderPass({}, {}, m_impl->shadow.depthAtlas, shadowClears, passRect);

                nexus::gfx::Viewport shadowVp{};
                shadowVp.x = static_cast<float>(passRect.offset.x);
                shadowVp.y = static_cast<float>(passRect.offset.y);
                shadowVp.width = static_cast<float>(passRect.extent.width);
                shadowVp.height = static_cast<float>(passRect.extent.height);
                shadowVp.minDepth = 0.f;
                shadowVp.maxDepth = 1.f;
                cmd.setViewport(shadowVp);
                cmd.setScissor(passRect);

                cmd.bindPipeline(m_impl->shadowPipeline);
                const uint32_t shadowDraws = submitShadowPackets(cmd, cascadePackets);
                const uint32_t shadowTriangles = shadowTriangleCount(cascadePackets);
                m_stats.drawCalls += shadowDraws;
                m_stats.triangles += shadowTriangles;
                m_impl->shadow.cascadeDrawCalls[cascadeIndex] = shadowDraws;
                m_impl->shadow.cascadeTriangles[cascadeIndex] = shadowTriangles;
                cmd.endRenderPass();
            }

            std::array<nexus::gfx::TextureBarrier, 1> toShadowSample = {{
                { m_impl->shadow.depthAtlas, nexus::gfx::TextureLayout::DepthWrite, nexus::gfx::TextureLayout::DepthRead }
            }};
            cmd.textureBarriers(toShadowSample);
            m_impl->shadowDepthLayout = nexus::gfx::TextureLayout::DepthRead;
        } else {
            m_impl->shadow.cascadeDrawCalls.fill(0);
            m_impl->shadow.cascadeTriangles.fill(0);
        }

        nexus::gfx::Viewport vp;
        vp.x = 0.f; vp.y = 0.f;
        vp.width  = static_cast<float>(fc.extent.width);
        vp.height = static_cast<float>(fc.extent.height);
        vp.minDepth = 0.f; vp.maxDepth = 1.f;
        cmd.setViewport(vp);
        cmd.setScissor({{0,0}, fc.extent});

        // Per-frame camera UBO for the geometry pass (set 0). Uploaded outside any
        // render pass; the matching descriptor set is bound on each pipeline change
        // inside submitGeometryPackets. On backends without descriptor sets the
        // allocate returns an invalid handle and the bind is skipped gracefully.
        uploadCameraUniform(camera);
        nexus::gfx::DescriptorSetHandle cameraSet{};
        if (m_impl->cameraBuffer.valid()) {
            std::array<nexus::gfx::DescriptorBindingDesc, 1> camBindings{};
            camBindings[0]        = geometryCameraSetLayout()[0]; // binding + type from contract
            camBindings[0].buffer = m_impl->cameraBuffer;

            nexus::gfx::DescriptorSetDesc camSetDesc{};
            camSetDesc.bindings  = std::span{ camBindings.data(), camBindings.size() };
            camSetDesc.debugName = "Geometry.CameraSet";

            cameraSet = m_ctx.device().allocateDescriptorSet(camSetDesc);
            if (cameraSet.valid()) {
                deferDescriptorSetFree(*m_impl, fc.frameSlot, cameraSet);
            }
        }

        // Geometry pass: write into GBuffer attachments.
        std::array<nexus::gfx::TextureBarrier, 4> toGeometryPass = {{
            { m_impl->gbuffer.albedoMaterial, m_impl->albedoLayout,   nexus::gfx::TextureLayout::ColorAttachment },
            { m_impl->gbuffer.normal,         m_impl->normalLayout,   nexus::gfx::TextureLayout::ColorAttachment },
            { m_impl->gbuffer.velocity,       m_impl->velocityLayout, nexus::gfx::TextureLayout::ColorAttachment },
            { m_impl->gbuffer.depth,          m_impl->depthLayout,    nexus::gfx::TextureLayout::DepthWrite }
        }};
        cmd.textureBarriers(toGeometryPass);
        m_impl->albedoLayout   = nexus::gfx::TextureLayout::ColorAttachment;
        m_impl->normalLayout   = nexus::gfx::TextureLayout::ColorAttachment;
        m_impl->velocityLayout = nexus::gfx::TextureLayout::ColorAttachment;
        m_impl->depthLayout    = nexus::gfx::TextureLayout::DepthWrite;

        std::array<nexus::gfx::TextureHandle, 3> gbufferColors = {
            m_impl->gbuffer.albedoMaterial,
            m_impl->gbuffer.normal,
            m_impl->gbuffer.velocity
        };
        std::array<nexus::gfx::ClearValue, 4> gbufferClears{};
        gbufferClears[0].color = {0.f, 0.f, 0.f, 1.f};
        gbufferClears[1].color = {0.5f, 0.5f, 1.f, 1.f};
        gbufferClears[2].color = {0.f, 0.f, 0.f, 0.f};
        gbufferClears[3].depth = {0.f, 0};

        cmd.beginRenderPass({}, gbufferColors, m_impl->gbuffer.depth, gbufferClears,
                            {{0,0}, fc.extent});
        uint32_t geometryTriangles = 0;
        const uint32_t geometryDraws = submitGeometryPackets(cmd,
                                                             drawPackets,
                                                             m_impl->fallbackGeometryPipeline,
                                                             m_impl->materialPipelines,
                                                             cameraSet,
                                                             geometryTriangles);
        m_stats.drawCalls += geometryDraws;
        m_stats.triangles += geometryTriangles;
        cmd.endRenderPass();

        // Transition GBuffer for sampling in lighting/composite stage.
        std::array<nexus::gfx::TextureBarrier, 4> toLightingInputs = {{
            { m_impl->gbuffer.albedoMaterial, nexus::gfx::TextureLayout::ColorAttachment, nexus::gfx::TextureLayout::ShaderRead },
            { m_impl->gbuffer.normal,         nexus::gfx::TextureLayout::ColorAttachment, nexus::gfx::TextureLayout::ShaderRead },
            { m_impl->gbuffer.velocity,       nexus::gfx::TextureLayout::ColorAttachment, nexus::gfx::TextureLayout::ShaderRead },
            { m_impl->gbuffer.depth,          nexus::gfx::TextureLayout::DepthWrite,      nexus::gfx::TextureLayout::DepthRead }
        }};
        cmd.textureBarriers(toLightingInputs);
        m_impl->albedoLayout   = nexus::gfx::TextureLayout::ShaderRead;
        m_impl->normalLayout   = nexus::gfx::TextureLayout::ShaderRead;
        m_impl->velocityLayout = nexus::gfx::TextureLayout::ShaderRead;
        m_impl->depthLayout    = nexus::gfx::TextureLayout::DepthRead;

        const bool hasCompositeColorTarget = fc.colorTarget.valid();
        if (hasCompositeColorTarget) {
            // Lighting/composite pass target: swapchain color image.
            cmd.textureBarrier({
                fc.colorTarget,
                m_impl->swapchainLayout,
                nexus::gfx::TextureLayout::ColorAttachment
            });
            m_impl->swapchainLayout = nexus::gfx::TextureLayout::ColorAttachment;

            std::array<nexus::gfx::ClearValue, 1> finalClears{};
            finalClears[0].color = {0.05f, 0.05f, 0.08f, 1.f};
            cmd.beginRenderPass({}, {&fc.colorTarget, 1}, {}, finalClears,
                                {{0,0}, fc.extent});
        }
        if (m_impl->lightingCompositePipeline.valid() && hasCompositeColorTarget) {
            cmd.bindPipeline(m_impl->lightingCompositePipeline);

            // Bind composite inputs through the descriptor set contract.
            // On backends that use bindless / push constants the allocate call
            // returns an invalid handle and the bind is skipped gracefully.
            const CompositePassBindingDesc compositeDesc = buildCompositePassBindingDesc();
            if (compositeDesc.readyForComposite()) {
                auto& dev = m_ctx.device();

                // Core GBuffer + sampler + material table bindings (set 0), sourced
                // from the published composite layout so the pipeline layout and these
                // runtime bindings cannot drift. Handles are filled per binding index.
                std::array<nexus::gfx::DescriptorBindingDesc, 9> coreBindings{};
                uint32_t nb = 0;
                for (const auto& slot : compositeCoreSetLayout()) {
                    nexus::gfx::DescriptorBindingDesc b = slot; // binding + type from contract
                    switch (slot.binding) {
                        case 0: b.texture = compositeDesc.albedoTexture;   break;
                        case 1: b.texture = compositeDesc.normalTexture;   break;
                        case 2: b.texture = compositeDesc.velocityTexture; break;
                        case 3: b.texture = compositeDesc.depthTexture;    break;
                        case 4: b.sampler = compositeDesc.albedoSampler;   break;
                        case 5: b.sampler = compositeDesc.normalSampler;   break;
                        case 6: b.sampler = compositeDesc.velocitySampler; break;
                        case 7: b.sampler = compositeDesc.depthSampler;    break;
                        case 8:
                            if (!compositeDesc.materialTable.valid()) continue; // optional
                            b.buffer = compositeDesc.materialTable;
                            break;
                        default: continue;
                    }
                    coreBindings[nb++] = b;
                }

                nexus::gfx::DescriptorSetDesc coreSetDesc{};
                coreSetDesc.bindings  = std::span{ coreBindings.data(), nb };
                coreSetDesc.debugName = "Composite.CoreSet";

                nexus::gfx::DescriptorSetHandle coreSet = dev.allocateDescriptorSet(coreSetDesc);
                if (coreSet.valid()) {
                    cmd.bindDescriptorSet(coreSet, 0);
                    deferDescriptorSetFree(*m_impl, fc.frameSlot, coreSet);
                }

                // Shadow lighting bindings (set 1) — only when fully populated. Sourced
                // from the published shadow set layout (single source of truth).
                if (compositeDesc.hasShadowInputs()) {
                    std::array<nexus::gfx::DescriptorBindingDesc, 3> shadowBindings{};
                    uint32_t sb = 0;
                    for (const auto& slot : compositeShadowSetLayout()) {
                        nexus::gfx::DescriptorBindingDesc b = slot;
                        switch (slot.binding) {
                            case 0: b.texture = compositeDesc.shadowDepthTexture;   break;
                            case 1: b.sampler = compositeDesc.shadowDepthSampler;   break;
                            case 2: b.buffer  = compositeDesc.shadowLightingBuffer; break;
                            default: continue;
                        }
                        shadowBindings[sb++] = b;
                    }

                    nexus::gfx::DescriptorSetDesc shadowSetDesc{};
                    shadowSetDesc.bindings  = std::span{ shadowBindings.data(), sb };
                    shadowSetDesc.debugName = "Composite.ShadowSet";

                    nexus::gfx::DescriptorSetHandle shadowSet = dev.allocateDescriptorSet(shadowSetDesc);
                    if (shadowSet.valid()) {
                        cmd.bindDescriptorSet(shadowSet, 1);
                        deferDescriptorSetFree(*m_impl, fc.frameSlot, shadowSet);
                    }
                }
            }

            // Fullscreen triangle pass; shader samples transitioned GBuffer targets.
            cmd.draw(3, 1, 0, 0);
            ++m_stats.drawCalls;
        }
        if (hasCompositeColorTarget) {
            cmd.endRenderPass();
        }

        if (runRayTracingPass) {
            cmd.bindPipeline(m_impl->rayTracingPipeline);
            cmd.traceRays(fc.extent.width, fc.extent.height, 1);

            m_stats.rayTracingDrawCalls = 1;
            m_stats.rayPayloads = m_stats.visibleNodes;
            m_stats.rayHits = scene.tlas().valid() ? 1u : 0u;
        }

        // ── RT output → composite merge (compute post-pass) ───────────────
        // Blend the ray-traced output into the lit composite color. Implemented
        // as a compute dispatch because re-opening a render pass on the color
        // target would clear the composite result (load-op is clear).
        if (runRayTracingMerge && hasCompositeColorTarget) {
            cmd.textureBarrier({
                fc.colorTarget,
                m_impl->swapchainLayout,                  // ColorAttachment after composite
                nexus::gfx::TextureLayout::General        // storage read/write for the merge
            });
            m_impl->swapchainLayout = nexus::gfx::TextureLayout::General;

            cmd.bindPipeline(m_impl->rayTracingMergePipeline);
            constexpr uint32_t kMergeTileSize = 8u;
            cmd.dispatch((fc.extent.width  + kMergeTileSize - 1u) / kMergeTileSize,
                         (fc.extent.height + kMergeTileSize - 1u) / kMergeTileSize,
                         1u);
            m_stats.rayMergeDispatches = 1;
        }

        if (hasCompositeColorTarget) {
            cmd.textureBarrier({
                fc.colorTarget,
                m_impl->swapchainLayout,                  // ColorAttachment, or General after a merge
                  fc.finalColorLayout
              });
              m_impl->swapchainLayout = fc.finalColorLayout;
            const bool ranShadow   = runShadowPass && m_impl->shadow.depthAtlas.valid();
            RenderGraphDesc rgDesc;
            if (ranShadow) {
                rgDesc.record(RenderPassType::Shadow,
                              nexus::gfx::TextureLayout::ColorAttachment,  // gbuffer irrelevant here
                              nexus::gfx::TextureLayout::DepthWrite);
            }
            rgDesc.record(RenderPassType::Geometry,
                          nexus::gfx::TextureLayout::ColorAttachment,
                          ranShadow ? nexus::gfx::TextureLayout::DepthWrite
                                    : nexus::gfx::TextureLayout::Undefined);
            rgDesc.record(RenderPassType::Composite,
                          nexus::gfx::TextureLayout::ShaderRead,
                          ranShadow ? nexus::gfx::TextureLayout::DepthRead
                                    : nexus::gfx::TextureLayout::DepthRead);
            if (runRayTracingPass) {
                rgDesc.record(RenderPassType::RayTracing,
                              nexus::gfx::TextureLayout::ShaderRead,
                              ranShadow ? nexus::gfx::TextureLayout::DepthRead
                                        : nexus::gfx::TextureLayout::Undefined);
            }
            if (runRayTracingMerge && hasCompositeColorTarget) {
                rgDesc.record(RenderPassType::RayTracingMerge,
                              nexus::gfx::TextureLayout::ShaderRead,
                              ranShadow ? nexus::gfx::TextureLayout::DepthRead
                                        : nexus::gfx::TextureLayout::Undefined,
                              nexus::gfx::TextureLayout::General);
            }
            m_impl->lastRenderGraphReport = RenderGraphValidator::validate(rgDesc);
        }

        // ── Frame capture ─────────────────────────────────────────────────
        if (m_impl->captureExporter) {
            if (runShadowPass && m_impl->shadow.depthAtlas.valid()) {
                uint32_t shadowDraws = 0;
                uint32_t shadowTris  = 0;
                for (uint32_t ci = 0; ci < m_impl->shadow.cascadeCount; ++ci) {
                    shadowDraws += m_impl->shadow.cascadeDrawCalls[ci];
                    shadowTris  += m_impl->shadow.cascadeTriangles[ci];
                }
                FramePassEntry shadowEntry{};
                shadowEntry.passType          = RenderPassType::Shadow;
                shadowEntry.drawCalls         = shadowDraws;
                shadowEntry.triangles         = shadowTris;
                shadowEntry.shadowAtlasLayout = nexus::gfx::TextureLayout::DepthRead; // post-transition
                m_impl->captureExporter->recordPass(shadowEntry);
            }

            FramePassEntry geoEntry{};
            geoEntry.passType           = RenderPassType::Geometry;
            geoEntry.drawCalls          = geometryDraws;
            geoEntry.triangles          = geometryTriangles;
            geoEntry.gbufferColorLayout = nexus::gfx::TextureLayout::ShaderRead; // post-transition
            m_impl->captureExporter->recordPass(geoEntry);

            FramePassEntry compEntry{};
            compEntry.passType           = RenderPassType::Composite;
            compEntry.drawCalls          = 1; // fullscreen triangle
            compEntry.gbufferColorLayout = nexus::gfx::TextureLayout::ShaderRead;
            compEntry.shadowAtlasLayout  = runShadowPass ? nexus::gfx::TextureLayout::DepthRead
                                                         : nexus::gfx::TextureLayout::Undefined;
            m_impl->captureExporter->recordPass(compEntry);

            if (runRayTracingPass) {
                FramePassEntry rtEntry{};
                rtEntry.passType          = RenderPassType::RayTracing;
                rtEntry.drawCalls         = 1;
                rtEntry.triangles         = 0;
                rtEntry.gbufferColorLayout = nexus::gfx::TextureLayout::ShaderRead;
                rtEntry.shadowAtlasLayout  = runShadowPass ? nexus::gfx::TextureLayout::DepthRead
                                                           : nexus::gfx::TextureLayout::Undefined;
                m_impl->captureExporter->recordPass(rtEntry);
            }

            if (runRayTracingMerge && hasCompositeColorTarget) {
                FramePassEntry mergeEntry{};
                mergeEntry.passType          = RenderPassType::RayTracingMerge;
                mergeEntry.drawCalls         = 0; // compute dispatch, not a draw
                mergeEntry.triangles         = 0;
                mergeEntry.gbufferColorLayout = nexus::gfx::TextureLayout::ShaderRead;
                mergeEntry.shadowAtlasLayout  = runShadowPass ? nexus::gfx::TextureLayout::DepthRead
                                                              : nexus::gfx::TextureLayout::Undefined;
                m_impl->captureExporter->recordPass(mergeEntry);
            }
        }
    } else {
        // ── Direct swapchain path (non-production compatibility fallback) ─────────
        // Scheduler path is the authoritative production flow; this branch is
        // intentionally minimal and does not aim for feature parity.
        auto& dev = m_ctx.device();
        dev.submit(nexus::gfx::QueueType::Graphics,
                   {&m_impl->cmdBufs[0], 1}, {}, {}, m_impl->frameFence);
    }

    ++m_impl->frameIndex;
}

void Renderer::endFrame()
{
    if (m_impl->scheduler) {
        if (m_impl->currentFrame)
            m_impl->scheduler->endFrame();
    } else {
        // Present the frame acquired in beginFrame()
        m_swapchain.present(m_impl->currentImageIndex, {});
    }

    if (m_impl->captureExporter) {
        m_impl->captureExporter->endCapture();
    }
}

void Renderer::applySettings(const RendererSettings& s)
{
    const bool shadowsWereEnabled = m_settings.enableShadows;
    m_settings = s;

    if (shadowsWereEnabled && !m_settings.enableShadows) {
        // Shadow resources and pass descriptors should not remain externally
        // visible once shadows are disabled.
        destroyShadowTargets();
        m_impl->shadow.cascadeDrawCalls.fill(0);
        m_impl->shadow.cascadeTriangles.fill(0);
    }

    selectRenderPath();
}

void Renderer::setFallbackGeometryPipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->fallbackGeometryPipeline = pipeline;
}

void Renderer::setShadowPipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->shadowPipeline = pipeline;
}

void Renderer::setLightingCompositePipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->lightingCompositePipeline = pipeline;
}

void Renderer::setRayTracingPipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->rayTracingPipeline = pipeline;
}

void Renderer::setRayTracingMergePipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->rayTracingMergePipeline = pipeline;
}

void Renderer::setMaterialPipeline(MaterialID material, nexus::gfx::PipelineHandle pipeline) noexcept
{
    if (!pipeline.valid()) {
        m_impl->materialPipelines.erase(material);
        return;
    }
    m_impl->materialPipelines[material] = pipeline;
}

void Renderer::clearMaterialPipelines() noexcept
{
    m_impl->materialPipelines.clear();
}

void Renderer::setCompositeMaterialBindings(const CompositeMaterialBindings& bindings) noexcept
{
    m_impl->compositeBindings = bindings;
}

void Renderer::clearCompositeMaterialBindings() noexcept
{
    m_impl->compositeBindings = {};
}

void Renderer::setShadowLightingContract(const ShadowLightingContract& contract) noexcept
{
    m_impl->shadowLightingContract = contract;
    if (m_impl->shadowLightingContract.cascadeCount > ShadowLightingContract::kMaxCascades)
        m_impl->shadowLightingContract.cascadeCount = ShadowLightingContract::kMaxCascades;
    m_impl->shadowLightingDirty = true;
}

void Renderer::clearShadowLightingContract() noexcept
{
    m_impl->shadowLightingContract = {};
    m_impl->shadowLightingDirty = true;
}

ShadowPassBindingDesc Renderer::buildShadowPassBindingDesc() const noexcept
{
    ShadowPassBindingDesc desc{};
    if (!m_settings.enableShadows) {
        return desc;
    }

    desc.atlasDepthTexture = m_impl->shadow.depthAtlas;
    desc.atlasExtent = m_impl->shadow.atlasExtent;
    desc.cascadeExtent = m_impl->shadow.cascadeExtent;
    desc.atlasDepthLayout = m_impl->shadowDepthLayout;
    desc.cascadeCount = m_impl->shadowLightingContract.cascadeCount;
    desc.atlasColumns = m_impl->shadow.atlasColumns;
    desc.atlasRows = m_impl->shadow.atlasRows;
    const uint32_t n = desc.cascadeCount < ShadowPassBindingDesc::kMaxCascades
                     ? desc.cascadeCount
                     : ShadowPassBindingDesc::kMaxCascades;
    for (uint32_t i = 0; i < n; ++i) {
        desc.cascadePasses[i].cascadeIndex = i;
        desc.cascadePasses[i].atlasRect = m_impl->shadow.cascadeViewports[i];
        desc.cascadePasses[i].drawCalls = m_impl->shadow.cascadeDrawCalls[i];
        desc.cascadePasses[i].triangles = m_impl->shadow.cascadeTriangles[i];
    }
    return desc;
}

std::span<const nexus::gfx::DescriptorBindingDesc> Renderer::compositeCoreSetLayout() noexcept
{
    return kCompositeCoreLayout;
}

std::span<const nexus::gfx::DescriptorBindingDesc> Renderer::compositeShadowSetLayout() noexcept
{
    return kCompositeShadowLayout;
}

std::span<const nexus::gfx::DescriptorBindingDesc> Renderer::geometryCameraSetLayout() noexcept
{
    return kGeometryCameraLayout;
}

std::span<const nexus::gfx::Format> Renderer::gbufferColorFormats() noexcept
{
    return kGBufferColorFormats;
}

nexus::gfx::Format Renderer::gbufferDepthFormat() noexcept
{
    return kGBufferDepthFormat;
}

nexus::gfx::PipelineHandle Renderer::createGBufferGeometryPipeline(
    nexus::gfx::IDevice& device, const GBufferGeometryPipelineDesc& desc)
{
    const std::array<nexus::gfx::DescriptorSetLayoutDesc, 1> sets{{ { geometryCameraSetLayout() } }};

    nexus::gfx::GraphicsPipelineDesc gp{};
    gp.vertexShader           = desc.vertexShader;
    gp.fragmentShader         = desc.fragmentShader;
    gp.topology               = nexus::gfx::Topology::TriangleList;
    gp.cullMode               = desc.cullMode;
    gp.depthTest              = true;
    gp.depthWrite             = true;  // reversed-Z, GreaterOrEqual (matches the GBuffer pass)
    gp.colorAttachmentFormats = gbufferColorFormats();
    gp.depthAttachmentFormat  = gbufferDepthFormat();
    gp.vertexBindings         = desc.vertexBindings;
    gp.vertexAttributes       = desc.vertexAttributes;
    gp.descriptorSetLayouts   = sets;
    gp.debugName              = desc.debugName;
    return device.createGraphicsPipeline(gp);
}

void Renderer::uploadCameraUniform(const Camera& camera)
{
    auto& dev = m_ctx.device();
    if (!m_impl->cameraBuffer.valid()) {
        nexus::gfx::BufferDesc bd{};
        bd.sizeBytes = sizeof(CameraUBO);
        bd.usage = nexus::gfx::BufferUsage::UniformBuffer | nexus::gfx::BufferUsage::TransferDst;
        bd.memory = nexus::gfx::MemoryHint::GpuOnly;
        bd.debugName = "Camera.UBO";
        m_impl->cameraBuffer = dev.createBuffer(bd);
    }
    if (m_impl->cameraBuffer.valid()) {
        const CameraUBO& ubo = camera.ubo();
        dev.uploadBuffer(m_impl->cameraBuffer, &ubo, sizeof(CameraUBO), 0);
    }
}

CompositePassBindingDesc Renderer::buildCompositePassBindingDesc() const noexcept
{
    CompositePassBindingDesc desc{};
    desc.albedoTexture = m_impl->gbuffer.albedoMaterial;
    desc.normalTexture = m_impl->gbuffer.normal;
    desc.velocityTexture = m_impl->gbuffer.velocity;
    desc.depthTexture = m_impl->gbuffer.depth;
    desc.albedoSampler = m_impl->compositeBindings.albedoMaterialSampler;
    desc.normalSampler = m_impl->compositeBindings.normalSampler;
    desc.velocitySampler = m_impl->compositeBindings.velocitySampler;
    desc.depthSampler = m_impl->compositeBindings.depthSampler;
    desc.materialTable = m_impl->compositeBindings.materialTable;
    desc.materialTableOffsetBytes = m_impl->compositeBindings.materialTableOffsetBytes;

    const bool hasCompleteShadowContract = m_settings.enableShadows
        && m_impl->shadow.depthAtlas.valid()
        && m_impl->compositeBindings.shadowDepthSampler.valid()
        && m_impl->shadowLightingBuffer.valid()
        && m_impl->shadowLightingContract.cascadeCount > 0;
    if (hasCompleteShadowContract) {
        desc.shadowDepthTexture = m_impl->shadow.depthAtlas;
        desc.shadowDepthSampler = m_impl->compositeBindings.shadowDepthSampler;
        desc.shadowLightingBuffer = m_impl->shadowLightingBuffer;
        desc.shadowCascadeCount = m_impl->shadowLightingContract.cascadeCount;
    }

    return desc;
}

ShadowLightingBindingDesc Renderer::buildShadowLightingBindingDesc() const noexcept
{
    ShadowLightingBindingDesc desc{};

    const bool hasCompleteShadowContract = m_settings.enableShadows
        && m_impl->shadow.depthAtlas.valid()
        && m_impl->compositeBindings.shadowDepthSampler.valid()
        && m_impl->shadowLightingBuffer.valid()
        && m_impl->shadowLightingContract.cascadeCount > 0;

    if (hasCompleteShadowContract) {
        desc.shadowDepthTexture   = m_impl->shadow.depthAtlas;
        desc.shadowDepthSampler   = m_impl->compositeBindings.shadowDepthSampler;
        desc.shadowLightingBuffer = m_impl->shadowLightingBuffer;
        desc.shadowCascadeCount   = m_impl->shadowLightingContract.cascadeCount;
        desc.shadowBias           = m_impl->shadowLightingContract.shadowBias;
        desc.normalBias           = m_impl->shadowLightingContract.normalBias;
        const uint32_t n = desc.shadowCascadeCount < ShadowLightingBindingDesc::kMaxCascades
                         ? desc.shadowCascadeCount : ShadowLightingBindingDesc::kMaxCascades;
        for (uint32_t i = 0; i < n; ++i) {
            desc.lightViewProj[i] = m_impl->shadowLightingContract.lightViewProj[i];
            desc.cascadeSplits[i] = m_impl->shadowLightingContract.cascadeSplits[i];
        }
    }

    return desc;
}

void Renderer::resetScene(SceneGraph& scene)
{
    auto& dev = m_ctx.device();
    dev.waitIdle();

    scene.clear();

    resetRendererSceneCaches(*m_impl, dev, m_stats);
}

void Renderer::resetSceneAndDestroyTLAS(SceneGraph& scene)
{
    auto& dev = m_ctx.device();
    dev.waitIdle();

    scene.clearAndDestroyTLAS(dev);

    resetRendererSceneCaches(*m_impl, dev, m_stats);
}

void Renderer::onResize(nexus::gfx::Extent2D newExtent)
{
    m_ctx.device().waitIdle();
    releaseAllDeferredDescriptorSets(*m_impl, m_ctx.device());
    if (m_impl->scheduler) {
        m_impl->scheduler->onResize(newExtent);
    } else {
        m_swapchain.resize(newExtent);
    }
    destroyShadowTargets();
    destroyGBuffer();
}

void Renderer::selectRenderPath()
{
    const auto& caps = m_ctx.caps();
    const auto  tier = m_ctx.hardwareTier();

    // Downgrade render mode if hardware doesn't support it
    if (m_settings.mode == RenderMode::PathTrace && caps.rayTracingTier < 2 && !m_settings.enableRayTracingStub)
        m_settings.mode = (caps.rayTracingTier == 1) ? RenderMode::HybridRT : RenderMode::Rasterize;

    if (m_settings.mode == RenderMode::HybridRT && caps.rayTracingTier < 1 && !m_settings.enableRayTracingStub)
        m_settings.mode = RenderMode::Rasterize;

    // Upscaler degradation on low-end
    if (tier == nexus::gfx::HardwareTier::Low || tier == nexus::gfx::HardwareTier::Mid)
        if (m_settings.upscale != UpscaleMode::Off && m_settings.upscale != UpscaleMode::Bilinear)
            m_settings.upscale = UpscaleMode::Bilinear;
}

// ── Render graph validation API ───────────────────────────────────────────────

void Renderer::setRenderGraphValidationEnabled(bool enabled) noexcept
{
    m_impl->renderGraphValidationEnabled = enabled;
    if (!enabled) {
        m_impl->lastRenderGraphReport = {};
    }
}

bool Renderer::isRenderGraphValidationEnabled() const noexcept
{
    return m_impl->renderGraphValidationEnabled;
}

const RenderGraphValidationReport& Renderer::lastRenderGraphReport() const noexcept
{
    return m_impl->lastRenderGraphReport;
}

// ── Frame capture API ─────────────────────────────────────────────────────────

void Renderer::setFrameCaptureExporter(IFrameCaptureExporter* exporter) noexcept
{
    m_impl->captureExporter = exporter;
}

IFrameCaptureExporter* Renderer::frameCaptureExporter() const noexcept
{
    return m_impl->captureExporter;
}

} // namespace nexus::render
