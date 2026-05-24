#include <nexus/render/Renderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/CommandBuffer.h>
#include <nexus/gfx/FrameScheduler.h>
#include <nexus/gfx/RenderContext.h>

#include <gtest/gtest.h>

#include "RendererTestSupport.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>
#include <vector>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

class RecordingCommandBuffer final : public ICommandBuffer {
public:
    struct Event {
        std::string kind;
        uint32_t a = 0;
        uint32_t b = 0;
    };

    struct BarrierBatch {
        std::vector<TextureBarrier> barriers;
    };

    std::vector<Event> events;
    std::vector<BarrierBatch> textureBarrierBatches;
    std::vector<TextureBarrier> singleTextureBarriers;

    void clearRecordedState()
    {
        events.clear();
        textureBarrierBatches.clear();
        singleTextureBarriers.clear();
    }

    void begin() override { events.push_back({"begin"}); }
    void end() override { events.push_back({"end"}); }
    void reset() override { events.push_back({"reset"}); }

    void beginRenderPass(RenderPassHandle, std::span<const TextureHandle> colorTargets,
                         TextureHandle depthTarget, std::span<const ClearValue>, const Rect2D&) override
    {
        events.push_back({"beginRenderPass", static_cast<uint32_t>(colorTargets.size()), depthTarget.valid() ? 1u : 0u});
    }
    void endRenderPass() override { events.push_back({"endRenderPass"}); }

    void bindPipeline(PipelineHandle p) override {
        events.push_back({"bindPipeline", static_cast<uint32_t>(p.id & 0xFFFFFFFFu), 0u});
    }

    void setViewport(const Viewport&) override { events.push_back({"setViewport"}); }
    void setScissor(const Rect2D&) override { events.push_back({"setScissor"}); }

    void bindVertexBuffer(BufferHandle, uint64_t, uint32_t) override { events.push_back({"bindVertexBuffer"}); }
    void bindIndexBuffer(BufferHandle, uint64_t, bool) override { events.push_back({"bindIndexBuffer"}); }

    void pushConstants(ShaderStage stages, const void*, uint32_t sizeBytes, uint32_t) override {
        events.push_back({"pushConstants", sizeBytes, static_cast<uint32_t>(stages)});
    }

    void draw(uint32_t vtx, uint32_t inst, uint32_t, uint32_t) override {
        events.push_back({"draw", vtx, inst});
    }
    void drawIndexed(uint32_t idx, uint32_t inst, uint32_t, int32_t, uint32_t) override {
        events.push_back({"drawIndexed", idx, inst});
    }
    void drawIndirect(BufferHandle, uint64_t, uint32_t, uint32_t) override {}
    void drawIndexedIndirect(BufferHandle, uint64_t, uint32_t, uint32_t) override {}

    void drawMeshTasks(uint32_t, uint32_t, uint32_t) override {}
    void drawMeshTasksIndirect(BufferHandle, uint64_t, uint32_t) override {}

    void dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t) override {
        events.push_back({"dispatch", groupsX, groupsY});
    }
    void dispatchIndirect(BufferHandle, uint64_t) override {}

    void traceRays(uint32_t width, uint32_t height, uint32_t depth) override {
        events.push_back({"traceRays", width, height});
    }

    void copyBuffer(BufferHandle, BufferHandle, uint64_t, uint64_t, uint64_t) override {}
    void copyTexture(TextureHandle, TextureHandle) override {}
    void blitTexture(TextureHandle, TextureHandle, const Rect2D&, const Rect2D&) override {}

    void globalBarrier(const GlobalMemoryBarrier&) override {}

    void textureBarrier(const TextureBarrier& b) override {
        singleTextureBarriers.push_back(b);
        events.push_back({"textureBarrier"});
    }

    void textureBarriers(std::span<const TextureBarrier> b) override {
        BarrierBatch batch{};
        batch.barriers.assign(b.begin(), b.end());
        textureBarrierBatches.push_back(std::move(batch));
        events.push_back({"textureBarriers", static_cast<uint32_t>(b.size()), 0u});
    }

    void bufferBarrier(const BufferBarrier&) override {}
    void bufferBarriers(std::span<const BufferBarrier>) override {}

    void resetQueryPool(QueryPoolHandle, uint32_t, uint32_t) override {}
    void writeTimestamp(QueryPoolHandle, uint32_t) override {}

    void beginDebugLabel(const char*, float, float, float) override {}
    void endDebugLabel() override {}
    void insertDebugLabel(const char*) override {}

    void bindDescriptorSet(DescriptorSetHandle set, uint32_t setIndex) override {
        events.push_back({"bindDescriptorSet",
                          static_cast<uint32_t>(set.id & 0xFFFFFFFFu),
                          setIndex});
    }
};

class RecordingScheduler final : public IFrameScheduler {
public:
    explicit RecordingScheduler(RecordingCommandBuffer& cb,
                                std::vector<uint32_t> frameSlots = {},
                                uint32_t maxFramesInFlight = 2)
        : m_cb(cb), m_frameSlots(std::move(frameSlots)), m_maxFramesInFlight(maxFramesInFlight) {}

    std::optional<FrameContext> beginFrame() override {
        const uint32_t frameSlot = currentFrameSlot();
        FrameContext fc{};
        fc.cmd = &m_cb;
        fc.colorTarget.id = 555;
        fc.extent = extent;
        fc.frameSlot = frameSlot;
        fc.imageIndex = static_cast<uint32_t>(beginCount);
        ++beginCount;
        return fc;
    }

    void endFrame() override { ++endCount; }
    void onResize(Extent2D newExtent) override {
        extent = newExtent;
        ++resizeCount;
    }
    uint32_t maxFramesInFlight() const noexcept override { return m_maxFramesInFlight; }

    int beginCount = 0;
    int endCount = 0;
    int resizeCount = 0;
    Extent2D extent{1280, 720};

private:
    uint32_t currentFrameSlot() const
    {
        if (m_frameSlots.empty()) {
            return 0;
        }

        const size_t index = static_cast<size_t>(beginCount) < m_frameSlots.size()
            ? static_cast<size_t>(beginCount)
            : (m_frameSlots.size() - 1);
        return m_frameSlots[index];
    }

    RecordingCommandBuffer& m_cb;
    std::vector<uint32_t> m_frameSlots;
    uint32_t m_maxFramesInFlight = 2;
};

struct BoundDescriptorSets {
    uint32_t core = 0;
    uint32_t shadow = 0;
};

BoundDescriptorSets collectBoundDescriptorSets(const RecordingCommandBuffer& cmd)
{
    BoundDescriptorSets result{};
    for (const auto& e : cmd.events) {
        if (e.kind != "bindDescriptorSet") {
            continue;
        }
        if (e.b == 0u) {
            result.core = e.a;
        } else if (e.b == 1u) {
            result.shadow = e.a;
        }
    }
    return result;
}

std::array<uint32_t, 2> sortedDescriptorSetIds(const BoundDescriptorSets& ids)
{
    std::array<uint32_t, 2> result{ ids.core, ids.shadow };
    std::sort(result.begin(), result.end());
    return result;
}

int indexOfKind(const std::vector<RecordingCommandBuffer::Event>& ev, const char* k)
{
    for (size_t i = 0; i < ev.size(); ++i)
        if (ev[i].kind == k) return static_cast<int>(i);
    return -1;
}

int nthIndexOfKind(const std::vector<RecordingCommandBuffer::Event>& ev, const char* k, int n)
{
    int seen = 0;
    for (size_t i = 0; i < ev.size(); ++i) {
        if (ev[i].kind == k) {
            if (seen == n) return static_cast<int>(i);
            ++seen;
        }
    }
    return -1;
}

void expectMat4Eq(const Mat4& actual, const Mat4& expected)
{
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            EXPECT_FLOAT_EQ(actual.m[r][c], expected.m[r][c]);
        }
    }
}

} // namespace

TEST(RendererBehavior, SchedulerPathRecordsPassSequenceAndTransitions)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle geomPipe{}; geomPipe.id = 101;
    PipelineHandle lightPipe{}; lightPipe.id = 202;

    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    BufferDesc materialTableDesc{};
    materialTableDesc.sizeBytes = 256;
    materialTableDesc.usage = BufferUsage::StorageBuffer;
    materialTableDesc.memory = MemoryHint::GpuOnly;
    materialTableDesc.debugName = "test.renderer.materialTable";
    BufferHandle materialTable = dev.createBuffer(materialTableDesc);
    ASSERT_TRUE(materialTable.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    bindings.materialTable = materialTable;
    bindings.materialTableOffsetBytes = 32;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("tri");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 10;
    n->mesh.indexBuffer.id  = 11;
    n->mesh.indexCount      = 36;

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    EXPECT_EQ(scheduler.beginCount, 1);
    EXPECT_EQ(scheduler.endCount, 1);

    ASSERT_GE(cmd.textureBarrierBatches.size(), 2u);
    ASSERT_EQ(cmd.textureBarrierBatches[0].barriers.size(), 4u);
    ASSERT_EQ(cmd.textureBarrierBatches[1].barriers.size(), 4u);

    // Geometry transition batch
    EXPECT_EQ(cmd.textureBarrierBatches[0].barriers[0].newLayout, TextureLayout::ColorAttachment);
    EXPECT_EQ(cmd.textureBarrierBatches[0].barriers[1].newLayout, TextureLayout::ColorAttachment);
    EXPECT_EQ(cmd.textureBarrierBatches[0].barriers[2].newLayout, TextureLayout::ColorAttachment);
    EXPECT_EQ(cmd.textureBarrierBatches[0].barriers[3].newLayout, TextureLayout::DepthWrite);

    // Lighting input transition batch
    EXPECT_EQ(cmd.textureBarrierBatches[1].barriers[0].newLayout, TextureLayout::ShaderRead);
    EXPECT_EQ(cmd.textureBarrierBatches[1].barriers[1].newLayout, TextureLayout::ShaderRead);
    EXPECT_EQ(cmd.textureBarrierBatches[1].barriers[2].newLayout, TextureLayout::ShaderRead);
    EXPECT_EQ(cmd.textureBarrierBatches[1].barriers[3].newLayout, TextureLayout::DepthRead);

    ASSERT_EQ(cmd.singleTextureBarriers.size(), 2u);
    EXPECT_EQ(cmd.singleTextureBarriers[0].oldLayout, TextureLayout::Present);
    EXPECT_EQ(cmd.singleTextureBarriers[0].newLayout, TextureLayout::ColorAttachment);
    EXPECT_EQ(cmd.singleTextureBarriers[1].oldLayout, TextureLayout::ColorAttachment);
    EXPECT_EQ(cmd.singleTextureBarriers[1].newLayout, TextureLayout::Present);

    const int firstPass  = nthIndexOfKind(cmd.events, "beginRenderPass", 0);
    const int secondPass = nthIndexOfKind(cmd.events, "beginRenderPass", 1);
    const int firstDrawIndexed = indexOfKind(cmd.events, "drawIndexed");
    const int toLightingBatch = nthIndexOfKind(cmd.events, "textureBarriers", 1);
    const int bindDescSet = indexOfKind(cmd.events, "bindDescriptorSet");
    const int fsDraw = indexOfKind(cmd.events, "draw");
    const int lastBarrier = nthIndexOfKind(cmd.events, "textureBarrier", 1);

    ASSERT_GE(firstPass, 0);
    ASSERT_GE(secondPass, 0);
    ASSERT_GE(toLightingBatch, 0);
    ASSERT_GE(bindDescSet, 0);
    ASSERT_GE(fsDraw, 0);
    ASSERT_GE(lastBarrier, 0);

    if (firstDrawIndexed >= 0) {
        EXPECT_LT(firstPass, firstDrawIndexed);
        EXPECT_LT(firstDrawIndexed, secondPass);
    }
    EXPECT_LT(toLightingBatch, bindDescSet);
    EXPECT_LT(secondPass, fsDraw);
    EXPECT_LT(bindDescSet, fsDraw);
    EXPECT_LT(fsDraw, lastBarrier);

    // Ensure first and second render pass signatures are as expected.
    EXPECT_EQ(cmd.events[firstPass].a, 3u);  // 3 GBuffer color attachments
    EXPECT_EQ(cmd.events[firstPass].b, 1u);  // depth attached
    EXPECT_EQ(cmd.events[secondPass].a, 1u); // final color pass
    EXPECT_EQ(cmd.events[secondPass].b, 0u); // no depth attached

    dev.destroyBuffer(materialTable);
    dev.destroySampler(sampler);
}

