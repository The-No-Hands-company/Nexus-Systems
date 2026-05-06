// ─────────────────────────────────────────────────────────────────────────────
//  Tests: NullDevice — all resource operations must succeed without crashing
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <gtest/gtest.h>

using namespace nexus::gfx;

class NullDeviceTest : public ::testing::Test {
protected:
    std::unique_ptr<IDevice> dev;
    void SetUp() override {
        dev = createDevice(Backend::Null);
        ASSERT_NE(dev, nullptr);
    }
};

TEST_F(NullDeviceTest, BackendIdentity)
{
    EXPECT_EQ(dev->backend(), Backend::Null);
    EXPECT_EQ(dev->tier(),    HardwareTier::Low);
    EXPECT_EQ(dev->deviceName(), "NullDevice");
}

TEST_F(NullDeviceTest, BufferCreateDestroy)
{
    BufferDesc desc{ .sizeBytes = 1024, .usage = BufferUsage::VertexBuffer };
    auto h = dev->createBuffer(desc);
    EXPECT_TRUE(h.valid());
    dev->destroyBuffer(h);
}

TEST_F(NullDeviceTest, TextureCreateDestroy)
{
    TextureDesc desc{ .extent = {256, 256, 1}, .format = Format::R8G8B8A8_Unorm };
    auto h = dev->createTexture(desc);
    EXPECT_TRUE(h.valid());
    dev->destroyTexture(h);
}

TEST_F(NullDeviceTest, FenceLifecycle)
{
    auto fence = dev->createFence(false);
    EXPECT_TRUE(fence.valid());
    dev->waitForFence(fence, 0);   // must not block
    dev->resetFence  (fence);
    dev->destroyFence(fence);
}

TEST_F(NullDeviceTest, CommandBufferAllocFree)
{
    auto cmd = dev->allocateCommandBuffer(QueueType::Graphics);
    EXPECT_TRUE(cmd.valid());
    dev->freeCommandBuffer(cmd);
}

TEST_F(NullDeviceTest, WaitIdleNoCrash)
{
    dev->waitIdle();
}
