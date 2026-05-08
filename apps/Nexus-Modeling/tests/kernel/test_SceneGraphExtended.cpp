#include <nexus/render/SceneGraph.h>
#include <nexus/gfx/Device.h>
#include <gtest/gtest.h>

using namespace nexus::render;
using namespace nexus::gfx;

namespace {

class CountingDestroyDevice final : public IDevice {
public:
    CountingDestroyDevice()
    {
        m_inner = createDevice(Backend::Null);
    }

    [[nodiscard]] Backend backend() const noexcept override { return m_inner->backend(); }
    [[nodiscard]] const DeviceCapabilities& caps() const noexcept override { return m_inner->caps(); }
    [[nodiscard]] HardwareTier tier() const noexcept override { return m_inner->tier(); }
    [[nodiscard]] std::string_view deviceName() const noexcept override { return m_inner->deviceName(); }

    [[nodiscard]] BufferHandle createBuffer(const BufferDesc& desc) override { return m_inner->createBuffer(desc); }
    [[nodiscard]] TextureHandle createTexture(const TextureDesc& desc) override { return m_inner->createTexture(desc); }
    [[nodiscard]] ShaderHandle createShader(const ShaderDesc& desc) override { return m_inner->createShader(desc); }
    [[nodiscard]] RenderPassHandle createRenderPass(const RenderPassDesc& desc) override { return m_inner->createRenderPass(desc); }
    [[nodiscard]] PipelineHandle createGraphicsPipeline(const GraphicsPipelineDesc& desc) override { return m_inner->createGraphicsPipeline(desc); }
    [[nodiscard]] PipelineHandle createMeshShaderPipeline(const MeshShaderPipelineDesc& desc) override { return m_inner->createMeshShaderPipeline(desc); }
    [[nodiscard]] PipelineHandle createComputePipeline(const ComputePipelineDesc& desc) override { return m_inner->createComputePipeline(desc); }
    [[nodiscard]] PipelineHandle createRayTracingPipeline(const RayTracingPipelineDesc& desc) override { return m_inner->createRayTracingPipeline(desc); }
    [[nodiscard]] SamplerHandle createSampler(const SamplerDesc& desc) override { return m_inner->createSampler(desc); }
    [[nodiscard]] QueryPoolHandle createQueryPool(const QueryPoolDesc& desc) override { return m_inner->createQueryPool(desc); }
    void readbackTimestamps(QueryPoolHandle pool, uint32_t first, uint32_t count, uint64_t* outNanos) override {
        m_inner->readbackTimestamps(pool, first, count, outNanos);
    }

    void destroyBuffer(BufferHandle h) override { ++destroyBufferCalls; m_inner->destroyBuffer(h); }
    void destroyTexture(TextureHandle h) override { m_inner->destroyTexture(h); }
    void destroyShader(ShaderHandle h) override { m_inner->destroyShader(h); }
    void destroyRenderPass(RenderPassHandle h) override { m_inner->destroyRenderPass(h); }
    void destroyPipeline(PipelineHandle h) override { m_inner->destroyPipeline(h); }
    void destroySampler(SamplerHandle h) override { m_inner->destroySampler(h); }
    void destroyQueryPool(QueryPoolHandle h) override { m_inner->destroyQueryPool(h); }

    [[nodiscard]] CmdBufHandle allocateCommandBuffer(QueueType queue = QueueType::Graphics) override {
        return m_inner->allocateCommandBuffer(queue);
    }
    void freeCommandBuffer(CmdBufHandle h) override { m_inner->freeCommandBuffer(h); }
    [[nodiscard]] ICommandBuffer* getCommandBuffer(CmdBufHandle h) override { return m_inner->getCommandBuffer(h); }

    [[nodiscard]] FenceHandle createFence(bool signaled = false) override { return m_inner->createFence(signaled); }
    [[nodiscard]] SemaphoreHandle createSemaphore() override { return m_inner->createSemaphore(); }
    void destroyFence(FenceHandle h) override { m_inner->destroyFence(h); }
    void destroySemaphore(SemaphoreHandle h) override { m_inner->destroySemaphore(h); }
    void waitForFence(FenceHandle h, uint64_t timeoutNs = UINT64_MAX) override { m_inner->waitForFence(h, timeoutNs); }
    void resetFence(FenceHandle h) override { m_inner->resetFence(h); }

    void uploadBuffer(BufferHandle dst, const void* data, uint64_t sizeBytes, uint64_t dstOffset = 0) override {
        m_inner->uploadBuffer(dst, data, sizeBytes, dstOffset);
    }
    void uploadTexture(TextureHandle dst, const void* data, uint64_t sizeBytes) override {
        m_inner->uploadTexture(dst, data, sizeBytes);
    }