TEST(RendererBehavior, RayTracingStubModeTriggersTraceRaysOnNullBackend)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle rtPipe{};
    rtPipe.id = 999;
    renderer.setRayTracingPipeline(rtPipe);

    RendererSettings settings{};
    settings.mode = RenderMode::PathTrace;
    settings.enableRayTracingStub = true;
    renderer.applySettings(settings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("raytracer");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 11;
    n->mesh.indexBuffer.id = 12;
    n->mesh.indexCount = 3;

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const int traceIndex = indexOfKind(cmd.events, "traceRays");
    EXPECT_GE(traceIndex, 0);
    EXPECT_EQ(cmd.events[traceIndex].a, 1280u);
    EXPECT_EQ(cmd.events[traceIndex].b, 720u);
    EXPECT_EQ(renderer.lastFrameStats().rayTracingDrawCalls, 1u);
    EXPECT_EQ(renderer.lastFrameStats().rayPayloads, renderer.lastFrameStats().visibleNodes);

    // Without a merge pipeline set, no merge dispatch runs.
    EXPECT_LT(indexOfKind(cmd.events, "dispatch"), 0);
    EXPECT_EQ(renderer.lastFrameStats().rayMergeDispatches, 0u);
}

TEST(RendererBehavior, RayTracingMergeRunsComputePassAfterTraceRays)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle rtPipe{};   rtPipe.id = 999;
    PipelineHandle mergePipe{}; mergePipe.id = 1001;
    renderer.setRayTracingPipeline(rtPipe);
    renderer.setRayTracingMergePipeline(mergePipe);

    RendererSettings settings{};
    settings.mode = RenderMode::PathTrace;
    settings.enableRayTracingStub = true;
    renderer.applySettings(settings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("raytracer");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 11;
    n->mesh.indexBuffer.id = 12;
    n->mesh.indexCount = 3;

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const int traceIndex    = indexOfKind(cmd.events, "traceRays");
    const int dispatchIndex = indexOfKind(cmd.events, "dispatch");

    // Merge runs as a compute dispatch, strictly after the RT trace.
    ASSERT_GE(traceIndex, 0);
    ASSERT_GE(dispatchIndex, 0);
    EXPECT_GT(dispatchIndex, traceIndex);

    // Dispatch covers the full target in 8x8 tiles: ceil(1280/8)=160, ceil(720/8)=90.
    EXPECT_EQ(cmd.events[dispatchIndex].a, 160u);
    EXPECT_EQ(cmd.events[dispatchIndex].b, 90u);
    EXPECT_EQ(renderer.lastFrameStats().rayMergeDispatches, 1u);
}

