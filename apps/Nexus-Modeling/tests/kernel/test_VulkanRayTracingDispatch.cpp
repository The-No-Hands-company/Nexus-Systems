// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Vulkan ray-tracing bring-up.
//
//  - ShadersCompileToModules runs anywhere a Vulkan device exists (GLSL->SPIR-V
//    compilation does not need GPU ray-tracing support).
//  - RayTracingPipelineBuildsOnHardware is GATED on a tier-2 device
//    (VK_KHR_ray_tracing_pipeline) and skips gracefully otherwise; it exercises
//    ray-tracing pipeline creation and the shader binding table build on real RT
//    hardware.
//
//  The inline GLSL mirrors shaders/rt/raytrace.{rgen,rmiss,rchit}.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/RenderContext.h>

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <string_view>

using namespace nexus::gfx;

namespace {

constexpr std::string_view kRaygen = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, rgba8) uniform image2D outImage;
layout(location = 0) rayPayloadEXT vec3 hitValue;
void main() {
    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 uv = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    const vec3 origin = vec3(uv * 2.0 - 1.0, 1.0);
    const vec3 direction = vec3(0.0, 0.0, -1.0);
    hitValue = vec3(0.0);
    traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0,
                origin, 0.001, direction, 100.0, 0);
    imageStore(outImage, ivec2(gl_LaunchIDEXT.xy), vec4(hitValue, 1.0));
}
)GLSL";

constexpr std::string_view kMiss = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadInEXT vec3 hitValue;
void main() { hitValue = vec3(0.05, 0.05, 0.08); }
)GLSL";

constexpr std::string_view kClosestHit = R"GLSL(
#version 460
#extension GL_EXT_ray_tracing : require
layout(location = 0) rayPayloadInEXT vec3 hitValue;
hitAttributeEXT vec2 attribs;
void main() {
    hitValue = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
}
)GLSL";

std::unique_ptr<RenderContext> makeContext(bool enableRayTracing)
{
    RenderContextDesc desc{};
    desc.preferredBackend  = Backend::Vulkan;
    desc.validation        = ValidationLevel::Off;
    desc.enableMeshShaders = false;
    desc.enableRayTracing  = enableRayTracing;
    return RenderContext::create(desc);
}

struct RtShaders {
    ShaderHandle raygen;
    ShaderHandle miss;
    ShaderHandle closestHit;
};

RtShaders createRtShaders(IDevice& dev)
{
    ShaderDesc rg{}; rg.stage = ShaderStage::RayGen;     rg.glslSource = kRaygen;     rg.debugName = "rt.bringup.rgen";
    ShaderDesc ms{}; ms.stage = ShaderStage::Miss;       ms.glslSource = kMiss;       ms.debugName = "rt.bringup.rmiss";
    ShaderDesc ch{}; ch.stage = ShaderStage::ClosestHit; ch.glslSource = kClosestHit; ch.debugName = "rt.bringup.rchit";
    return { dev.createShader(rg), dev.createShader(ms), dev.createShader(ch) };
}

void destroyRtShaders(IDevice& dev, const RtShaders& s)
{
    if (s.raygen.valid())     dev.destroyShader(s.raygen);
    if (s.miss.valid())       dev.destroyShader(s.miss);
    if (s.closestHit.valid()) dev.destroyShader(s.closestHit);
}

} // namespace

TEST(VulkanRayTracingDispatch, ShadersCompileToModules)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext(/*enableRayTracing=*/false);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();
    ASSERT_EQ(dev.backend(), Backend::Vulkan);

    // GLSL->SPIR-V compilation and shader-module creation do not require the
    // device to support ray tracing — only well-formed RT SPIR-V.
    const RtShaders shaders = createRtShaders(dev);
    EXPECT_TRUE(shaders.raygen.valid());
    EXPECT_TRUE(shaders.miss.valid());
    EXPECT_TRUE(shaders.closestHit.valid());
    destroyRtShaders(dev, shaders);
}

TEST(VulkanRayTracingDispatch, RayTracingPipelineBuildsOnHardware)
{
    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = makeContext(/*enableRayTracing=*/true);
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Vulkan backend unavailable: " << e.what();
    }
    ASSERT_NE(ctx, nullptr);
    IDevice& dev = ctx->device();

    if (dev.caps().rayTracingTier < 2) {
        GTEST_SKIP() << "device lacks VK_KHR_ray_tracing_pipeline (tier 2); "
                        "RT pipeline + SBT build cannot be exercised here";
    }

    const RtShaders shaders = createRtShaders(dev);
    ASSERT_TRUE(shaders.raygen.valid());
    ASSERT_TRUE(shaders.miss.valid());
    ASSERT_TRUE(shaders.closestHit.valid());

    const std::array<ShaderHandle, 1> miss{ shaders.miss };
    const std::array<ShaderHandle, 1> hit{ shaders.closestHit };

    RayTracingPipelineDesc rtd{};
    rtd.rayGenShader      = shaders.raygen;
    rtd.missShaders       = miss;
    rtd.closestHitShaders = hit;
    rtd.maxRecursionDepth = 1;
    rtd.debugName         = "rt.bringup.pipeline";

    // Creating the pipeline also builds its shader binding table; a valid handle
    // means group-handle query + SBT layout + buffer fill all succeeded on hardware.
    const PipelineHandle pso = dev.createRayTracingPipeline(rtd);
    EXPECT_TRUE(pso.valid());

    if (pso.valid()) dev.destroyPipeline(pso);
    destroyRtShaders(dev, shaders);
}