    void submit(QueueType queue,
                std::span<const CmdBufHandle> cmds,
                std::span<const SemaphoreHandle> waitSemaphores = {},
                std::span<const SemaphoreHandle> signalSemaphores = {},
                FenceHandle signalFence = {}) override {
        m_inner->submit(queue, cmds, waitSemaphores, signalSemaphores, signalFence);
    }

    void waitIdle() override { m_inner->waitIdle(); }

    [[nodiscard]] AccelStructHandle buildBLAS(BufferHandle vertexBuffer,
                                              BufferHandle indexBuffer,
                                              uint32_t vertexCount,
                                              uint32_t indexCount) override {
        return m_inner->buildBLAS(vertexBuffer, indexBuffer, vertexCount, indexCount);
    }
    [[nodiscard]] AccelStructHandle buildTLAS(std::span<const AccelStructHandle> blases) override {
        (void)blases;
        AccelStructHandle h{};
        h.id = nextAccelStructId++;
        return h;
    }
    void destroyAccelStruct(AccelStructHandle h) override {
        (void)h;
        ++destroyAccelStructCalls;
    }

    uint32_t destroyBufferCalls = 0;
    uint32_t destroyAccelStructCalls = 0;

private:
    std::unique_ptr<IDevice> m_inner;
    uint64_t nextAccelStructId = 1;
};

} // namespace

TEST(SceneGraphExtended, RemoveNodeErasesFromIdIndex)
{
    SceneGraph sg;
    auto* n = sg.createNode("to_remove");
    ASSERT_NE(n, nullptr);
    const auto id = n->id();
    ASSERT_NE(sg.findNode(id), nullptr);

    sg.removeNode(id);
    EXPECT_EQ(sg.findNode(id), nullptr);
}

TEST(SceneGraphExtended, RemoveNodeAlsoErasesDescendants)
{
    SceneGraph sg;
    auto* p = sg.createNode("parent");
    auto* c = sg.createNode("child", p);
    auto* g = sg.createNode("grandchild", c);

    const auto pid = p->id();
    const auto cid = c->id();
    const auto gid = g->id();

    sg.removeNode(pid);

    EXPECT_EQ(sg.findNode(pid), nullptr);
    EXPECT_EQ(sg.findNode(cid), nullptr);
    EXPECT_EQ(sg.findNode(gid), nullptr);
}

TEST(SceneGraphExtended, RemoveRootIsNoOp)
{
    SceneGraph sg;
    auto* a = sg.createNode("a");
    ASSERT_NE(a, nullptr);

    // Root id is 0 by construction.
    sg.removeNode(0);

    EXPECT_NE(sg.findNode(a->id()), nullptr);
    EXPECT_EQ(sg.findNode("__root__"), &sg.root());
}

TEST(SceneGraphExtended, RemoveNodeDetachesFromParentChildren)
{
    SceneGraph sg;
    auto* p = sg.createNode("parent");
    auto* c = sg.createNode("child", p);
    ASSERT_NE(c, nullptr);

    const auto before = p->children().size();
    sg.removeNode(c->id());
    const auto after = p->children().size();

    EXPECT_EQ(before, 1u);
    EXPECT_EQ(after, 0u);
}

TEST(SceneGraphExtended, RemoveNodeAutoDestroysGpuPayloadWhenEnabled)
{
    CountingDestroyDevice dev;
    SceneGraph sg;
    sg.enableAutoDestroyNodeGpuPayloadOnRemove(dev);

    auto* p = sg.createNode("parent");
    auto* c = sg.createNode("child", p);
    ASSERT_NE(p, nullptr);
    ASSERT_NE(c, nullptr);

    p->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    p->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    p->mesh.blas.id = 111;  // simulate valid RT handle

    c->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    c->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    c->mesh.blas.id = 222;  // simulate valid RT handle

    sg.removeNode(p->id());

    EXPECT_EQ(dev.destroyBufferCalls, 4u);
    EXPECT_EQ(dev.destroyAccelStructCalls, 2u);
}

TEST(SceneGraphExtended, RemoveNodeDoesNotDestroyGpuPayloadWhenDisabled)
{
    CountingDestroyDevice dev;
    SceneGraph sg;
    sg.enableAutoDestroyNodeGpuPayloadOnRemove(dev);
    sg.disableAutoDestroyNodeGpuPayloadOnRemove();

    auto* n = sg.createNode("node");
    ASSERT_NE(n, nullptr);

    n->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    n->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    n->mesh.blas.id = 333;

    sg.removeNode(n->id());

    EXPECT_EQ(dev.destroyBufferCalls, 0u);
    EXPECT_EQ(dev.destroyAccelStructCalls, 0u);
}

