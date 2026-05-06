// ─────────────────────────────────────────────────────────────────────────────
//  Tests: RenderContext (null backend — no GPU required)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/RenderContext.h>
#include <gtest/gtest.h>

using namespace nexus::gfx;

TEST(RenderContext, CreateWithNullBackend)
{
    RenderContextDesc desc;
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(ctx->activeBackend(), Backend::Null);
}

TEST(RenderContext, DeviceAccessible)
{
    RenderContextDesc desc;
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    EXPECT_EQ(ctx->device().backend(), Backend::Null);
}

TEST(RenderContext, AllocatorAccessible)
{
    RenderContextDesc desc;
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    // Null allocator always returns 0
    EXPECT_EQ(ctx->allocator().budgetBytes(), 0u);
}

TEST(RenderContext, SwapchainCreatable)
{
    RenderContextDesc desc;
    desc.preferredBackend = Backend::Null;
    desc.validation       = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);

    SwapchainDesc sd;
    sd.extent = {1920, 1080};
    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);
    EXPECT_EQ(sc->extent().width, 1920u);
}