TEST(RendererBehavior, ShadowPassRunsBeforeGeometryAndTransitionsForSampling)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 111;
    PipelineHandle geomPipe{};   geomPipe.id = 112;
    PipelineHandle lightPipe{};  lightPipe.id = 113;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract shadowContract{};
    shadowContract.cascadeCount = 2;
    shadowContract.cascadeSplits[0] = 0.15f;
    shadowContract.cascadeSplits[1] = 0.45f;
    shadowContract.lightViewProj[0] = Mat4::identity();
    shadowContract.lightViewProj[1] = Mat4::identity();
    renderer.setShadowLightingContract(shadowContract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("shadow-caster");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 51;
    n->mesh.indexBuffer.id  = 52;
    n->mesh.indexCount      = 12;
    n->castShadow = true;

    Camera cam;
    cam.setPerspective(70.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    ASSERT_GE(cmd.textureBarrierBatches.size(), 4u);
    EXPECT_EQ(cmd.textureBarrierBatches[0].barriers.size(), 1u); // shadow -> depth write
    EXPECT_EQ(cmd.textureBarrierBatches[0].barriers[0].newLayout, TextureLayout::DepthWrite);
    EXPECT_EQ(cmd.textureBarrierBatches[1].barriers.size(), 1u); // shadow -> depth read
    EXPECT_EQ(cmd.textureBarrierBatches[1].barriers[0].newLayout, TextureLayout::DepthRead);

    const int shadowPass0 = nthIndexOfKind(cmd.events, "beginRenderPass", 0);
    const int shadowPass1 = nthIndexOfKind(cmd.events, "beginRenderPass", 1);
    const int gbufferPass = nthIndexOfKind(cmd.events, "beginRenderPass", 2);
    const int lightingPass = nthIndexOfKind(cmd.events, "beginRenderPass", 3);
    const int shadowToReadBatch = nthIndexOfKind(cmd.events, "textureBarriers", 1);
    const int bindDescSet = indexOfKind(cmd.events, "bindDescriptorSet");
    const int fsDraw = indexOfKind(cmd.events, "draw");

    ASSERT_GE(shadowPass0, 0);
    ASSERT_GE(shadowPass1, 0);
    ASSERT_GE(gbufferPass, 0);
    ASSERT_GE(lightingPass, 0);
    ASSERT_GE(shadowToReadBatch, 0);
    ASSERT_GE(bindDescSet, 0);
    ASSERT_GE(fsDraw, 0);

    EXPECT_EQ(cmd.events[shadowPass0].a, 0u); // depth-only
    EXPECT_EQ(cmd.events[shadowPass0].b, 1u); // has depth
    EXPECT_EQ(cmd.events[shadowPass1].a, 0u); // depth-only
    EXPECT_EQ(cmd.events[shadowPass1].b, 1u); // has depth
    EXPECT_LT(shadowPass0, shadowPass1);
    EXPECT_LT(shadowPass1, gbufferPass);
    EXPECT_LT(gbufferPass, lightingPass);
    EXPECT_LT(shadowToReadBatch, bindDescSet);
    EXPECT_LT(bindDescSet, fsDraw);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowPassBindingDescTracksTargetExtentAndSamplingState)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {960, 540};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 118;
    PipelineHandle geomPipe{};   geomPipe.id = 119;
    PipelineHandle lightPipe{};  lightPipe.id = 120;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 2;
    contract.cascadeSplits[0] = 0.2f;
    contract.cascadeSplits[1] = 0.6f;
    renderer.setShadowLightingContract(contract);

    const ShadowPassBindingDesc shadowBeforeRender = renderer.buildShadowPassBindingDesc();
    EXPECT_FALSE(shadowBeforeRender.hasDepthTarget());
    EXPECT_FALSE(shadowBeforeRender.readyForLightingSampling());

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {960, 540};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("shadow-pass-desc");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 61;
    n->mesh.indexBuffer.id  = 62;
    n->mesh.indexCount      = 18;
    n->castShadow = true;
    n->localTransform().translation = {0.f, 0.f, -5.f};

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    EXPECT_GE(renderer.lastFrameStats().visibleNodes, 1u);

    const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
    EXPECT_TRUE(shadowDesc.hasDepthTarget());
    EXPECT_TRUE(shadowDesc.readyForDepthPass());
    EXPECT_TRUE(shadowDesc.readyForLightingSampling());
    EXPECT_EQ(shadowDesc.cascadeExtent.width, 960u);
    EXPECT_EQ(shadowDesc.cascadeExtent.height, 540u);
    EXPECT_EQ(shadowDesc.atlasExtent.width, 1920u);
    EXPECT_EQ(shadowDesc.atlasExtent.height, 540u);
    EXPECT_EQ(shadowDesc.atlasDepthLayout, TextureLayout::DepthRead);
    EXPECT_EQ(shadowDesc.cascadeCount, 2u);
    EXPECT_EQ(shadowDesc.atlasColumns, 2u);
    EXPECT_EQ(shadowDesc.atlasRows, 1u);
    EXPECT_TRUE(shadowDesc.hasPassDescriptors());
    EXPECT_EQ(shadowDesc.cascadePasses[0].cascadeIndex, 0u);
    EXPECT_EQ(shadowDesc.cascadePasses[0].atlasRect.offset.x, 0);
    EXPECT_EQ(shadowDesc.cascadePasses[0].atlasRect.extent.width, 960u);
    EXPECT_EQ(shadowDesc.cascadePasses[1].cascadeIndex, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[1].atlasRect.offset.x, 960);
    EXPECT_EQ(shadowDesc.cascadePasses[1].atlasRect.extent.width, 960u);

    const uint32_t totalCascadeDrawCalls = shadowDesc.cascadePasses[0].drawCalls
                                         + shadowDesc.cascadePasses[1].drawCalls;
    const uint32_t totalCascadeTriangles = shadowDesc.cascadePasses[0].triangles
                                         + shadowDesc.cascadePasses[1].triangles;
    // A single visible packet should not be replayed across every cascade anymore.
    EXPECT_LE(totalCascadeDrawCalls, 1u);
    EXPECT_LE(totalCascadeTriangles, 6u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowDescriptorsTrackEnableShadowsToggleDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 148;
    PipelineHandle geomPipe{};   geomPipe.id = 149;
    PipelineHandle lightPipe{};  lightPipe.id = 150;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 1;
    contract.cascadeSplits[0] = 0.5f;
    contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(0.0f);
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* caster = testsupport::addDeterministicShadowCaster(
        scene, "toggle-shadow-caster", 103, 104, {0.0f, 0.0f, -5.0f});
    ASSERT_NE(caster, nullptr);

    Camera cam;

    // Frame 1: shadows enabled and populated.
    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc enabledDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(enabledDesc.hasDepthTarget());
    ASSERT_EQ(enabledDesc.cascadeCount, 1u);
    EXPECT_EQ(enabledDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_TRUE(enabledDesc.readyForLightingSampling());
    EXPECT_TRUE(renderer.buildShadowLightingBindingDesc().isComplete());

    // Frame 2: disable shadows; descriptors must not expose stale depth targets.
    RendererSettings disabledSettings = renderer.settings();
    disabledSettings.enableShadows = false;
    renderer.applySettings(disabledSettings);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc disabledDesc = renderer.buildShadowPassBindingDesc();
    EXPECT_FALSE(disabledDesc.hasDepthTarget());
    EXPECT_FALSE(disabledDesc.hasPassDescriptors());
    EXPECT_EQ(disabledDesc.cascadeCount, 0u);
    EXPECT_FALSE(disabledDesc.readyForLightingSampling());
    EXPECT_FALSE(renderer.buildShadowLightingBindingDesc().isComplete());

    // Frame 3: re-enable and verify shadow descriptors repopulate correctly.
    RendererSettings reenabledSettings = renderer.settings();
    reenabledSettings.enableShadows = true;
    renderer.applySettings(reenabledSettings);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc reenabledDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(reenabledDesc.hasDepthTarget());
    ASSERT_EQ(reenabledDesc.cascadeCount, 1u);
    EXPECT_EQ(reenabledDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_TRUE(reenabledDesc.readyForLightingSampling());
    EXPECT_TRUE(renderer.buildShadowLightingBindingDesc().isComplete());

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowLightingBindingDescTracksLightViewProjAcrossEnableShadowsTransitionsDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 166;
    PipelineHandle geomPipe{};   geomPipe.id = 167;
    PipelineHandle lightPipe{};  lightPipe.id = 168;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* caster = testsupport::addDeterministicShadowCaster(
        scene, "toggle-matrix-caster", 115, 116, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(caster, nullptr);

    Camera cam;

    struct FrameCase {
        bool shadowsEnabled;
        float center0;
        uint32_t expectedDrawCalls;
    };

    const std::array<FrameCase, 8> frames = {{
        {true,  -2.0f, 3u},
        {false, -1.0f, 2u},
        {true,  -1.0f, 3u},
        {false, -2.0f, 2u},
        {true,  -2.0f, 3u},
        {false, -1.0f, 2u},
        {true,  -1.0f, 3u},
        {false, -2.0f, 2u},
    }};

    for (const auto& frameCase : frames) {
        ShadowLightingContract contract{};
        contract.cascadeCount = 1;
        contract.cascadeSplits[0] = 0.5f;
        contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0);
        renderer.setShadowLightingContract(contract);

        RendererSettings settings = renderer.settings();
        settings.enableShadows = frameCase.shadowsEnabled;
        renderer.applySettings(settings);

        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();

        const ShadowPassBindingDesc shadowPassDesc = renderer.buildShadowPassBindingDesc();
        const ShadowLightingBindingDesc shadowLightingDesc = renderer.buildShadowLightingBindingDesc();

        if (frameCase.shadowsEnabled) {
            ASSERT_TRUE(shadowPassDesc.hasPassDescriptors());
            ASSERT_EQ(shadowPassDesc.cascadeCount, 1u);
            EXPECT_EQ(shadowPassDesc.cascadePasses[0].drawCalls, 1u);

            ASSERT_TRUE(shadowLightingDesc.isComplete());
            EXPECT_EQ(shadowLightingDesc.shadowCascadeCount, 1u);
            EXPECT_FLOAT_EQ(shadowLightingDesc.cascadeSplits[0], 0.5f);
            expectMat4Eq(
                shadowLightingDesc.lightViewProj[0],
                testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0));
        } else {
            EXPECT_FALSE(shadowPassDesc.hasDepthTarget());
            EXPECT_FALSE(shadowPassDesc.hasPassDescriptors());
            EXPECT_EQ(shadowPassDesc.cascadeCount, 0u);

            EXPECT_FALSE(shadowLightingDesc.isComplete());
            EXPECT_EQ(shadowLightingDesc.shadowCascadeCount, 0u);
        }

        EXPECT_EQ(renderer.lastFrameStats().drawCalls, frameCase.expectedDrawCalls);
    }

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowDescriptorsTrackEnableShadowsAndRapidCascadeCountTransitionsDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 169;
    PipelineHandle geomPipe{};   geomPipe.id = 170;
    PipelineHandle lightPipe{};  lightPipe.id = 171;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* caster = testsupport::addDeterministicShadowCaster(
        scene, "toggle-cascade-transition-caster", 117, 118, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(caster, nullptr);

    Camera cam;

    struct FrameCase {
        bool shadowsEnabled;
        uint32_t cascadeCount;
        float center0;
        float center1;
        uint32_t expectedDrawCalls;
        bool expectShadowComplete;
        uint32_t expectedAtlasColumns;
        uint32_t expectedAtlasRows;
    };

    const std::array<FrameCase, 10> frames = {{
        {true,  2u, -2.0f,  8.0f, 3u, true,  2u, 1u},
        {false, 0u, -2.0f,  8.0f, 2u, false, 0u, 0u},
        {true,  1u, -2.0f,  8.0f, 3u, true,  1u, 1u},
        {false, 2u, -2.0f, -1.0f, 2u, false, 0u, 0u},
        {true,  2u, -2.0f, -1.0f, 4u, true,  2u, 1u},
        {false, 1u, -2.0f,  8.0f, 2u, false, 0u, 0u},
        {true,  1u, -2.0f,  8.0f, 3u, true,  1u, 1u},
        {false, 0u, -2.0f,  8.0f, 2u, false, 0u, 0u},
        {true,  2u, -2.0f,  8.0f, 3u, true,  2u, 1u},
        {false, 1u, -2.0f,  8.0f, 2u, false, 0u, 0u},
    }};

    for (const auto& frameCase : frames) {
        ShadowLightingContract contract{};
        contract.cascadeCount = frameCase.cascadeCount;
        contract.cascadeSplits[0] = 0.2f;
        contract.cascadeSplits[1] = 0.7f;
        contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0);
        contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center1);
        renderer.setShadowLightingContract(contract);

        RendererSettings settings = renderer.settings();
        settings.enableShadows = frameCase.shadowsEnabled;
        renderer.applySettings(settings);

        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();

        const ShadowPassBindingDesc shadowPassDesc = renderer.buildShadowPassBindingDesc();
        const ShadowLightingBindingDesc shadowLightingDesc = renderer.buildShadowLightingBindingDesc();

        if (frameCase.expectShadowComplete) {
            ASSERT_TRUE(shadowPassDesc.hasPassDescriptors());
            EXPECT_EQ(shadowPassDesc.cascadeCount, frameCase.cascadeCount);
            EXPECT_EQ(shadowPassDesc.atlasColumns, frameCase.expectedAtlasColumns);
            EXPECT_EQ(shadowPassDesc.atlasRows, frameCase.expectedAtlasRows);

            ASSERT_TRUE(shadowLightingDesc.isComplete());
            EXPECT_EQ(shadowLightingDesc.shadowCascadeCount, frameCase.cascadeCount);
            expectMat4Eq(
                shadowLightingDesc.lightViewProj[0],
                testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0));
            if (frameCase.cascadeCount > 1u) {
                expectMat4Eq(
                    shadowLightingDesc.lightViewProj[1],
                    testsupport::makeDeterministicCascadeLightViewProj(frameCase.center1));
            }
        } else {
            EXPECT_FALSE(shadowPassDesc.hasDepthTarget());
            EXPECT_FALSE(shadowPassDesc.hasPassDescriptors());
            EXPECT_EQ(shadowPassDesc.cascadeCount, 0u);
            EXPECT_EQ(shadowPassDesc.atlasColumns, frameCase.expectedAtlasColumns);
            EXPECT_EQ(shadowPassDesc.atlasRows, frameCase.expectedAtlasRows);

            EXPECT_FALSE(shadowLightingDesc.isComplete());
            EXPECT_EQ(shadowLightingDesc.shadowCascadeCount, 0u);
        }

        EXPECT_EQ(renderer.lastFrameStats().drawCalls, frameCase.expectedDrawCalls);
    }

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeFrustumCullingUsesPerCascadeLightBoundsDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 121;
    PipelineHandle geomPipe{};   geomPipe.id = 122;
    PipelineHandle lightPipe{};  lightPipe.id = 123;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 2;
    contract.cascadeSplits[0] = 0.2f;
    contract.cascadeSplits[1] = 0.6f;
    contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(2.0f);
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* cascade0Node = testsupport::addDeterministicShadowCaster(scene, "cascade-0", 71, 72, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(cascade0Node, nullptr);
    Node* cascade1Node = testsupport::addDeterministicShadowCaster(scene, "cascade-1", 73, 74, {2.0f, 0.0f, -5.0f});
    ASSERT_NE(cascade1Node, nullptr);

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    EXPECT_EQ(renderer.lastFrameStats().visibleNodes, 2u);

    const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDesc.hasPassDescriptors());
    ASSERT_EQ(shadowDesc.cascadeCount, 2u);

    EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[1].drawCalls, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[0].triangles, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[1].triangles, 1u);

    const uint32_t totalCascadeDrawCalls = shadowDesc.cascadePasses[0].drawCalls
                                         + shadowDesc.cascadePasses[1].drawCalls;
    EXPECT_EQ(totalCascadeDrawCalls, 2u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeFrustumCullingRejectsNodesOutsideAllCascadeBoundsDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 124;
    PipelineHandle geomPipe{};   geomPipe.id = 125;
    PipelineHandle lightPipe{};  lightPipe.id = 126;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 2;
    contract.cascadeSplits[0] = 0.2f;
    contract.cascadeSplits[1] = 0.6f;
    contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(2.0f);
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* inCascade0 = testsupport::addDeterministicShadowCaster(scene, "cascade-in", 75, 76, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(inCascade0, nullptr);
    Node* outsideAll = testsupport::addDeterministicShadowCaster(scene, "cascade-out", 77, 78, {8.0f, 0.0f, -5.0f});
    ASSERT_NE(outsideAll, nullptr);

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    EXPECT_GE(renderer.lastFrameStats().visibleNodes, 1u);

    const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDesc.hasPassDescriptors());
    ASSERT_EQ(shadowDesc.cascadeCount, 2u);

    EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[1].drawCalls, 0u);
    EXPECT_EQ(shadowDesc.cascadePasses[0].triangles, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[1].triangles, 0u);

    const uint32_t totalCascadeDrawCalls = shadowDesc.cascadePasses[0].drawCalls
                                         + shadowDesc.cascadePasses[1].drawCalls;
    EXPECT_EQ(totalCascadeDrawCalls, 1u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeFrustumCullingIncludesBoundaryWithinMarginDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 127;
    PipelineHandle geomPipe{};   geomPipe.id = 128;
    PipelineHandle lightPipe{};  lightPipe.id = 129;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 1;
    contract.cascadeSplits[0] = 0.4f;
    contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(0.0f);
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* onMargin = testsupport::addDeterministicShadowCaster(scene, "margin-in", 79, 80, {1.1f, 0.0f, -5.0f});
    ASSERT_NE(onMargin, nullptr);
    Node* outsideMargin = testsupport::addDeterministicShadowCaster(scene, "margin-out", 81, 82, {1.11f, 0.0f, -5.0f});
    ASSERT_NE(outsideMargin, nullptr);

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    EXPECT_GE(renderer.lastFrameStats().visibleNodes, 1u);

    const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDesc.hasPassDescriptors());
    ASSERT_EQ(shadowDesc.cascadeCount, 1u);

    EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[0].triangles, 1u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeFrustumCullingAllowsIntentionalMultiCascadeOverlap)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 130;
    PipelineHandle geomPipe{};   geomPipe.id = 131;
    PipelineHandle lightPipe{};  lightPipe.id = 132;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 2;
    contract.cascadeSplits[0] = 0.2f;
    contract.cascadeSplits[1] = 0.6f;
    contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(-1.0f);
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* overlapNode = testsupport::addDeterministicShadowCaster(scene, "cascade-overlap", 83, 84, {-1.45f, 0.0f, -5.0f});
    ASSERT_NE(overlapNode, nullptr);

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    EXPECT_GE(renderer.lastFrameStats().visibleNodes, 1u);

    const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDesc.hasPassDescriptors());
    ASSERT_EQ(shadowDesc.cascadeCount, 2u);

    EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[1].drawCalls, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[0].triangles, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[1].triangles, 1u);

    const uint32_t totalCascadeDrawCalls = shadowDesc.cascadePasses[0].drawCalls
                                         + shadowDesc.cascadePasses[1].drawCalls;
    EXPECT_EQ(totalCascadeDrawCalls, 2u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeFrustumCullingExcludesNonShadowCastersDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 133;
    PipelineHandle geomPipe{};   geomPipe.id = 134;
    PipelineHandle lightPipe{};  lightPipe.id = 135;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 1;
    contract.cascadeSplits[0] = 0.5f;
    contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(0.0f);
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* shadowCaster = testsupport::addDeterministicShadowCaster(
        scene, "shadow-caster", 85, 86, {0.0f, 0.0f, -5.0f}, true);
    ASSERT_NE(shadowCaster, nullptr);
    Node* nonCaster = testsupport::addDeterministicShadowCaster(
        scene, "non-caster", 87, 88, {0.3f, 0.0f, -5.0f}, false);
    ASSERT_NE(nonCaster, nullptr);

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    EXPECT_GE(renderer.lastFrameStats().visibleNodes, 2u);

    const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDesc.hasPassDescriptors());
    ASSERT_EQ(shadowDesc.cascadeCount, 1u);

    EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDesc.cascadePasses[0].triangles, 1u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeFrustumCullingHandlesDynamicContractCascadeCountReductionDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 136;
    PipelineHandle geomPipe{};   geomPipe.id = 137;
    PipelineHandle lightPipe{};  lightPipe.id = 138;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* cascade0Node = testsupport::addDeterministicShadowCaster(
        scene, "dynamic-cascade-0", 89, 90, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(cascade0Node, nullptr);
    Node* cascade1Node = testsupport::addDeterministicShadowCaster(
        scene, "dynamic-cascade-1", 91, 92, {2.0f, 0.0f, -5.0f});
    ASSERT_NE(cascade1Node, nullptr);

    Camera cam;

    ShadowLightingContract contractTwoCascades{};
    contractTwoCascades.cascadeCount = 2;
    contractTwoCascades.cascadeSplits[0] = 0.2f;
    contractTwoCascades.cascadeSplits[1] = 0.6f;
    contractTwoCascades.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    contractTwoCascades.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(2.0f);
    renderer.setShadowLightingContract(contractTwoCascades);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc shadowDescTwoCascades = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDescTwoCascades.hasPassDescriptors());
    ASSERT_EQ(shadowDescTwoCascades.cascadeCount, 2u);
    EXPECT_EQ(shadowDescTwoCascades.atlasColumns, 2u);
    EXPECT_EQ(shadowDescTwoCascades.atlasRows, 1u);
    EXPECT_EQ(shadowDescTwoCascades.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDescTwoCascades.cascadePasses[1].drawCalls, 1u);

    ShadowLightingContract contractOneCascade{};
    contractOneCascade.cascadeCount = 1;
    contractOneCascade.cascadeSplits[0] = 0.6f;
    contractOneCascade.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    renderer.setShadowLightingContract(contractOneCascade);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc shadowDescOneCascade = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDescOneCascade.hasPassDescriptors());
    ASSERT_EQ(shadowDescOneCascade.cascadeCount, 1u);
    EXPECT_EQ(shadowDescOneCascade.atlasColumns, 1u);
    EXPECT_EQ(shadowDescOneCascade.atlasRows, 1u);
    EXPECT_EQ(shadowDescOneCascade.cascadeExtent.width, 1024u);
    EXPECT_EQ(shadowDescOneCascade.atlasExtent.width, 1024u);

    EXPECT_EQ(shadowDescOneCascade.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDescOneCascade.cascadePasses[0].triangles, 1u);
    EXPECT_EQ(shadowDescOneCascade.cascadePasses[1].drawCalls, 0u);
    EXPECT_EQ(shadowDescOneCascade.cascadePasses[1].triangles, 0u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeFrustumCullingHandlesDynamicContractCascadeCountGrowthDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 139;
    PipelineHandle geomPipe{};   geomPipe.id = 140;
    PipelineHandle lightPipe{};  lightPipe.id = 141;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* cascade0Node = testsupport::addDeterministicShadowCaster(
        scene, "growth-cascade-0", 93, 94, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(cascade0Node, nullptr);
    Node* cascade1Node = testsupport::addDeterministicShadowCaster(
        scene, "growth-cascade-1", 95, 96, {2.0f, 0.0f, -5.0f});
    ASSERT_NE(cascade1Node, nullptr);

    Camera cam;

    ShadowLightingContract contractOneCascade{};
    contractOneCascade.cascadeCount = 1;
    contractOneCascade.cascadeSplits[0] = 0.6f;
    contractOneCascade.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    renderer.setShadowLightingContract(contractOneCascade);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc shadowDescOneCascade = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDescOneCascade.hasPassDescriptors());
    ASSERT_EQ(shadowDescOneCascade.cascadeCount, 1u);
    EXPECT_EQ(shadowDescOneCascade.atlasColumns, 1u);
    EXPECT_EQ(shadowDescOneCascade.atlasRows, 1u);
    EXPECT_EQ(shadowDescOneCascade.atlasExtent.width, 1024u);
    EXPECT_EQ(shadowDescOneCascade.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDescOneCascade.cascadePasses[0].triangles, 1u);
    EXPECT_EQ(shadowDescOneCascade.cascadePasses[1].drawCalls, 0u);
    EXPECT_EQ(shadowDescOneCascade.cascadePasses[1].triangles, 0u);

    ShadowLightingContract contractTwoCascades{};
    contractTwoCascades.cascadeCount = 2;
    contractTwoCascades.cascadeSplits[0] = 0.2f;
    contractTwoCascades.cascadeSplits[1] = 0.6f;
    contractTwoCascades.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    contractTwoCascades.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(2.0f);
    renderer.setShadowLightingContract(contractTwoCascades);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc shadowDescTwoCascades = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(shadowDescTwoCascades.hasPassDescriptors());
    ASSERT_EQ(shadowDescTwoCascades.cascadeCount, 2u);
    EXPECT_EQ(shadowDescTwoCascades.atlasColumns, 2u);
    EXPECT_EQ(shadowDescTwoCascades.atlasRows, 1u);
    EXPECT_EQ(shadowDescTwoCascades.atlasExtent.width, 2048u);
    EXPECT_EQ(shadowDescTwoCascades.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(shadowDescTwoCascades.cascadePasses[1].drawCalls, 1u);
    EXPECT_EQ(shadowDescTwoCascades.cascadePasses[0].triangles, 1u);
    EXPECT_EQ(shadowDescTwoCascades.cascadePasses[1].triangles, 1u);
    EXPECT_EQ(shadowDescTwoCascades.cascadePasses[1].atlasRect.offset.x, 1024);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeLightingContractMutationsPropagatePerFrameDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 142;
    PipelineHandle geomPipe{};   geomPipe.id = 143;
    PipelineHandle lightPipe{};  lightPipe.id = 144;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* leftNode = testsupport::addDeterministicShadowCaster(
        scene, "mutation-left", 97, 98, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(leftNode, nullptr);
    Node* rightNode = testsupport::addDeterministicShadowCaster(
        scene, "mutation-right", 99, 100, {2.0f, 0.0f, -5.0f});
    ASSERT_NE(rightNode, nullptr);

    Camera cam;

    // Frame 1: cascade 0 targets left node, cascade 1 targets empty space.
    ShadowLightingContract frame1Contract{};
    frame1Contract.cascadeCount = 2;
    frame1Contract.cascadeSplits[0] = 0.2f;
    frame1Contract.cascadeSplits[1] = 0.6f;
    frame1Contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    frame1Contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(8.0f);
    renderer.setShadowLightingContract(frame1Contract);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame1ShadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(frame1ShadowDesc.hasPassDescriptors());
    ASSERT_EQ(frame1ShadowDesc.cascadeCount, 2u);
    EXPECT_EQ(frame1ShadowDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(frame1ShadowDesc.cascadePasses[1].drawCalls, 0u);

    const ShadowLightingBindingDesc frame1LightingDesc = renderer.buildShadowLightingBindingDesc();
    ASSERT_TRUE(frame1LightingDesc.isComplete());
    EXPECT_EQ(frame1LightingDesc.shadowCascadeCount, 2u);
    EXPECT_FLOAT_EQ(frame1LightingDesc.cascadeSplits[0], 0.2f);
    EXPECT_FLOAT_EQ(frame1LightingDesc.cascadeSplits[1], 0.6f);

    // Frame 2: rapidly mutate contract so cascade 0 targets empty space and cascade 1 targets right node.
    ShadowLightingContract frame2Contract{};
    frame2Contract.cascadeCount = 2;
    frame2Contract.cascadeSplits[0] = 0.1f;
    frame2Contract.cascadeSplits[1] = 0.8f;
    frame2Contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(8.0f);
    frame2Contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(2.0f);
    renderer.setShadowLightingContract(frame2Contract);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame2ShadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(frame2ShadowDesc.hasPassDescriptors());
    ASSERT_EQ(frame2ShadowDesc.cascadeCount, 2u);
    EXPECT_EQ(frame2ShadowDesc.cascadePasses[0].drawCalls, 0u);
    EXPECT_EQ(frame2ShadowDesc.cascadePasses[1].drawCalls, 1u);

    const ShadowLightingBindingDesc frame2LightingDesc = renderer.buildShadowLightingBindingDesc();
    ASSERT_TRUE(frame2LightingDesc.isComplete());
    EXPECT_EQ(frame2LightingDesc.shadowCascadeCount, 2u);
    EXPECT_FLOAT_EQ(frame2LightingDesc.cascadeSplits[0], 0.1f);
    EXPECT_FLOAT_EQ(frame2LightingDesc.cascadeSplits[1], 0.8f);

    // Frame 3: shrink to one cascade and point at right node.
    ShadowLightingContract frame3Contract{};
    frame3Contract.cascadeCount = 1;
    frame3Contract.cascadeSplits[0] = 0.5f;
    frame3Contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(2.0f);
    renderer.setShadowLightingContract(frame3Contract);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame3ShadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(frame3ShadowDesc.hasPassDescriptors());
    ASSERT_EQ(frame3ShadowDesc.cascadeCount, 1u);
    EXPECT_EQ(frame3ShadowDesc.atlasColumns, 1u);
    EXPECT_EQ(frame3ShadowDesc.atlasRows, 1u);
    EXPECT_EQ(frame3ShadowDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(frame3ShadowDesc.cascadePasses[1].drawCalls, 0u);

    const ShadowLightingBindingDesc frame3LightingDesc = renderer.buildShadowLightingBindingDesc();
    ASSERT_TRUE(frame3LightingDesc.isComplete());
    EXPECT_EQ(frame3LightingDesc.shadowCascadeCount, 1u);
    EXPECT_FLOAT_EQ(frame3LightingDesc.cascadeSplits[0], 0.5f);

    // Frame 4: grow back to two cascades and verify both activate fresh.
    ShadowLightingContract frame4Contract{};
    frame4Contract.cascadeCount = 2;
    frame4Contract.cascadeSplits[0] = 0.25f;
    frame4Contract.cascadeSplits[1] = 0.75f;
    frame4Contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    frame4Contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(2.0f);
    renderer.setShadowLightingContract(frame4Contract);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame4ShadowDesc = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(frame4ShadowDesc.hasPassDescriptors());
    ASSERT_EQ(frame4ShadowDesc.cascadeCount, 2u);
    EXPECT_EQ(frame4ShadowDesc.atlasColumns, 2u);
    EXPECT_EQ(frame4ShadowDesc.atlasRows, 1u);
    EXPECT_EQ(frame4ShadowDesc.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(frame4ShadowDesc.cascadePasses[1].drawCalls, 1u);
    EXPECT_EQ(frame4ShadowDesc.cascadePasses[1].atlasRect.offset.x, 1024);

    const ShadowLightingBindingDesc frame4LightingDesc = renderer.buildShadowLightingBindingDesc();
    ASSERT_TRUE(frame4LightingDesc.isComplete());
    EXPECT_EQ(frame4LightingDesc.shadowCascadeCount, 2u);
    EXPECT_FLOAT_EQ(frame4LightingDesc.cascadeSplits[0], 0.25f);
    EXPECT_FLOAT_EQ(frame4LightingDesc.cascadeSplits[1], 0.75f);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CascadeLightingContractMutationSequenceAlternatesOverlapAndExclusionDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 145;
    PipelineHandle geomPipe{};   geomPipe.id = 146;
    PipelineHandle lightPipe{};  lightPipe.id = 147;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* overlapNode = testsupport::addDeterministicShadowCaster(
        scene, "mutation-sequence-overlap", 101, 102, {-1.45f, 0.0f, -5.0f});
    ASSERT_NE(overlapNode, nullptr);

    Camera cam;

    struct FrameCase {
        float center0;
        float center1;
        float split0;
        float split1;
        uint32_t expectedDraws0;
        uint32_t expectedDraws1;
    };

    const std::array<FrameCase, 6> frames = {{
        {-2.0f, -1.0f, 0.15f, 0.35f, 1u, 1u}, // overlap
        {-2.0f,  8.0f, 0.20f, 0.40f, 1u, 0u}, // exclusion
        {-2.0f, -1.0f, 0.25f, 0.45f, 1u, 1u}, // overlap
        { 8.0f, -1.0f, 0.30f, 0.50f, 0u, 1u}, // exclusion
        {-2.0f, -1.0f, 0.35f, 0.55f, 1u, 1u}, // overlap
        { 8.0f,  9.0f, 0.40f, 0.60f, 0u, 0u}, // exclusion
    }};

    for (const auto& frameCase : frames) {
        ShadowLightingContract contract{};
        contract.cascadeCount = 2;
        contract.cascadeSplits[0] = frameCase.split0;
        contract.cascadeSplits[1] = frameCase.split1;
        contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0);
        contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center1);
        renderer.setShadowLightingContract(contract);

        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();

        const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
        ASSERT_TRUE(shadowDesc.hasPassDescriptors());
        ASSERT_EQ(shadowDesc.cascadeCount, 2u);
        EXPECT_EQ(shadowDesc.atlasColumns, 2u);
        EXPECT_EQ(shadowDesc.atlasRows, 1u);
        EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, frameCase.expectedDraws0);
        EXPECT_EQ(shadowDesc.cascadePasses[1].drawCalls, frameCase.expectedDraws1);
        EXPECT_EQ(shadowDesc.cascadePasses[0].triangles, frameCase.expectedDraws0);
        EXPECT_EQ(shadowDesc.cascadePasses[1].triangles, frameCase.expectedDraws1);
        EXPECT_EQ(shadowDesc.cascadePasses[1].atlasRect.offset.x, 1024);

        const ShadowLightingBindingDesc lightingDesc = renderer.buildShadowLightingBindingDesc();
        ASSERT_TRUE(lightingDesc.isComplete());
        EXPECT_EQ(lightingDesc.shadowCascadeCount, 2u);
        EXPECT_FLOAT_EQ(lightingDesc.cascadeSplits[0], frameCase.split0);
        EXPECT_FLOAT_EQ(lightingDesc.cascadeSplits[1], frameCase.split1);
    }

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowLightingBindingDescPropagatesBiasMutationsPerFrameDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 151;
    PipelineHandle geomPipe{};   geomPipe.id = 152;
    PipelineHandle lightPipe{};  lightPipe.id = 153;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* overlapNode = testsupport::addDeterministicShadowCaster(
        scene, "bias-mutation-overlap", 105, 106, {-1.45f, 0.0f, -5.0f});
    ASSERT_NE(overlapNode, nullptr);

    Camera cam;

    struct FrameCase {
        float center0;
        float center1;
        float split0;
        float split1;
        float shadowBias;
        float normalBias;
        uint32_t expectedDraws0;
        uint32_t expectedDraws1;
    };

    const std::array<FrameCase, 5> frames = {{
        {-2.0f, -1.0f, 0.20f, 0.60f, 0.0001f, 0.0010f, 1u, 1u}, // overlap
        {-2.0f,  8.0f, 0.25f, 0.65f, 0.0002f, 0.0015f, 1u, 0u}, // exclusion
        {-2.0f, -1.0f, 0.30f, 0.70f, 0.0003f, 0.0020f, 1u, 1u}, // overlap
        { 8.0f, -1.0f, 0.35f, 0.75f, 0.0004f, 0.0025f, 0u, 1u}, // exclusion
        {-2.0f, -1.0f, 0.40f, 0.80f, 0.0005f, 0.0030f, 1u, 1u}, // overlap
    }};

    for (const auto& frameCase : frames) {
        ShadowLightingContract contract{};
        contract.cascadeCount = 2;
        contract.cascadeSplits[0] = frameCase.split0;
        contract.cascadeSplits[1] = frameCase.split1;
        contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0);
        contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center1);
        contract.shadowBias = frameCase.shadowBias;
        contract.normalBias = frameCase.normalBias;
        renderer.setShadowLightingContract(contract);

        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();

        const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
        ASSERT_TRUE(shadowDesc.hasPassDescriptors());
        ASSERT_EQ(shadowDesc.cascadeCount, 2u);
        EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, frameCase.expectedDraws0);
        EXPECT_EQ(shadowDesc.cascadePasses[1].drawCalls, frameCase.expectedDraws1);

        const ShadowLightingBindingDesc lightingDesc = renderer.buildShadowLightingBindingDesc();
        ASSERT_TRUE(lightingDesc.isComplete());
        EXPECT_EQ(lightingDesc.shadowCascadeCount, 2u);
        EXPECT_FLOAT_EQ(lightingDesc.cascadeSplits[0], frameCase.split0);
        EXPECT_FLOAT_EQ(lightingDesc.cascadeSplits[1], frameCase.split1);
        expectMat4Eq(lightingDesc.lightViewProj[0], testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0));
        expectMat4Eq(lightingDesc.lightViewProj[1], testsupport::makeDeterministicCascadeLightViewProj(frameCase.center1));
        EXPECT_FLOAT_EQ(lightingDesc.shadowBias, frameCase.shadowBias);
        EXPECT_FLOAT_EQ(lightingDesc.normalBias, frameCase.normalBias);
    }

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowLightingBindingDescClearsAndReappliesContractDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 154;
    PipelineHandle geomPipe{};   geomPipe.id = 155;
    PipelineHandle lightPipe{};  lightPipe.id = 156;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* caster = testsupport::addDeterministicShadowCaster(
        scene, "clear-reapply-caster", 107, 108, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(caster, nullptr);

    Camera cam;

    // Frame 1: complete contract should produce one shadow draw in cascade 0.
    ShadowLightingContract frame1{};
    frame1.cascadeCount = 2;
    frame1.cascadeSplits[0] = 0.2f;
    frame1.cascadeSplits[1] = 0.6f;
    frame1.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    frame1.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(8.0f);
    frame1.shadowBias = 0.0002f;
    frame1.normalBias = 0.0020f;
    renderer.setShadowLightingContract(frame1);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame1Shadow = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(frame1Shadow.hasPassDescriptors());
    ASSERT_EQ(frame1Shadow.cascadeCount, 2u);
    EXPECT_EQ(frame1Shadow.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(frame1Shadow.cascadePasses[1].drawCalls, 0u);

    const ShadowLightingBindingDesc frame1Lighting = renderer.buildShadowLightingBindingDesc();
    ASSERT_TRUE(frame1Lighting.isComplete());
    EXPECT_EQ(frame1Lighting.shadowCascadeCount, 2u);
    EXPECT_FLOAT_EQ(frame1Lighting.cascadeSplits[0], 0.2f);
    EXPECT_FLOAT_EQ(frame1Lighting.cascadeSplits[1], 0.6f);
    expectMat4Eq(frame1Lighting.lightViewProj[0], testsupport::makeDeterministicCascadeLightViewProj(-2.0f));
    expectMat4Eq(frame1Lighting.lightViewProj[1], testsupport::makeDeterministicCascadeLightViewProj(8.0f));
    EXPECT_FLOAT_EQ(frame1Lighting.shadowBias, 0.0002f);
    EXPECT_FLOAT_EQ(frame1Lighting.normalBias, 0.0020f);
    EXPECT_EQ(renderer.lastFrameStats().drawCalls, 3u);

    // Frame 2: clear contract and verify shadow pass is skipped with no stale descriptor state.
    renderer.clearShadowLightingContract();

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame2Shadow = renderer.buildShadowPassBindingDesc();
    EXPECT_EQ(frame2Shadow.cascadeCount, 0u);
    EXPECT_FALSE(frame2Shadow.hasPassDescriptors());

    const ShadowLightingBindingDesc frame2Lighting = renderer.buildShadowLightingBindingDesc();
    EXPECT_FALSE(frame2Lighting.isComplete());
    EXPECT_EQ(frame2Lighting.shadowCascadeCount, 0u);
    EXPECT_FLOAT_EQ(frame2Lighting.shadowBias, 0.0f);
    EXPECT_FLOAT_EQ(frame2Lighting.normalBias, 0.0f);
    EXPECT_EQ(renderer.lastFrameStats().drawCalls, 2u);

    // Frame 3: reapply different contract and verify fresh values propagate.
    ShadowLightingContract frame3{};
    frame3.cascadeCount = 1;
    frame3.cascadeSplits[0] = 0.9f;
    frame3.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    frame3.shadowBias = 0.0006f;
    frame3.normalBias = 0.0030f;
    renderer.setShadowLightingContract(frame3);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame3Shadow = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(frame3Shadow.hasPassDescriptors());
    ASSERT_EQ(frame3Shadow.cascadeCount, 1u);
    EXPECT_EQ(frame3Shadow.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(frame3Shadow.cascadePasses[1].drawCalls, 0u);

    const ShadowLightingBindingDesc frame3Lighting = renderer.buildShadowLightingBindingDesc();
    ASSERT_TRUE(frame3Lighting.isComplete());
    EXPECT_EQ(frame3Lighting.shadowCascadeCount, 1u);
    EXPECT_FLOAT_EQ(frame3Lighting.cascadeSplits[0], 0.9f);
    expectMat4Eq(frame3Lighting.lightViewProj[0], testsupport::makeDeterministicCascadeLightViewProj(-2.0f));
    EXPECT_FLOAT_EQ(frame3Lighting.shadowBias, 0.0006f);
    EXPECT_FLOAT_EQ(frame3Lighting.normalBias, 0.0030f);
    EXPECT_EQ(renderer.lastFrameStats().drawCalls, 3u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowLightingBindingDescPropagatesLightViewProjMutationsPerFrameDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 157;
    PipelineHandle geomPipe{};   geomPipe.id = 158;
    PipelineHandle lightPipe{};  lightPipe.id = 159;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* overlapNode = testsupport::addDeterministicShadowCaster(
        scene, "lvp-mutation-overlap", 109, 110, {-1.45f, 0.0f, -5.0f});
    ASSERT_NE(overlapNode, nullptr);

    Camera cam;

    struct FrameCase {
        float center0;
        float center1;
        uint32_t expectedDraws0;
        uint32_t expectedDraws1;
    };

    const std::array<FrameCase, 4> frames = {{
        {-2.0f, -1.0f, 1u, 1u}, // overlap
        {-2.0f,  8.0f, 1u, 0u}, // exclusion
        { 8.0f, -1.0f, 0u, 1u}, // exclusion
        {-2.0f, -1.0f, 1u, 1u}, // overlap
    }};

    for (const auto& frameCase : frames) {
        ShadowLightingContract contract{};
        contract.cascadeCount = 2;
        contract.cascadeSplits[0] = 0.25f;
        contract.cascadeSplits[1] = 0.75f;
        contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0);
        contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(frameCase.center1);
        renderer.setShadowLightingContract(contract);

        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();

        const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
        ASSERT_TRUE(shadowDesc.hasPassDescriptors());
        ASSERT_EQ(shadowDesc.cascadeCount, 2u);
        EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, frameCase.expectedDraws0);
        EXPECT_EQ(shadowDesc.cascadePasses[1].drawCalls, frameCase.expectedDraws1);

        const ShadowLightingBindingDesc lightingDesc = renderer.buildShadowLightingBindingDesc();
        ASSERT_TRUE(lightingDesc.isComplete());
        EXPECT_EQ(lightingDesc.shadowCascadeCount, 2u);
        expectMat4Eq(lightingDesc.lightViewProj[0], testsupport::makeDeterministicCascadeLightViewProj(frameCase.center0));
        expectMat4Eq(lightingDesc.lightViewProj[1], testsupport::makeDeterministicCascadeLightViewProj(frameCase.center1));
    }

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowLightingContractLifecycleAlternatesSetClearAcrossFramesDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 160;
    PipelineHandle geomPipe{};   geomPipe.id = 161;
    PipelineHandle lightPipe{};  lightPipe.id = 162;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* caster = testsupport::addDeterministicShadowCaster(
        scene, "lifecycle-set-clear-caster", 111, 112, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(caster, nullptr);

    Camera cam;

    const uint32_t frameCount = 12;
    int32_t previousDrawCalls = -1;
    for (uint32_t i = 0; i < frameCount; ++i) {
        const bool setContract = (i % 2u) == 0u;
        if (setContract) {
            ShadowLightingContract contract{};
            contract.cascadeCount = 1;
            contract.cascadeSplits[0] = 0.2f + 0.03f * static_cast<float>(i);
            contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
            contract.shadowBias = 0.0001f + 0.00005f * static_cast<float>(i);
            contract.normalBias = 0.0010f + 0.0002f * static_cast<float>(i);
            renderer.setShadowLightingContract(contract);
        } else {
            renderer.clearShadowLightingContract();
        }

        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();

        const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
        const ShadowLightingBindingDesc lightingDesc = renderer.buildShadowLightingBindingDesc();
        const int32_t currentDrawCalls = static_cast<int32_t>(renderer.lastFrameStats().drawCalls);

        if (setContract) {
            ASSERT_TRUE(shadowDesc.hasPassDescriptors());
            ASSERT_EQ(shadowDesc.cascadeCount, 1u);
            EXPECT_EQ(shadowDesc.cascadePasses[0].drawCalls, 1u);
            ASSERT_TRUE(lightingDesc.isComplete());
            EXPECT_EQ(lightingDesc.shadowCascadeCount, 1u);
            expectMat4Eq(lightingDesc.lightViewProj[0], testsupport::makeDeterministicCascadeLightViewProj(-2.0f));
            EXPECT_EQ(currentDrawCalls, 3);
        } else {
            EXPECT_EQ(shadowDesc.cascadeCount, 0u);
            EXPECT_FALSE(shadowDesc.hasPassDescriptors());
            EXPECT_FALSE(lightingDesc.isComplete());
            EXPECT_EQ(lightingDesc.shadowCascadeCount, 0u);
            EXPECT_EQ(currentDrawCalls, 2);
        }

        if (previousDrawCalls >= 0) {
            const int32_t delta = currentDrawCalls - previousDrawCalls;
            EXPECT_EQ(delta, setContract ? 1 : -1);
        }
        previousDrawCalls = currentDrawCalls;
    }

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowLightingContractCascadeCountIsClampedAndMatricesPropagateDeterministically)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 163;
    PipelineHandle geomPipe{};   geomPipe.id = 164;
    PipelineHandle lightPipe{};  lightPipe.id = 165;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    scheduler.extent = {1024, 1024};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* caster = testsupport::addDeterministicShadowCaster(
        scene, "clamp-cascade-caster", 113, 114, {-2.0f, 0.0f, -5.0f});
    ASSERT_NE(caster, nullptr);

    Camera cam;

    // Frame 1: request more cascades than supported and verify clamping + matrix propagation.
    ShadowLightingContract frame1{};
    frame1.cascadeCount = ShadowLightingContract::kMaxCascades + 2u;
    frame1.cascadeSplits[0] = 0.1f;
    frame1.cascadeSplits[1] = 0.3f;
    frame1.cascadeSplits[2] = 0.6f;
    frame1.cascadeSplits[3] = 0.9f;
    frame1.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    frame1.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(8.0f);
    frame1.lightViewProj[2] = testsupport::makeDeterministicCascadeLightViewProj(9.0f);
    frame1.lightViewProj[3] = testsupport::makeDeterministicCascadeLightViewProj(10.0f);
    renderer.setShadowLightingContract(frame1);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame1Shadow = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(frame1Shadow.hasPassDescriptors());
    ASSERT_EQ(frame1Shadow.cascadeCount, ShadowLightingContract::kMaxCascades);
    EXPECT_EQ(frame1Shadow.atlasColumns, 2u);
    EXPECT_EQ(frame1Shadow.atlasRows, 2u);
    EXPECT_EQ(frame1Shadow.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(frame1Shadow.cascadePasses[1].drawCalls, 0u);
    EXPECT_EQ(frame1Shadow.cascadePasses[2].drawCalls, 0u);
    EXPECT_EQ(frame1Shadow.cascadePasses[3].drawCalls, 0u);

    const ShadowLightingBindingDesc frame1Lighting = renderer.buildShadowLightingBindingDesc();
    ASSERT_TRUE(frame1Lighting.isComplete());
    EXPECT_EQ(frame1Lighting.shadowCascadeCount, ShadowLightingContract::kMaxCascades);
    EXPECT_FLOAT_EQ(frame1Lighting.cascadeSplits[0], 0.1f);
    EXPECT_FLOAT_EQ(frame1Lighting.cascadeSplits[1], 0.3f);
    EXPECT_FLOAT_EQ(frame1Lighting.cascadeSplits[2], 0.6f);
    EXPECT_FLOAT_EQ(frame1Lighting.cascadeSplits[3], 0.9f);
    expectMat4Eq(frame1Lighting.lightViewProj[0], testsupport::makeDeterministicCascadeLightViewProj(-2.0f));
    expectMat4Eq(frame1Lighting.lightViewProj[1], testsupport::makeDeterministicCascadeLightViewProj(8.0f));
    expectMat4Eq(frame1Lighting.lightViewProj[2], testsupport::makeDeterministicCascadeLightViewProj(9.0f));
    expectMat4Eq(frame1Lighting.lightViewProj[3], testsupport::makeDeterministicCascadeLightViewProj(10.0f));

    // Frame 2: shrink back to one cascade and verify stale multi-cascade state is not exposed.
    ShadowLightingContract frame2{};
    frame2.cascadeCount = 1;
    frame2.cascadeSplits[0] = 0.55f;
    frame2.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    renderer.setShadowLightingContract(frame2);

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc frame2Shadow = renderer.buildShadowPassBindingDesc();
    ASSERT_TRUE(frame2Shadow.hasPassDescriptors());
    ASSERT_EQ(frame2Shadow.cascadeCount, 1u);
    EXPECT_EQ(frame2Shadow.atlasColumns, 1u);
    EXPECT_EQ(frame2Shadow.atlasRows, 1u);
    EXPECT_EQ(frame2Shadow.cascadePasses[0].drawCalls, 1u);
    EXPECT_EQ(frame2Shadow.cascadePasses[1].drawCalls, 0u);

    const ShadowLightingBindingDesc frame2Lighting = renderer.buildShadowLightingBindingDesc();
    ASSERT_TRUE(frame2Lighting.isComplete());
    EXPECT_EQ(frame2Lighting.shadowCascadeCount, 1u);
    EXPECT_FLOAT_EQ(frame2Lighting.cascadeSplits[0], 0.55f);
    expectMat4Eq(frame2Lighting.lightViewProj[0], testsupport::makeDeterministicCascadeLightViewProj(-2.0f));

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ResizeRecreatesGBufferAndPreservesTransitionContract)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {800, 600};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle lightPipe{};
    lightPipe.id = 303;
    renderer.setLightingCompositePipeline(lightPipe);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Camera cam;
    cam.setPerspective(70.f, 4.f / 3.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    ASSERT_GE(cmd.textureBarrierBatches.size(), 2u);
    const uint64_t gbufferAlbedoBeforeResize = cmd.textureBarrierBatches[0].barriers[0].texture.id;

    renderer.onResize({1600, 900});
    scheduler.extent = {1600, 900};
    cmd.clearRecordedState();

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    ASSERT_GE(cmd.textureBarrierBatches.size(), 2u);
    const uint64_t gbufferAlbedoAfterResize = cmd.textureBarrierBatches[0].barriers[0].texture.id;

    EXPECT_NE(gbufferAlbedoBeforeResize, gbufferAlbedoAfterResize);
    EXPECT_EQ(cmd.textureBarrierBatches[0].barriers[0].newLayout, TextureLayout::ColorAttachment);
    EXPECT_EQ(cmd.textureBarrierBatches[1].barriers[0].newLayout, TextureLayout::ShaderRead);
}

TEST(RendererBehavior, SectionPacketsUsePerSectionMaterialPipelinesAndTrackDrawStats)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle fallbackPipe{}; fallbackPipe.id = 600;
    PipelineHandle matPipeA{}; matPipeA.id = 601;
    PipelineHandle matPipeB{}; matPipeB.id = 602;
    PipelineHandle lightPipe{}; lightPipe.id = 603;

    renderer.setFallbackGeometryPipeline(fallbackPipe);
    renderer.setMaterialPipeline(11, matPipeA);
    renderer.setMaterialPipeline(22, matPipeB);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("sectioned");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 44;
    n->mesh.indexBuffer.id = 45;
    n->mesh.indexCount = 6;
    n->localTransform().translation = {0.f, 0.f, -5.f};
    MeshSectionDrawPacket secA{};
    secA.firstIndex = 0u;
    secA.indexCount = 3u;
    secA.material = 11u;
    MeshSectionDrawPacket secB{};
    secB.firstIndex = 3u;
    secB.indexCount = 3u;
    secB.material = 22u;
    n->sectionDrawPackets = {secA, secB};

    Camera cam;

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    int geometryDrawIndexedCount = 0;
    int matPipeABinds = 0;
    int matPipeBBinds = 0;
    for (const auto& e : cmd.events) {
        if (e.kind == "drawIndexed" && e.a == 3u) {
            ++geometryDrawIndexedCount;
        }
        if (e.kind == "bindPipeline" && e.a == static_cast<uint32_t>(matPipeA.id)) {
            ++matPipeABinds;
        }
        if (e.kind == "bindPipeline" && e.a == static_cast<uint32_t>(matPipeB.id)) {
            ++matPipeBBinds;
        }
    }

    EXPECT_EQ(geometryDrawIndexedCount, 2);
    EXPECT_EQ(matPipeABinds, 1);
    EXPECT_EQ(matPipeBBinds, 1);

    const FrameStats stats = renderer.lastFrameStats();
    EXPECT_EQ(stats.visibleNodes, 1u);
    EXPECT_EQ(stats.drawCalls, 3u);  // 2 indexed geometry + 1 fullscreen composite
    EXPECT_EQ(stats.triangles, 2u);  // 2 section triangles in geometry pass

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ResetSceneClearsSceneAndInvalidatesMaterialBindings)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle fallbackPipe{}; fallbackPipe.id = 710;
    PipelineHandle matPipe{}; matPipe.id = 711;
    PipelineHandle lightPipe{}; lightPipe.id = 712;

    renderer.setFallbackGeometryPipeline(fallbackPipe);
    renderer.setMaterialPipeline(11, matPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    BufferDesc materialTableDesc{};
    materialTableDesc.sizeBytes = 64;
    materialTableDesc.usage = BufferUsage::StorageBuffer;
    materialTableDesc.memory = MemoryHint::GpuOnly;
    materialTableDesc.debugName = "test.renderer.reset.materialTable";
    BufferHandle materialTable = dev.createBuffer(materialTableDesc);
    ASSERT_TRUE(materialTable.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.materialTable = materialTable;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract shadowContract{};
    shadowContract.cascadeCount = 1;
    renderer.setShadowLightingContract(shadowContract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* original = scene.createNode("before-reset");
    ASSERT_NE(original, nullptr);
    original->mesh.vertexBuffer.id = 90;
    original->mesh.indexBuffer.id = 91;
    original->mesh.indexCount = 3;
    original->material = 11;

    renderer.resetScene(scene);
    EXPECT_EQ(scene.root().children().size(), 0u);

    Node* after = scene.createNode("after-reset");
    ASSERT_NE(after, nullptr);
    after->mesh.vertexBuffer.id = 92;
    after->mesh.indexBuffer.id = 93;
    after->mesh.indexCount = 3;
    after->material = 11;
    after->localTransform().translation = {0.f, 0.f, -5.f};

    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    int matPipeBinds = 0;
    int pushConstantsCalls = 0;
    for (const auto& e : cmd.events) {
        if (e.kind == "bindPipeline" && e.a == static_cast<uint32_t>(matPipe.id)) {
            ++matPipeBinds;
        }
        if (e.kind == "pushConstants") {
            ++pushConstantsCalls;
        }
    }

    EXPECT_EQ(matPipeBinds, 0);
    EXPECT_EQ(pushConstantsCalls, 0);

    dev.destroyBuffer(materialTable);
    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ResetSceneAndDestroyTLASClearsSceneReleasesTLASAndInvalidatesBindings)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle fallbackPipe{}; fallbackPipe.id = 810;
    PipelineHandle matPipe{}; matPipe.id = 811;
    PipelineHandle lightPipe{}; lightPipe.id = 812;

    renderer.setFallbackGeometryPipeline(fallbackPipe);
    renderer.setMaterialPipeline(11, matPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    BufferDesc materialTableDesc{};
    materialTableDesc.sizeBytes = 64;
    materialTableDesc.usage = BufferUsage::StorageBuffer;
    materialTableDesc.memory = MemoryHint::GpuOnly;
    materialTableDesc.debugName = "test.renderer.reset.tlas.materialTable";
    BufferHandle materialTable = dev.createBuffer(materialTableDesc);
    ASSERT_TRUE(materialTable.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.materialTable = materialTable;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract shadowContract{};
    shadowContract.cascadeCount = 1;
    renderer.setShadowLightingContract(shadowContract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("with-tlas");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 120;
    n->mesh.indexBuffer.id = 121;
    n->mesh.indexCount = 3;
    n->mesh.blas.id = 777;

    EXPECT_FALSE(scene.tlas().valid());
    
    // Note: Null backend's buildTLAS returns invalid handle intentionally,
    // so we just verify it completes successfully and scene can be reset
    scene.rebuildTLAS(dev);

    renderer.resetSceneAndDestroyTLAS(scene);

    EXPECT_EQ(scene.root().children().size(), 0u);

    Node* after = scene.createNode("after-reset");
    ASSERT_NE(after, nullptr);
    after->mesh.vertexBuffer.id = 122;
    after->mesh.indexBuffer.id = 123;
    after->mesh.indexCount = 3;
    after->material = 11;
    after->localTransform().translation = {0.f, 0.f, -5.f};

    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    int matPipeBinds = 0;
    int pushConstantsCalls = 0;
    for (const auto& e : cmd.events) {
        if (e.kind == "bindPipeline" && e.a == static_cast<uint32_t>(matPipe.id)) {
            ++matPipeBinds;
        }
        if (e.kind == "pushConstants") {
            ++pushConstantsCalls;
        }
    }

    EXPECT_EQ(matPipeBinds, 0);
    EXPECT_EQ(pushConstantsCalls, 0);

    dev.destroyBuffer(materialTable);
    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CompositePassBindingDescTracksCoreMaterialAndShadowInputs)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 901;
    PipelineHandle geomPipe{};   geomPipe.id = 902;
    PipelineHandle lightPipe{};  lightPipe.id = 903;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    BufferDesc materialTableDesc{};
    materialTableDesc.sizeBytes = 128;
    materialTableDesc.usage = BufferUsage::StorageBuffer;
    materialTableDesc.memory = MemoryHint::GpuOnly;
    materialTableDesc.debugName = "test.renderer.composite.desc.materialTable";
    BufferHandle materialTable = dev.createBuffer(materialTableDesc);
    ASSERT_TRUE(materialTable.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    bindings.materialTable = materialTable;
    bindings.materialTableOffsetBytes = 48;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract shadowContract{};
    shadowContract.cascadeCount = 2;
    shadowContract.cascadeSplits[0] = 0.2f;
    shadowContract.cascadeSplits[1] = 0.6f;
    shadowContract.lightViewProj[0] = Mat4::identity();
    shadowContract.lightViewProj[1] = Mat4::identity();
    renderer.setShadowLightingContract(shadowContract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("descriptor-ready");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 201;
    n->mesh.indexBuffer.id  = 202;
    n->mesh.indexCount      = 6;
    n->castShadow = true;

    Camera cam;
    cam.setPerspective(70.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const CompositePassBindingDesc compositeDesc = renderer.buildCompositePassBindingDesc();
    EXPECT_TRUE(compositeDesc.hasCoreInputs());
    EXPECT_TRUE(compositeDesc.hasMaterialTableInput());
    EXPECT_TRUE(compositeDesc.readyForComposite());
    EXPECT_TRUE(compositeDesc.hasShadowInputs());
    EXPECT_TRUE(compositeDesc.albedoTexture.valid());
    EXPECT_TRUE(compositeDesc.normalTexture.valid());
    EXPECT_TRUE(compositeDesc.velocityTexture.valid());
    EXPECT_TRUE(compositeDesc.depthTexture.valid());
    EXPECT_EQ(compositeDesc.materialTable.id, materialTable.id);
    EXPECT_EQ(compositeDesc.materialTableOffsetBytes, 48u);
    EXPECT_EQ(compositeDesc.shadowCascadeCount, 2u);

    dev.destroyBuffer(materialTable);
    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CompositePassBindingDescRequiresMaterialTableWhenOffsetIsSet)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle lightPipe{};
    lightPipe.id = 904;
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.materialTableOffsetBytes = 16;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const CompositePassBindingDesc compositeDesc = renderer.buildCompositePassBindingDesc();
    EXPECT_TRUE(compositeDesc.hasCoreInputs());
    EXPECT_FALSE(compositeDesc.hasMaterialTableInput());
    EXPECT_FALSE(compositeDesc.readyForComposite());

    int pushConstantsCalls = 0;
    for (const auto& e : cmd.events) {
        if (e.kind == "pushConstants") {
            ++pushConstantsCalls;
        }
    }
    EXPECT_EQ(pushConstantsCalls, 0);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CompositePassBindingDescOmitsPartialShadowInputs)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 905;
    PipelineHandle geomPipe{};   geomPipe.id = 906;
    PipelineHandle lightPipe{};  lightPipe.id = 907;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.shadowDepthSampler = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("partial-shadow");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 211;
    n->mesh.indexBuffer.id  = 212;
    n->mesh.indexCount      = 3;
    n->castShadow = true;

    Camera cam;
    cam.setPerspective(70.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const CompositePassBindingDesc compositeDesc = renderer.buildCompositePassBindingDesc();
    EXPECT_TRUE(compositeDesc.hasCoreInputs());
    EXPECT_TRUE(compositeDesc.readyForComposite());
    EXPECT_FALSE(compositeDesc.hasShadowInputs());
    EXPECT_FALSE(compositeDesc.shadowDepthTexture.valid());
    EXPECT_FALSE(compositeDesc.shadowLightingBuffer.valid());
    EXPECT_EQ(compositeDesc.shadowCascadeCount, 0u);

    dev.destroySampler(sampler);
}

// ─────────────────────────────────────────────────────────────────────────────
//  CompositeInputMissing diagnostic API
// ─────────────────────────────────────────────────────────────────────────────

TEST(RendererBehavior, CompositeInputDiagnosticReportsNoneWhenReadyForComposite)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 910;
    PipelineHandle geomPipe{};   geomPipe.id   = 911;
    PipelineHandle lightPipe{};  lightPipe.id  = 912;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    BufferDesc bd{};
    bd.sizeBytes = 64;
    bd.usage = BufferUsage::StorageBuffer;
    bd.memory = MemoryHint::GpuOnly;
    BufferHandle materialTable = dev.createBuffer(bd);
    ASSERT_TRUE(materialTable.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler    = sampler;
    bindings.normalSampler            = sampler;
    bindings.velocitySampler          = sampler;
    bindings.depthSampler             = sampler;
    bindings.shadowDepthSampler       = sampler;
    bindings.materialTable            = materialTable;
    bindings.materialTableOffsetBytes = 0;   // offset=0 → table not required
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount     = 1;
    contract.cascadeSplits[0] = 0.5f;
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Camera cam;
    cam.setPerspective(70.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const CompositePassBindingDesc compositeDesc = renderer.buildCompositePassBindingDesc();
    ASSERT_TRUE(compositeDesc.readyForComposite());

    const CompositeInputMissing diag = compositeDesc.diagnose();
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::GBufferAlbedo));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::GBufferNormal));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::GBufferVelocity));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::GBufferDepth));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::SamplerAlbedo));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::SamplerNormal));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::SamplerVelocity));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::SamplerDepth));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::MaterialTable));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::ShadowDepthTexture));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::ShadowDepthSampler));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::ShadowLightingBuffer));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::ShadowCascadeCount));

    dev.destroyBuffer(materialTable);
    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CompositeInputDiagnosticFlagsMissingMaterialTableWhenOffsetSet)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle lightPipe{};
    lightPipe.id = 913;
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    // Non-zero offset but no materialTable → should be flagged.
    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler    = sampler;
    bindings.normalSampler            = sampler;
    bindings.velocitySampler          = sampler;
    bindings.depthSampler             = sampler;
    bindings.materialTableOffsetBytes = 32;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const CompositePassBindingDesc compositeDesc = renderer.buildCompositePassBindingDesc();
    ASSERT_FALSE(compositeDesc.readyForComposite());

    const CompositeInputMissing diag = compositeDesc.diagnose();
    EXPECT_TRUE(hasFlag(diag, CompositeInputMissing::MaterialTable));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::GBufferAlbedo));
    EXPECT_FALSE(hasFlag(diag, CompositeInputMissing::SamplerAlbedo));
    // Shadow absent — those flags should be set.
    EXPECT_TRUE(hasFlag(diag, CompositeInputMissing::ShadowDepthTexture));
    EXPECT_TRUE(hasFlag(diag, CompositeInputMissing::ShadowCascadeCount));

    dev.destroySampler(sampler);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Descriptor set binding path
