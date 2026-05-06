#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/FrameScheduler.h>
#include <nexus/render/Renderer.h>

#include "../../src/kernel/src/backend/vulkan/VulkanDevice.h"
#include "../../src/kernel/src/backend/vulkan/VulkanSwapchain.h"
#include "../../src/kernel/src/backend/vulkan/VulkanFrameScheduler.h"

#include <gtest/gtest.h>

#include <array>
#include <vector>

using namespace nexus::gfx;
using namespace nexus::render;

namespace {

PipelineHandle createVulkanDepthOnlyShadowPipeline(IDevice& dev,
                                                   ShaderHandle& outVertexShader,
                                                   ShaderHandle& outFragmentShader)
{
    outVertexShader = {};
    outFragmentShader = {};

    constexpr std::string_view kShadowVert = R"GLSL(
#version 460
void main() {
    const vec2 p[3] = vec2[3](
        vec2(-0.5, -0.5),
        vec2( 0.5, -0.5),
        vec2( 0.0,  0.5)
    );
    gl_Position = vec4(p[gl_VertexIndex], 0.0, 1.0);
}
)GLSL";

    constexpr std::string_view kShadowFrag = R"GLSL(
#version 460
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
)GLSL";

    ShaderDesc vsDesc{};
    vsDesc.stage = ShaderStage::Vertex;
    vsDesc.glslSource = kShadowVert;
    vsDesc.debugName = "vulkan.regression.shadow.vert";

    ShaderDesc fsDesc{};
    fsDesc.stage = ShaderStage::Fragment;
    fsDesc.glslSource = kShadowFrag;
    fsDesc.debugName = "vulkan.regression.shadow.frag";

    outVertexShader = dev.createShader(vsDesc);
    if (!outVertexShader.valid()) {
        return {};
    }

    outFragmentShader = dev.createShader(fsDesc);
    if (!outFragmentShader.valid()) {
        dev.destroyShader(outVertexShader);
        outVertexShader = {};
        return {};
    }

    GraphicsPipelineDesc gp{};
    gp.vertexShader = outVertexShader;
    gp.fragmentShader = outFragmentShader;
    gp.topology = Topology::TriangleList;
    gp.depthTest = true;
    gp.depthWrite = true;
    gp.blendEnable = false;
    gp.colorAttachmentFormat = Format::Undefined;
    gp.depthAttachmentFormat = Format::D32_Float;
    gp.debugName = "vulkan.regression.shadow.pipeline";

    return dev.createGraphicsPipeline(gp);
}

struct VulkanTriangleBuffers {
    BufferHandle vertex;
    BufferHandle index;
};

VulkanTriangleBuffers createVulkanTriangleBuffers(IDevice& dev)
{
    VulkanTriangleBuffers buffers{};

    BufferDesc vbDesc{};
    vbDesc.sizeBytes = 3u * 3u * sizeof(float);
    vbDesc.usage = BufferUsage::VertexBuffer | BufferUsage::TransferDst;
    vbDesc.memory = MemoryHint::GpuOnly;
    vbDesc.debugName = "vulkan.regression.shadow.vb";
    buffers.vertex = dev.createBuffer(vbDesc);

    BufferDesc ibDesc{};
    ibDesc.sizeBytes = 3u * sizeof(uint32_t);
    ibDesc.usage = BufferUsage::IndexBuffer | BufferUsage::TransferDst;
    ibDesc.memory = MemoryHint::GpuOnly;
    ibDesc.debugName = "vulkan.regression.shadow.ib";
    buffers.index = dev.createBuffer(ibDesc);

    if (!buffers.vertex.valid() || !buffers.index.valid()) {
        return {};
    }

    const std::array<float, 9> verts = {
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.0f,  0.5f, 0.0f
    };
    const std::array<uint32_t, 3> indices = {0u, 1u, 2u};
    dev.uploadBuffer(buffers.vertex, verts.data(), sizeof(verts), 0);
    dev.uploadBuffer(buffers.index, indices.data(), sizeof(indices), 0);

    return buffers;
}

void destroyVulkanTriangleBuffers(IDevice& dev, const VulkanTriangleBuffers& buffers)
{
    if (buffers.vertex.valid()) {
        dev.destroyBuffer(buffers.vertex);
    }
    if (buffers.index.valid()) {
        dev.destroyBuffer(buffers.index);
    }
}

} // namespace

TEST(VulkanRegression, CreateComputePipelineFromInlineGlsl)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }

    ASSERT_NE(dev, nullptr);

    constexpr std::string_view kComp = R"GLSL(
