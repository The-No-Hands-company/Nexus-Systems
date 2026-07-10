#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Viewport
//
//  Modern Vulkan-based viewport replacing legacy OpenGL immediate-mode rendering.
//  Uses Renderer + SceneGraph for frustum-culled rendering and GeometryRenderBridge
//  for GPU mesh upload. Provides hardware-accelerated picking via MeshBVH.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Swapchain.h>
#include <nexus/gfx/FrameScheduler.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>
#include <nexus/render/Camera.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/app/AppMode.h>
#include <nexus/app/ViewportGrid.h>
#include <nexus/app/TransformGizmo.h>
#include <nexus/app/SelectionHighlight.h>
#include <nexus/app/SketchPreview.h>

#include <memory>
#include <vector>
#include <unordered_map>
#include <functional>
#include <string>

struct GLFWwindow;

namespace nexus::app {

using FeatureId = nexus::parametric::FeatureId;

struct ViewportConfig {
    uint32_t width = 1280;
    uint32_t height = 720;
    bool enableVSync = true;
    bool enableMSAA = false;
    nexus::render::RendererSettings rendererSettings{};
};

struct ViewportStats {
    uint32_t totalNodes = 0;
    uint32_t visibleNodes = 0;
    uint32_t drawCalls = 0;
    uint32_t triangles = 0;
    double gpuTimeMs = 0.0;
    double cpuCullTimeMs = 0.0;
    double uploadTimeMs = 0.0;
};

struct PickingResult {
    FeatureId featureId = nexus::parametric::kInvalidFeatureId;
    uint32_t faceIndex = ~0u;
    uint32_t vertexIndex = ~0u;
    nexus::render::Vec3 hitPoint{};
    float depth = 0.0f;
    bool hit = false;
};

struct HoverState {
    FeatureId hoveredId = nexus::parametric::kInvalidFeatureId;
    bool valid = false;
};

class Viewport {
public:
    explicit Viewport(GLFWwindow* window, const ViewportConfig& config = {});
    ~Viewport();

    Viewport(const Viewport&) = delete;
    Viewport& operator=(const Viewport&) = delete;
    Viewport(Viewport&&) = delete;
    Viewport& operator=(Viewport&&) = delete;

    // ── Initialization ────────────────────────────────────────────────────────
    bool initialize();

    // ── CadDocument synchronization ───────────────────────────────────────────
    // Call when document structure changes (features added/removed/modified).
    void syncDocument(const nexus::cad::CadDocument& doc);

    // Call when a feature's mesh was modified in-place.
    void updateFeatureMesh(FeatureId fid, const nexus::geometry::Mesh& mesh);

    // Call when a feature was deleted.
    void removeFeature(FeatureId fid);

    // Call when selection changes (for gizmo/highlight).
    void setSelection(const std::vector<FeatureId>& selectedIds, FeatureId primaryId = nexus::parametric::kInvalidFeatureId);

    // ── Render loop ───────────────────────────────────────────────────────────
    void beginFrame();
    void render();
    void endFrame();

    // Headless capture: read the last-rendered colour image back to a PNG file.
    // Vulkan-only; returns false on other backends or if nothing has rendered.
    [[nodiscard]] bool captureToPNG(const std::string& path) const;

    // ── Resize ────────────────────────────────────────────────────────────────
    void onResize(uint32_t width, uint32_t height);

    // ── Camera access ─────────────────────────────────────────────────────────
    nexus::render::Camera& camera() noexcept { return m_camera; }
    const nexus::render::Camera& camera() const noexcept { return m_camera; }

    // ── Picking / Hover ───────────────────────────────────────────────────────
    // Ray-cast pick at normalized device coordinates (-1..1).
    // Returns PickingResult with featureId, face/vertex indices, and world hit point.
    PickingResult pick(float ndcX, float ndcY) const;

    // Hover pre-highlight (uses same ray-cast but lighter weight).
    void updateHover(float ndcX, float ndcY);
    [[nodiscard]] const HoverState& hoverState() const noexcept { return m_hoverState; }

    // ── Grid / Gizmo / Overlays ───────────────────────────────────────────────
    void setGridOptions(const GridOptions& opts) { m_gridOpts = opts; }
    void setGizmoMode(TransformGizmo::Mode mode) { m_gizmo.setMode(mode); }
    void setGizmoActive(bool active) { m_gizmoActive = active; }
    void gizmoBeginDrag(const nexus::render::Vec3& worldPos, TransformGizmo::Axis axis);
    void gizmoDrag(const nexus::render::Vec3& worldPos);
    void gizmoEndDrag();

