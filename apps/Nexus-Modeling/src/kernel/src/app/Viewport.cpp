#include <nexus/app/Viewport.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadRenderBridge.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/SurfacePrimitives.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/render/Camera.h>
#include <nexus/gfx/Device.h>
#include <nexus/gfx/Types.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/gfx/FrameScheduler.h>
#include <nexus/gfx/CommandBuffer.h>
#include <nexus/app/ModelingApplication.h>
#include <nexus/app/ViewportGrid.h>
#include <nexus/app/TransformGizmo.h>
#include <nexus/app/SelectionHighlight.h>
#include <nexus/app/SketchPreview.h>
#include <nexus/parametric/ParametricSketchProfile.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "backend/vulkan/VulkanSwapchain.h"
#include "softrast/ImageWriter.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <utility>
#include <string_view>

namespace nexus::app {

// ──────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ──────────────────────────────────────────────────────────────────────────────

Viewport::Viewport(GLFWwindow* window, const ViewportConfig& config)
    : m_window(window)
    , m_config(config)
    , m_width(config.width)
    , m_height(config.height)
{
}

Viewport::~Viewport() {
    if (m_renderContext) {
        for (auto& [fid, uploaded] : m_uploadedMeshes) {
            nexus::geometry::GeometryRenderBridge::destroyUploadedMesh(m_renderContext->device(), uploaded);
        }
        m_uploadedMeshes.clear();
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Deferred pipeline setup — geometry (GBuffer) + fullscreen lighting composite.
// The renderer skips all draws without these; the composite writes the final
// swapchain colour. Samplers must be app-provided (the renderer doesn't make them).
// ──────────────────────────────────────────────────────────────────────────────
namespace {

bool setupDeferredPipelines(nexus::gfx::IDevice& dev, nexus::render::Renderer& renderer,
                            nexus::gfx::Format swapchainFormat) {
    using namespace nexus::gfx;
    namespace geom = nexus::geometry;

    // ── Geometry pass: transform position, write flat albedo into the GBuffer. ──
    static constexpr std::string_view kGeomVert = R"GLSL(
#version 460
layout(set = 0, binding = 0, std140) uniform Camera { mat4 view; mat4 projection; mat4 viewProj; } cam;
layout(location = 0) in vec3 inPos;
void main() { gl_Position = vec4(inPos, 1.0) * cam.viewProj; }
)GLSL";
    static constexpr std::string_view kGeomFrag = R"GLSL(
#version 460
layout(location = 0) out vec4 albedo;
layout(location = 1) out vec4 normal;
layout(location = 2) out vec4 velocity;
void main() {
    albedo   = vec4(0.75, 0.76, 0.80, 1.0);
    normal   = vec4(0.0, 0.0, 1.0, 0.0);
    velocity = vec4(0.0, 0.0, 0.0, 0.0);
}
)GLSL";

    ShaderDesc gvd{}; gvd.stage = ShaderStage::Vertex;   gvd.glslSource = kGeomVert;
    ShaderDesc gfd{}; gfd.stage = ShaderStage::Fragment; gfd.glslSource = kGeomFrag;
    ShaderHandle gvs = dev.createShader(gvd);
    ShaderHandle gfs = dev.createShader(gfd);
    if (!gvs.valid() || !gfs.valid()) {
        std::fprintf(stderr, "Viewport: geometry shader compile failed\n"); return false;
    }

    // Vertex-input layout derived from a representative mesh (all meshes share it).
    geom::Mesh sample = geom::primitives::makeBox(1.f, 1.f, 1.f);
    (void)sample.computeVertexNormals();
    const auto idx    = geom::MeshUploadContract::buildTriangulatedIndexBuffer(sample);
    const auto secs   = geom::MeshUploadContract::makeSingleSection(idx, nexus::render::kInvalidMaterial);
    const auto view   = geom::MeshUploadContract::buildView(sample, idx, secs);
    const auto packed = geom::MeshUploadContract::buildPackedVertexLayout(view);
    const auto vtx    = geom::MeshUploadContract::toGpuVertexInputLayout(packed);

    nexus::render::GBufferGeometryPipelineDesc gpd{};
    gpd.vertexShader     = gvs;
    gpd.fragmentShader   = gfs;
    gpd.vertexBindings   = vtx.bindings;
    gpd.vertexAttributes = vtx.attributes;
    gpd.cullMode         = CullMode::None;
    gpd.debugName        = "viewport.gbuffer.geometry";
    PipelineHandle geomPipe = nexus::render::Renderer::createGBufferGeometryPipeline(dev, gpd);
    if (!geomPipe.valid()) { std::fprintf(stderr, "Viewport: geometry pipeline failed\n"); return false; }
    renderer.setFallbackGeometryPipeline(geomPipe);

    // ── Composite pass: fullscreen triangle samples the GBuffer albedo. ─────────
    static constexpr std::string_view kCompVert = R"GLSL(
#version 460
layout(location = 0) out vec2 vUV;
void main() {
    vUV = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(vUV * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";
    static constexpr std::string_view kCompFrag = R"GLSL(
#version 460
layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;
layout(set = 0, binding = 0) uniform texture2D gAlbedo;
layout(set = 0, binding = 4) uniform sampler gSamp;
void main() { outColor = texture(sampler2D(gAlbedo, gSamp), vUV); }
)GLSL";

    ShaderDesc cvd{}; cvd.stage = ShaderStage::Vertex;   cvd.glslSource = kCompVert;
    ShaderDesc cfd{}; cfd.stage = ShaderStage::Fragment; cfd.glslSource = kCompFrag;
    ShaderHandle cvs = dev.createShader(cvd);
    ShaderHandle cfs = dev.createShader(cfd);
    if (!cvs.valid() || !cfs.valid()) {
        std::fprintf(stderr, "Viewport: composite shader compile failed\n"); return false;
    }

    nexus::render::LightingCompositePipelineDesc cpd{};
    cpd.vertexShader   = cvs;
    cpd.fragmentShader = cfs;
    cpd.colorFormat    = swapchainFormat;
    cpd.debugName      = "viewport.lighting.composite";
    PipelineHandle compPipe = nexus::render::Renderer::createLightingCompositePipeline(dev, cpd);
    if (!compPipe.valid()) { std::fprintf(stderr, "Viewport: composite pipeline failed\n"); return false; }
    renderer.setLightingCompositePipeline(compPipe);

    // GBuffer samplers (the renderer references these when binding the composite set).
    SamplerDesc sd{};
    nexus::render::CompositeMaterialBindings cb{};
    cb.albedoMaterialSampler = dev.createSampler(sd);
    cb.normalSampler         = dev.createSampler(sd);
    cb.velocitySampler       = dev.createSampler(sd);
    cb.depthSampler          = dev.createSampler(sd);
    renderer.setCompositeMaterialBindings(cb);
    return true;
}

}  // namespace

// ──────────────────────────────────────────────────────────────────────────────
// Initialization
// ──────────────────────────────────────────────────────────────────────────────

bool Viewport::initialize() {
    // Create render context
    nexus::gfx::RenderContextDesc desc;
    desc.preferredBackend = nexus::gfx::Backend::Vulkan;
    desc.validation = nexus::gfx::ValidationLevel::Off;
    desc.appName = "Nexus Modeling";
    desc.appVersion = 1;
    desc.enableHDR = false;

    m_renderContext = nexus::gfx::RenderContext::create(desc);
    if (!m_renderContext) {
        std::fprintf(stderr, "Viewport: RenderContext creation failed\n");
        return false;
    }

    // Create swapchain
    nexus::gfx::SwapchainDesc scDesc;
    scDesc.nativeWindowHandle = nullptr; // Will be set by platform-specific code
    scDesc.extent = nexus::gfx::Extent2D{m_width, m_height};
    scDesc.colorFormat = nexus::gfx::Format::B8G8R8A8_Srgb;
    scDesc.imageCount = 3;
    scDesc.presentMode = m_config.enableVSync ? nexus::gfx::PresentMode::Fifo : nexus::gfx::PresentMode::Mailbox;

    m_swapchain = m_renderContext->createSwapchain(scDesc);
    if (!m_swapchain) {
        std::fprintf(stderr, "Viewport: Swapchain creation failed\n");
        return false;
    }

    // Create renderer (no shadow pass — no shadow pipeline is bound).
    m_renderer = std::make_unique<nexus::render::Renderer>(*m_renderContext, *m_swapchain);
    nexus::render::RendererSettings rs = m_config.rendererSettings;
    rs.enableShadows = false;
    m_renderer->applySettings(rs);

    // Wire the frame scheduler so beginFrame/render/endFrame actually acquire,
    // record, submit (and present, once the swapchain has a window surface).
    m_frameScheduler = m_renderContext->createFrameScheduler(*m_swapchain);
    if (m_frameScheduler) m_renderer->setFrameScheduler(m_frameScheduler.get());

    // Build the deferred geometry + composite pipelines (else nothing is drawn).
    if (!setupDeferredPipelines(m_renderContext->device(), *m_renderer, m_swapchain->colorFormat())) {
        std::fprintf(stderr, "Viewport: deferred pipeline setup failed — viewport will clear only\n");
    }

    // Create scene graph
    m_sceneGraph = std::make_unique<nexus::render::SceneGraph>();

    // Configure camera
    m_camera.setPerspective(60.0f, float(m_width) / float(m_height), 0.1f, 10000.0f);
    m_camera.lookAt({0.f, 5.f, 10.f}, {0.f, 0.f, 0.f}, {0.f, 1.f, 0.f});

    std::printf("Viewport: Initialized (%ux%u)\n", m_width, m_height);
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// CadDocument synchronization
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::syncDocument(const nexus::cad::CadDocument& doc) {
    m_document = const_cast<nexus::cad::CadDocument*>(&doc);

    for (auto& [fid, uploaded] : m_uploadedMeshes) {
        nexus::geometry::GeometryRenderBridge::destroyUploadedMesh(m_renderContext->device(), uploaded);
    }
    m_uploadedMeshes.clear();
    m_featureNodes.clear();
    m_meshBVHs.clear();
    m_sceneGraph->clear();

    auto& history = doc.history();
    size_t fc = history.featureCount();
    for (FeatureId fid = 1; fid <= static_cast<FeatureId>(fc); ++fid) {
        auto* node = history.node(fid);
        if (!node || !node->mesh || node->deleted || node->hidden) continue;
        uploadMesh(*node->mesh, fid);
    }

    rebuildSceneGraph();
}

void Viewport::updateFeatureMesh(FeatureId fid, const nexus::geometry::Mesh& mesh) {
    auto it = m_uploadedMeshes.find(fid);
    if (it != m_uploadedMeshes.end()) {
        nexus::geometry::GeometryRenderBridge::destroyUploadedMesh(m_renderContext->device(), it->second);
        m_uploadedMeshes.erase(it);
    }
    m_meshBVHs.erase(fid);

    uploadMesh(mesh, fid);

    auto nodeIt = m_featureNodes.find(fid);
    if (nodeIt != m_featureNodes.end()) {
        nexus::geometry::GeometryRenderBridge::adoptUploadedMeshByNode(m_uploadedMeshes[fid], *nodeIt->second);
    }

    rebuildSceneGraph();
}

void Viewport::removeFeature(FeatureId fid) {
    auto it = m_uploadedMeshes.find(fid);
    if (it != m_uploadedMeshes.end()) {
        nexus::geometry::GeometryRenderBridge::destroyUploadedMesh(m_renderContext->device(), it->second);
        m_uploadedMeshes.erase(it);
    }
    m_meshBVHs.erase(fid);

    auto nodeIt = m_featureNodes.find(fid);
    if (nodeIt != m_featureNodes.end()) {
        nexus::geometry::GeometryRenderBridge::destroyNodeMeshPayload(m_renderContext->device(), *nodeIt->second);
        m_sceneGraph->removeNode(nodeIt->second->id());
        m_featureNodes.erase(nodeIt);
    }

    m_selectedIds.erase(std::remove(m_selectedIds.begin(), m_selectedIds.end(), fid), m_selectedIds.end());
    if (m_primarySelectedId == fid) m_primarySelectedId = nexus::parametric::kInvalidFeatureId;

    if (m_hoverState.hoveredId == fid) {
        m_hoverState = {};
    }

    rebuildSceneGraph();
}

void Viewport::setSelection(const std::vector<FeatureId>& selectedIds, FeatureId primaryId) {
    m_selectedIds = selectedIds;
    m_primarySelectedId = primaryId;
    updateSelectionHighlight();
}

// ──────────────────────────────────────────────────────────────────────────────
// Render loop
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::beginFrame() {
    m_renderer->beginFrame();
}

void Viewport::render() {
    m_renderer->render(m_camera, *m_sceneGraph);
}

void Viewport::endFrame() {
    m_renderer->endFrame();

    const auto& frameStats = m_renderer->lastFrameStats();
    m_stats.totalNodes = frameStats.totalNodes;
    m_stats.visibleNodes = frameStats.visibleNodes;
    m_stats.drawCalls = frameStats.drawCalls;
    m_stats.triangles = frameStats.triangles;
    m_stats.gpuTimeMs = frameStats.gpuTimeMs;
    m_stats.cpuCullTimeMs = frameStats.cpuCullTimeMs;
    m_stats.uploadTimeMs = m_lastUploadTime;
}

nexus::gfx::ICommandBuffer* Viewport::currentCommandBuffer() noexcept {
    if (!m_frameScheduler) return nullptr;
    auto frame = m_frameScheduler->getCurrentFrame();
    if (!frame) return nullptr;
    return frame->cmd;
}

// ──────────────────────────────────────────────────────────────────────────────
// Offscreen capture — read the last-rendered colour image back to a PNG.
// ──────────────────────────────────────────────────────────────────────────────
bool Viewport::captureToPNG(const std::string& path) const {
    if (!m_renderContext || !m_swapchain) return false;
    const auto handles = m_renderContext->device().nativeVulkanHandles();
    auto dev   = static_cast<VkDevice>(handles.device);
    auto phys  = static_cast<VkPhysicalDevice>(handles.physicalDevice);
    auto queue = static_cast<VkQueue>(handles.graphicsQueue);
    const uint32_t qfamily = handles.graphicsQueueFamily;
    if (!dev || !queue) return false;

    auto* sc = static_cast<nexus::gfx::VulkanSwapchain*>(m_swapchain.get());
    const uint32_t W = sc->extent().width, H = sc->extent().height;
    VkImage src = sc->image(sc->lastImageIndex());
    if (src == VK_NULL_HANDLE || W == 0 || H == 0) return false;

    vkDeviceWaitIdle(dev);

    // Host-visible readback buffer.
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(W) * H * 4u;
    VkBuffer buf = VK_NULL_HANDLE; VkDeviceMemory mem = VK_NULL_HANDLE;
    VkBufferCreateInfo bci{}; bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = bytes; bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT; bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(dev, &bci, nullptr, &buf) != VK_SUCCESS) return false;
    VkMemoryRequirements mr{}; vkGetBufferMemoryRequirements(dev, buf, &mr);
    VkPhysicalDeviceMemoryProperties mp{}; vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    const VkMemoryPropertyFlags want = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    uint32_t mt = 0;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((mr.memoryTypeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) { mt = i; break; }
    VkMemoryAllocateInfo mai{}; mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = mr.size; mai.memoryTypeIndex = mt;
    if (vkAllocateMemory(dev, &mai, nullptr, &mem) != VK_SUCCESS) { vkDestroyBuffer(dev, buf, nullptr); return false; }
    vkBindBufferMemory(dev, buf, mem, 0);

    // One-time command buffer: transition image → TRANSFER_SRC, copy to buffer.
    VkCommandPool pool = VK_NULL_HANDLE;
    VkCommandPoolCreateInfo pci{}; pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = qfamily; pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    vkCreateCommandPool(dev, &pci, nullptr, &pool);
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo cai{}; cai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cai.commandPool = pool; cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; cai.commandBufferCount = 1;
    vkAllocateCommandBuffers(dev, &cai, &cmd);
    VkCommandBufferBeginInfo cbi{}; cbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &cbi);

    VkImageMemoryBarrier b{}; b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    b.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // scheduler's final colour layout
    b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    b.srcAccessMask = 0; b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    b.image = src; b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &b);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {W, H, 1};
    vkCmdCopyImageToBuffer(cmd, src, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf, 1, &region);
    vkEndCommandBuffer(cmd);

    VkFence fence = VK_NULL_HANDLE; VkFenceCreateInfo fci{}; fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(dev, &fci, nullptr, &fence);
    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; si.commandBufferCount = 1; si.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &si, fence);
    vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);

    // Map and convert (B8G8R8A8 → RGB). SRGB bytes are already display-encoded.
    void* data = nullptr; vkMapMemory(dev, mem, 0, bytes, 0, &data);
    const auto* px = static_cast<const uint8_t*>(data);
    const bool bgra = (sc->colorFormat() == nexus::gfx::Format::B8G8R8A8_Srgb ||
                       sc->colorFormat() == nexus::gfx::Format::B8G8R8A8_Unorm);
    nexus::softrast::PixelBuffer image(W, H);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const size_t o = (static_cast<size_t>(y) * W + x) * 4u;
            uint8_t r = px[o + 0], g = px[o + 1], bl = px[o + 2];
            if (bgra) std::swap(r, bl);
            image.setPixel(x, y, {r, g, bl, 255});
        }
    vkUnmapMemory(dev, mem);
    const bool ok = nexus::softrast::writePNG(path, image);

    vkDestroyFence(dev, fence, nullptr);
    vkDestroyCommandPool(dev, pool, nullptr);
    vkDestroyBuffer(dev, buf, nullptr);
    vkFreeMemory(dev, mem, nullptr);
    return ok;
}

