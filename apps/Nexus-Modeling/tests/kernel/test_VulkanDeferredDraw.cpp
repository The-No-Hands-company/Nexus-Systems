// ─────────────────────────────────────────────────────────────────────────────
//  Tests: deferred-draw integration harness (real Vulkan device).
//
//  Renders to an offscreen color target and reads it back, exercising the render
//  target + readback path and (build-up over increments) descriptor-bound drawing
//  the way the deferred composite pass does. Runs on any Vulkan device (including
//  the software rasterizer), so it verifies the path on real hardware/CI.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/CommandBuffer.h>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

using namespace nexus::gfx;

namespace {

std::unique_ptr<RenderContext> makeContext()
{
    RenderContextDesc desc{};
    desc.preferredBackend  = Backend::Vulkan;
    desc.validation        = ValidationLevel::Off;
    desc.enableMeshShaders = false;
    desc.enableRayTracing  = false;
    return RenderContext::create(desc);
}

void submitAndWait(IDevice& dev, CmdBufHandle cbh)
{
    const FenceHandle fence = dev.createFence(false);
    const std::array<CmdBufHandle, 1> cmds{ cbh };
    dev.submit(QueueType::Graphics, cmds, {}, {}, fence);
    dev.waitForFence(fence);
    dev.destroyFence(fence);
}

} // namespace

TEST(VulkanDeferredDraw, OffscreenClearAndReadback)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();

    constexpr uint32_t W = 4, H = 4;

    TextureDesc td{};
    td.extent = {W, H, 1};
    td.format = Format::R8G8B8A8_Unorm;
    td.usage  = TextureUsage::ColorAttachment | TextureUsage::TransferSrc;
    td.debugName = "deferred.offscreen.color";
    const TextureHandle color = dev.createTexture(td);
    ASSERT_TRUE(color.valid());

    BufferDesc bd{};
    bd.sizeBytes = uint64_t(W) * H * 4u;
    bd.usage     = BufferUsage::TransferDst;
    bd.memory    = MemoryHint::GpuToCpu;
    bd.debugName = "deferred.readback";
    const BufferHandle readback = dev.createBuffer(bd);
    ASSERT_TRUE(readback.valid());

    const CmdBufHandle cbh = dev.allocateCommandBuffer(QueueType::Graphics);
    ICommandBuffer* cmd = dev.getCommandBuffer(cbh);
    ASSERT_NE(cmd, nullptr);

    cmd->begin();
    // beginRenderPass transitions Undefined -> ColorAttachment and clears.
    ClearValue clear{};
    clear.color = {0.25f, 0.5f, 0.75f, 1.0f};
    cmd->beginRenderPass({}, {&color, 1}, {}, {&clear, 1}, {{0, 0}, {W, H}});
    cmd->endRenderPass();
    // Make the cleared image readable by a transfer copy, then copy to the buffer.
    cmd->textureBarrier({color, TextureLayout::ColorAttachment, TextureLayout::TransferSrc});
    cmd->copyTextureToBuffer(color, readback);
    cmd->end();

    submitAndWait(dev, cbh);

    std::array<uint8_t, W * H * 4> pixels{};
    dev.readbackBuffer(readback, pixels.data(), pixels.size());

    // R8G8B8A8_Unorm: 0.25->64, 0.5->128, 0.75->191, 1.0->255 (allow rounding).
    EXPECT_NEAR(pixels[0], 64,  4);
    EXPECT_NEAR(pixels[1], 128, 4);
    EXPECT_NEAR(pixels[2], 191, 4);
    EXPECT_EQ(pixels[3], 255);

    dev.freeCommandBuffer(cbh);
    dev.destroyBuffer(readback);
    dev.destroyTexture(color);
}