TEST(SceneGraphExtended, ClearRemovesAllNonRootNodes)
{
    SceneGraph sg;
    auto* a = sg.createNode("a");
    auto* b = sg.createNode("b");
    auto* c = sg.createNode("c", a);
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    const auto aId = a->id();
    const auto bId = b->id();
    const auto cId = c->id();

    sg.clear();

    EXPECT_EQ(sg.findNode(aId), nullptr);
    EXPECT_EQ(sg.findNode(bId), nullptr);
    EXPECT_EQ(sg.findNode(cId), nullptr);
    EXPECT_EQ(sg.root().children().size(), 0u);
    EXPECT_EQ(sg.findNode("__root__"), &sg.root());
}

TEST(SceneGraphExtended, ClearAutoDestroysGpuPayloadWhenEnabled)
{
    CountingDestroyDevice dev;
    SceneGraph sg;
    sg.enableAutoDestroyNodeGpuPayloadOnRemove(dev);

    auto* p = sg.createNode("parent");
    auto* c = sg.createNode("child", p);
    ASSERT_NE(p, nullptr);
    ASSERT_NE(c, nullptr);

    p->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    p->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    p->mesh.blas.id = 444;

    c->mesh.vertexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::VertexBuffer});
    c->mesh.indexBuffer = dev.createBuffer({.sizeBytes = 64, .usage = BufferUsage::IndexBuffer});
    c->mesh.blas.id = 555;

    sg.clear();

    EXPECT_EQ(dev.destroyBufferCalls, 4u);
    EXPECT_EQ(dev.destroyAccelStructCalls, 2u);
    EXPECT_EQ(sg.root().children().size(), 0u);
}

TEST(SceneGraphExtended, ClearAndDestroyTLASClearsNodesAndReleasesLiveTLAS)
{
    CountingDestroyDevice dev;
    SceneGraph sg;

    auto* n = sg.createNode("tlas-node");
    ASSERT_NE(n, nullptr);
    const auto nid = n->id();
    n->mesh.blas.id = 900;

    ASSERT_TRUE(sg.rebuildTLAS(dev));
    ASSERT_TRUE(sg.tlas().valid());

    sg.clearAndDestroyTLAS(dev);

    EXPECT_EQ(sg.findNode(nid), nullptr);
    EXPECT_EQ(sg.root().children().size(), 0u);
    EXPECT_FALSE(sg.tlas().valid());
    EXPECT_EQ(dev.destroyAccelStructCalls, 1u);
}

TEST(SceneGraphExtended, ClearAndDestroyTLASWithoutLiveTLASStillClearsNodes)
{
    CountingDestroyDevice dev;
    SceneGraph sg;
    auto* n = sg.createNode("node");
    ASSERT_NE(n, nullptr);
    const auto nid = n->id();

    sg.clearAndDestroyTLAS(dev);

    EXPECT_EQ(sg.findNode(nid), nullptr);
    EXPECT_EQ(sg.root().children().size(), 0u);
    EXPECT_EQ(dev.destroyAccelStructCalls, 0u);
}

TEST(SceneGraphExtended, CollectVisibleUsesScaleAwareRadius)
{
    SceneGraph sg;
    auto* n = sg.createNode("scaled");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 1;
    n->localTransform().translation = {-2.0f, 0.0f, 0.0f};
    n->localTransform().scale = {3.0f, 3.0f, 3.0f};

    Frustum f{};
    // Plane x >= 0 (inside toward +X); with radius=3 and center x=-2, node intersects.
    f.planes[0] = {1.0f, 0.0f, 0.0f, 0.0f};

    std::vector<Node*> out;
    sg.collectVisible(f, out);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], n);
}

TEST(SceneGraphExtended, CollectVisibleHandlesMirroredScaleConservatively)
{
    SceneGraph sg;
    auto* n = sg.createNode("mirrored");
    ASSERT_NE(n, nullptr);
    n->mesh.vertexBuffer.id = 2;
    n->localTransform().translation = {-2.0f, 0.0f, 0.0f};
    n->localTransform().scale = {-3.0f, 2.0f, 1.0f};

    Frustum f{};
    f.planes[0] = {1.0f, 0.0f, 0.0f, 0.0f};

    std::vector<Node*> out;
    sg.collectVisible(f, out);

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], n);
}
