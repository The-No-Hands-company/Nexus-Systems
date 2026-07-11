#include <nexus/app/EditorUI.h>
#include <nexus/cad/CadCommand.h>

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/MeshDecimator.h>
#include <nexus/geometry/MeshLaplacian.h>
#include <nexus/geometry/SubdivisionSurface.h>
#include <nexus/geometry/DirectModeling.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include <nexus/parametric/ParametricSketchProfile.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/geometry/SurfacePrimitives.h>
#include <nexus/geometry/MeshBoolean.h>
#include <nexus/geometry/SurfaceOffset.h>
#include <nexus/geometry/FaceFillet.h>
#include <nexus/geometry/DirectModeling.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include "backend/vulkan/VulkanDevice.h"
#include "backend/vulkan/VulkanCommandBuffer.h"
#include "backend/vulkan/VulkanSwapchain.h"

#include <cstdio>
#include <vector>

namespace nexus::app {

using FeatureId = nexus::parametric::FeatureId;
using Vec3 = nexus::render::Vec3;

static nexus::gfx::RenderContext* g_renderContext = nullptr;
static nexus::gfx::ISwapchain* g_swapchain = nullptr;
static VkDescriptorPool g_imguiDescriptorPool = VK_NULL_HANDLE;
static VkRenderPass g_imguiRenderPass = VK_NULL_HANDLE;
static std::vector<VkFramebuffer> g_imguiFramebuffers;
static VkExtent2D g_imguiExtent = {0, 0};

static VkRenderPass createImGuiRenderPass(VkDevice device, VkFormat colorFormat) {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = colorFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // The 3D scene leaves the swapchain image in PRESENT_SRC (direct render path);
    // loadOp=LOAD preserves it and we overlay the UI, ending back in PRESENT_SRC.
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkRenderPass renderPass;
    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        fprintf(stderr, "Failed to create ImGui render pass\n");
        return VK_NULL_HANDLE;
    }
    return renderPass;
}

void EditorUI::initialize(GLFWwindow* w) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForVulkan(w, true);
    ImGui::StyleColorsDark();
}

void EditorUI::initializeVulkan(nexus::gfx::RenderContext* renderContext, nexus::gfx::ISwapchain* swapchain) {
    g_renderContext = renderContext;
    g_swapchain = swapchain;

    // Get Vulkan handles from the RenderContext
    auto* vulkanDevice = dynamic_cast<nexus::gfx::VulkanDevice*>(&renderContext->device());
    if (!vulkanDevice) {
        fprintf(stderr, "EditorUI::initializeVulkan: RenderContext is not using Vulkan backend\n");
        return;
    }

    VkInstance instance = vulkanDevice->instance();
    VkPhysicalDevice physicalDevice = vulkanDevice->physical();
    VkDevice device = vulkanDevice->logical();
    uint32_t queueFamily = vulkanDevice->queueFamily(nexus::gfx::QueueType::Graphics);
    VkQueue queue = vulkanDevice->queue(nexus::gfx::QueueType::Graphics);
    VkFormat colorFormat = static_cast<VkFormat>(swapchain->colorFormat());
    uint32_t imageCount = swapchain->imageCount();
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
    poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    VkResult err = vkCreateDescriptorPool(device, &poolInfo, nullptr, &g_imguiDescriptorPool);
    if (err != VK_SUCCESS) {
        fprintf(stderr, "EditorUI::initializeVulkan: vkCreateDescriptorPool failed: %d\n", err);
        return;
    }

// Initialize ImGui Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo = {};
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamily;
    initInfo.Queue = queue;
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = g_imguiDescriptorPool;
    initInfo.Subpass = 0;
    initInfo.MinImageCount = imageCount;
    initInfo.ImageCount = imageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Allocator = nullptr;
    initInfo.CheckVkResultFn = nullptr;

    VkRenderPass renderPass = createImGuiRenderPass(device, colorFormat);
    initInfo.RenderPass = renderPass;   // was previously created but never assigned
    ImGui_ImplVulkan_Init(&initInfo);
    g_imguiRenderPass = renderPass;

    // Framebuffers over each swapchain image view, for the UI overlay pass that
    // draws ImGui on top of the rendered 3D scene each frame.
    if (auto* vkSc = dynamic_cast<nexus::gfx::VulkanSwapchain*>(swapchain);
        vkSc && renderPass != VK_NULL_HANDLE) {
        const auto ext = swapchain->extent();
        g_imguiExtent = VkExtent2D{ext.width, ext.height};
        g_imguiFramebuffers.assign(imageCount, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < imageCount; ++i) {
            VkImageView view = vkSc->imageView(i);
            if (view == VK_NULL_HANDLE) continue;
            VkFramebufferCreateInfo fci{};
            fci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fci.renderPass      = renderPass;
            fci.attachmentCount = 1;
            fci.pAttachments    = &view;
            fci.width           = ext.width;
            fci.height          = ext.height;
            fci.layers          = 1;
            if (vkCreateFramebuffer(device, &fci, nullptr, &g_imguiFramebuffers[i]) != VK_SUCCESS) {
                fprintf(stderr, "EditorUI: failed to create ImGui framebuffer %u\n", i);
            }
        }
    }

    // Upload fonts
    ImGui_ImplVulkan_CreateFontsTexture();
}