TEST(VulkanDeferredDraw, DescriptorBoundDrawReadsUniformBuffer)
{
    // Deferred-style draw: a fragment shader reads a uniform buffer bound via a
    // descriptor set, writes it, and we read the framebuffer back. Proves
    // descriptor binding is valid AT DRAW TIME on a real device (not just that the
    // pipeline/descriptor sets create successfully). The two-set composite shape is
    // covered by DescriptorBoundDrawReadsTwoSets.
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();

    constexpr uint32_t W = 4, H = 4;

    TextureDesc td{};
    td.extent = {W, H, 1};
    td.format = Format::R8G8B8A8_Unorm;
    td.usage  = TextureUsage::ColorAttachment | TextureUsage::TransferSrc;
    const TextureHandle color = dev.createTexture(td);
    ASSERT_TRUE(color.valid());

    BufferDesc rb{};
    rb.sizeBytes = uint64_t(W) * H * 4u;
    rb.usage     = BufferUsage::TransferDst;
    rb.memory    = MemoryHint::GpuToCpu;
    const BufferHandle readback = dev.createBuffer(rb);
    ASSERT_TRUE(readback.valid());

    const float tint[4] = {0.25f, 0.5f, 0.0f, 1.0f};
    BufferDesc ub{};
    ub.sizeBytes = 16;
    ub.usage     = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
    ub.memory    = MemoryHint::GpuOnly;
    const BufferHandle ubo = dev.createBuffer(ub);
    ASSERT_TRUE(ubo.valid());
    dev.uploadBuffer(ubo, tint, 16);

    constexpr std::string_view kVert = R"GLSL(
#version 460
void main() {
    const vec2 p[3] = vec2[3](vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0));
    gl_Position = vec4(p[gl_VertexIndex], 0.0, 1.0);
}
)GLSL";
    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(set = 0, binding = 0) uniform A { vec4 tint; };
layout(location = 0) out vec4 outColor;
void main() { outColor = tint; }
)GLSL";

    ShaderDesc vsDesc{}; vsDesc.stage = ShaderStage::Vertex;   vsDesc.glslSource = kVert;
    ShaderDesc fsDesc{}; fsDesc.stage = ShaderStage::Fragment; fsDesc.glslSource = kFrag;
    const ShaderHandle vs = dev.createShader(vsDesc);
    const ShaderHandle fs = dev.createShader(fsDesc);
    ASSERT_TRUE(vs.valid());
    ASSERT_TRUE(fs.valid());

    const std::array<DescriptorBindingDesc, 1> layout{{ {0, DescriptorType::UniformBuffer, {}, {}, {}, {}} }};
    const std::array<DescriptorSetLayoutDesc, 1> sets{{ {layout} }};

    GraphicsPipelineDesc gp{};
    gp.vertexShader          = vs;
    gp.fragmentShader        = fs;
    gp.topology              = Topology::TriangleList;
    gp.cullMode              = CullMode::None;  // fullscreen triangle: don't cull by winding
    gp.depthTest             = false;
    gp.depthWrite            = false;
    gp.colorAttachmentFormat = Format::R8G8B8A8_Unorm;  // match the offscreen target
    gp.descriptorSetLayouts  = sets;
    gp.debugName             = "deferred.draw.pipeline";
    const PipelineHandle pso = dev.createGraphicsPipeline(gp);
    ASSERT_TRUE(pso.valid());

    std::array<DescriptorBindingDesc, 1> write{{ {0, DescriptorType::UniformBuffer, ubo, {}, {}, {}} }};
    DescriptorSetDesc dsd{}; dsd.bindings = write;
    const DescriptorSetHandle ds = dev.allocateDescriptorSet(dsd);
    ASSERT_TRUE(ds.valid());
    dev.updateDescriptorSet(ds, write);

    const CmdBufHandle cbh = dev.allocateCommandBuffer(QueueType::Graphics);
    ICommandBuffer* cmd = dev.getCommandBuffer(cbh);
    ASSERT_NE(cmd, nullptr);

    cmd->begin();
    ClearValue clear{};
    clear.color = {0.0f, 0.0f, 0.0f, 1.0f};
    cmd->beginRenderPass({}, {&color, 1}, {}, {&clear, 1}, {{0, 0}, {W, H}});
    cmd->bindPipeline(pso);
    cmd->setViewport({ .x = 0.f, .y = 0.f, .width = float(W), .height = float(H), .minDepth = 0.f, .maxDepth = 1.f });
    cmd->setScissor({ .offset = {0, 0}, .extent = {W, H} });
    cmd->bindDescriptorSet(ds, 0);
    cmd->draw(3, 1, 0, 0);
    cmd->endRenderPass();
    cmd->textureBarrier({color, TextureLayout::ColorAttachment, TextureLayout::TransferSrc});
    cmd->copyTextureToBuffer(color, readback);
    cmd->end();

    submitAndWait(dev, cbh);

    std::array<uint8_t, W * H * 4> pixels{};
    dev.readbackBuffer(readback, pixels.data(), pixels.size());

    // The bound uniform tint (0.25, 0.5, 0.0, 1.0) -> (64, 128, 0, 255).
    EXPECT_NEAR(pixels[0], 64,  4);
    EXPECT_NEAR(pixels[1], 128, 4);
    EXPECT_NEAR(pixels[2], 0,   4);
    EXPECT_EQ(pixels[3], 255);

    dev.freeCommandBuffer(cbh);
    dev.freeDescriptorSet(ds);
    dev.destroyPipeline(pso);
    dev.destroyShader(vs);
    dev.destroyShader(fs);
    dev.destroyBuffer(ubo);
    dev.destroyBuffer(readback);
    dev.destroyTexture(color);
}

