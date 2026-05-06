// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Vulkan graphics pipeline creation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;

TEST(VulkanPipeline, CreateGraphicsPipelineFromInlineGlsl)
{
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);
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
    std::unique_ptr<IDevice> dev;
    try {
        dev = createDevice(Backend::Vulkan);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable on this machine: " << e.what();
    }
    ASSERT_NE(dev, nullptr);

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
