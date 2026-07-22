# Nexus Modeling — Editor Architecture

*Deep dive into the application framework, UI system, and editor integration.*

---

## Overview

The editor (`nexus::app`) is built on **ImGui + Vulkan** with a **mode-based state architecture**. It follows the kernel-first principle: all editor operations consume kernel APIs, never the reverse.

---

## Application Framework

### Main Entry Point

```cpp
// app/main.cpp
int main(int argc, char** argv) {
    // 1. Parse args (--shot for headless render)
    // 2. Initialize GLFW window
    // 3. Create AppContext (DI container)
    // 4. Initialize Vulkan/Null backend
    // 5. Create ModelingApplication
    // 6. Run event loop
    // 7. Cleanup
}
```

### AppContext (Dependency Injection)

```cpp
struct AppContext {
    // Core services
    std::unique_ptr<VulkanContext> vulkan;
    std::unique_ptr<RenderContext> renderContext;
    std::unique_ptr<Renderer> renderer;
    
    // Application state
    std::unique_ptr<CadDocument> document;
    std::unique_ptr<CadViewer> viewer;
    std::unique_ptr<ViewportController> viewport;
    std::unique_ptr<ModeOrchestrator> orchestrator;
    std::unique_ptr<EditorUI> editorUI;
    std::unique_ptr<TransformGizmo> gizmo;
    std::unique_ptr<SelectionManager> selection;
    std::unique_ptr<CommandDispatcher> commands;
    std::unique_ptr<WorkspaceManager> workspace;
    std::unique_ptr<AssetBrowser> assetBrowser;
    
    // Input
    InputState input;
    bool headless = false;
    std::string shotPath;
};
```

---

## Mode System (State Pattern)

### AppMode Base Class

```cpp
class AppMode {
public:
    virtual ~AppMode() = default;
    virtual void onEnter(AppContext&) {}
    virtual void onExit(AppContext&) {}
    virtual void onUpdate(AppContext&, float dt) {}
    virtual void onRender(AppContext&, RenderContext&) {}
    virtual void onInput(AppContext&, const InputEvent&) {}
    virtual void onImGui(AppContext&) {}
    virtual std::string name() const = 0;
    virtual std::string group() const { return "Modes"; }
    virtual bool isModal() const { return false; }
};
```

### ModeOrchestrator

```cpp
class ModeOrchestrator {
    std::unique_ptr<AppMode> currentMode;
    std::unordered_map<std::string, std::function<std::unique_ptr<AppMode>()>> modeFactories;
    
    void switchTo(AppContext& ctx, const std::string& modeName) {
        if (currentMode) currentMode->onExit(ctx);
        currentMode = modeFactories[modeName]();
        currentMode->onEnter(ctx);
    }
    
    void update(AppContext& ctx, float dt) {
        if (currentMode) currentMode->onUpdate(ctx, dt);
    }
    
    // ... render, input, imgui forwarding
};
```

### ModeRegistry

```cpp
class ModeRegistry {
    struct ModeDesc {
        std::string id;
        std::string displayName;
        std::string group;      // "Modes", "Transform", "Primitives", "Viewport"
        std::string icon;       // ImGui font icon
        std::function<std::unique_ptr<AppMode>()> factory;
        std::vector<std::string> dependencies;  // Required modes
    };
    
    void registerMode(const ModeDesc& desc);
    std::vector<ModeDesc> getModesByGroup(const std::string& group);
    std::optional<ModeDesc> findMode(const std::string& id);
};
```

### Built-in Modes

| Mode ID | Display Name | Group | Description |
|---------|-------------|-------|-------------|
| `select` | Select | Modes | Object/face/edge/vertex selection |
| `sketch` | Sketch | Modes | 2D constraint sketching |
| `extrude` | Extrude | Modes | Extrude sketch/profile |
| `revolve` | Revolve | Modes | Revolve sketch/profile |
| `fillet` | Fillet | Modes | Edge fillet |
| `chamfer` | Chamfer | Modes | Edge chamfer |
| `boolean` | Boolean | Modes | Union/difference/intersect |
| `translate` | Translate | Transform | Move gizmo |
| `rotate` | Rotate | Transform | Rotate gizmo |
| `scale` | Scale | Transform | Scale gizmo |
| `box` | Box | Primitives | Create box |
| `cylinder` | Cylinder | Primitives | Create cylinder |
| `sphere` | Sphere | Primitives | Create sphere |
| `cone` | Cone | Primitives | Create cone |
| `torus` | Torus | Primitives | Create torus |

---

## EditorUI (ImGui + Vulkan)

### Initialization