TEST(VulkanDeferredDraw, DescriptorBoundDrawReadsTwoSets)
{
    // Full deferred-composite shape: a fragment shader reads a UBO from set 0 and a
    // UBO from set 1 and writes their sum. Proves multi-set descriptor binding is
    // valid AT DRAW TIME on a real device.
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();

    constexpr uint32_t W = 4, H = 4;

    TextureDesc td{};
    td.extent = {W, H, 1};
    td.format = Format::R8G8B8A8_Unorm;
    td.usage  = TextureUsage::ColorAttachment | TextureUsage::TransferSrc;
    const TextureHandle color = dev.createTexture(td);
    ASSERT_TRUE(color.valid());

    BufferDesc rb{};
    rb.sizeBytes = uint64_t(W) * H * 4u;
    rb.usage     = BufferUsage::TransferDst;
    rb.memory    = MemoryHint::GpuToCpu;
    const BufferHandle readback = dev.createBuffer(rb);
    ASSERT_TRUE(readback.valid());

    auto makeUbo = [&](const float v[4]) {
        BufferDesc ub{};
        ub.sizeBytes = 16;
        ub.usage     = BufferUsage::UniformBuffer | BufferUsage::TransferDst;
        ub.memory    = MemoryHint::GpuOnly;
        const BufferHandle h = dev.createBuffer(ub);
        dev.uploadBuffer(h, v, 16);
        return h;
    };
    const float colorA[4] = {0.25f, 0.0f, 0.0f, 1.0f};
    const float colorB[4] = {0.0f, 0.5f, 0.0f, 0.0f};
    const BufferHandle uboA = makeUbo(colorA);
    const BufferHandle uboB = makeUbo(colorB);
    ASSERT_TRUE(uboA.valid() && uboB.valid());

    constexpr std::string_view kVert = R"GLSL(
#version 460
void main() {
    const vec2 p[3] = vec2[3](vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0));
    gl_Position = vec4(p[gl_VertexIndex], 0.0, 1.0);
}
)GLSL";
    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(set = 0, binding = 0) uniform A { vec4 ca; };
