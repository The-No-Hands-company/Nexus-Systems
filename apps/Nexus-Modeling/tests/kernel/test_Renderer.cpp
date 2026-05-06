#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/render/Camera.h>
#include <nexus/gfx/RenderContext.h>
#include <gtest/gtest.h>

using namespace nexus::gfx;
using namespace nexus::render;

class RendererTest : public ::testing::Test {
protected:
    std::unique_ptr<RenderContext> ctx;
    std::unique_ptr<ISwapchain> swapchain;
    std::unique_ptr<Renderer> renderer;

    void SetUp() override
    {
        RenderContextDesc desc{};
        desc.preferredBackend = Backend::Null;
        desc.validation = ValidationLevel::Off;

        ctx = RenderContext::create(desc);
        ASSERT_NE(ctx, nullptr);

        SwapchainDesc sd{};
        sd.extent = {1280, 720};
        swapchain = ctx->createSwapchain(sd);
        ASSERT_NE(swapchain, nullptr);

        renderer = std::make_unique<Renderer>(*ctx, *swapchain);
        ASSERT_NE(renderer, nullptr);
    }
};

TEST_F(RendererTest, BeginRenderEndFrameNoSchedulerNoCrash)
{
    Camera cam;
    cam.setPerspective(70.f, 16.f / 9.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    SceneGraph sg;
    ASSERT_NO_THROW(renderer->beginFrame());
    ASSERT_NO_THROW(renderer->render(cam, sg));
    ASSERT_NO_THROW(renderer->endFrame());
}

TEST_F(RendererTest, SettingsDowngradePathTraceWhenNoRT)
{
    RendererSettings s{};
    s.mode = RenderMode::PathTrace;
    renderer->applySettings(s);

    EXPECT_EQ(renderer->settings().mode, RenderMode::Rasterize);
}

TEST_F(RendererTest, MaterialPipelineApiRoundTrip)
{
    PipelineHandle p{};
    p.id = 42;
    PipelineHandle shadowPipe{};
    shadowPipe.id = 43;
    SamplerHandle s{};
    s.id = 77;
    BufferHandle materialTable{};
    materialTable.id = 88;

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = s;
    bindings.normalSampler = s;
    bindings.velocitySampler = s;
    bindings.depthSampler = s;
    bindings.shadowDepthSampler = s;
    bindings.materialTable = materialTable;
    bindings.materialTableOffsetBytes = 64;

    ShadowLightingContract shadowContract{};
    shadowContract.cascadeCount = 2;
    shadowContract.cascadeSplits[0] = 0.1f;
    shadowContract.cascadeSplits[1] = 0.3f;

    ASSERT_NO_THROW(renderer->setFallbackGeometryPipeline(p));
    ASSERT_NO_THROW(renderer->setShadowPipeline(shadowPipe));
    ASSERT_NO_THROW(renderer->setMaterialPipeline(7, p));
    ASSERT_NO_THROW(renderer->setCompositeMaterialBindings(bindings));
    ASSERT_NO_THROW(renderer->setShadowLightingContract(shadowContract));
    ASSERT_NO_THROW(renderer->clearShadowLightingContract());
    ASSERT_NO_THROW(renderer->clearCompositeMaterialBindings());
    ASSERT_NO_THROW(renderer->clearMaterialPipelines());
}

TEST_F(RendererTest, OnResizeNoCrash)
{
    ASSERT_NO_THROW(renderer->onResize({1920, 1080}));
}