#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main() {}
)GLSL";

    ShaderDesc csDesc{};
    csDesc.stage = ShaderStage::Compute;
    csDesc.glslSource = kComp;
    csDesc.debugName = "test.compute.regression";

    ShaderHandle cs = dev->createShader(csDesc);
    ASSERT_TRUE(cs.valid());

    ComputePipelineDesc cp{};
    cp.computeShader = cs;
    cp.debugName = "test.compute.pipeline";

    PipelineHandle pipe = dev->createComputePipeline(cp);
    EXPECT_TRUE(pipe.valid());

    if (pipe.valid()) dev->destroyPipeline(pipe);
    dev->destroyShader(cs);
}

TEST(VulkanRegression, SamplerAndQueryPoolLifecycle)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    SamplerDesc s{};
    s.anisotropyEnable = true;
    s.maxAnisotropy = 8.f;
    auto sampler = dev->createSampler(s);
    EXPECT_TRUE(sampler.valid());

    QueryPoolDesc q{};
    q.count = 4;
    auto qp = dev->createQueryPool(q);
    EXPECT_TRUE(qp.valid());

    if (sampler.valid()) dev->destroySampler(sampler);
    if (qp.valid()) dev->destroyQueryPool(qp);
}

TEST(VulkanRegression, TimelineSemaphoreSignalWaitAndQuery)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    SemaphoreHandle timeline = dev->createTimelineSemaphore(2);
    ASSERT_TRUE(timeline.valid());

    const uint64_t initial = dev->querySemaphoreValue(timeline);
    EXPECT_EQ(initial, 2u);

    dev->signalSemaphore(timeline, 5);
    const uint64_t afterSignal = dev->querySemaphoreValue(timeline);
    EXPECT_EQ(afterSignal, 5u);

    dev->waitSemaphore(timeline, 5, UINT64_MAX);
    const uint64_t afterWait = dev->querySemaphoreValue(timeline);
    EXPECT_EQ(afterWait, 5u);

    dev->destroySemaphore(timeline);
}

TEST(VulkanRegression, TimelineSubmitChainsWaitAndSignalValues)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    SemaphoreHandle producer = dev->createTimelineSemaphore(0);
    SemaphoreHandle consumer = dev->createTimelineSemaphore(0);
    ASSERT_TRUE(producer.valid());
    ASSERT_TRUE(consumer.valid());

    // Seed producer timeline so queue submit has a satisfied wait value.
    dev->signalSemaphore(producer, 1);

    const std::array<SemaphoreHandle, 1> waitSems{producer};
    const std::array<uint64_t, 1> waitVals{1};
    const std::array<SemaphoreHandle, 1> signalSems{consumer};
    const std::array<uint64_t, 1> signalVals{3};

    dev->submitWithTimeline(
        QueueType::Graphics,
        {},
        std::span<const SemaphoreHandle>(waitSems),
        std::span<const uint64_t>(waitVals),
        std::span<const SemaphoreHandle>(signalSems),
        std::span<const uint64_t>(signalVals),
        {}
    );

    dev->waitSemaphore(consumer, 3, UINT64_MAX);
    EXPECT_EQ(dev->querySemaphoreValue(consumer), 3u);

    dev->destroySemaphore(consumer);
    dev->destroySemaphore(producer);
}

TEST(VulkanRegression, UploadTextureStagingPathNoCrash)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    TextureDesc td{};
    td.extent = {4, 4, 1};
    td.format = Format::R8G8B8A8_Unorm;
    td.usage  = TextureUsage::Sampled | TextureUsage::TransferDst;

    TextureHandle tex = dev->createTexture(td);
    ASSERT_TRUE(tex.valid());

    std::array<uint8_t, 4 * 4 * 4> pixels{};
    for (size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<uint8_t>(i);

    ASSERT_NO_THROW(dev->uploadTexture(tex, pixels.data(), pixels.size()));
    dev->destroyTexture(tex);
}