// ─────────────────────────────────────────────────────────────────────────────

TEST(RendererBehavior, CompositePathBindsDescriptorSetInsteadOfPushConstants)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle lightPipe{};
    lightPipe.id = 914;
    renderer.setFallbackGeometryPipeline(lightPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler         = sampler;
    bindings.velocitySampler       = sampler;
    bindings.depthSampler          = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Camera cam;
    cam.setPerspective(70.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const CompositePassBindingDesc compositeDesc = renderer.buildCompositePassBindingDesc();
    ASSERT_TRUE(compositeDesc.readyForComposite());

    int bindDescriptorSetCalls = 0;
    int pushConstantsCalls = 0;
    for (const auto& e : cmd.events) {
        if (e.kind == "bindDescriptorSet") ++bindDescriptorSetCalls;
        if (e.kind == "pushConstants")     ++pushConstantsCalls;
    }
    EXPECT_GE(bindDescriptorSetCalls, 1) << "Expected at least one bindDescriptorSet call";
    EXPECT_EQ(pushConstantsCalls, 0)     << "Push-constant handle packing must not be used";

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, CompositeAndShadowDescriptorSetsStayDistinctWithinFrame)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 915;
    PipelineHandle geomPipe{};   geomPipe.id   = 916;
    PipelineHandle lightPipe{};  lightPipe.id  = 917;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler         = sampler;
    bindings.velocitySampler       = sampler;
    bindings.depthSampler          = sampler;
    bindings.shadowDepthSampler    = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 1;
    contract.cascadeSplits[0] = 0.25f;
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Camera cam;
    cam.setPerspective(70.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    uint32_t coreSetId = 0;
    uint32_t shadowSetId = 0;
    for (const auto& e : cmd.events) {
        if (e.kind != "bindDescriptorSet") {
            continue;
        }
        if (e.b == 0u) {
            coreSetId = e.a;
        } else if (e.b == 1u) {
            shadowSetId = e.a;
        }
    }

    EXPECT_NE(coreSetId, 0u);
    EXPECT_NE(shadowSetId, 0u);
    EXPECT_NE(coreSetId, shadowSetId)
        << "Descriptor sets bound in different slots must remain distinct until the frame retires";

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, DescriptorSetsStayAliveUntilOwningFrameSlotIsReused)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 930;
    PipelineHandle geomPipe{};   geomPipe.id   = 931;
    PipelineHandle lightPipe{};  lightPipe.id  = 932;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler         = sampler;
    bindings.velocitySampler       = sampler;
    bindings.depthSampler          = sampler;
    bindings.shadowDepthSampler    = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 1;
    contract.cascadeSplits[0] = 0.3f;
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd, {0, 1, 0}, 2);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("frame-slot-reuse");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 71;
    n->mesh.indexBuffer.id  = 72;
    n->mesh.indexCount      = 3;
    n->castShadow = true;

    Camera cam;
    cam.setPerspective(70.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();
    const BoundDescriptorSets frame0Sets = collectBoundDescriptorSets(cmd);

    cmd.clearRecordedState();
    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();
    const BoundDescriptorSets frame1Sets = collectBoundDescriptorSets(cmd);

    cmd.clearRecordedState();
    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();
    const BoundDescriptorSets frame2Sets = collectBoundDescriptorSets(cmd);

    EXPECT_NE(frame0Sets.core, 0u);
    EXPECT_NE(frame0Sets.shadow, 0u);
    EXPECT_NE(frame1Sets.core, 0u);
    EXPECT_NE(frame1Sets.shadow, 0u);
    EXPECT_NE(frame2Sets.core, 0u);
    EXPECT_NE(frame2Sets.shadow, 0u);
    EXPECT_NE(frame0Sets.core, frame0Sets.shadow);
    EXPECT_NE(frame1Sets.core, frame1Sets.shadow);
    EXPECT_NE(frame2Sets.core, frame2Sets.shadow);

    EXPECT_NE(sortedDescriptorSetIds(frame0Sets), sortedDescriptorSetIds(frame1Sets));
    EXPECT_EQ(sortedDescriptorSetIds(frame0Sets), sortedDescriptorSetIds(frame2Sets));

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ResizeReleasesDeferredDescriptorSetsAndRebuildsShadowChain)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {800, 600};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 933;
    PipelineHandle geomPipe{};   geomPipe.id   = 934;
    PipelineHandle lightPipe{};  lightPipe.id  = 935;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler         = sampler;
    bindings.velocitySampler       = sampler;
    bindings.depthSampler          = sampler;
    bindings.shadowDepthSampler    = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 1;
    contract.cascadeSplits[0] = 0.25f;
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd, {0, 1}, 2);
    scheduler.extent = {800, 600};
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("resize-shadow-chain");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 81;
    n->mesh.indexBuffer.id  = 82;
    n->mesh.indexCount      = 6;
    n->castShadow = true;

    Camera cam;
    cam.setPerspective(70.f, 4.f / 3.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const BoundDescriptorSets beforeResizeSets = collectBoundDescriptorSets(cmd);
    const ShadowPassBindingDesc beforeResizeShadow = renderer.buildShadowPassBindingDesc();
    EXPECT_TRUE(beforeResizeShadow.readyForLightingSampling());
    EXPECT_EQ(beforeResizeShadow.cascadeExtent.width, 800u);
    EXPECT_EQ(beforeResizeShadow.cascadeExtent.height, 600u);

    renderer.onResize({1600, 900});
    EXPECT_EQ(scheduler.resizeCount, 1);

    const ShadowPassBindingDesc clearedShadow = renderer.buildShadowPassBindingDesc();
    EXPECT_FALSE(clearedShadow.hasDepthTarget());
    EXPECT_FALSE(clearedShadow.readyForLightingSampling());
    EXPECT_FALSE(renderer.buildShadowLightingBindingDesc().isComplete());

    cmd.clearRecordedState();
    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const BoundDescriptorSets afterResizeSets = collectBoundDescriptorSets(cmd);
    const ShadowPassBindingDesc afterResizeShadow = renderer.buildShadowPassBindingDesc();

    EXPECT_EQ(sortedDescriptorSetIds(beforeResizeSets), sortedDescriptorSetIds(afterResizeSets));
    EXPECT_TRUE(afterResizeShadow.readyForLightingSampling());
    EXPECT_EQ(afterResizeShadow.cascadeExtent.width, 1600u);
    EXPECT_EQ(afterResizeShadow.cascadeExtent.height, 900u);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ResetSceneReleasesDeferredDescriptorSetsAndClearsShadowChain)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 936;
    PipelineHandle geomPipe{};   geomPipe.id   = 937;
    PipelineHandle lightPipe{};  lightPipe.id  = 938;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler         = sampler;
    bindings.velocitySampler       = sampler;
    bindings.depthSampler          = sampler;
    bindings.shadowDepthSampler    = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount = 1;
    contract.cascadeSplits[0] = 0.4f;
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd, {0, 1}, 2);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* initial = scene.createNode("before-reset-shadow");
    ASSERT_NE(initial, nullptr);
    initial->mesh.vertexBuffer.id = 91;
    initial->mesh.indexBuffer.id  = 92;
    initial->mesh.indexCount      = 6;
    initial->castShadow = true;

    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const BoundDescriptorSets beforeResetSets = collectBoundDescriptorSets(cmd);
    EXPECT_TRUE(renderer.buildShadowPassBindingDesc().readyForLightingSampling());

    renderer.resetScene(scene);

    EXPECT_EQ(scene.root().children().size(), 0u);
    EXPECT_FALSE(renderer.buildShadowPassBindingDesc().hasDepthTarget());
    EXPECT_FALSE(renderer.buildShadowLightingBindingDesc().isComplete());

    renderer.setCompositeMaterialBindings(bindings);
    renderer.setShadowLightingContract(contract);

    Node* afterReset = scene.createNode("after-reset-shadow");
    ASSERT_NE(afterReset, nullptr);
    afterReset->mesh.vertexBuffer.id = 93;
    afterReset->mesh.indexBuffer.id  = 94;
    afterReset->mesh.indexCount      = 6;
    afterReset->castShadow = true;
    afterReset->localTransform().translation = {0.f, 0.f, -5.f};

    cmd.clearRecordedState();
    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const BoundDescriptorSets afterResetSets = collectBoundDescriptorSets(cmd);
    EXPECT_EQ(sortedDescriptorSetIds(beforeResetSets), sortedDescriptorSetIds(afterResetSets));
    EXPECT_TRUE(renderer.buildShadowPassBindingDesc().readyForLightingSampling());

    dev.destroySampler(sampler);
}