// ──────────────────────────────────────────────────────────────────────────────
// Resize
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::onResize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    if (m_swapchain) {
        m_swapchain->resize(nexus::gfx::Extent2D{width, height});
    }
    m_renderer->onResize(nexus::gfx::Extent2D{width, height});
}

// ──────────────────────────────────────────────────────────────────────────────
// Mesh upload
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::uploadMesh(const nexus::geometry::Mesh& meshIn, FeatureId fid) {
    if (!meshIn.isValid()) return;

    // Ensure every uploaded mesh carries normals so the packed vertex layout is
    // consistent with the fallback geometry pipeline (position + normal). Without
    // this, a normal-less mesh yields a shorter stride than the pipeline expects
    // and the GPU reads past the vertex buffer (crash).
    nexus::geometry::Mesh meshCopy;
    const nexus::geometry::Mesh* meshPtr = &meshIn;
    if (!meshIn.attributes().hasNormals()) {
        meshCopy = meshIn;
        (void)meshCopy.computeVertexNormals();
        meshPtr = &meshCopy;
    }
    const nexus::geometry::Mesh& mesh = *meshPtr;

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<uint32_t> indices = nexus::geometry::MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    if (indices.empty()) return;

    nexus::render::MaterialID matId = static_cast<nexus::render::MaterialID>(fid);
    std::vector<nexus::geometry::MeshSection> sections = nexus::geometry::MeshUploadContract::makeSingleSection(indices, matId);

    nexus::geometry::MeshUploadView view = nexus::geometry::MeshUploadContract::buildView(mesh, indices, sections);
    auto validation = nexus::geometry::MeshUploadContract::validateView(view);
    if (!validation.valid) {
        std::fprintf(stderr, "Viewport: Mesh upload validation failed for feature %u\n", fid);
        return;
    }

    nexus::geometry::PackedVertexLayout layout = nexus::geometry::MeshUploadContract::buildPackedVertexLayout(view);

    nexus::geometry::UploadedGeometryMesh uploaded;
    auto report = nexus::geometry::GeometryRenderBridge::uploadToDevice(
        m_renderContext->device(), view, layout, sections,
        nexus::geometry::GeometryUploadOptions{true}, uploaded);

    if (!report.valid) {
        std::fprintf(stderr, "Viewport: GPU upload failed for feature %u\n", fid);
        return;
    }

    m_uploadedMeshes[fid] = std::move(uploaded);
    m_featureNodes[fid] = nullptr; // Will be populated in rebuildSceneGraph

    // Build BVH for picking
    auto bvh = std::make_unique<nexus::geometry::MeshBVH>();
    bvh->build(mesh);
    if (bvh) {
        m_meshBVHs[fid] = std::move(bvh);
    }

    auto end = std::chrono::high_resolution_clock::now();
    m_lastUploadTime = std::chrono::duration<double, std::milli>(end - start).count();
}