TEST(VulkanRegression, SparseAllocatorCommitEvictTileNoCrash)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Vulkan;
        desc.validation = ValidationLevel::Off;
        desc.enableRayTracing = false;
        desc.enableMeshShaders = false;
        ctx = RenderContext::create(desc);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }

    ASSERT_NE(ctx, nullptr);
    if (!ctx->caps().tiledResources) {
        GTEST_SKIP() << "Sparse tiled resources not supported on this GPU";
    }

    TextureDesc td{};
    td.extent = {256, 256, 1};
    td.format = Format::R8G8B8A8_Unorm;
    td.usage  = TextureUsage::Sampled | TextureUsage::TransferDst;
    td.isTiled = true;
    td.debugName = "vulkan.regression.sparse.tiledTexture";

    auto& dev = ctx->device();
    TextureHandle tex = dev.createTexture(td);
    ASSERT_TRUE(tex.valid());

    auto& alloc = ctx->allocator();
    EXPECT_NO_THROW(alloc.commitTile(tex, 0, 0, 0));
    EXPECT_NO_THROW(alloc.evictTile(tex, 0, 0, 0));

    dev.destroyTexture(tex);
}

TEST(VulkanRegression, SparseAllocatorCommitEvictOnNonTiledTextureIsNoOp)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Vulkan;
        desc.validation = ValidationLevel::Off;
        desc.enableRayTracing = false;
        desc.enableMeshShaders = false;
        ctx = RenderContext::create(desc);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }

    ASSERT_NE(ctx, nullptr);

    TextureDesc td{};
    td.extent = {128, 128, 1};
    td.format = Format::R8G8B8A8_Unorm;
    td.usage  = TextureUsage::Sampled | TextureUsage::TransferDst;
    td.isTiled = false;
    td.debugName = "vulkan.regression.nonSparse.texture";

    auto& dev = ctx->device();
    TextureHandle tex = dev.createTexture(td);
    ASSERT_TRUE(tex.valid());

    auto& alloc = ctx->allocator();
    EXPECT_NO_THROW(alloc.commitTile(tex, 0, 0, 0));
    EXPECT_NO_THROW(alloc.evictTile(tex, 0, 0, 0));

    dev.destroyTexture(tex);
}

TEST(VulkanRegression, TimestampQueryPathThroughFrameScheduler)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Vulkan;
        desc.validation = ValidationLevel::Off;
        desc.enableRayTracing = false;
        desc.enableMeshShaders = false;
        ctx = RenderContext::create(desc);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }

    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.nativeWindowHandle = nullptr;
    sd.extent = {640, 360};
    sd.imageCount = 3;

    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    auto* vkDev = dynamic_cast<VulkanDevice*>(&ctx->device());
    auto* vkSc  = dynamic_cast<VulkanSwapchain*>(sc.get());
    if (!vkDev || !vkSc) GTEST_SKIP() << "Not running on Vulkan concrete backend";

    VulkanFrameScheduler scheduler(*vkDev, *vkSc, 2);

    QueryPoolDesc q{};
    q.count = 2;
    auto qp = vkDev->createQueryPool(q);
    ASSERT_TRUE(qp.valid());

    auto frame = scheduler.beginFrame();
    ASSERT_TRUE(frame.has_value());

    frame->cmd->resetQueryPool(qp, 0, 2);
    frame->cmd->writeTimestamp(qp, 0);
    frame->cmd->writeTimestamp(qp, 1);
    scheduler.endFrame();

    uint64_t out[2] = {0, 0};
    ASSERT_NO_THROW(vkDev->readbackTimestamps(qp, 0, 2, out));
    EXPECT_GE(out[1], out[0]);

    vkDev->destroyQueryPool(qp);
}

TEST(VulkanRegression, BufferLifecycleForCompositeMaterialTable)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    BufferDesc materialTableDesc{};
    materialTableDesc.sizeBytes = 1024;
    materialTableDesc.usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
    materialTableDesc.memory = MemoryHint::GpuOnly;
    materialTableDesc.debugName = "vulkan.regression.compositeMaterialTable";

    BufferHandle materialTable = dev->createBuffer(materialTableDesc);
    EXPECT_TRUE(materialTable.valid());

    if (materialTable.valid()) {
        dev->destroyBuffer(materialTable);
    }
}

TEST(VulkanRegression, ShadowMapDepthTextureLifecycle)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    TextureDesc shadowDepth{};
    shadowDepth.extent = {1024, 1024, 1};
    shadowDepth.format = Format::D32_Float;
    shadowDepth.usage  = TextureUsage::DepthAttachment | TextureUsage::Sampled;
    shadowDepth.memory = MemoryHint::GpuOnly;
    shadowDepth.debugName = "vulkan.regression.shadowDepth";

    TextureHandle tex = dev->createTexture(shadowDepth);
    EXPECT_TRUE(tex.valid());
    if (tex.valid()) {
        dev->destroyTexture(tex);
    }
}