    // ── Stats / Debug ─────────────────────────────────────────────────────────
    [[nodiscard]] const ViewportStats& stats() const noexcept { return m_stats; }
    [[nodiscard]] const nexus::render::FrameStats& frameStats() const noexcept;

    // ── Work plane (for sketching) ────────────────────────────────────────────
    enum class WorkPlane { XZ, XY, YZ };
    void setWorkPlane(WorkPlane plane) { m_workPlane = plane; }
    [[nodiscard]] WorkPlane workPlane() const noexcept { return m_workPlane; }

    // Intersects mouse ray with work plane, returns world position.
    // Returns false if ray is parallel to plane.
    [[nodiscard]] bool intersectWorkPlane(float ndcX, float ndcY, nexus::render::Vec3& outPos) const;

    // ── Sketch preview ────────────────────────────────────────────────────────
    void setSketchPreview(const nexus::cad::CadAutoConstraintSketch* sketch);

    // ── Viewport config ───────────────────────────────────────────────────────
    [[nodiscard]] uint32_t width() const noexcept { return m_width; }
    [[nodiscard]] uint32_t height() const noexcept { return m_height; }
    [[nodiscard]] float aspectRatio() const noexcept { return m_height ? float(m_width) / float(m_height) : 1.0f; }

    // ── Internal accessors ──────────────────────────────────────────────────────
    nexus::gfx::RenderContext* renderContext() noexcept { return m_renderContext.get(); }
    const nexus::gfx::RenderContext* renderContext() const noexcept { return m_renderContext.get(); }
    nexus::gfx::ISwapchain* swapchain() noexcept { return m_swapchain.get(); }
    const nexus::gfx::ISwapchain* swapchain() const noexcept { return m_swapchain.get(); }
    nexus::gfx::ICommandBuffer* currentCommandBuffer() noexcept;

private:
    // ── Internal helpers ──────────────────────────────────────────────────────
    void uploadMesh(const nexus::geometry::Mesh& mesh, FeatureId fid);
    void rebuildSceneGraph();
    void updateSelectionHighlight();
    void screenToWorldRay(float ndcX, float ndcY, nexus::render::Vec3& outOrigin, nexus::render::Vec3& outDir) const;
    bool intersectMesh(const nexus::geometry::Mesh& mesh, const nexus::render::Vec3& origin, const nexus::render::Vec3& dir,
                       float& outT, uint32_t& outFace, uint32_t& outVert, nexus::render::Vec3& outHit) const;

    // ── Members ───────────────────────────────────────────────────────────────
    GLFWwindow* m_window = nullptr;
    ViewportConfig m_config;

    uint32_t m_width = 1280;
    uint32_t m_height = 720;

    std::unique_ptr<nexus::gfx::RenderContext> m_renderContext;
    std::unique_ptr<nexus::gfx::ISwapchain> m_swapchain;
    std::unique_ptr<nexus::gfx::IFrameScheduler> m_frameScheduler;
    std::unique_ptr<nexus::render::Renderer> m_renderer;
    std::unique_ptr<nexus::render::SceneGraph> m_sceneGraph;

    nexus::render::Camera m_camera;
    nexus::cad::CadDocument* m_document = nullptr;

    // FeatureId → SceneGraph Node* mapping
    std::unordered_map<FeatureId, nexus::render::Node*> m_featureNodes;
    std::unordered_map<FeatureId, std::unique_ptr<nexus::geometry::MeshBVH>> m_meshBVHs;

    // Uploaded mesh tracking for cleanup
    std::unordered_map<FeatureId, nexus::geometry::UploadedGeometryMesh> m_uploadedMeshes;

    // Selection state
    std::vector<FeatureId> m_selectedIds;
    FeatureId m_primarySelectedId = nexus::parametric::kInvalidFeatureId;

    // Hover state
    HoverState m_hoverState;

    // Grid / Gizmo / Sketch preview
    GridOptions m_gridOpts;
    TransformGizmo m_gizmo;
    bool m_gizmoActive = false;
    nexus::render::Vec3 m_gizmoStartWorldPos{};
    TransformGizmo::Axis m_gizmoDragAxis = TransformGizmo::Axis::None;
    std::vector<std::unique_ptr<nexus::geometry::Mesh>> m_gizmoSavedMeshes;
    SelectionHighlight m_selectionHighlight;
    SketchPreview m_sketchPreview;
    const nexus::cad::CadAutoConstraintSketch* m_sketchPreviewSource = nullptr;

    // Work plane
    WorkPlane m_workPlane = WorkPlane::XZ;

    // Stats
    ViewportStats m_stats;
    double m_lastUploadTime = 0.0;
};

} // namespace nexus::app