```cpp
class EditorUI {
    ImGuiContext* imguiCtx = nullptr;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    VkDescriptorPool descriptorPool;
    
    bool initializeVulkan(VkDevice device, VkPhysicalDevice physDevice,
                          VkRenderPass renderPass, uint32_t subpass,
                          VkDescriptorPool pool) {
        // 1. Create ImGui context
        // 2. Setup ImGui Vulkan backend
        // 3. Create font atlas
        // 4. Create pipeline for ImGui draw data
        // 5. Upload font texture
    }
    
    void shutdownVulkan() {
        // Destroy pipeline, descriptor pool, font texture
        // ImGui::DestroyContext()
    }
    
    void newFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();
    }
    
    void render(CommandBuffer& cmd) {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    }
};
```

### Main Layout

```cpp
void EditorUI::renderMainLayout(AppContext& ctx) {
    // Dockspace
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
    
    // Menu bar
    renderMenuBar(ctx);
    
    // Toolbar (vertical, left)
    renderToolbar(ctx);
    
    // Viewport (center)
    renderViewport(ctx);
    
    // Panels (right/bottom)
    renderOutliner(ctx);
    renderInspector(ctx);
    renderAssetBrowser(ctx);
    renderConsole(ctx);
}
```

### Vertical Toolbar with Groups

```cpp
void EditorUI::renderToolbar(AppContext& ctx) {
    ImGui::Begin("##Toolbar", nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    
    ImGui::SetWindowPos(ImVec2(0, 0));
    ImGui::SetWindowSize(ImVec2(52, 0));
    
    // Group: Modes
    renderToolGroup(ctx, "Modes", {
        {"select", ICON_FA_MOUSE_POINTER},
        {"sketch", ICON_FA_PENCIL_ALT},
        {"extrude", ICON_FA_ARROW_UP},
        {"revolve", ICON_FA_SYNC_ALT},
        {"fillet", ICON_FA_CIRCLE},
        {"chamfer", ICON_FA_CUT},
        {"boolean", ICON_FA_CUBES},
    });
    
    ImGui::Separator();
    
    // Group: Transform
    renderToolGroup(ctx, "Transform", {
        {"translate", ICON_FA_ARROWS_ALT},
        {"rotate", ICON_FA_UNDO},
        {"scale", ICON_FA_EXPAND_ARROWS_ALT},
    });
    
    ImGui::Separator();
    
    // Group: Primitives
    renderToolGroup(ctx, "Primitives", {
        {"box", ICON_FA_CUBE},
        {"cylinder", ICON_FA_CIRCLE},
        {"sphere", ICON_FA_GLOBE},
        {"cone", ICON_FA_CONE},
        {"torus", ICON_FA_DONUT},
    });
    
    ImGui::Separator();
    
    // Group: Viewport
    renderToolGroup(ctx, "Viewport", {
        {"wireframe", ICON_FA_BORDER_NONE},
        {"shaded", ICON_FA_SQUARE},
        {"grid", ICON_FA_TH},
        {"axes", ICON_FA_ARROWS_ALT_H},
    });
    
    ImGui::End();
}

void EditorUI::renderToolGroup(AppContext& ctx, const char* groupName, 
                                const std::vector<std::pair<std::string, std::string>>& tools) {
    ImGui::TextUnformatted(groupName);
    for (auto& [modeId, icon] : tools) {
        bool active = ctx.orchestrator->currentMode() == modeId;
        if (ImGui::ToolbarButton(icon.c_str(), active)) {
            ctx.orchestrator->switchTo(ctx, modeId);
        }
        // Hover pre-highlight
        if (ImGui::IsItemHovered()) {
            // Show tooltip with mode description
        }
    }
}
```

### Hover Pre-Highlight (Pale Cyan Outline)

```cpp
void EditorUI::renderViewport(AppContext& ctx) {
    ImGui::Begin("Viewport");
    
    // Get viewport size
    ImVec2 size = ImGui::GetContentRegionAvail();
    ctx.viewport->resize(size.x, size.y);
    
    // Render scene to texture
    TextureHandle sceneTexture = ctx.renderer->renderScene(
        *ctx.viewport, *ctx.viewer, *ctx.document);
    
    // Display with ImGui
    ImGui::Image((ImTextureID)sceneTexture, size);
    
    // Hover pre-highlight
    if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive()) {
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 windowPos = ImGui::GetWindowPos();
        Vec2 localPos = {mousePos.x - windowPos.x, mousePos.y - windowPos.y};
        
        // Raycast from viewport camera
        auto pick = ctx.viewport->pick(localPos.x, localPos.y, size.x, size.y);
        if (pick.has_value()) {
            // Draw pale cyan outline
            drawHoverHighlight(ctx, pick.value());
        }
    }
    
    ImGui::End();
}

void EditorUI::drawHoverHighlight(AppContext& ctx, FeatureId featureId) {
    // Get mesh for feature
    // Render to stencil buffer
    // Draw scaled outline in pale cyan
    // Stencil test: only where stencil == 0 (not the object itself)
}
```

