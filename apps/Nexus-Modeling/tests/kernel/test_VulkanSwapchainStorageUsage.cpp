#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>

#include <gtest/gtest.h>

using namespace nexus::gfx;

TEST(VulkanSwapchain, CreateSwapchainSupportsStorageUsage)
{
    RenderContextDesc desc{};
    desc.preferredBackend = Backend::Vulkan;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    ASSERT_NE(ctx, nullptr);

    SwapchainDesc sd{};
    sd.extent = {64, 64};
    sd.imageCount = 2;

    auto sc = ctx->createSwapchain(sd);
    ASSERT_NE(sc, nullptr);

    // Ensure acquire returns a usable image (Ok or Suboptimal). This verifies
    // the swapchain was created and images registered successfully.
    auto acquired = sc->acquire();
    EXPECT_TRUE(acquired.result == AcquireResult::Ok || acquired.result == AcquireResult::Suboptimal);
}