layout(set = 1, binding = 0) uniform B { vec4 cb; };
layout(location = 0) out vec4 outColor;
void main() { outColor = ca + cb; }
)GLSL";

    ShaderDesc vsDesc{}; vsDesc.stage = ShaderStage::Vertex;   vsDesc.glslSource = kVert;
    ShaderDesc fsDesc{}; fsDesc.stage = ShaderStage::Fragment; fsDesc.glslSource = kFrag;
    const ShaderHandle vs = dev.createShader(vsDesc);
    const ShaderHandle fs = dev.createShader(fsDesc);
    ASSERT_TRUE(vs.valid() && fs.valid());

    const std::array<DescriptorBindingDesc, 1> uboLayout{{ {0, DescriptorType::UniformBuffer, {}, {}, {}, {}} }};
    const std::array<DescriptorSetLayoutDesc, 2> sets{{ {uboLayout}, {uboLayout} }};

    GraphicsPipelineDesc gp{};
    gp.vertexShader          = vs;
    gp.fragmentShader        = fs;
    gp.topology              = Topology::TriangleList;
    gp.cullMode              = CullMode::None;
    gp.depthTest             = false;
    gp.depthWrite            = false;
    gp.colorAttachmentFormat = Format::R8G8B8A8_Unorm;
    gp.descriptorSetLayouts  = sets;
    gp.debugName             = "deferred.draw.twoset";
    const PipelineHandle pso = dev.createGraphicsPipeline(gp);
    ASSERT_TRUE(pso.valid());

    std::array<DescriptorBindingDesc, 1> w0{{ {0, DescriptorType::UniformBuffer, uboA, {}, {}, {}} }};
    std::array<DescriptorBindingDesc, 1> w1{{ {0, DescriptorType::UniformBuffer, uboB, {}, {}, {}} }};
    DescriptorSetDesc d0{}; d0.bindings = w0;
    DescriptorSetDesc d1{}; d1.bindings = w1;
    const DescriptorSetHandle ds0 = dev.allocateDescriptorSet(d0);
    const DescriptorSetHandle ds1 = dev.allocateDescriptorSet(d1);
    ASSERT_TRUE(ds0.valid() && ds1.valid());
    dev.updateDescriptorSet(ds0, w0);
    dev.updateDescriptorSet(ds1, w1);

    const CmdBufHandle cbh = dev.allocateCommandBuffer(QueueType::Graphics);
    ICommandBuffer* cmd = dev.getCommandBuffer(cbh);
    ASSERT_NE(cmd, nullptr);

    cmd->begin();
    ClearValue clear{};
    clear.color = {0.0f, 0.0f, 0.0f, 1.0f};
    cmd->beginRenderPass({}, {&color, 1}, {}, {&clear, 1}, {{0, 0}, {W, H}});
    cmd->bindPipeline(pso);
    cmd->setViewport({ .x = 0.f, .y = 0.f, .width = float(W), .height = float(H), .minDepth = 0.f, .maxDepth = 1.f });
    cmd->setScissor({ .offset = {0, 0}, .extent = {W, H} });
    cmd->bindDescriptorSet(ds0, 0);
    cmd->bindDescriptorSet(ds1, 1);
    cmd->draw(3, 1, 0, 0);
    cmd->endRenderPass();
    cmd->textureBarrier({color, TextureLayout::ColorAttachment, TextureLayout::TransferSrc});
    cmd->copyTextureToBuffer(color, readback);
    cmd->end();

    submitAndWait(dev, cbh);

    std::array<uint8_t, W * H * 4> pixels{};
    dev.readbackBuffer(readback, pixels.data(), pixels.size());

    // ca + cb = (0.25, 0.5, 0.0, 1.0) -> (64, 128, 0, 255).
    EXPECT_NEAR(pixels[0], 64,  4);
    EXPECT_NEAR(pixels[1], 128, 4);
    EXPECT_NEAR(pixels[2], 0,   4);
    EXPECT_EQ(pixels[3], 255);

    dev.freeCommandBuffer(cbh);
    dev.freeDescriptorSet(ds0);
    dev.freeDescriptorSet(ds1);
    dev.destroyPipeline(pso);
    dev.destroyShader(vs);
    dev.destroyShader(fs);
    dev.destroyBuffer(uboA);
    dev.destroyBuffer(uboB);
    dev.destroyBuffer(readback);
    dev.destroyTexture(color);
}

TEST(VulkanDeferredDraw, MultiRenderTargetPipelineWritesAllAttachments)
{
    // MRT: a pipeline built with colorAttachmentFormats (>1) and a fragment shader
    // writing distinct colors to two outputs renders into two attachments in one
    // pass; both read back independently. This is the capability the deferred
    // GBuffer geometry pipeline (albedo/normal/velocity) needs.
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();

    constexpr uint32_t W = 4, H = 4;

    auto makeColorTarget = [&](const char* name) {
        TextureDesc td{};
        td.extent = {W, H, 1};
        td.format = Format::R8G8B8A8_Unorm;
        td.usage  = TextureUsage::ColorAttachment | TextureUsage::TransferSrc;
        td.debugName = name;
        return dev.createTexture(td);
    };
    const TextureHandle color0 = makeColorTarget("mrt.color0");
    const TextureHandle color1 = makeColorTarget("mrt.color1");
    ASSERT_TRUE(color0.valid() && color1.valid());

    auto makeReadback = [&]() {
        BufferDesc bd{};
        bd.sizeBytes = uint64_t(W) * H * 4u;
        bd.usage     = BufferUsage::TransferDst;
        bd.memory    = MemoryHint::GpuToCpu;
        return dev.createBuffer(bd);
    };
    const BufferHandle rb0 = makeReadback();
    const BufferHandle rb1 = makeReadback();
    ASSERT_TRUE(rb0.valid() && rb1.valid());

    constexpr std::string_view kVert = R"GLSL(
#version 460
void main() {
    const vec2 p[3] = vec2[3](vec2(-1.0,-1.0), vec2(3.0,-1.0), vec2(-1.0,3.0));
    gl_Position = vec4(p[gl_VertexIndex], 0.0, 1.0);
}
)GLSL";
    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(location = 0) out vec4 c0;
