// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Vulkan graphics pipeline creation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;

namespace {

std::unique_ptr<RenderContext> createMinimalVulkanContext()
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Vulkan;
    desc.validation = ValidationLevel::Off;
    desc.enableMeshShaders = false;
    desc.enableRayTracing = false;
    return RenderContext::create(desc);
}

} // namespace

TEST(VulkanPipeline, CreateGraphicsPipelineFromInlineGlsl)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = createMinimalVulkanContext();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice* dev = &ctx->device();
    ASSERT_EQ(dev->backend(), Backend::Vulkan);

    constexpr std::string_view kVert = R"GLSL(
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

    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(0.1, 0.7, 0.9, 1.0);
}
)GLSL";

    ShaderDesc vsDesc{};
    vsDesc.stage = ShaderStage::Vertex;
    vsDesc.glslSource = kVert;
    vsDesc.debugName = "test.pipeline.vert";

    ShaderDesc fsDesc{};
    fsDesc.stage = ShaderStage::Fragment;
    fsDesc.glslSource = kFrag;
    fsDesc.debugName = "test.pipeline.frag";

    const ShaderHandle vs = dev->createShader(vsDesc);
    const ShaderHandle fs = dev->createShader(fsDesc);
    ASSERT_TRUE(vs.valid());
    ASSERT_TRUE(fs.valid());

    GraphicsPipelineDesc gp{};
    gp.vertexShader = vs;
    gp.fragmentShader = fs;
    gp.topology = Topology::TriangleList;
    gp.depthTest = false;
    gp.depthWrite = false;
    gp.blendEnable = false;
    gp.debugName = "test.graphics.pipeline";

    const PipelineHandle pipe = dev->createGraphicsPipeline(gp);
    EXPECT_TRUE(pipe.valid());

    if (pipe.valid()) dev->destroyPipeline(pipe);
    dev->destroyShader(vs);
    dev->destroyShader(fs);
}

TEST(VulkanPipeline, PSOCacheReturnsSamePipelineForIdenticalDescriptors)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = createMinimalVulkanContext();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice* dev = &ctx->device();

    constexpr std::string_view kVert = R"GLSL(
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

    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(0.1, 0.7, 0.9, 1.0);
}
)GLSL";

    ShaderDesc vsDesc{};
    vsDesc.stage = ShaderStage::Vertex;
    vsDesc.glslSource = kVert;
    vsDesc.debugName = "test.cache.vert";

    ShaderDesc fsDesc{};
    fsDesc.stage = ShaderStage::Fragment;
    fsDesc.glslSource = kFrag;
    fsDesc.debugName = "test.cache.frag";

    const ShaderHandle vs = dev->createShader(vsDesc);
    const ShaderHandle fs = dev->createShader(fsDesc);
    ASSERT_TRUE(vs.valid());
    ASSERT_TRUE(fs.valid());

    // First pipeline creation
    GraphicsPipelineDesc gp1{};
    gp1.vertexShader = vs;
    gp1.fragmentShader = fs;
    gp1.topology = Topology::TriangleList;
    gp1.depthTest = true;
    gp1.depthWrite = true;
    gp1.depthCompare = CompareOp::Less;
    gp1.blendEnable = false;
    gp1.debugName = "test.cache.pipeline.1";

    const PipelineHandle pipe1 = dev->createGraphicsPipeline(gp1);
    ASSERT_TRUE(pipe1.valid());

    // Second pipeline creation with identical descriptor
    GraphicsPipelineDesc gp2{};
    gp2.vertexShader = vs;
    gp2.fragmentShader = fs;
    gp2.topology = Topology::TriangleList;
    gp2.depthTest = true;
    gp2.depthWrite = true;
    gp2.depthCompare = CompareOp::Less;
    gp2.blendEnable = false;
    gp2.debugName = "test.cache.pipeline.2";  // Different debugName shouldn't affect cache

    const PipelineHandle pipe2 = dev->createGraphicsPipeline(gp2);
    ASSERT_TRUE(pipe2.valid());

    // Both should return the same handle from cache
    EXPECT_EQ(pipe1.id, pipe2.id);

    // Cleanup
    dev->destroyPipeline(pipe1);
    dev->destroyShader(vs);
    dev->destroyShader(fs);
}

TEST(VulkanPipeline, ComputePSOCacheReturnsSamePipelineForIdenticalDescriptors)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = createMinimalVulkanContext();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice* dev = &ctx->device();

    constexpr std::string_view kComp = R"GLSL(
#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main() {}
)GLSL";

    ShaderDesc csDesc{};
    csDesc.stage = ShaderStage::Compute;
    csDesc.glslSource = kComp;
    csDesc.debugName = "test.compute.cache";

    const ShaderHandle cs = dev->createShader(csDesc);
    ASSERT_TRUE(cs.valid());

    ComputePipelineDesc cp1{};
    cp1.computeShader = cs;
    cp1.debugName = "test.compute.cache.1";

    ComputePipelineDesc cp2{};
    cp2.computeShader = cs;
    cp2.debugName = "test.compute.cache.2";  // Different debugName should not affect cache

    const PipelineHandle pipe1 = dev->createComputePipeline(cp1);
    ASSERT_TRUE(pipe1.valid());
    const PipelineHandle pipe2 = dev->createComputePipeline(cp2);
    ASSERT_TRUE(pipe2.valid());

    EXPECT_EQ(pipe1.id, pipe2.id);

    dev->destroyPipeline(pipe1);
    dev->destroyShader(cs);
}

TEST(VulkanPipeline, DestroyGraphicsPipelineInvalidatesCacheAndRecreateReturnsFreshHandle)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = createMinimalVulkanContext();
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice* dev = &ctx->device();

    constexpr std::string_view kVert = R"GLSL(
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

    constexpr std::string_view kFrag = R"GLSL(
#version 460
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(0.2, 0.8, 0.3, 1.0);
}
)GLSL";

    ShaderDesc vsDesc{};
    vsDesc.stage = ShaderStage::Vertex;
    vsDesc.glslSource = kVert;
    vsDesc.debugName = "test.destroy.cache.vert";

    ShaderDesc fsDesc{};
    fsDesc.stage = ShaderStage::Fragment;
    fsDesc.glslSource = kFrag;
    fsDesc.debugName = "test.destroy.cache.frag";

    const ShaderHandle vs = dev->createShader(vsDesc);
    const ShaderHandle fs = dev->createShader(fsDesc);
    ASSERT_TRUE(vs.valid());
    ASSERT_TRUE(fs.valid());

    GraphicsPipelineDesc gp{};
    gp.vertexShader = vs;
    gp.fragmentShader = fs;
    gp.topology = Topology::TriangleList;
    gp.depthTest = false;
    gp.depthWrite = false;
    gp.blendEnable = false;
    gp.debugName = "test.destroy.cache.pipeline";

    const PipelineHandle first = dev->createGraphicsPipeline(gp);
    ASSERT_TRUE(first.valid());

    const PipelineHandle cached = dev->createGraphicsPipeline(gp);
    ASSERT_TRUE(cached.valid());
    EXPECT_EQ(first.id, cached.id);

    dev->destroyPipeline(first);

    const PipelineHandle recreated = dev->createGraphicsPipeline(gp);
    ASSERT_TRUE(recreated.valid());
    EXPECT_NE(recreated.id, first.id);

    dev->destroyPipeline(recreated);
    dev->destroyShader(vs);
    dev->destroyShader(fs);
}