TEST(VulkanRegression, ShadowLightingContractBufferUploadLifecycle)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    BufferDesc bd{};
    bd.sizeBytes = sizeof(ShadowLightingContract);
    bd.usage = BufferUsage::StorageBuffer | BufferUsage::TransferDst;
    bd.memory = MemoryHint::GpuOnly;
    bd.debugName = "vulkan.regression.shadowLightingContract";

    BufferHandle b = dev->createBuffer(bd);
    ASSERT_TRUE(b.valid());

    ShadowLightingContract c{};
    c.cascadeCount = 4;
    c.cascadeSplits[0] = 0.05f;
    c.cascadeSplits[1] = 0.15f;
    c.cascadeSplits[2] = 0.35f;
    c.cascadeSplits[3] = 1.0f;
    c.lightViewProj[0] = Mat4::identity();

    ASSERT_NO_THROW(dev->uploadBuffer(b, &c, sizeof(c), 0));
    dev->destroyBuffer(b);
}

TEST(VulkanRegression, ShadowPassBindingDescBuildsCascadeAtlasPassDescriptors)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Vulkan;
        desc.validation = ValidationLevel::Off;
        desc.enableRayTracing = false;
        desc.enableMeshShaders = false;
        ctx = RenderContext::create(desc);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.nativeWindowHandle = nullptr;
    sd.extent = {640, 360};
    sd.imageCount = 3;
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    auto* vkDev = dynamic_cast<VulkanDevice*>(&ctx->device());
    auto* vkSc = dynamic_cast<VulkanSwapchain*>(sc.get());
    if (!vkDev || !vkSc) {
        GTEST_SKIP() << "Not running on Vulkan concrete backend";
    }

    ShaderHandle shadowVs{};
    ShaderHandle shadowFs{};
    const PipelineHandle shadowPipeline = createVulkanDepthOnlyShadowPipeline(*vkDev, shadowVs, shadowFs);
    if (!shadowPipeline.valid()) {
        GTEST_SKIP() << "Unable to create Vulkan shadow pipeline in this environment";
    }

    VulkanFrameScheduler scheduler(*vkDev, *vkSc, 2);
    Renderer renderer(*ctx, *sc);
    renderer.setFrameScheduler(&scheduler);
    renderer.setShadowPipeline(shadowPipeline);

    ShadowLightingContract contract{};
    contract.cascadeCount = 3;
    contract.cascadeSplits[0] = 0.1f;
    contract.cascadeSplits[1] = 0.35f;
    contract.cascadeSplits[2] = 0.75f;
    renderer.setShadowLightingContract(contract);

    const VulkanTriangleBuffers triangleBuffers = createVulkanTriangleBuffers(*vkDev);
    ASSERT_TRUE(triangleBuffers.vertex.valid());
    ASSERT_TRUE(triangleBuffers.index.valid());

    SceneGraph scene;
    Node* node = scene.createNode("vk-shadow-cascade-atlas");
    ASSERT_NE(node, nullptr);
    node->mesh.vertexBuffer = triangleBuffers.vertex;
    node->mesh.indexBuffer = triangleBuffers.index;
    node->mesh.indexCount = 3;
    node->castShadow = true;
    node->localTransform().translation = {0.f, 0.f, -5.f};

    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc shadowDesc = renderer.buildShadowPassBindingDesc();
    EXPECT_TRUE(shadowDesc.hasAtlasTarget());
    EXPECT_TRUE(shadowDesc.hasPassDescriptors());
    EXPECT_TRUE(shadowDesc.readyForDepthPass());
    EXPECT_TRUE(shadowDesc.readyForLightingSampling());
    EXPECT_EQ(shadowDesc.cascadeCount, 3u);
    EXPECT_EQ(shadowDesc.cascadeExtent.width, 640u);
    EXPECT_EQ(shadowDesc.cascadeExtent.height, 360u);
    EXPECT_EQ(shadowDesc.atlasExtent.width, 1280u);
    EXPECT_EQ(shadowDesc.atlasExtent.height, 720u);
    EXPECT_EQ(shadowDesc.atlasColumns, 2u);
    EXPECT_EQ(shadowDesc.atlasRows, 2u);
    EXPECT_EQ(shadowDesc.atlasDepthLayout, TextureLayout::DepthRead);

    EXPECT_EQ(shadowDesc.cascadePasses[0].atlasRect.offset.x, 0);
    EXPECT_EQ(shadowDesc.cascadePasses[0].atlasRect.offset.y, 0);
    EXPECT_EQ(shadowDesc.cascadePasses[1].atlasRect.offset.x, 640);
    EXPECT_EQ(shadowDesc.cascadePasses[1].atlasRect.offset.y, 0);
    EXPECT_EQ(shadowDesc.cascadePasses[2].atlasRect.offset.x, 0);
    EXPECT_EQ(shadowDesc.cascadePasses[2].atlasRect.offset.y, 360);

    vkDev->waitIdle();
    destroyVulkanTriangleBuffers(*vkDev, triangleBuffers);
    vkDev->destroyPipeline(shadowPipeline);
    vkDev->destroyShader(shadowVs);
    vkDev->destroyShader(shadowFs);
}