### Inspector Panel

```cpp
void EditorUI::renderInspector(AppContext& ctx) {
    ImGui::Begin("Inspector");
    
    if (ctx.selection->hasSelection()) {
        FeatureId fid = ctx.selection->primary();
        if (auto* node = ctx.document->history().node(fid)) {
            renderFeatureInspector(ctx, *node);
        }
    }
    
    ImGui::End();
}

void EditorUI::renderFeatureInspector(AppContext& ctx, CadFeatureNode& node) {
    ImGui::Text("%s", node.feature->name.c_str());
    ImGui::Separator();
    
    // Common properties
    ImGui::Checkbox("Visible", &node.visible);
    ImGui::Checkbox("Locked", &node.locked);
    ImGui::DragFloat3("Position", &node.transform.translation.x);
    ImGui::DragFloat4("Rotation", &node.transform.rotation.x, 0.01f);
    ImGui::DragFloat3("Scale", &node.transform.scale.x, 0.01f);
    
    // Feature-specific properties
    if (auto* extrude = dynamic_cast<ExtrudeFeature*>(node.feature.get())) {
        ImGui::DragFloat("Depth", &extrude->params.depth, 0.1f, 0.1f, 1000.0f);
        ImGui::Checkbox("Symmetric", &extrude->params.symmetric);
        ImGui::Combo("Direction", &extrude->params.direction, "Normal\0Reverse\0Both\0");
        if (ImGui::Button("Regenerate")) {
            auto cmd = std::make_unique<RegenerateCommand>(node.id);
            ctx.commands->execute(std::move(cmd));
        }
    }
    // ... other feature types
}
```

---

## ViewportController

### Camera

```cpp
struct Camera {
    enum class Type { Perspective, Orthographic };
    Type type = Type::Perspective;
    float fov = 60.0f;        // degrees
    float nearPlane = 0.1f;
    float farPlane = 10000.0f;
    float orthoSize = 10.0f;
    
    Vec3 position = {0, 0, 10};
    Vec3 target = {0, 0, 0};
    Vec3 up = {0, 1, 0};
    
    Mat4 view() const;
    Mat4 proj(float aspect) const;
    Mat4 viewProj(float aspect) const;
    Frustum frustum(float aspect) const;
};
```

### ViewportController

```cpp
class ViewportController {
    Camera camera;
    bool orbiting = false, panning = false, zooming = false;
    Vec2 lastMousePos;
    
    void onInput(AppContext& ctx, const InputEvent& e) {
        if (e.type == InputEvent::MouseButton) {
            if (e.button == MouseButton::Middle) {
                orbiting = e.pressed;
            } else if (e.button == MouseButton::Right) {
                panning = e.pressed;
            } else if (e.button == MouseButton::Left && e.modifiers & KeyMod::Alt) {
                orbiting = e.pressed;
            }
        } else if (e.type == InputEvent::MouseMove) {
            Vec2 delta = e.position - lastMousePos;
            if (orbiting) orbit(delta);
            else if (panning) pan(delta);
            lastMousePos = e.position;
        } else if (e.type == InputEvent::Scroll) {
            zoom(e.scroll.y);
        }
    }
    
    void orbit(const Vec2& delta) {
        // Spherical coordinates around target
    }
    
    void pan(const Vec2& delta) {
        // Move target in view plane
    }
    
    void zoom(float delta) {
        // Dolly camera or adjust ortho size
    }
    
    // Picking
    std::optional<PickResult> pick(float x, float y, float width, float height) {
        // 1. Coarse pick (BVH sphere test on features)
        // 2. Fine pick (BVH ray-mesh test)
        // Return {featureId, faceId, edgeId, vertexId, worldPos, normal}
    }
};
```

---

## TransformGizmo

```cpp
class TransformGizmo {
    enum class Mode { Translate, Rotate, Scale };
    Mode mode = Mode::Translate;
    enum class Space { World, Local };
    Space space = Space::World;
    
    bool active = false;
    FeatureId targetFeature;
    Vec3 startPosition;
    Quat startRotation;
    Vec3 startScale;
    int activeAxis = -1;  // 0=X, 1=Y, 2=Z, 3=XY, 4=YZ, 5=ZX
    
    void render(const Camera& camera, const Vec3& targetPos, const Quat& targetRot) {
        // Draw axes/handles
        // Handle input
        // Return delta transform
    }
    
    std::optional<Transform> getDelta() const {
        if (!active) return std::nullopt;
        return Transform{deltaPos, deltaRot, deltaScale};
    }
};
```

---

## SelectionManager