layout(location = 1) out vec4 c1;
void main() {
    c0 = vec4(0.25, 0.0, 0.0, 1.0);
    c1 = vec4(0.0, 0.5, 0.75, 1.0);
}
)GLSL";

    ShaderDesc vsDesc{}; vsDesc.stage = ShaderStage::Vertex;   vsDesc.glslSource = kVert;
    ShaderDesc fsDesc{}; fsDesc.stage = ShaderStage::Fragment; fsDesc.glslSource = kFrag;
    const ShaderHandle vs = dev.createShader(vsDesc);
    const ShaderHandle fs = dev.createShader(fsDesc);
    ASSERT_TRUE(vs.valid() && fs.valid());

    const std::array<Format, 2> colorFormats{ Format::R8G8B8A8_Unorm, Format::R8G8B8A8_Unorm };

    GraphicsPipelineDesc gp{};
    gp.vertexShader           = vs;
    gp.fragmentShader         = fs;
    gp.topology               = Topology::TriangleList;
    gp.cullMode               = CullMode::None;
    gp.depthTest              = false;
    gp.depthWrite             = false;
    gp.colorAttachmentFormats = colorFormats;  // MRT: two color attachments
    gp.debugName              = "mrt.pipeline";
    const PipelineHandle pso = dev.createGraphicsPipeline(gp);
    ASSERT_TRUE(pso.valid());

    const CmdBufHandle cbh = dev.allocateCommandBuffer(QueueType::Graphics);
    ICommandBuffer* cmd = dev.getCommandBuffer(cbh);
    ASSERT_NE(cmd, nullptr);

    const std::array<TextureHandle, 2> targets{ color0, color1 };
    std::array<ClearValue, 2> clears{};
    clears[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clears[1].color = {0.0f, 0.0f, 0.0f, 1.0f};

    cmd->begin();
    cmd->beginRenderPass({}, targets, {}, clears, {{0, 0}, {W, H}});
    cmd->bindPipeline(pso);
    cmd->setViewport({ .x = 0.f, .y = 0.f, .width = float(W), .height = float(H), .minDepth = 0.f, .maxDepth = 1.f });
    cmd->setScissor({ .offset = {0, 0}, .extent = {W, H} });
    cmd->draw(3, 1, 0, 0);
    cmd->endRenderPass();
    cmd->textureBarrier({color0, TextureLayout::ColorAttachment, TextureLayout::TransferSrc});
    cmd->textureBarrier({color1, TextureLayout::ColorAttachment, TextureLayout::TransferSrc});
    cmd->copyTextureToBuffer(color0, rb0);
    cmd->copyTextureToBuffer(color1, rb1);
    cmd->end();

    submitAndWait(dev, cbh);

    std::array<uint8_t, W * H * 4> px0{};
    std::array<uint8_t, W * H * 4> px1{};
    dev.readbackBuffer(rb0, px0.data(), px0.size());
    dev.readbackBuffer(rb1, px1.data(), px1.size());

    // Attachment 0 = (0.25, 0, 0, 1) -> (64, 0, 0, 255).
    EXPECT_NEAR(px0[0], 64, 4);
    EXPECT_NEAR(px0[1], 0,  4);
    EXPECT_NEAR(px0[2], 0,  4);
    EXPECT_EQ(px0[3], 255);
    // Attachment 1 = (0, 0.5, 0.75, 1) -> (0, 128, 191, 255). Proves the second
    // attachment received its own distinct output, not a copy of the first.
    EXPECT_NEAR(px1[0], 0,   4);
    EXPECT_NEAR(px1[1], 128, 4);
    EXPECT_NEAR(px1[2], 191, 4);
    EXPECT_EQ(px1[3], 255);

    dev.freeCommandBuffer(cbh);
    dev.destroyPipeline(pso);
    dev.destroyShader(vs);
    dev.destroyShader(fs);
    dev.destroyBuffer(rb0);
    dev.destroyBuffer(rb1);
    dev.destroyTexture(color0);
    dev.destroyTexture(color1);
}