TEST(VulkanRegression, ShadowPassBindingDescClearsAndRebuildsAcrossResize)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Vulkan;
        desc.validation = ValidationLevel::Off;
        desc.enableRayTracing = false;
        desc.enableMeshShaders = false;
        ctx = RenderContext::create(desc);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.nativeWindowHandle = nullptr;
    sd.extent = {640, 360};
    sd.imageCount = 3;
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    auto* vkDev = dynamic_cast<VulkanDevice*>(&ctx->device());
    auto* vkSc = dynamic_cast<VulkanSwapchain*>(sc.get());
    if (!vkDev || !vkSc) {
        GTEST_SKIP() << "Not running on Vulkan concrete backend";
    }

    ShaderHandle shadowVs{};
    ShaderHandle shadowFs{};
    const PipelineHandle shadowPipeline = createVulkanDepthOnlyShadowPipeline(*vkDev, shadowVs, shadowFs);
    if (!shadowPipeline.valid()) {
        GTEST_SKIP() << "Unable to create Vulkan shadow pipeline in this environment";
    }

    VulkanFrameScheduler scheduler(*vkDev, *vkSc, 2);
    Renderer renderer(*ctx, *sc);
    renderer.setFrameScheduler(&scheduler);
    renderer.setShadowPipeline(shadowPipeline);

    ShadowLightingContract contract{};
    contract.cascadeCount = 2;
    contract.cascadeSplits[0] = 0.2f;
    contract.cascadeSplits[1] = 0.6f;
    renderer.setShadowLightingContract(contract);

    const VulkanTriangleBuffers triangleBuffers = createVulkanTriangleBuffers(*vkDev);
    ASSERT_TRUE(triangleBuffers.vertex.valid());
    ASSERT_TRUE(triangleBuffers.index.valid());

    SceneGraph scene;
    Node* node = scene.createNode("vk-shadow-resize");
    ASSERT_NE(node, nullptr);
    node->mesh.vertexBuffer = triangleBuffers.vertex;
    node->mesh.indexBuffer = triangleBuffers.index;
    node->mesh.indexCount = 3;
    node->castShadow = true;
    node->localTransform().translation = {0.f, 0.f, -5.f};

    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc beforeResize = renderer.buildShadowPassBindingDesc();
    EXPECT_TRUE(beforeResize.readyForLightingSampling());
    EXPECT_EQ(beforeResize.cascadeExtent.width, 640u);
    EXPECT_EQ(beforeResize.cascadeExtent.height, 360u);
    EXPECT_EQ(beforeResize.atlasExtent.width, 1280u);
    EXPECT_EQ(beforeResize.atlasExtent.height, 360u);

    renderer.onResize({800, 600});

    const ShadowPassBindingDesc afterResizeClear = renderer.buildShadowPassBindingDesc();
    EXPECT_FALSE(afterResizeClear.hasAtlasTarget());
    EXPECT_FALSE(afterResizeClear.hasPassDescriptors());
    EXPECT_FALSE(afterResizeClear.readyForLightingSampling());

    renderer.beginFrame();
    renderer.render(cam, scene);
    renderer.endFrame();

    const ShadowPassBindingDesc afterResizeRender = renderer.buildShadowPassBindingDesc();
    EXPECT_TRUE(afterResizeRender.readyForLightingSampling());
    EXPECT_EQ(afterResizeRender.cascadeExtent.width, 800u);
    EXPECT_EQ(afterResizeRender.cascadeExtent.height, 600u);
    EXPECT_EQ(afterResizeRender.atlasExtent.width, 1600u);
    EXPECT_EQ(afterResizeRender.atlasExtent.height, 600u);

    vkDev->waitIdle();
    destroyVulkanTriangleBuffers(*vkDev, triangleBuffers);
    vkDev->destroyPipeline(shadowPipeline);
    vkDev->destroyShader(shadowVs);
    vkDev->destroyShader(shadowFs);
}