// ──────────────────────────────────────────────────────────────────────────────
// SceneGraph rebuild
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::rebuildSceneGraph() {
    m_sceneGraph->clear();
    m_featureNodes.clear();

    if (!m_document) return;

    auto& history = m_document->history();
    size_t fc = history.featureCount();

    for (FeatureId fid = 1; fid <= static_cast<FeatureId>(fc); ++fid) {
        auto* node = history.node(fid);
        if (!node || !node->mesh || node->deleted || node->hidden) continue;

        auto name = node->name.empty() ? ("Feature_" + std::to_string(fid)) : node->name;

        auto* sceneNode = m_sceneGraph->createNode(name);
        if (!sceneNode) continue;

        sceneNode->parametricBindingId = static_cast<nexus::parametric::ParametricEntityId>(fid);

        // Assign GPU mesh
        auto it = m_uploadedMeshes.find(fid);
        if (it != m_uploadedMeshes.end()) {
            nexus::geometry::GeometryRenderBridge::adoptUploadedMeshByNode(it->second, *sceneNode);
        }

        // Local bounds for frustum culling
        auto bounds = node->mesh->computeBounds();
        if (bounds.min != bounds.max) {
            sceneNode->setLocalBounds(bounds);
        }

        m_featureNodes[fid] = sceneNode;
    }

    // Rebuild TLAS
    if (m_renderContext && m_renderContext->device().caps().rayTracingTier > 0) {
        m_sceneGraph->rebuildTLAS(m_renderContext->device());
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Selection highlight
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::updateSelectionHighlight() {
    for (auto& [fid, sceneNode] : m_featureNodes) {
        bool isSelected = std::find(m_selectedIds.begin(), m_selectedIds.end(), fid) != m_selectedIds.end();
        (void)isSelected;
        bool isPrimary = (fid == m_primarySelectedId);
        (void)isPrimary;
        // Selection highlight handled by renderer's debug overlay
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// Picking / Hover
// ──────────────────────────────────────────────────────────────────────────────

PickingResult Viewport::pick(float ndcX, float ndcY) const {
    PickingResult result;
    result.hit = false;

    nexus::render::Vec3 origin, dir;
    screenToWorldRay(ndcX, ndcY, origin, dir);

    float bestT = 1e30f;
    FeatureId bestFid = nexus::parametric::kInvalidFeatureId;
    uint32_t bestFace = ~0u, bestVert = ~0u;
    nexus::render::Vec3 bestHit{};

    if (!m_document) return result;

    auto& history = m_document->history();
    size_t fc = history.featureCount();

    for (FeatureId fid = 1; fid <= static_cast<FeatureId>(fc); ++fid) {
        auto* node = history.node(fid);
        if (!node || !node->mesh || node->deleted || node->hidden) continue;

        float t;
        uint32_t faceIdx, vertIdx;
        nexus::render::Vec3 hit;
        if (intersectMesh(*node->mesh, origin, dir, t, faceIdx, vertIdx, hit)) {
            if (t < bestT) {
                bestT = t;
                bestFid = fid;
                bestFace = faceIdx;
                bestVert = vertIdx;
                bestHit = hit;
            }
        }
    }

    if (bestFid != nexus::parametric::kInvalidFeatureId) {
        result.hit = true;
        result.featureId = bestFid;
        result.faceIndex = bestFace;
        result.vertexIndex = bestVert;
        result.hitPoint = bestHit;
        result.depth = bestT;
    }
    return result;
}

void Viewport::updateHover(float ndcX, float ndcY) {
    auto result = pick(ndcX, ndcY);
    m_hoverState.valid = result.hit;
    m_hoverState.hoveredId = result.featureId;
}

// ──────────────────────────────────────────────────────────────────────────────
// Screen-space to world ray
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::screenToWorldRay(float ndcX, float ndcY, nexus::render::Vec3& outOrigin, nexus::render::Vec3& outDir) const {
    const auto& ubo = m_camera.ubo();
    const nexus::render::Mat4& invViewProj = ubo.invViewProj;

    nexus::render::Vec4 nearP = invViewProj * nexus::render::Vec4{ndcX, ndcY, -1.f, 1.f};
    nexus::render::Vec4 farP  = invViewProj * nexus::render::Vec4{ndcX, ndcY,  1.f, 1.f};
    nearP = nearP * (1.f / nearP.w);
    farP  = farP  * (1.f / farP.w);

    outOrigin = {nearP.x, nearP.y, nearP.z};
    outDir = {farP.x - nearP.x, farP.y - nearP.y, farP.z - nearP.z};
    outDir = outDir.normalize();
}

// ──────────────────────────────────────────────────────────────────────────────
// Mesh intersection
// ──────────────────────────────────────────────────────────────────────────────

bool Viewport::intersectMesh(const nexus::geometry::Mesh& mesh,
                             const nexus::render::Vec3& origin,
                             const nexus::render::Vec3& dir,
                             float& outT,
                             uint32_t& outFace,
                             uint32_t& outVert,
                             nexus::render::Vec3& outHit) const {
    const auto& pos = mesh.attributes().positions();
    const auto& topo = mesh.topology();

    float bestT = 1e30f;
    uint32_t bestFace = ~0u, bestVert = ~0u;
    nexus::render::Vec3 bestHit{};

    for (uint32_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        if (face.vertexCount() < 3) continue;

        for (size_t j = 0; j + 2 < face.vertexCount(); ++j) {
            uint32_t i0 = face.indices[0];
            uint32_t i1 = face.indices[j + 1];
            uint32_t i2 = face.indices[j + 2];
            if (i0 >= pos.size() || i1 >= pos.size() || i2 >= pos.size()) continue;

            const auto& v0 = pos[i0];
            const auto& v1 = pos[i1];
            const auto& v2 = pos[i2];

            nexus::render::Vec3 e1 = v1 - v0;
            nexus::render::Vec3 e2 = v2 - v0;
            nexus::render::Vec3 h = dir.cross(e2);
            float a = e1.dot(h);
            if (std::fabs(a) < 1e-7f) continue;

            float f = 1.f / a;
            nexus::render::Vec3 s = origin - v0;
            float u = f * s.dot(h);
            if (u < 0.f || u > 1.f) continue;

            nexus::render::Vec3 q = s.cross(e1);
            float v = f * dir.dot(q);
            if (v < 0.f || u + v > 1.f) continue;

            float t = f * e2.dot(q);
            if (t < 1e-4f || t >= bestT) continue;

            bestT = t;
            bestFace = fi;
            bestHit = origin + dir * t;

            // Find closest vertex
            bestVert = face.indices[0];
            float bestVDist = (pos[bestVert] - bestHit).lengthSq();
            for (size_t k = 1; k < face.vertexCount(); ++k) {
                float d2 = (pos[face.indices[k]] - bestHit).lengthSq();
                if (d2 < bestVDist) { bestVDist = d2; bestVert = face.indices[k]; }
            }
        }
    }

    if (bestFace != ~0u) {
        outT = bestT;
        outFace = bestFace;
        outVert = bestVert;
        outHit = bestHit;
        return true;
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────────────
// Work plane intersection
// ──────────────────────────────────────────────────────────────────────────────

bool Viewport::intersectWorkPlane(float ndcX, float ndcY, nexus::render::Vec3& outPos) const {
    nexus::render::Vec3 origin, dir;
    screenToWorldRay(ndcX, ndcY, origin, dir);

    nexus::render::Vec3 planeN{0, 1, 0}; // XZ default
    if (m_workPlane == WorkPlane::XY) planeN = {0, 0, 1};
    else if (m_workPlane == WorkPlane::YZ) planeN = {1, 0, 0};

    float denom = dir.dot(planeN);
    if (std::fabs(denom) < 1e-6f) return false;

    float t = -origin.dot(planeN) / denom;
    if (t < 0) return false;

    outPos = origin + dir * t;
    return true;
}

// ──────────────────────────────────────────────────────────────────────────────
// Gizmo
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::gizmoBeginDrag(const nexus::render::Vec3& worldPos, TransformGizmo::Axis axis) {
    m_gizmoActive = true;
    m_gizmoDragAxis = axis;
    m_gizmoStartWorldPos = worldPos;
    m_gizmoSavedMeshes.clear();

    for (FeatureId fid : m_selectedIds) {
        auto nodeIt = m_featureNodes.find(fid);
        if (nodeIt != m_featureNodes.end() && nodeIt->second && nodeIt->second->mesh.indexBuffer.valid()) {
            if (m_document) {
                auto* node = m_document->history().node(fid);
                if (node && node->mesh) {
                    m_gizmoSavedMeshes.push_back(std::make_unique<nexus::geometry::Mesh>(*node->mesh));
                }
            }
        }
    }
}

void Viewport::gizmoDrag(const nexus::render::Vec3& worldPos) {
    if (!m_gizmoActive) return;

    nexus::render::Vec3 delta = worldPos - m_gizmoStartWorldPos;
    for (size_t i = 0; i < m_selectedIds.size(); ++i) {
        FeatureId fid = m_selectedIds[i];
        auto nodeIt = m_featureNodes.find(fid);
        if (nodeIt != m_featureNodes.end() && nodeIt->second) {
            m_gizmo.translate(nodeIt->second->localBounds.center(), m_gizmoDragAxis,
                              (m_gizmoDragAxis == TransformGizmo::Axis::X) ? delta.x :
                              (m_gizmoDragAxis == TransformGizmo::Axis::Y) ? delta.y : delta.z,
                              *m_document, fid, false);
        }
    }
    m_gizmoStartWorldPos = worldPos;
}

void Viewport::gizmoEndDrag() {
    if (!m_gizmoActive) return;
    m_gizmoActive = false;

// Create undo commands
    if (m_document && !m_gizmoSavedMeshes.empty()) {
        for (size_t i = 0; i < m_selectedIds.size() && i < m_gizmoSavedMeshes.size(); ++i) {
            FeatureId fid = m_selectedIds[i];
            auto cmd = std::make_unique<nexus::cad::TransformCommand>(fid, std::move(*m_gizmoSavedMeshes[i]));
            m_document->executeCommand(std::move(cmd));
        }
    }
    m_gizmoSavedMeshes.clear();
}

// ──────────────────────────────────────────────────────────────────────────────
// Sketch preview
// ──────────────────────────────────────────────────────────────────────────────

void Viewport::setSketchPreview(const nexus::cad::CadAutoConstraintSketch* sketch) {
    m_sketchPreviewSource = sketch;
}

} // namespace nexus::app