// ─────────────────────────────────────────────────────────────────────────────
//  ShadowLightingBindingDesc — dedicated shadow descriptor
// ─────────────────────────────────────────────────────────────────────────────

TEST(RendererBehavior, ShadowLightingBindingDescExposesContractFields)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    PipelineHandle shadowPipe{}; shadowPipe.id = 920;
    PipelineHandle geomPipe{};   geomPipe.id   = 921;
    PipelineHandle lightPipe{};  lightPipe.id  = 922;

    renderer.setShadowPipeline(shadowPipe);
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    auto& dev = ctx->device();
    SamplerHandle sampler = dev.createSampler({});
    ASSERT_TRUE(sampler.valid());

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler         = sampler;
    bindings.velocitySampler       = sampler;
    bindings.depthSampler          = sampler;
    bindings.shadowDepthSampler    = sampler;
    renderer.setCompositeMaterialBindings(bindings);

    ShadowLightingContract contract{};
    contract.cascadeCount     = 3;
    contract.cascadeSplits[0] = 0.1f;
    contract.cascadeSplits[1] = 0.4f;
    contract.cascadeSplits[2] = 0.8f;
    contract.lightViewProj[0] = testsupport::makeDeterministicCascadeLightViewProj(-2.0f);
    contract.lightViewProj[1] = testsupport::makeDeterministicCascadeLightViewProj(-1.0f);
    contract.lightViewProj[2] = testsupport::makeDeterministicCascadeLightViewProj(0.5f);
    contract.shadowBias       = 0.0002f;
    contract.normalBias       = 0.002f;
    renderer.setShadowLightingContract(contract);

    RecordingCommandBuffer cmd;
    RecordingScheduler scheduler(cmd);
    renderer.setFrameScheduler(&scheduler);

    SceneGraph scene;
    Node* n = scene.createNode("shadow-binding-desc-test");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 301;
    n->mesh.indexBuffer.id  = 302;
    n->mesh.indexCount      = 6;
    n->castShadow = true;

    Camera cam;
    cam.setPerspective(70.f, 1.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowLightingBindingDesc shadowDesc = renderer.buildShadowLightingBindingDesc();
    EXPECT_TRUE(shadowDesc.isComplete());
    EXPECT_TRUE(shadowDesc.shadowDepthTexture.valid());
    EXPECT_TRUE(shadowDesc.shadowDepthSampler.valid());
    EXPECT_TRUE(shadowDesc.shadowLightingBuffer.valid());
    EXPECT_EQ(shadowDesc.shadowCascadeCount, 3u);
    EXPECT_FLOAT_EQ(shadowDesc.cascadeSplits[0], 0.1f);
    EXPECT_FLOAT_EQ(shadowDesc.cascadeSplits[1], 0.4f);
    EXPECT_FLOAT_EQ(shadowDesc.cascadeSplits[2], 0.8f);
    expectMat4Eq(shadowDesc.lightViewProj[0], testsupport::makeDeterministicCascadeLightViewProj(-2.0f));
    expectMat4Eq(shadowDesc.lightViewProj[1], testsupport::makeDeterministicCascadeLightViewProj(-1.0f));
    expectMat4Eq(shadowDesc.lightViewProj[2], testsupport::makeDeterministicCascadeLightViewProj(0.5f));
    EXPECT_FLOAT_EQ(shadowDesc.shadowBias, 0.0002f);
    EXPECT_FLOAT_EQ(shadowDesc.normalBias, 0.002f);

    dev.destroySampler(sampler);
}