TEST(VulkanRegression, CreateMeshShaderPipelineFromInlineGlsl)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);
    if (!dev->caps().meshShaders) {
        GTEST_SKIP() << "Mesh shaders not supported on this GPU";
    }

    constexpr std::string_view kMesh = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1) in;
layout(triangles, max_vertices = 3, max_primitives = 1) out;
void main() {
  SetMeshOutputsEXT(3, 1);
  gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
  gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);
  gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);
  gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
}
)GLSL";

    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(location=0) out vec4 outColor;
void main() { outColor = vec4(1.0, 1.0, 1.0, 1.0); }
)GLSL";

    ShaderDesc meshDesc{};
    meshDesc.stage = ShaderStage::Mesh;
    meshDesc.glslSource = kMesh;
    meshDesc.debugName = "test.mesh.inline";
    ShaderHandle mesh = dev->createShader(meshDesc);
    if (!mesh.valid()) {
        GTEST_SKIP() << "Mesh shader compilation unavailable in this environment";
    }

    ShaderDesc fragDesc{};
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.glslSource = kFrag;
    fragDesc.debugName = "test.mesh.frag.inline";
    ShaderHandle frag = dev->createShader(fragDesc);
    ASSERT_TRUE(frag.valid());

    MeshShaderPipelineDesc mp{};
    mp.meshShader = mesh;
    mp.fragmentShader = frag;
    mp.depthTest = true;
    mp.depthWrite = true;
    mp.debugName = "test.mesh.pipeline";

    PipelineHandle pipe = dev->createMeshShaderPipeline(mp);
    EXPECT_TRUE(pipe.valid());

    if (pipe.valid()) dev->destroyPipeline(pipe);
    dev->destroyShader(mesh);
    dev->destroyShader(frag);
}

TEST(VulkanRegression, CreateMeshShaderPipelineRejectsMissingFragmentShader)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);
    if (!dev->caps().meshShaders) {
        GTEST_SKIP() << "Mesh shaders not supported on this GPU";
    }

    constexpr std::string_view kMesh = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1) in;
layout(triangles, max_vertices = 3, max_primitives = 1) out;
void main() {
  SetMeshOutputsEXT(3, 1);
  gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
  gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);
  gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);
  gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
}
)GLSL";

    ShaderDesc meshDesc{};
    meshDesc.stage = ShaderStage::Mesh;
    meshDesc.glslSource = kMesh;
    meshDesc.debugName = "test.mesh.no-frag.inline";
    ShaderHandle mesh = dev->createShader(meshDesc);
    if (!mesh.valid()) {
        GTEST_SKIP() << "Mesh shader compilation unavailable in this environment";
    }

    MeshShaderPipelineDesc mp{};
    mp.meshShader = mesh;
    mp.depthTest = true;
    mp.depthWrite = true;
    mp.debugName = "test.mesh.pipeline.no-frag";

    PipelineHandle pipe = dev->createMeshShaderPipeline(mp);
    EXPECT_FALSE(pipe.valid());

    dev->destroyShader(mesh);
}

TEST(VulkanRegression, CreateMeshShaderPipelineWithTaskShaderFromInlineGlsl)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);
    if (!dev->caps().meshShaders) {
        GTEST_SKIP() << "Mesh shaders not supported on this GPU";
    }

    constexpr std::string_view kTask = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1) in;
void main() {
  EmitMeshTasksEXT(1, 1, 1);
}
)GLSL";

    constexpr std::string_view kMesh = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1) in;
layout(triangles, max_vertices = 3, max_primitives = 1) out;
void main() {
  SetMeshOutputsEXT(3, 1);
  gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
  gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);
  gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);
  gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
}
)GLSL";

    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(location=0) out vec4 outColor;
void main() { outColor = vec4(0.2, 0.6, 0.8, 1.0); }
)GLSL";

    ShaderDesc taskDesc{};
    taskDesc.stage = ShaderStage::Task;
    taskDesc.glslSource = kTask;
    taskDesc.debugName = "test.mesh.task.inline";
    ShaderHandle task = dev->createShader(taskDesc);
    if (!task.valid()) {
        GTEST_SKIP() << "Task shader compilation unavailable in this environment";
    }

    ShaderDesc meshDesc{};
    meshDesc.stage = ShaderStage::Mesh;
    meshDesc.glslSource = kMesh;
    meshDesc.debugName = "test.mesh.taskpath.mesh.inline";
    ShaderHandle mesh = dev->createShader(meshDesc);
    ASSERT_TRUE(mesh.valid());

    ShaderDesc fragDesc{};
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.glslSource = kFrag;
    fragDesc.debugName = "test.mesh.taskpath.frag.inline";
    ShaderHandle frag = dev->createShader(fragDesc);
    ASSERT_TRUE(frag.valid());

    MeshShaderPipelineDesc mp{};
    mp.taskShader = task;
    mp.meshShader = mesh;
    mp.fragmentShader = frag;
    mp.depthTest = true;
    mp.depthWrite = true;
    mp.debugName = "test.mesh.pipeline.task";

    PipelineHandle pipe = dev->createMeshShaderPipeline(mp);
    EXPECT_TRUE(pipe.valid());

    if (pipe.valid()) dev->destroyPipeline(pipe);
    dev->destroyShader(frag);
    dev->destroyShader(mesh);
    dev->destroyShader(task);
}