void EditorUI::shutdown() {
    if (g_imguiDescriptorPool != VK_NULL_HANDLE) {
        auto* vulkanDevice = dynamic_cast<nexus::gfx::VulkanDevice*>(&g_renderContext->device());
        if (vulkanDevice) {
            VkDevice dev = vulkanDevice->logical();
            for (VkFramebuffer fb : g_imguiFramebuffers) {
                if (fb != VK_NULL_HANDLE) vkDestroyFramebuffer(dev, fb, nullptr);
            }
            g_imguiFramebuffers.clear();
            if (g_imguiRenderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(dev, g_imguiRenderPass, nullptr);
                g_imguiRenderPass = VK_NULL_HANDLE;
            }
            vkDestroyDescriptorPool(dev, g_imguiDescriptorPool, nullptr);
        }
    }
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void EditorUI::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::endFrame(nexus::gfx::ICommandBuffer* cmd) {
    ImGui::Render();
    VkCommandBuffer vkCmd = static_cast<nexus::gfx::VulkanCommandBuffer*>(cmd)->handle();

    // Overlay the UI onto the current swapchain image inside a load-not-clear
    // render pass, so ImGui composites on top of the already-rendered 3D scene.
    auto* vkSc = dynamic_cast<nexus::gfx::VulkanSwapchain*>(g_swapchain);
    if (g_imguiRenderPass != VK_NULL_HANDLE && vkSc) {
        const uint32_t idx = vkSc->lastImageIndex();
        if (idx < g_imguiFramebuffers.size() && g_imguiFramebuffers[idx] != VK_NULL_HANDLE) {
            VkRenderPassBeginInfo rpbi{};
            rpbi.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpbi.renderPass        = g_imguiRenderPass;
            rpbi.framebuffer       = g_imguiFramebuffers[idx];
            rpbi.renderArea.offset = {0, 0};
            rpbi.renderArea.extent = g_imguiExtent;
            rpbi.clearValueCount   = 0;
            vkCmdBeginRenderPass(vkCmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmd);
            vkCmdEndRenderPass(vkCmd);
            return;
        }
    }
    // Fallback (no swapchain framebuffers, e.g. headless): record bare.
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), vkCmd);
}

// Helper: place a primitive mesh into the document as a feature.
static void placePrimitive(AppContext& ctx, int primType) {
    using namespace nexus::geometry;
    Mesh prim;
    const char* name = "?";
    switch(primType) {
        case 0: prim = primitives::makeBox(2,2,2);     name="Box"; break;
        case 1: prim = primitives::makeSphere(1.5f);   name="Sphere"; break;
        case 2: prim = primitives::makeCylinder(1,3);  name="Cylinder"; break;
        case 3: prim = primitives::makeCone(1,3);      name="Cone"; break;
        case 4: prim = primitives::makeTorus(2,0.5f);  name="Torus"; break;
        case 5: prim = primitives::makePlane(4,4);     name="Plane"; break;
    }
    if(!prim.isValid() || !ctx.document) return;
    // Position at cursor world position.
    auto pos = prim.attributes().positions();
    auto& cwp = ctx.cursorWorldPos;
    for(auto& v : pos) { v.x += cwp.x; v.y += cwp.y; v.z += cwp.z; }
    prim.attributes().setPositions(std::move(pos));
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = ctx.document->history().node(fid);
    if(node) { node->mesh.emplace(std::move(prim)); node->dirty = false; }
    ctx.activeSelectedFeature = fid;
    // Note: main.cpp's AppState.selectedId will sync on next click via auto-select
    if (ctx.orchestrator) ctx.orchestrator->switchTo("select");
    printf("Placed %s at (%.2f, %.2f, %.2f)\n", name, cwp.x, cwp.y, cwp.z);
}


// ── Import / Export ──────────────────────────────────────────────────────────
// All mesh interchange routes through the kernel's MeshIO (OBJ/STL/PLY/glTF),
// which is the tested, hardened path — the editor is just the trigger.

// Combines every visible, non-deleted feature mesh into a single mesh for export.
static bool buildCombinedMesh(AppContext& ctx, geometry::Mesh& out) {
    if(!ctx.document) return false;
    auto& hist = ctx.document->history();
    bool any = false;
    for(parametric::FeatureId i=1; i<=static_cast<parametric::FeatureId>(hist.featureCount()); ++i) {
        auto* n = hist.node(i);
        if(!n || !n->mesh || n->deleted || n->hidden) continue;
        if(!any) { out = *n->mesh; any = true; }
        else     { (void)out.appendMesh(*n->mesh); }
    }
    return any;
}

static void exportMeshFile(AppContext& ctx, geometry::MeshExportFormat fmt, const char* path) {
    geometry::Mesh m;
    if(!buildCombinedMesh(ctx, m)) { printf("Export: nothing visible to export\n"); return; }
    geometry::MeshExportOptions opts; opts.format = fmt;
    const auto rep = geometry::MeshIO::exportMesh(m, path, opts);
    if(rep.valid) printf("Exported %u verts / %u faces -> %s\n", rep.verticesWritten, rep.facesWritten, path);
    else printf("Export failed (%s): %s\n", path, rep.messages.empty() ? "" : rep.messages.front().c_str());
}

static void importMeshFile(AppContext& ctx, const char* path) {
    if(!ctx.document) return;
    geometry::Mesh mesh;
    geometry::MeshImportOptions opts;  // format auto-detected from the extension
    const auto rep = geometry::MeshIO::importMesh(path, opts, mesh);
    if(!rep.valid) { printf("Import failed (%s): %s\n", path, rep.messages.empty() ? "" : rep.messages.front().c_str()); return; }
    auto sk  = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = ctx.document->history().node(fid);
    if(node) { node->mesh.emplace(std::move(mesh)); node->dirty = false; }
    printf("Imported %u verts / %u faces (%s) -> feature %u\n", rep.verticesRead, rep.facesRead, path, fid);
}