TEST(RendererBehavior, ShadowLightingBindingDescIsIncompleteWhenContractNotSet)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Null;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {1024, 1024};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    Renderer renderer(*ctx, *sc);

    // No shadow contract set — shadow binding desc must be incomplete.
    const ShadowLightingBindingDesc shadowDesc = renderer.buildShadowLightingBindingDesc();
    EXPECT_FALSE(shadowDesc.isComplete());
    EXPECT_FALSE(shadowDesc.shadowDepthTexture.valid());
    EXPECT_FALSE(shadowDesc.shadowDepthSampler.valid());
    EXPECT_FALSE(shadowDesc.shadowLightingBuffer.valid());
    EXPECT_EQ(shadowDesc.shadowCascadeCount, 0u);
}


TEST(RendererBehavior, CompositeDescriptorSetLayoutContractIsStable)
{
    // The renderer publishes the composite pass descriptor layouts as the single
    // source of truth; render() binds from these and pipeline creators must build
    // the composite pipeline's set layouts from them. Guard the contract so an
    // accidental edit that would desync pipeline layout vs runtime bindings fails here.
    const std::span<const DescriptorBindingDesc> core   = Renderer::compositeCoreSetLayout();
    const std::span<const DescriptorBindingDesc> shadow = Renderer::compositeShadowSetLayout();

    ASSERT_EQ(core.size(), 9u);
    ASSERT_EQ(shadow.size(), 3u);

    for (uint32_t i = 0; i < core.size(); ++i)   EXPECT_EQ(core[i].binding, i);
    for (uint32_t i = 0; i < shadow.size(); ++i) EXPECT_EQ(shadow[i].binding, i);

    // Set 0: albedo/normal/velocity/depth textures, their samplers, material table.
    EXPECT_EQ(core[0].type, DescriptorType::SampledTexture);
    EXPECT_EQ(core[3].type, DescriptorType::SampledTexture);
    EXPECT_EQ(core[4].type, DescriptorType::Sampler);
    EXPECT_EQ(core[7].type, DescriptorType::Sampler);
    EXPECT_EQ(core[8].type, DescriptorType::StorageBuffer);

    // Set 1: shadow depth texture + sampler + lighting buffer.
    EXPECT_EQ(shadow[0].type, DescriptorType::SampledTexture);
    EXPECT_EQ(shadow[1].type, DescriptorType::Sampler);
    EXPECT_EQ(shadow[2].type, DescriptorType::StorageBuffer);
}
