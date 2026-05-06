#include <nexus/gfx/Device.h>
#include <nexus/gfx/CommandBuffer.h>
#include <gtest/gtest.h>

using namespace nexus::gfx;

class NullDeviceExtendedTest : public ::testing::Test {
protected:
    std::unique_ptr<IDevice> dev;

    void SetUp() override
    {
        dev = createDevice(Backend::Null);
        ASSERT_NE(dev, nullptr);
    }
};

TEST_F(NullDeviceExtendedTest, SamplerLifecycle)
{
    SamplerDesc s{};
    auto h = dev->createSampler(s);
    EXPECT_TRUE(h.valid());
    dev->destroySampler(h);
}

TEST_F(NullDeviceExtendedTest, QueryPoolLifecycleAndReadback)
{
    QueryPoolDesc q{};
    q.count = 8;
    auto h = dev->createQueryPool(q);
    EXPECT_TRUE(h.valid());

    uint64_t out[1] = {12345};
    dev->readbackTimestamps(h, 0, 1, out);
    EXPECT_EQ(out[0], 0u);

    dev->destroyQueryPool(h);
}

TEST_F(NullDeviceExtendedTest, CommandBufferFineGrainedBarrierAndTimestampApisNoCrash)
{
    auto h = dev->allocateCommandBuffer(QueueType::Graphics);
    ASSERT_TRUE(h.valid());
    auto* cb = dev->getCommandBuffer(h);
    ASSERT_NE(cb, nullptr);

    TextureBarrier tb{};
    BufferBarrier bb{};
    cb->textureBarrier(tb);
    cb->textureBarriers({&tb, 1});
    cb->bufferBarrier(bb);
    cb->bufferBarriers({&bb, 1});
    cb->resetQueryPool({}, 0, 1);
    cb->writeTimestamp({}, 0);

    dev->freeCommandBuffer(h);
}

TEST_F(NullDeviceExtendedTest, PipelineFactoriesReturnValidHandles)
{
    ShaderHandle fakeShader{};
    fakeShader.id = 1;

    GraphicsPipelineDesc g{};
    g.vertexShader   = fakeShader;
    g.fragmentShader = fakeShader;

    ComputePipelineDesc c{};
    c.computeShader = fakeShader;

    MeshShaderPipelineDesc m{};
    m.meshShader     = fakeShader;
    m.fragmentShader = fakeShader;

    RayTracingPipelineDesc rt{};
    rt.rayGenShader = fakeShader;

    EXPECT_TRUE(dev->createGraphicsPipeline(g).valid());
    EXPECT_TRUE(dev->createComputePipeline(c).valid());
    EXPECT_TRUE(dev->createMeshShaderPipeline(m).valid());
    EXPECT_TRUE(dev->createRayTracingPipeline(rt).valid());
}