static void mirrorSelected(AppContext& ctx, FeatureId selectedId, int axis) {
    if(!ctx.document || selectedId == nexus::parametric::kInvalidFeatureId) return;
    auto* n = ctx.document->history().node(selectedId);
    if(!n || !n->mesh) return;
    auto pos = n->mesh->attributes().positions();
    for(auto& v : pos) {
        if(axis == 0) v.x = -v.x;
        else if(axis == 1) v.y = -v.y;
        else v.z = -v.z;
    }
    n->mesh->attributes().setPositions(std::move(pos));
    printf("Mirrored across %s plane\n", axis==0?"YZ":axis==1?"XZ":"XY");
}

static void screenshotPPM(AppContext& ctx) {
    (void)ctx;
    GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
    int w=vp[2], h=vp[3];
    std::vector<unsigned char> pixels(w*h*3);
    glReadPixels(0,0,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    FILE* f=fopen("screenshot.ppm","wb");
    if(!f){printf("Cannot write screenshot.ppm\n");return;}
    fprintf(f,"P6\n%d %d\n255\n",w,h);
    // Flip vertically (OpenGL origin is bottom-left).
    for(int y=h-1;y>=0;--y) fwrite(&pixels[y*w*3],1,w*3,f);
    fclose(f);
    printf("Screenshot: %dx%d → screenshot.ppm\n",w,h);
}

// Apply boolean operation between selected feature and most recent other feature.
static void applyBool(AppContext& ctx, FeatureId selectedId, nexus::geometry::BooleanOp op) {
    if(!ctx.document || selectedId == nexus::parametric::kInvalidFeatureId) {
        printf("Boolean: need document and selection\n"); return;
    }
    using namespace nexus::geometry;
    auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    if(fc < 2 || selectedId == nexus::parametric::kInvalidFeatureId) {
        printf("Boolean: need at least 2 features with 1 selected\n"); return;
    }
    FeatureId otherId = static_cast<FeatureId>(fc); // most recent
    if(otherId == selectedId) {
        otherId = static_cast<FeatureId>(fc - 1);
        if(otherId == nexus::parametric::kInvalidFeatureId) { printf("Boolean: no second feature\n"); return; }
    }
    auto* nodeA = hist.node(selectedId);
    auto* nodeB = hist.node(otherId);
    if(!nodeA || !nodeB || !nodeA->mesh || !nodeB->mesh) {
        printf("Boolean: both features must have meshes\n"); return;
    }
    auto result = MeshBoolean::compute(*nodeA->mesh, *nodeB->mesh, op);
    if(!result.ok) { printf("Boolean failed: %s\n", result.error.c_str()); return; }
    const char* opName = (op==BooleanOp::Union)?"Union":(op==BooleanOp::Difference)?"Difference":"Intersection";
    auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
    auto fid = ctx.document->addSketch(sk);
    auto* node = hist.node(fid);
    if(node) { node->mesh.emplace(std::move(result.result)); node->dirty = false; }
    printf("Boolean %s: feature %u = %u %s %u\n", opName, fid, selectedId, opName, otherId);
}

// ── Modify helpers ──────────────────────────────────────────────────

static void modifyMesh(AppContext& ctx, std::function<void(nexus::geometry::Mesh&)> op) {
    if (!ctx.document) return;
    auto fid = static_cast<FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(fid);
    if (!node || !node->mesh || node->deleted) return;
    auto saved = *node->mesh;
    op(*node->mesh);
    auto cmd = std::make_unique<nexus::cad::TransformCommand>(fid, std::move(saved));
    ctx.document->executeCommand(std::move(cmd));
}

static void modifyMeshHEM(AppContext& ctx, std::function<void(nexus::geometry::HalfEdgeMesh&)> op) {
    if (!ctx.document) return;
    auto fid = static_cast<FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(fid);
    if (!node || !node->mesh || node->deleted) return;
    auto hemOpt = nexus::geometry::HalfEdgeMesh::fromMesh(*node->mesh);
    if (!hemOpt) return;
    auto saved = *node->mesh;
    op(*hemOpt);
    node->mesh.emplace(hemOpt->toMesh());
    auto cmd = std::make_unique<nexus::cad::TransformCommand>(fid, std::move(saved));
    ctx.document->executeCommand(std::move(cmd));
}

bool EditorUI::renderMenuBar(AppContext& ctx, TransformGizmo& gizmo,
                               ViewportController& viewport) {
    bool action = false;
    if(!ImGui::BeginMainMenuBar()) return false;

    if(ImGui::BeginMenu("File")) {
        if(ImGui::MenuItem("Save", "Ctrl+S")) {
            auto data = ctx.document->serialize();
            FILE* f = fopen("scene.nxm","wb");
            if(f){if(fwrite(data.data(),1,data.size(),f)!=data.size()){fclose(f);return false;}fclose(f); printf("Saved\n");}
            action=true;
        }
        if(ImGui::MenuItem("Load", "Ctrl+O")) {
            FILE* f = fopen("scene.nxm","rb");
            if(f){fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
                std::vector<uint8_t> d(sz);fread(d.data(),1,sz,f);fclose(f);
                (void)ctx.document->deserialize(d.data(),d.size()); printf("Loaded\n");}
            action=true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Export OBJ"))  { exportMeshFile(ctx, geometry::MeshExportFormat::OBJ,  "export.obj"); action=true; }
        if(ImGui::MenuItem("Export PLY"))  { exportMeshFile(ctx, geometry::MeshExportFormat::PLY,  "export.ply"); action=true; }
        if(ImGui::MenuItem("Export STL"))  { exportMeshFile(ctx, geometry::MeshExportFormat::STL,  "export.stl"); action=true; }
        if(ImGui::MenuItem("Export glTF")) { exportMeshFile(ctx, geometry::MeshExportFormat::GLTF, "export.glb"); action=true; }
        if(ImGui::MenuItem("Export USD"))  { exportMeshFile(ctx, geometry::MeshExportFormat::USDA, "export.usda"); action=true; }
        ImGui::Separator();
        if(ImGui::MenuItem("Import OBJ"))  { importMeshFile(ctx, "import.obj"); action=true; }
        if(ImGui::MenuItem("Import PLY"))  { importMeshFile(ctx, "import.ply"); action=true; }
        if(ImGui::MenuItem("Import STL"))  { importMeshFile(ctx, "import.stl"); action=true; }
        if(ImGui::MenuItem("Import glTF")) { importMeshFile(ctx, "import.glb"); action=true; }
        if(ImGui::MenuItem("Import USD"))  { importMeshFile(ctx, "import.usda"); action=true; }
        ImGui::Separator();
        if(ImGui::MenuItem("Screenshot (PPM)")) {
            screenshotPPM(ctx);
            action=true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Quit")) { glfwSetWindowShouldClose(glfwGetCurrentContext(),1); }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Edit")) {
        if(ImGui::MenuItem("Undo","Ctrl+Z")) { ctx.document->undo(); action=true; }
        if(ImGui::MenuItem("Redo","Ctrl+Y"))  { ctx.document->redo(); action=true; }
        ImGui::Separator();
        if(ImGui::MenuItem("Delete","Del")) {
            auto fid = static_cast<FeatureId>(ctx.activeSelectedFeature);
            if(ctx.document && fid != nexus::parametric::kInvalidFeatureId) {
                ctx.document->deleteFeature(fid);
                printf("Deleted feature %u\n", fid);
            }
            action=true;
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("View")) {
        const char* vp[]={"Wireframe","Solid","Shaded","Material","Rendered","XRay","HiddenLine"};
        for(int i=0;i<7;++i)
            if(ImGui::MenuItem(vp[i],nullptr,static_cast<int>(viewport.mode())==i))
                viewport.setMode(static_cast<ViewportMode>(i));
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Create")) {
        if(ImGui::MenuItem("Box"))      placePrimitive(ctx,0);
        if(ImGui::MenuItem("Sphere"))   placePrimitive(ctx,1);
        if(ImGui::MenuItem("Cylinder")) placePrimitive(ctx,2);
        if(ImGui::MenuItem("Cone"))     placePrimitive(ctx,3);
        if(ImGui::MenuItem("Torus"))    placePrimitive(ctx,4);
        if(ImGui::MenuItem("Plane"))    placePrimitive(ctx,5);
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Mode")) {
        const char* ids[]={"select","sketch","extrude","revolve","fillet","modeling","dimension","face-edit","edge-edit","vertex-edit","boolean","pattern","mirror"};
        const char* names[]={"Select","Sketch","Extrude","Revolve","Fillet","Modeling","Dimension","FaceEdit","EdgeEdit","VertexEdit","Boolean","Pattern","Mirror"};
        const int n = 13;
        for(int i=0;i<n;++i) {
            bool active = ctx.orchestrator && ctx.orchestrator->activeModeId()==ids[i];
            if(ImGui::MenuItem(names[i],nullptr,active)) {
                if(ctx.orchestrator) ctx.orchestrator->switchTo(ids[i]);
                printf("Mode: %s\n", ids[i]);
            }
        }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Gizmo")) {
        bool t = gizmo.mode()==TransformGizmo::Mode::Translate;
        bool s = gizmo.mode()==TransformGizmo::Mode::Scale;
        bool r = gizmo.mode()==TransformGizmo::Mode::Rotate;
        if(ImGui::MenuItem("Translate","W",&t)) gizmo.setMode(TransformGizmo::Mode::Translate);
        if(ImGui::MenuItem("Scale","E",&s))     gizmo.setMode(TransformGizmo::Mode::Scale);
        if(ImGui::MenuItem("Rotate","R",&r))    gizmo.setMode(TransformGizmo::Mode::Rotate);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    return action;
}

void EditorUI::renderToolbar(AppContext& ctx, TransformGizmo& gizmo) {
    ImGui::Begin("Toolbar",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize);
    if(ctx.orchestrator) {
        if(ImGui::Button("Select"))   ctx.orchestrator->switchTo("select");
        ImGui::SameLine();
        if(ImGui::Button("Sketch"))   ctx.orchestrator->switchTo("sketch");
        ImGui::SameLine();
        if(ImGui::Button("Modeling")) ctx.orchestrator->switchTo("modeling");
        ImGui::SameLine();
    }
    ImGui::Separator(); ImGui::SameLine();
    if(ImGui::Button("Translate")) gizmo.setMode(TransformGizmo::Mode::Translate);
    ImGui::SameLine();
    if(ImGui::Button("Rotate")) gizmo.setMode(TransformGizmo::Mode::Rotate);
    ImGui::SameLine();
    if(ImGui::Button("Scale")) gizmo.setMode(TransformGizmo::Mode::Scale);
    ImGui::SameLine(); ImGui::Separator(); ImGui::SameLine();
    if(ImGui::SmallButton("Box"))      { placePrimitive(ctx,0); } ImGui::SameLine();
    if(ImGui::SmallButton("Sphere"))   { placePrimitive(ctx,1); } ImGui::SameLine();
    if(ImGui::SmallButton("Cyl"))      { placePrimitive(ctx,2); } ImGui::SameLine();
    if(ImGui::SmallButton("Cone"))     { placePrimitive(ctx,3); } ImGui::SameLine();
    if(ImGui::SmallButton("Torus"))    { placePrimitive(ctx,4); } ImGui::SameLine();
    if(ImGui::SmallButton("Plane"))    { placePrimitive(ctx,5); }
    ImGui::End();
}

void EditorUI::renderContextMenu(AppContext& ctx, TransformGizmo& gizmo, FeatureId selectedId) {
    (void)gizmo;
    if(!ImGui::BeginPopupContextVoid("ViewportContext", ImGuiPopupFlags_MouseButtonRight)) return;
    if(ImGui::MenuItem("Create Box"))      placePrimitive(ctx,0);
    if(ImGui::MenuItem("Create Sphere"))   placePrimitive(ctx,1);
    if(ImGui::MenuItem("Create Cylinder")) placePrimitive(ctx,2);
    if(ImGui::MenuItem("Create Cone"))     placePrimitive(ctx,3);
    if(ImGui::MenuItem("Create Torus"))    placePrimitive(ctx,4);
    if(ImGui::MenuItem("Create Plane"))    placePrimitive(ctx,5);
    ImGui::Separator();
    if(ImGui::MenuItem("Delete Selected")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId)
            ctx.document->deleteFeature(selectedId);
    }
    if(ImGui::MenuItem("Instance")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* n = ctx.document->history().node(selectedId);
            if(n && n->mesh) {
                nexus::geometry::Mesh copy = *n->mesh;
                auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
                auto fid = ctx.document->addSketch(sk);
                auto* newNode = ctx.document->history().node(fid);
                if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty=false; }
                printf("Instanced %u → %u\n", selectedId, fid);
            }
        }
    }
    if(ImGui::MenuItem("Set Material to All")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId && ctx.document) {
            auto* src = ctx.document->history().node(selectedId);
            if(src) {
                auto& hist = ctx.document->history();
                for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
                    auto* n = hist.node(i);
                    if(n && n->mesh && !n->deleted) n->material = src->material;
                }
                printf("Material set to all features\n");
            }
        }
    }
    ImGui::Separator();
    if(ImGui::MenuItem("Mirror XZ"))   { mirrorSelected(ctx, selectedId, 1); }
    if(ImGui::MenuItem("Mirror XY"))   { mirrorSelected(ctx, selectedId, 2); }
    if(ImGui::MenuItem("Mirror YZ"))   { mirrorSelected(ctx, selectedId, 0); }
    ImGui::Separator();
    if(ImGui::MenuItem("Align to Ground")) {
        if(selectedId != nexus::parametric::kInvalidFeatureId && ctx.document) {
            auto* n = ctx.document->history().node(selectedId);
            if(n && n->mesh) {
                auto b = n->mesh->computeBounds();
                auto pos = n->mesh->attributes().positions();
                for(auto& v : pos) v.y -= b.min.y;
                n->mesh->attributes().setPositions(std::move(pos));
                printf("Aligned to ground (Y=%.2f)\n", b.min.y);
            }
        }
    }
    if(ImGui::MenuItem("Sweep (Z)")) {
        if(ctx.document && selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* n = ctx.document->history().node(selectedId);
            if(n && n->mesh) {
                // Simple sweep: copy vertices, offset Z, bridge.
                auto orig = n->mesh->attributes().positions();
                std::vector<Vec3> newPos = orig;
                for(auto& v : newPos) v.z += 2.f;
                auto allPos = orig;
                allPos.insert(allPos.end(), newPos.begin(), newPos.end());
                n->mesh->attributes().setPositions(std::move(allPos));
                printf("Sweep: extended mesh (Z+2)\n");
            }
        }
    }
    ImGui::Separator();
    if(ImGui::BeginMenu("Boolean")) {
        if(ImGui::MenuItem("Union", "Shift+U"))     applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Union);
        if(ImGui::MenuItem("Difference", "Shift+D")) applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Difference);
        if(ImGui::MenuItem("Intersection", "Shift+I")) applyBool(ctx, selectedId, nexus::geometry::BooleanOp::Intersection);
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if(ImGui::MenuItem("Unhide All", "Ctrl+H")) {
        if(ctx.document) ctx.document->history().unhideAll();
    }
    if(ImGui::MenuItem("View All", nullptr, false, true)) {
        auto& cam = ctx.cameraPosition;
        cam = {0,0,10};
    }
    ImGui::EndPopup();
}