```cpp
class SelectionManager {
    std::unordered_set<FeatureId> selected;
    FeatureId active = kInvalidFeatureId;  // Last selected
    
    void select(FeatureId id, bool additive = false) {
        if (!additive) selected.clear();
        selected.insert(id);
        active = id;
    }
    
    void deselect(FeatureId id) {
        selected.erase(id);
        if (active == id) active = selected.empty() ? kInvalidFeatureId : *selected.begin();
    }
    
    void clear() { selected.clear(); active = kInvalidFeatureId; }
    
    bool hasSelection() const { return !selected.empty(); }
    FeatureId primary() const { return active; }
    const std::unordered_set<FeatureId>& all() const { return selected; }
    
    // Multi-level: object/face/edge/vertex
    enum class Level { Object, Face, Edge, Vertex };
    Level level = Level::Object;
    std::unordered_set<uint32_t> subSelection;  // Face/edge/vertex indices
};
```

---

## CommandDispatcher

```cpp
class CommandDispatcher {
    std::unordered_map<std::string, std::function<void(AppContext&)>> commands;
    
    void registerCommand(const std::string& name, std::function<void(AppContext&)> fn) {
        commands[name] = std::move(fn);
    }
    
    bool execute(const std::string& name, AppContext& ctx) {
        auto it = commands.find(name);
        if (it != commands.end()) {
            it->second(ctx);
            return true;
        }
        return false;
    }
    
    // Input mapping
    struct KeyBinding { std::string command; Key key; KeyMod mods; };
    std::vector<KeyBinding> bindings;
    
    void onKey(AppContext& ctx, Key key, KeyMod mods, bool pressed) {
        for (auto& b : bindings) {
            if (b.key == key && b.mods == mods && pressed) {
                execute(b.command, ctx);
            }
        }
    }
};
```

### Default Key Bindings

| Key | Mod | Command |
|-----|-----|---------|
| `Escape` | - | `mode.select` |
| `S` | - | `mode.sketch` |
| `E` | - | `mode.extrude` |
| `R` | - | `mode.revolve` |
| `F` | - | `mode.fillet` |
| `C` | - | `mode.chamfer` |
| `B` | - | `mode.boolean` |
| `G` | - | `gizmo.translate` |
| `R` | - | `gizmo.rotate` |
| `S` | Shift | `gizmo.scale` |
| `1` | - | `primitive.box` |
| `2` | - | `primitive.cylinder` |
| `3` | - | `primitive.sphere` |
| `4` | - | `primitive.cone` |
| `5` | - | `primitive.torus` |
| `W` | - | `viewport.wireframe` |
| `Z` | - | `viewport.shaded` |
| `Grid` | - | `viewport.grid` |
| `A` | - | `viewport.axes` |

---

## WorkspaceManager

```cpp
class WorkspaceManager {
    std::filesystem::path workspaceRoot;
    std::string currentProject;
    
    bool createProject(const std::string& name);
    bool openProject(const std::string& name);
    bool saveProject();
    bool closeProject();
    
    // Document management
    std::unique_ptr<CadDocument> newDocument();
    bool openDocument(const std::filesystem::path& path);
    bool saveDocument(const CadDocument& doc, const std::filesystem::path& path);
    bool saveDocumentAs(const CadDocument& doc);
    
    // Recent files
    std::vector<std::string> recentProjects;
    std::vector<std::string> recentDocuments;
};
```

---

## AssetBrowser

```cpp
class AssetBrowser {
    std::filesystem::path assetRoot;
    std::vector<AssetEntry> entries;
    
    struct AssetEntry {
        std::filesystem::path path;
        AssetType type;  // Mesh, Texture, Material, Scene, Script
        std::string preview;  // Thumbnail path
        FileTime modified;
    };
    
    void refresh();
    void filter(AssetType type, const std::string& search);
    void render(AppContext& ctx);
    
    // Drag-drop
    bool beginDragDropSource(const AssetEntry& entry);
    bool acceptDragDropPayload(const std::filesystem::path& path);
};
```

---

## Integration with Kernel

### CadViewer (Visual Representation)

```cpp
class CadViewer {
    CadDocument& document;
    std::unordered_map<FeatureId, Mesh> featureMeshes;
    std::unordered_map<FeatureId, BVH> featureBVHs;
    
    void regenerate(FeatureId fid) {
        auto* node = document.history().node(fid);
        if (node && node->feature) {
            node->feature->regenerate(document);
            if (node->mesh) {
                featureMeshes[fid] = *node->mesh;
                featureBVHs[fid] = BVH::build(*node->mesh);
            }
        }
    }
    
    void render(RenderContext& ctx, const Viewport& viewport) {
        for (auto& [fid, mesh] : featureMeshes) {
            auto* node = document.history().node(fid);
            if (!node || node->deleted || node->hidden) continue;
            
            // Draw mesh with material
            ctx.renderer->drawMesh(mesh, node->transform, node->material);
        }
    }
};
```

---

*Kernel v0.1.0-dev | 2026 tests passing | C++26 | Vulkan 1.3*