TEST(VulkanRegression, MeshShaderPipelineCacheReturnsSameHandleForIdenticalDescriptors)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);
    if (!dev->caps().meshShaders) {
        GTEST_SKIP() << "Mesh shaders not supported on this GPU";
    }

    constexpr std::string_view kMesh = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1) in;
layout(triangles, max_vertices = 3, max_primitives = 1) out;
void main() {
  SetMeshOutputsEXT(3, 1);
  gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
  gl_MeshVerticesEXT[1].gl_Position = vec4( 3.0, -1.0, 0.0, 1.0);
  gl_MeshVerticesEXT[2].gl_Position = vec4(-1.0,  3.0, 0.0, 1.0);
  gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0, 1, 2);
}
)GLSL";

    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(location=0) out vec4 outColor;
void main() { outColor = vec4(1.0, 0.5, 0.0, 1.0); }
)GLSL";

    ShaderDesc meshDesc{};
    meshDesc.stage = ShaderStage::Mesh;
    meshDesc.glslSource = kMesh;
    meshDesc.debugName = "test.mesh.cache.mesh.inline";
    ShaderHandle mesh = dev->createShader(meshDesc);
    if (!mesh.valid()) {
        GTEST_SKIP() << "Mesh shader compilation unavailable in this environment";
    }

    ShaderDesc fragDesc{};
    fragDesc.stage = ShaderStage::Fragment;
    fragDesc.glslSource = kFrag;
    fragDesc.debugName = "test.mesh.cache.frag.inline";
    ShaderHandle frag = dev->createShader(fragDesc);
    ASSERT_TRUE(frag.valid());

    MeshShaderPipelineDesc mp1{};
    mp1.meshShader = mesh;
    mp1.fragmentShader = frag;
    mp1.depthTest = true;
    mp1.depthWrite = true;
    mp1.debugName = "test.mesh.pipeline.cache.1";

    MeshShaderPipelineDesc mp2{};
    mp2.meshShader = mesh;
    mp2.fragmentShader = frag;
    mp2.depthTest = true;
    mp2.depthWrite = true;
    mp2.debugName = "test.mesh.pipeline.cache.2";

    PipelineHandle pipe1 = dev->createMeshShaderPipeline(mp1);
    PipelineHandle pipe2 = dev->createMeshShaderPipeline(mp2);
    ASSERT_TRUE(pipe1.valid());
    ASSERT_TRUE(pipe2.valid());
    EXPECT_EQ(pipe1.id, pipe2.id);

    if (pipe1.valid()) dev->destroyPipeline(pipe1);
    dev->destroyShader(frag);
    dev->destroyShader(mesh);
}

TEST(VulkanRegression, CreateRayTracingPipelineFromInlineGlsl)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);
    if (!dev->caps().rayTracingPipeline) {
        GTEST_SKIP() << "Ray tracing pipelines not supported on this GPU";
    }

    constexpr std::string_view kRayGen = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadEXT vec3 payload;
void main() {
  payload = vec3(0.0, 0.0, 0.0);
}
)GLSL";

    constexpr std::string_view kMiss = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadInEXT vec3 payload;
void main() { payload = vec3(0.0, 0.0, 0.0); }
)GLSL";

    constexpr std::string_view kClosestHit = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadInEXT vec3 payload;