void EditorUI::renderStatusBar(const AppContext& ctx, bool snapToGrid, FeatureId selectedId) {
    ImGui::Begin("Status",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_AlwaysAutoResize);
    int liveFc = 0;
    if(ctx.document) {
        auto& hist = ctx.document->history();
        for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
            auto* n=hist.node(i); if(n&&!n->deleted) liveFc++;
        }
    }
    const char* mode = ctx.orchestrator ? ctx.orchestrator->activeModeId().c_str() : "none";
    const char* plane = (ctx.workPlane == AppContext::WorkPlane::XZ) ? "XZ" :
                        (ctx.workPlane == AppContext::WorkPlane::XY) ? "XY" : "YZ";
    ImGui::Text("Objects: %d | Sel: %llu | Snap: %s | Plane: %s | Mode: %s | Cursor: (%.1f, %.1f, %.1f)", liveFc,
        static_cast<unsigned long long>(selectedId), snapToGrid ? "ON" : "OFF", plane, mode,
        ctx.cursorWorldPos.x, ctx.cursorWorldPos.y, ctx.cursorWorldPos.z);
    ImGui::End();
}

void EditorUI::renderOutliner(AppContext& ctx, FeatureId& selectedId) {
    ImGui::Begin("Outliner");
    const auto& hist = ctx.document->history();
    size_t fc = hist.featureCount();
    for(FeatureId i = 1; i <= static_cast<FeatureId>(fc); ++i) {
        auto* n = hist.node(i);
        if(!n || n->deleted || n->hidden) continue;
        const char* kindStr = "?";
        switch(n->kind) {
            case nexus::parametric::FeatureKind::Sketch:  kindStr="Sketch"; break;
            case nexus::parametric::FeatureKind::Extrude: kindStr="Extrude"; break;
            case nexus::parametric::FeatureKind::Revolve: kindStr="Revolve"; break;
        }
        char label[128];
        (void)snprintf(label,sizeof(label),"[%u] %s",i,kindStr);
        bool sel = (selectedId == i);
        if(ImGui::Selectable(label,sel) && !sel) {
            selectedId = i;
            ctx.selectedFace = ~0u;
            ctx.selectedVertex = ~0u;
        }
        if(ImGui::BeginPopupContextItem(label)) {
            if(ImGui::MenuItem("Delete")) {
                if(ctx.document) ctx.document->deleteFeature(i);
                if(selectedId == i) selectedId = nexus::parametric::kInvalidFeatureId;
            }
            if(ImGui::BeginMenu("Display")) {
                using DM = nexus::parametric::FeatureNode::DisplayMode;
                auto* mutableNode = ctx.document ? ctx.document->history().node(i) : nullptr;
                if(mutableNode) {
                    if(ImGui::MenuItem("Solid", nullptr, mutableNode->displayMode==DM::Solid)) mutableNode->displayMode=DM::Solid;
                    if(ImGui::MenuItem("Wireframe", nullptr, mutableNode->displayMode==DM::Wireframe)) mutableNode->displayMode=DM::Wireframe;
                    if(ImGui::MenuItem("BoundingBox", nullptr, mutableNode->displayMode==DM::BoundingBox)) mutableNode->displayMode=DM::BoundingBox;
                }
        ImGui::EndMenu();
    }

    if(ImGui::BeginMenu("Modify")) {
        auto fid = static_cast<FeatureId>(ctx.activeSelectedFeature);
        auto* node = ctx.document ? ctx.document->history().node(fid) : nullptr;
        bool hasMesh = node && node->mesh && !node->deleted;

        if(ImGui::MenuItem("Subdivide", nullptr, false, hasMesh)) modifyMesh(ctx, [](auto& m){ 
            auto h = nexus::geometry::HalfEdgeMesh::fromMesh(m); if(h){nexus::geometry::SubdivisionOptions o;o.levels=1;auto r=nexus::geometry::SubdivisionSurface::catmullClark(*h,o);if(r)m=r->toMesh();} });
        if(ImGui::MenuItem("Decimate (50%)", nullptr, false, hasMesh)) modifyMesh(ctx, [](auto& m){
            auto h = nexus::geometry::HalfEdgeMesh::fromMesh(m); if(h){nexus::geometry::DecimationOptions o;o.targetFaceCount=static_cast<uint32_t>(m.topology().faceCount()/2);if(o.targetFaceCount<3)o.targetFaceCount=3;auto r=nexus::geometry::MeshDecimator::decimate(*h,o);if(r)m=r->first.toMesh();} });
        if(ImGui::MenuItem("Triangulate", nullptr, false, hasMesh)) modifyMesh(ctx, [](auto& m){ (void)m.topology().triangulate(); });
        ImGui::Separator();
        if(ImGui::MenuItem("Flip Normals", nullptr, false, hasMesh)) modifyMesh(ctx, [](auto& m){ (void)m.computeVertexNormals(); });
        if(ImGui::MenuItem("Poke Face", nullptr, false, hasMesh)) modifyMeshHEM(ctx, [](auto& h){ h.pokeFace(0); });
        if(ImGui::MenuItem("Inset Faces", nullptr, false, hasMesh)) modifyMeshHEM(ctx, [](auto& h){ 
            std::vector<uint32_t> faces; for(uint32_t fi=0;fi<h.faceCount();++fi)if(h.face(fi).edge!=nexus::geometry::HalfEdgeMesh::kInvalid)faces.push_back(fi);
            if(!faces.empty()) { auto r = nexus::geometry::DirectModeling::pushFaces(h,faces,0.05f); (void)r; } });
        ImGui::Separator();
        if(ImGui::MenuItem("Grid Fill", nullptr, false, hasMesh)) modifyMeshHEM(ctx, [](auto& h){
            auto loops=h.boundaryLoops(); for(auto& l:loops) h.gridFill(l); });
        if(ImGui::MenuItem("Insert Edge Loop", nullptr, false, hasMesh)) modifyMeshHEM(ctx, [](auto& h){
            for(uint32_t ei=0;ei<h.edgeCount();++ei)if(h.edge(ei).face!=nexus::geometry::HalfEdgeMesh::kInvalid&&h.edge(ei).twin!=nexus::geometry::HalfEdgeMesh::kInvalid){h.insertEdgeLoop(ei,0.5f);break;} });
        ImGui::Separator();
        if(ImGui::MenuItem("Smooth (Laplacian)", nullptr, false, hasMesh)) modifyMesh(ctx, [](auto& m){
            auto r = nexus::geometry::MeshLaplacian::smooth(m); m = r; });
        if(ImGui::MenuItem("Center Pivot", nullptr, false, hasMesh)) modifyMesh(ctx, [](auto& m){
            auto b = m.computeBounds(); Vec3 c = b.center();
            auto p = m.attributes().positions();
            for(auto& v : p){ v.x -= c.x; v.y -= c.y; v.z -= c.z; }
            m.attributes().setPositions(std::move(p)); });
        ImGui::EndMenu();
    }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void EditorUI::renderProperties(AppContext& ctx, FeatureId selectedId) {
    ImGui::Begin("Inspector");
    if(selectedId == nexus::parametric::kInvalidFeatureId || ctx.document->history().featureCount() == 0) {
        ImGui::TextUnformatted("No object selected.");
        ImGui::End(); return;
    }
    auto* n = ctx.document->history().node(selectedId);
    if(!n || !n->mesh) { ImGui::TextUnformatted("Selected object not found."); ImGui::End(); return; }

    const char* kindStr = "?";
    switch(n->kind) {
        case nexus::parametric::FeatureKind::Sketch:  kindStr="Sketch"; break;
        case nexus::parametric::FeatureKind::Extrude: kindStr="Extrude"; break;
        case nexus::parametric::FeatureKind::Revolve: kindStr="Revolve"; break;
    }
    ImGui::Text("Feature ID: %u", selectedId);
    ImGui::Text("Type: %s", kindStr);
    // Editable name.
    char nameBuf[128];
    snprintf(nameBuf,sizeof(nameBuf),"%s",n->name.c_str());
    if(ImGui::InputText("Name",nameBuf,sizeof(nameBuf))) {
        ctx.document->history().setName(selectedId, nameBuf);
    }
    ImGui::Text("Vertices: %zu", n->mesh->attributes().vertexCount());
    ImGui::Text("Faces: %zu", n->mesh->topology().faceCount());

    ImGui::Separator();
    if(ImGui::Button("Instance")) {
        nexus::geometry::Mesh copy = *n->mesh;
        auto sk = nexus::parametric::ParametricSketchFactory::createSketch();
        auto fid = ctx.document->addSketch(sk);
        auto* newNode = ctx.document->history().node(fid);
        if(newNode) { newNode->mesh.emplace(std::move(copy)); newNode->dirty=false; }
        printf("Instanced feature %u → %u\n", selectedId, fid);
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Transform");

    auto bounds = n->mesh->computeBounds();
    Vec3 center = bounds.center();
    float pos[3] = {center.x, center.y, center.z};
    if(ImGui::DragFloat3("Position", pos, 0.1f)) {
        Vec3 delta{pos[0]-center.x, pos[1]-center.y, pos[2]-center.z};
        auto verts = n->mesh->attributes().positions();
        for(auto& v : verts) { v.x+=delta.x; v.y+=delta.y; v.z+=delta.z; }
        n->mesh->attributes().setPositions(std::move(verts));
    }

    Vec3 extents = bounds.extents();
    float scl[3] = {extents.x*2, extents.y*2, extents.z*2};
    ImGui::Text("Size: %.2f x %.2f x %.2f", scl[0], scl[1], scl[2]);

    // Parametric primitive editing.
    if(n->primType != nexus::parametric::FeatureNode::PrimType::None) {
        ImGui::Separator();
        ImGui::TextUnformatted("Primitive Params");
        bool changed = false;
        auto& pp = n->primParams;
        using PT = nexus::parametric::FeatureNode::PrimType;
        if(n->primType == PT::Box) {
            changed|=ImGui::DragFloat("Width",&pp[0],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Height",&pp[1],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Depth",&pp[2],0.1f,0.1f,100.f);
        } else if(n->primType == PT::Sphere) {
            changed|=ImGui::DragFloat("Radius",&pp[0],0.1f,0.1f,100.f);
        } else if(n->primType == PT::Cylinder||n->primType==PT::Cone) {
            changed|=ImGui::DragFloat("Radius",&pp[0],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Height",&pp[1],0.1f,0.1f,100.f);
        } else if(n->primType == PT::Torus) {
            changed|=ImGui::DragFloat("Major R",&pp[0],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Minor R",&pp[1],0.1f,0.1f,100.f);
        } else if(n->primType == PT::Plane) {
            changed|=ImGui::DragFloat("Width",&pp[0],0.1f,0.1f,100.f);
            changed|=ImGui::DragFloat("Height",&pp[1],0.1f,0.1f,100.f);
        }
        if(changed && ctx.document) {
            auto ctr = n->mesh->computeBounds().center();
            geometry::Mesh newMesh;
            if(n->primType==PT::Box) newMesh=nexus::geometry::primitives::makeBox(pp[0],pp[1],pp[2]);
            else if(n->primType==PT::Sphere) newMesh=nexus::geometry::primitives::makeSphere(pp[0]);
            else if(n->primType==PT::Cylinder) newMesh=nexus::geometry::primitives::makeCylinder(pp[0],pp[1]);
            else if(n->primType==PT::Cone) newMesh=nexus::geometry::primitives::makeCone(pp[0],pp[1]);
            else if(n->primType==PT::Torus) newMesh=nexus::geometry::primitives::makeTorus(pp[0],pp[1]);
            else if(n->primType==PT::Plane) newMesh=nexus::geometry::primitives::makePlane(pp[0],pp[1]);
            if(newMesh.isValid()) {
                auto pos=newMesh.attributes().positions();
                for(auto& v:pos){v.x+=ctr.x;v.y+=ctr.y;v.z+=ctr.z;}
                newMesh.attributes().setPositions(std::move(pos));
                (void)newMesh.computeVertexNormals();
                n->mesh.emplace(std::move(newMesh));
            }
        }
    }

    ImGui::Separator();
    if(ImGui::Button("Shell")) {
        if(ctx.document && selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* nd = ctx.document->history().node(selectedId);
            if(nd && nd->mesh) {
                geometry::Mesh saved = *nd->mesh;
                geometry::SurfaceOffsetOptions opts;
                opts.distance = -0.15f;
                auto shelled = geometry::SurfaceOffset::offset(*nd->mesh, opts);
                if(shelled.isValid()) {
                    nd->mesh.emplace(std::move(shelled));
                    printf("Shell: offset -0.15\n");
                } else { printf("Shell failed\n"); }
            }
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Draft")) {
        if(ctx.document && selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* nd = ctx.document->history().node(selectedId);
            if(nd && nd->mesh && ctx.selectedFace != ~0u) {
                auto heOpt = geometry::HalfEdgeMesh::fromMesh(*nd->mesh);
                if(heOpt) {
                    geometry::FaceOffsetOptions opts;
                    opts.offset = 0.f;
                    opts.draftAngleDeg = 5.f;
                    auto result = geometry::offsetFaceWithDraft(*heOpt, ctx.selectedFace, opts);
                    if(result.isValid()) { nd->mesh.emplace(std::move(result)); printf("Draft: 5 deg on face %u\n", ctx.selectedFace); }
                    else printf("Draft failed\n");
                }
            }
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("Center")) {
        if(ctx.document && selectedId != nexus::parametric::kInvalidFeatureId) {
            auto* nd = ctx.document->history().node(selectedId);
            if(nd && nd->mesh) {
                auto ctr = nd->mesh->computeBounds().center();
                auto pos = nd->mesh->attributes().positions();
                for(auto& v : pos) { v.x -= ctr.x; v.y -= ctr.y; v.z -= ctr.z; }
                nd->mesh->attributes().setPositions(std::move(pos));
                printf("Centered to origin\n");
            }
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Material (PBR)");
    ImGui::ColorEdit4("Albedo", n->material.albedo);
    ImGui::DragFloat("Roughness", &n->material.roughness, 0.01f, 0.f, 1.f);
    ImGui::DragFloat("Metallic", &n->material.metallic, 0.01f, 0.f, 1.f);

    ImGui::End();
}

void EditorUI::renderTimeline(FeatureId /*selectedId*/, bool& playing,
                                int& frame, int maxFrame) {
    ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize);
    if(ImGui::Button(playing ? "Pause" : "Play")) playing = !playing;
    ImGui::SameLine();
    if(ImGui::Button("|<")) frame = 0;
    ImGui::SameLine();
    if(ImGui::Button("<"))  frame = std::max(0, frame-1);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::DragInt("##frame", &frame, 1.f, 0, maxFrame, "frame %d");
    ImGui::SameLine();
    if(ImGui::Button(">"))  frame = std::min(maxFrame, frame+1);
    ImGui::SameLine();
    if(ImGui::Button(">|")) frame = maxFrame;
    ImGui::SameLine();
    ImGui::Text("I=key   K=clear");
    ImGui::End();
}

void EditorUI::renderKeybindings() {
    ImGui::Begin("Keybinding Reference", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Modes: ESC=Select  S=Sketch  E=Extrude  R=Revolve  F=Fillet  M=Modeling  D=Dimension");
    ImGui::Text("       Q=FaceEdit  B=EdgeEdit  V=VertexEdit  K=Boolean  P=Pattern  N=Mirror");
    ImGui::Separator();
    ImGui::Text("Gizmo: W=Translate  E=Scale  R=Rotate  G=SnapToggle  Ctrl=Snap-to-geometry");
    ImGui::Text("       Shift+Tab=Cycle snap mode  Tab=Cycle work plane");
    ImGui::Separator();
    ImGui::Text("Edit:  Ctrl+Z=Undo  Ctrl+Y=Redo  Ctrl+D=Duplicate  Del=Delete");
    ImGui::Text("       Ctrl+C=Copy  Ctrl+V=Paste  Ctrl+A=SelectAll  A=Deselect");
    ImGui::Separator();
    ImGui::Text("View:  Home=ViewAll  F=FrameSel  KP5/O=Ortho  L=Lighting");
    ImGui::Text("       KP0=ISO  KP1=Front  KP3=Right  KP7=Top  Ctrl+Q=QuadView");
    ImGui::Separator();
    ImGui::Text("Bool:  Shift+U=Union  Shift+D=Diff  Shift+I=Intersect");
    ImGui::Text("Sketch: 1=Point  2=Rect  3=Circle");
    ImGui::Text("Anim:  Space=Play  I=Keyframe  K=Clear  Arrows=Frame");
    ImGui::Text("Hide:  H=Hide  Shift+H=Isolate  Ctrl+H=UnhideAll");
    ImGui::Text("Grid:  [=Finer  ]=Coarser");
    ImGui::End();
}

void EditorUI::renderUndoHistory(AppContext& ctx) {
    ImGui::Begin("Undo History");
    if(!ctx.document) { ImGui::End(); return; }
    auto undoNames = ctx.document->undoDescriptions();
    auto redoNames = ctx.document->redoDescriptions();
    ImGui::Text("Undo stack (%zu):", undoNames.size());
    for(auto& name : undoNames) ImGui::Text("  %s", name.c_str());
    ImGui::Separator();
    ImGui::Text("Redo stack (%zu):", redoNames.size());
    for(auto& name : redoNames) ImGui::Text("  %s", name.c_str());
    ImGui::End();
}

} // namespace nexus::app