hitAttributeEXT vec2 attribs;
void main() { payload = vec3(1.0, 1.0, 1.0); }
)GLSL";

    ShaderDesc rgDesc{};
    rgDesc.stage = ShaderStage::RayGen;
    rgDesc.glslSource = kRayGen;
    rgDesc.debugName = "test.rt.rgen";
    ShaderHandle rg = dev->createShader(rgDesc);
    if (!rg.valid()) {
        GTEST_SKIP() << "Ray tracing shader compilation unavailable in this environment";
    }

    ShaderDesc missDesc{};
    missDesc.stage = ShaderStage::Miss;
    missDesc.glslSource = kMiss;
    missDesc.debugName = "test.rt.miss";
    ShaderHandle miss = dev->createShader(missDesc);
    ASSERT_TRUE(miss.valid());

    ShaderDesc chDesc{};
    chDesc.stage = ShaderStage::ClosestHit;
    chDesc.glslSource = kClosestHit;
    chDesc.debugName = "test.rt.closesthit";
    ShaderHandle ch = dev->createShader(chDesc);
    ASSERT_TRUE(ch.valid());

    std::array<ShaderHandle, 1> missShaders{miss};
    std::array<ShaderHandle, 1> hitShaders{ch};

    RayTracingPipelineDesc rp{};
    rp.rayGenShader = rg;
    rp.missShaders = std::span<const ShaderHandle>(missShaders);
    rp.closestHitShaders = std::span<const ShaderHandle>(hitShaders);
    rp.maxRecursionDepth = 1;
    rp.debugName = "test.rt.pipeline";

    PipelineHandle pipe = dev->createRayTracingPipeline(rp);
    EXPECT_TRUE(pipe.valid());

    if (pipe.valid()) dev->destroyPipeline(pipe);
    dev->destroyShader(ch);
    dev->destroyShader(miss);
    dev->destroyShader(rg);
}

TEST(VulkanRegression, DescriptorSetAllocateUpdateFreeLifecycle)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    // Allocate a uniform buffer for binding
    BufferDesc bd{};
    bd.sizeBytes = 256;
    bd.usage     = BufferUsage::UniformBuffer;
    bd.memory    = MemoryHint::CpuToGpu;
    bd.debugName = "vk.regression.descset.ubo";
    BufferHandle ubo = dev->createBuffer(bd);
    ASSERT_TRUE(ubo.valid());

    // Allocate a descriptor set with one uniform buffer binding
    DescriptorBindingDesc binding{};
    binding.binding = 0;
    binding.type    = DescriptorType::UniformBuffer;
    binding.buffer  = ubo;

    DescriptorSetDesc setDesc{};
    setDesc.bindings  = std::span{ &binding, 1 };
    setDesc.debugName = "vk.regression.descset";

    DescriptorSetHandle set = dev->allocateDescriptorSet(setDesc);
    ASSERT_TRUE(set.valid());

    // Update the set with the buffer
    ASSERT_NO_THROW(dev->updateDescriptorSet(set, std::span{ &binding, 1 }));

    // Free — must not crash and must tolerate a second free (no-op)
    ASSERT_NO_THROW(dev->freeDescriptorSet(set));
    ASSERT_NO_THROW(dev->freeDescriptorSet(set));  // idempotent: pool/layout zeroed after first free

    dev->destroyBuffer(ubo);
}

TEST(VulkanRegression, DescriptorSetAllocateSamplerBinding)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

    // Create a sampler to bind
    SamplerDesc sd{};
    sd.debugName = "vk.regression.descset.sampler";
    SamplerHandle sampler = dev->createSampler(sd);
    ASSERT_TRUE(sampler.valid());

    // Create a texture to bind as sampled image
    TextureDesc td{};
    td.extent    = { 4, 4, 1 };
    td.format    = Format::R8G8B8A8_Unorm;
    td.usage     = TextureUsage::Sampled | TextureUsage::TransferDst;
    td.debugName = "vk.regression.descset.tex";
    TextureHandle tex = dev->createTexture(td);
    ASSERT_TRUE(tex.valid());

    // Allocate a set with a sampled image + sampler binding
    std::array<DescriptorBindingDesc, 2> bindings{};
    bindings[0].binding = 0;
    bindings[0].type    = DescriptorType::SampledTexture;
    bindings[0].texture = tex;
    bindings[1].binding = 1;
    bindings[1].type    = DescriptorType::Sampler;
    bindings[1].sampler = sampler;

    DescriptorSetDesc setDesc{};
    setDesc.bindings  = std::span{ bindings };
    setDesc.debugName = "vk.regression.descset.img";

    DescriptorSetHandle set = dev->allocateDescriptorSet(setDesc);
    ASSERT_TRUE(set.valid());

    ASSERT_NO_THROW(dev->updateDescriptorSet(set, std::span{ bindings }));
    ASSERT_NO_THROW(dev->freeDescriptorSet(set));

    dev->destroyTexture(tex);
    dev->destroySampler(sampler);
}

