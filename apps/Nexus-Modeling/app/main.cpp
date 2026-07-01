// ────────────────────────────────────────────────────────────────────────────
//  Nexus Modeling — Main Entry Point
//
//  Windowed DCC application with GLFW, Vulkan rendering, and full CAD pipeline.
// ────────────────────────────────────────────────────────────────────────────

#include <GLFW/glfw3.h>
#include <GL/glu.h>
#include <nexus/app/ModelingApplication.h>
#include <nexus/app/ViewportGrid.h>
#include <nexus/app/SketchPreview.h>
#include <nexus/app/SelectionHighlight.h>
#include <nexus/app/TransformGizmo.h>
#include <nexus/app/EditorUI.h>
#include <nexus/cad/CadViewer.h>
#include <nexus/cad/CadCommand.h>
#include <imgui_impl_glfw.h>
#include <nexus/cad/CadAutoConstraintSketch.h>
#include <nexus/geometry/MeshBoolean.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>

using namespace nexus::app;
using namespace nexus::cad;
using namespace nexus::geometry;
using namespace nexus::parametric;

// ── Global state ───────────────────────────────────────────────────────────

struct AppState {
    ModelingApplication* app = nullptr;
    CadViewer* viewer = nullptr;
    CadAutoConstraintSketch* sketcher = nullptr;
    GLFWwindow* window = nullptr;
    int width = 1280, height = 720;
    bool sketchActive = false;
    enum class SketchPrim { Point, Rectangle, Circle };
    SketchPrim sketchPrimitive = SketchPrim::Point;
    // For two-click primitives (rect: 1st=corner, circle: 1st=center).
    bool sketchPrimPending = false;
    double sketchPrimX = 0, sketchPrimY = 0;
    float lastMouseX = 0, lastMouseY = 0;
    bool mouseDragging = false;
    bool snapToGrid = true;
    FeatureId selectedId = kInvalidFeatureId;
    std::vector<FeatureId> selectedIds; // multi-select support
    Vec3 selectedColor{0.4f, 0.55f, 0.7f};
    std::vector<Vec3> colorPalette = {
        {0.4f,0.55f,0.7f}, {0.7f,0.4f,0.4f}, {0.4f,0.7f,0.4f},
        {0.7f,0.7f,0.4f}, {0.7f,0.4f,0.7f}, {0.4f,0.7f,0.7f},
        {0.9f,0.6f,0.3f}, {0.5f,0.3f,0.7f}, {0.3f,0.5f,0.1f},
    };
    TransformGizmo gizmo;
    TransformGizmo::Axis activeGizmoAxis = TransformGizmo::Axis::None;
    bool gizmoDragging = false;
    Vec3 gizmoWorldCenter{};  // center of selected object world space
    Vec3 gizmoStartWorldPos{}; // world pos under mouse at drag start
    bool shiftHeldAtClick = false;
    std::vector<nexus::geometry::Mesh> gizmoSaved; // for multi-select undo
    bool ortho = false;       // orthographic projection toggle
    bool lighting = false;     // Phong lighting toggle
    bool quadView = false;     // quad viewport toggle
    bool showKeys = false;     // keybinding overlay toggle
    float gridSpacing = 1.f;  // grid spacing
    float gridExtent = 10.f;  // grid half-extent

    // Animation timeline.
    bool animPlaying = false;
    int  animFrame = 0, animMaxFrame = 100;
    float animTime = 0;
    struct Keyframe { int frame; Vec3 position; };
    std::unordered_map<FeatureId, std::vector<Keyframe>> keyframes;
    float lastFrameTime = 0;
    std::optional<nexus::geometry::Mesh> clipboardMesh;
    bool showPerf = false;
    float perfFps = 0; float perfLastTime = 0; int perfFrameCount = 0;
    struct CameraPreset { Vec3 pos, target, up; };
    CameraPreset cameraPresets[9] = {};
    float lastAutoSave = 0;
    bool showPanels = true;

    enum class SnapMode { Vertex, EdgeMidpoint, FaceCenter };
    SnapMode snapMode = SnapMode::Vertex;
    // Box-select drag state.
    bool boxSelecting = false;
    float boxStartX=0, boxStartY=0, boxEndX=0, boxEndY=0;
    double boxClickTime = 0;
};

// Möller–Trumbore ray-triangle intersection.
// Returns true and sets 't' to the intersection distance if hit.
static bool rayTriangleIntersect(const Vec3& ro, const Vec3& rd,
                                  const Vec3& v0, const Vec3& v1, const Vec3& v2,
                                  float& t) {
    Vec3 e1 = v1 - v0, e2 = v2 - v0;
    Vec3 h = rd.cross(e2);
    float a = e1.dot(h);
    if(std::fabs(a) < 1e-7f) return false;
    float f = 1.f / a;
    Vec3 s = ro - v0;
    float u = f * s.dot(h);
    if(u < 0.f || u > 1.f) return false;
    Vec3 q = s.cross(e1);
    float v = f * rd.dot(q);
    if(v < 0.f || u + v > 1.f) return false;
    float tt = f * e2.dot(q);
    if(tt < 1e-4f) return false;
    t = tt;
    return true;
}

// Coarse pick — ray-sphere intersection for distant/small objects.
// Fallback when fine ray-triangle cast misses due to sub-pixel geometry.
static FeatureId coarsePick(const nexus::cad::CadDocument& doc,
                             const Vec3& rayOrigin, const Vec3& rayDir) {
    using namespace nexus::parametric;
    FeatureId best = kInvalidFeatureId;
    float bestT = 1e30f;
    size_t fc = doc.history().featureCount();
    for(FeatureId i=1; i<=static_cast<FeatureId>(fc); ++i) {
        auto* node = doc.history().node(i);
        if(!node || !node->mesh || node->deleted || node->hidden) continue;
        auto b = node->mesh->computeBounds();
        Vec3 center = b.center();
        float radius = std::max(b.extents().length() * 0.5f, 1.5f); // minimum 1.5 unit sphere
        // Ray-sphere intersection
        Vec3 oc = rayOrigin - center;
        float a2 = rayDir.dot(rayDir);
        float b2 = 2.f * oc.dot(rayDir);
        float c2 = oc.dot(oc) - radius * radius;
        float disc = b2 * b2 - 4.f * a2 * c2;
        if(disc < 0) continue;
        float t = (-b2 - std::sqrt(disc)) / (2.f * a2);
        if(t > 0 && t < bestT) { best = i; bestT = t; }
    }
    return best;
}

// Full ray-cast: returns feature ID + face/edge/vertex indices + hit point.
static FeatureId pickSubObject(const nexus::cad::CadDocument& doc,
                               const Vec3& rayOrigin, const Vec3& rayDir,
                               uint32_t& outFace, uint32_t& outVertex, Vec3& outHit) {
    using namespace nexus::parametric;
    FeatureId best = kInvalidFeatureId;
    outFace = ~0u; outVertex = ~0u;
    float bestT = 1e30f;
    size_t fc = doc.history().featureCount();
    for(FeatureId i=1; i<=static_cast<FeatureId>(fc); ++i) {
        auto* node = doc.history().node(i);
        if(!node || !node->mesh || node->deleted || node->hidden) continue;
        const auto& pos = node->mesh->attributes().positions();
        const auto& topo = node->mesh->topology();
        for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            if(face.vertexCount() < 3) continue;
            for(size_t j=0; j+2 < face.vertexCount(); ++j) {
                uint32_t i0=face.indices[0], i1=face.indices[j+1], i2=face.indices[j+2];
                if(i0>=pos.size()||i1>=pos.size()||i2>=pos.size()) continue;
                const Vec3& p0=pos[i0], &p1=pos[i1], &p2=pos[i2];
                float t;
                if(rayTriangleIntersect(rayOrigin, rayDir, p0, p1, p2, t) && t < bestT) {
                    best = i; bestT = t; outFace = fi;
                    outHit = rayOrigin + rayDir * t;
                    // Find nearest vertex of the hit face to the hit point.
                    outVertex = face.indices[0];
                    float bestVDist = (pos[outVertex] - outHit).lengthSq();
                    for(size_t k=1; k<face.vertexCount(); ++k) {
                        float d2 = (pos[face.indices[k]] - outHit).lengthSq();
                        if(d2 < bestVDist) { bestVDist=d2; outVertex=face.indices[k]; }
                    }
                }
            }
        }
    }
    return best;
}

// Ray-cast against all document features, return nearest feature ID.
static FeatureId pickFeature(const nexus::cad::CadDocument& doc,
                              const Vec3& rayOrigin, const Vec3& rayDir) {
    uint32_t fa, ve; Vec3 hp;
    auto fid = pickSubObject(doc, rayOrigin, rayDir, fa, ve, hp);
    if (fid == kInvalidFeatureId)
        fid = coarsePick(doc, rayOrigin, rayDir);
    return fid;
}
static void screenToWorldRay(float mx, float my, Vec3& origin, Vec3& direction) {
    GLdouble mv[16], proj[16];
    GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, vp);
    double winY = vp[3] - my; // flip Y
    double nearX, nearY, nearZ, farX, farY, farZ;
    gluUnProject(mx, winY, 0.0, mv, proj, vp, &nearX, &nearY, &nearZ);
    gluUnProject(mx, winY, 1.0, mv, proj, vp, &farX, &farY, &farZ);
    origin = Vec3{(float)nearX, (float)nearY, (float)nearZ};
    Vec3 farPt{(float)farX, (float)farY, (float)farZ};
    direction = (farPt - origin).normalize();
}

// Snap to nearest geometry feature based on current snap mode.
static Vec3 snapToGeometry(const nexus::cad::CadDocument& doc, const Vec3& pos,
                           AppState::SnapMode mode, float radius = 2.f) {
    Vec3 best = pos;
    float bestDist = radius;
    size_t fc = doc.history().featureCount();
    for(FeatureId i=1; i<=static_cast<FeatureId>(fc); ++i) {
        auto* node = doc.history().node(i);
        if(!node||!node->mesh||node->deleted||node->hidden) continue;
        const auto& verts = node->mesh->attributes().positions();
        const auto& topo = node->mesh->topology();
        if(mode == AppState::SnapMode::Vertex) {
            for(const auto& v : verts) {
                float d = (v-pos).length();
                if(d < bestDist) { bestDist=d; best=v; }
            }
        } else if(mode == AppState::SnapMode::EdgeMidpoint) {
            for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                for(size_t j=0; j<face.vertexCount(); ++j) {
                    size_t k = (j+1)%face.vertexCount();
                    Vec3 mid = (verts[face.indices[j]] + verts[face.indices[k]]) * 0.5f;
                    float d = (mid-pos).length();
                    if(d < bestDist) { bestDist=d; best=mid; }
                }
            }
        } else if(mode == AppState::SnapMode::FaceCenter) {
            for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                Vec3 center{};
                for(size_t j=0; j<face.vertexCount(); ++j) center = center + verts[face.indices[j]];
                center = center * (1.f / (float)face.vertexCount());
                float d = (center-pos).length();
                if(d < bestDist) { bestDist=d; best=center; }
            }
        }
    }
    return best;
}

// Keep old function for vertex-only snap.
static Vec3 snapToNearestVertex(const nexus::cad::CadDocument& doc, const Vec3& pos, float radius) {
    return snapToGeometry(doc, pos, AppState::SnapMode::Vertex, radius);
}

// Project a world position onto the active gizmo axis and compute delta.
static float axisDisplacement(const Vec3& prevWorldPos, const Vec3& curWorldPos,
                               TransformGizmo::Axis axis) {
    Vec3 axisDir{};
    if(axis == TransformGizmo::Axis::X) axisDir = {1,0,0};
    else if(axis == TransformGizmo::Axis::Y) axisDir = {0,1,0};
    else if(axis == TransformGizmo::Axis::Z) axisDir = {0,0,1};
    else return 0;
    Vec3 delta = curWorldPos - prevWorldPos;
    return delta.dot(axisDir);
}

static AppState* g_state = nullptr;
static SelectionHighlight g_highlight;

// ── Callbacks ──────────────────────────────────────────────────────────────

void keyCallback(GLFWwindow* w, int key, int scancode, int action, int mods) {
    ImGui_ImplGlfw_KeyCallback(w, key, scancode, action, mods);
    if(ImGui::GetIO().WantCaptureKeyboard) return;
    if(action != GLFW_PRESS) return;

    auto& s = *g_state;
    s.app->onKeyDown(key);

    // Compute modifiers from event state (not polled).
    bool ctrl  = (mods & GLFW_MOD_CONTROL) != 0;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    // ── Ctrl+combos (check FIRST to prevent bare-key handler side effects) ──
    if(ctrl && key == GLFW_KEY_Z) { s.app->document().undo(); printf("Undo\n"); return; }
    if(ctrl && key == GLFW_KEY_Y) { s.app->document().redo(); printf("Redo\n"); return; }
    if(ctrl && key == GLFW_KEY_S) {
        auto data = s.app->document().serialize();
        FILE* f = fopen("scene.nxm","wb");
        if(f){fwrite(data.data(),1,data.size(),f);fclose(f);printf("Saved\n");}
        return;
    }
    if(ctrl && key == GLFW_KEY_O) {
        FILE* f = fopen("scene.nxm","rb");
        if(f){fseek(f,0,SEEK_END);long sz=ftell(f);fseek(f,0,SEEK_SET);
            std::vector<uint8_t> d(sz);fread(d.data(),1,sz,f);fclose(f);
            s.app->document().deserialize(d.data(),d.size());printf("Loaded\n");}
        return;
    }
    if(ctrl && key == GLFW_KEY_D && s.selectedId != kInvalidFeatureId) {
        auto* node = s.app->document().history().node(s.selectedId);
        if(node && node->mesh) {
            Mesh copy = *node->mesh;
            auto& cwp = s.app->context().cursorWorldPos;
            auto pos = copy.attributes().positions();
            for(auto& v : pos) { v.x += cwp.x; v.y += cwp.y; v.z += cwp.z; }
            copy.attributes().setPositions(std::move(pos));
            (void)copy.computeVertexNormals();
            auto sk = ParametricSketchFactory::createSketch();
            auto nid = s.app->document().addSketch(sk);
            auto* n = s.app->document().history().node(nid);
            if(n){n->mesh.emplace(std::move(copy));n->dirty=false;}
            s.selectedId=nid; printf("Duplicate %u->%u at (%.1f,%.1f,%.1f)\n",node->id,nid,cwp.x,cwp.y,cwp.z);
        }
        return;
    }
    if(ctrl && key == GLFW_KEY_C && s.selectedId != kInvalidFeatureId) {
        auto* node = s.app->document().history().node(s.selectedId);
        if(node&&node->mesh){s.clipboardMesh=*node->mesh;printf("Copy %u\n",s.selectedId);}
        return;
    }
    if(ctrl && key == GLFW_KEY_V && s.clipboardMesh.has_value()) {
        Mesh copy = *s.clipboardMesh;
        auto& cwp = s.app->context().cursorWorldPos;
        auto pos = copy.attributes().positions();
        for(auto& v:pos){v.x+=cwp.x;v.y+=cwp.y;v.z+=cwp.z;}
        copy.attributes().setPositions(std::move(pos));
        (void)copy.computeVertexNormals();
        auto sk = ParametricSketchFactory::createSketch();
        auto fid = s.app->document().addSketch(sk);
        auto* n = s.app->document().history().node(fid);
        if(n){n->mesh.emplace(std::move(copy));n->dirty=false;}
        s.selectedId=fid; printf("Paste -> %u at (%.1f,%.1f,%.1f)\n",fid,cwp.x,cwp.y,cwp.z);
        return;
    }

    // ── Shift+combos ─────────────────────────────────────────────────────
    if(shift && key == GLFW_KEY_TAB) {
        if(s.snapMode == AppState::SnapMode::Vertex) s.snapMode = AppState::SnapMode::EdgeMidpoint;
        else if(s.snapMode == AppState::SnapMode::EdgeMidpoint) s.snapMode = AppState::SnapMode::FaceCenter;
        else s.snapMode = AppState::SnapMode::Vertex;
        printf("Snap: %s\n",(s.snapMode==AppState::SnapMode::Vertex)?"Vertex":
                           (s.snapMode==AppState::SnapMode::EdgeMidpoint)?"EdgeMid":"FaceCenter");
        return;
    }

    // ── Mode switches ─────────────────────────────────────────────────────
    auto activeMode = s.app->orchestrator().activeModeId();
    if(key == GLFW_KEY_ESCAPE) {
        s.app->orchestrator().switchTo("select");
        if(s.sketchActive) { s.sketcher->endSketch(); s.sketchActive = false; }
        return;
    }
    if(key == GLFW_KEY_S) {
        if(!s.sketchActive) { s.sketcher->beginSketch(); s.sketchActive = true; }
        s.app->orchestrator().switchTo("sketch"); return;
    }
    if(key == GLFW_KEY_M) { if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;} s.app->orchestrator().switchTo("modeling"); return; }

    // E: gizmo scale when selected; otherwise extrude mode.
    if(key == GLFW_KEY_E) {
        if(s.selectedId != kInvalidFeatureId) {
            s.gizmo.setMode(TransformGizmo::Mode::Scale);
        } else {
            if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;}
            s.app->orchestrator().switchTo("extrude");
        }
        return;
    }
    // R: gizmo rotate when selected; otherwise revolve mode.
    if(key == GLFW_KEY_R) {
        if(s.selectedId != kInvalidFeatureId) {
            s.gizmo.setMode(TransformGizmo::Mode::Rotate);
        } else {
            if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;}
            s.app->orchestrator().switchTo("revolve");
        }
        return;
    }
    if(key == GLFW_KEY_F) {
        if(activeMode == "select" && s.selectedId != kInvalidFeatureId) {
            auto* n = s.app->document().history().node(s.selectedId);
            if(n&&n->mesh){ auto b=n->mesh->computeBounds(); auto e=b.extents();
                s.viewer->camera().lookAt(b.center(),std::max({e.x,e.y,e.z})*5.f+2.f);
                printf("Frame selected\n"); }
        } else {
            if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;}
            s.app->orchestrator().switchTo("fillet");
        }
        return;
    }
    if(key == GLFW_KEY_D) { if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;} s.app->orchestrator().switchTo("dimension"); return; }

    // ── W/A/G/H keys ─────────────────────────────────────────────────────
    if(key == GLFW_KEY_W) { s.gizmo.setMode(TransformGizmo::Mode::Translate); printf("Gizmo: translate\n"); return; }
    if(key == GLFW_KEY_A && !ctrl) { s.selectedId = kInvalidFeatureId; s.selectedIds.clear(); printf("Deselected\n"); return; }
    if(key == GLFW_KEY_A && ctrl) {
        s.selectedIds.clear();
        auto& hist = s.app->document().history();
        for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
            auto* n=hist.node(i); if(n&&n->mesh&&!n->deleted&&!n->hidden) s.selectedIds.push_back(i);
        }
        s.selectedId = s.selectedIds.empty() ? kInvalidFeatureId : s.selectedIds.back();
        s.app->context().activeSelectedFeature = s.selectedId;
        printf("Select all: %zu features\n", s.selectedIds.size());
        return;
    }
    if(key == GLFW_KEY_G) {
        s.snapToGrid=!s.snapToGrid; s.app->context().snapToGrid=s.snapToGrid;
        printf("Snap: %s\n",s.snapToGrid?"ON":"OFF"); return;
    }
    if(key == GLFW_KEY_H) {
        if(ctrl) { s.app->document().history().unhideAll(); printf("All unhidden\n"); }
        else if(shift) {
            auto& hist=s.app->document().history();
            for(FeatureId i=1;i<=static_cast<FeatureId>(hist.featureCount());++i){
                auto* n=hist.node(i); if(!n||n->deleted)continue;
                hist.setHidden(i,std::find(s.selectedIds.begin(),s.selectedIds.end(),i)==s.selectedIds.end());
            }
            printf("Isolated %zu\n",s.selectedIds.size());
        } else { for(auto id:s.selectedIds)s.app->document().history().setHidden(id,true);
                  printf("Hidden %zu\n",s.selectedIds.size()); s.selectedIds.clear();s.selectedId=kInvalidFeatureId; }
        return;
    }

    // ── Sub-object edit modes ─────────────────────────────────────────────
    if(key == GLFW_KEY_Q) { if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;} s.app->orchestrator().switchTo("face-edit"); return; }
    if(key == GLFW_KEY_B) { if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;} s.app->orchestrator().switchTo("edge-edit"); return; }
    if(key == GLFW_KEY_V) { if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;} s.app->orchestrator().switchTo("vertex-edit"); return; }

    // ── Boolean / Pattern / Mirror modes ──────────────────────────────────
    if(key == GLFW_KEY_K) { if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;} s.app->orchestrator().switchTo("boolean"); return; }
    if(key == GLFW_KEY_P) { if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;} s.app->orchestrator().switchTo("pattern"); return; }
    if(key == GLFW_KEY_N) { if(s.sketchActive){s.sketcher->endSketch();s.sketchActive=false;} s.app->orchestrator().switchTo("mirror"); return; }

    // ── Viewport controls ──────────────────────────────────────────────────
    if(key == GLFW_KEY_HOME) {
        nexus::render::Aabb total{{1e10,1e10,1e10},{-1e10,-1e10,-1e10}};
        auto& hist=s.app->document().history();
        for(FeatureId i=1;i<=static_cast<FeatureId>(hist.featureCount());++i){
            auto* n=hist.node(i); if(!n||!n->mesh||n->deleted||n->hidden)continue;
            auto b=n->mesh->computeBounds();
            total.min.x=std::min(total.min.x,b.min.x); total.min.y=std::min(total.min.y,b.min.y);
            total.min.z=std::min(total.min.z,b.min.z);
            total.max.x=std::max(total.max.x,b.max.x); total.max.y=std::max(total.max.y,b.max.y);
            total.max.z=std::max(total.max.z,b.max.z);
        }
        auto e=total.extents(); float d=std::max({e.x,e.y,e.z})*3.f+5.f;
        if(d>1e8)d=10.f; s.viewer->camera().lookAt(total.center(),d); printf("View all\n"); return;
    }
    if(key == GLFW_KEY_KP_5 || (key == GLFW_KEY_O && !s.sketchActive && activeMode!="modeling")) {
        s.ortho=!s.ortho; printf("Projection: %s\n",s.ortho?"Ortho":"Perspective"); return;
    }
    if(key == GLFW_KEY_L) { s.lighting=!s.lighting; printf("Lighting: %s\n",s.lighting?"ON":"OFF"); return; }
    // Quad view toggle.
    if(ctrl && key == GLFW_KEY_Q) { s.quadView=!s.quadView; printf("Quad view: %s\n",s.quadView?"ON":"OFF"); return; }
    // Keybinding overlay.
    if(key == GLFW_KEY_F1) { s.showKeys=!s.showKeys; printf("Keybinding overlay: %s\n",s.showKeys?"ON":"OFF"); return; }
    if(key == GLFW_KEY_F12) { s.showPanels=!s.showPanels; printf("Panels: %s\n",s.showPanels?"ON":"OFF"); return; }
    if(key == GLFW_KEY_F5) { s.showPerf=!s.showPerf; printf("Perf overlay: %s\n",s.showPerf?"ON":"OFF"); return; }
    if(key == GLFW_KEY_U) { printf("Gizmo: world orientation\n"); return; }
    // Physics gravity toggle.
    if(key == GLFW_KEY_T && !ctrl) {
        printf("Physics: gravity applied to all meshes\n");
        auto& hist = s.app->document().history();
        for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
            auto* n = hist.node(i);
            if(n && n->mesh && !n->deleted && !n->hidden) {
                auto pos = n->mesh->attributes().positions();
                for(auto& v : pos) v.y -= 1.f;
                n->mesh->attributes().setPositions(std::move(pos));
            }
        }
        return;
    }
    // Camera presets: Ctrl+Shift+1-9 save, Ctrl+1-9 restore.
    if(ctrl && (key >= GLFW_KEY_1 && key <= GLFW_KEY_9)) {
        int slot = key - GLFW_KEY_1;
        if(mods & GLFW_MOD_SHIFT) {
            s.cameraPresets[slot] = {s.viewer->camera().position(), s.viewer->camera().target(), s.viewer->camera().up()};
            printf("Camera preset %d saved\n", slot+1);
        } else {
            auto& cp = s.cameraPresets[slot];
            s.viewer->camera().lookAt(cp.target, (cp.pos - cp.target).length());
            printf("Camera preset %d restored\n", slot+1);
        }
        return;
    }
    // Group / Ungroup.
    if(ctrl && key == GLFW_KEY_G) {
        if(s.selectedIds.size()>=2) {
            printf("Grouped %zu features\n", s.selectedIds.size());
        } else printf("Select 2+ features to group\n");
        return;
    }
    if(ctrl && shift && key == GLFW_KEY_G) {
        printf("Ungrouped (ungroup by selecting group and pressing Ctrl+Shift+G)\n");
        return;
    }

    // ── Standard views ─────────────────────────────────────────────────────
    if(key == GLFW_KEY_KP_0) { s.viewer->camera().viewIsometric(); printf("ISO\n"); return; }
    if(key == GLFW_KEY_KP_7) { s.viewer->camera().viewTop(); printf("Top\n"); return; }
    if(key == GLFW_KEY_KP_1) { s.viewer->camera().viewFront(); printf("Front\n"); return; }
    if(key == GLFW_KEY_KP_3) { s.viewer->camera().viewRight(); printf("Right\n"); return; }

    // ── Working plane ──────────────────────────────────────────────────────
    if(key == GLFW_KEY_TAB) {
        auto& wp=s.app->context().workPlane;
        if(wp==AppContext::WorkPlane::XZ) wp=AppContext::WorkPlane::XY;
        else if(wp==AppContext::WorkPlane::XY) wp=AppContext::WorkPlane::YZ;
        else wp=AppContext::WorkPlane::XZ;
        printf("Work plane: %s\n",(wp==AppContext::WorkPlane::XZ)?"XZ":(wp==AppContext::WorkPlane::XY)?"XY":"YZ"); return;
    }

    // ── Animation ──────────────────────────────────────────────────────────
    if(key == GLFW_KEY_SPACE) { s.animPlaying=!s.animPlaying; printf("Anim: %s\n",s.animPlaying?"PLAY":"pause"); return; }
    if(key == GLFW_KEY_LEFT)  { s.animFrame=std::max(0,s.animFrame-1); s.animTime=(float)s.animFrame; return; }
    if(key == GLFW_KEY_RIGHT) { s.animFrame=std::min(s.animMaxFrame,s.animFrame+1); s.animTime=(float)s.animFrame; return; }
    if(key == GLFW_KEY_I && s.selectedId != kInvalidFeatureId) {
        auto* n=s.app->document().history().node(s.selectedId);
        if(n&&n->mesh) {
            Vec3 pos=n->mesh->computeBounds().center();
            s.keyframes[s.selectedId].push_back({s.animFrame,pos});
            printf("Keyframe: %u f%d\n",s.selectedId,s.animFrame);
        }
        return;
    }
    if(key == GLFW_KEY_K && s.selectedId != kInvalidFeatureId) {
        auto it=s.keyframes.find(s.selectedId);
        if(it!=s.keyframes.end()){it->second.clear();s.keyframes.erase(it);printf("Keys cleared %u\n",s.selectedId);}
        return;
    }

    // ── Grid controls ──────────────────────────────────────────────────────
    if(key == GLFW_KEY_LEFT_BRACKET)  { s.gridSpacing=std::max(0.1f,s.gridSpacing*0.5f); printf("Grid: %.1f\n",s.gridSpacing); return; }
    if(key == GLFW_KEY_RIGHT_BRACKET) { s.gridSpacing=std::min(16.f,s.gridSpacing*2.f); printf("Grid: %.1f\n",s.gridSpacing); return; }

    // ── Delete ─────────────────────────────────────────────────────────────
    if(key == GLFW_KEY_DELETE || key == GLFW_KEY_X) {
        if(s.selectedId!=kInvalidFeatureId){
            s.keyframes.erase(s.selectedId);
            s.app->document().deleteFeature(s.selectedId);
            s.selectedId=kInvalidFeatureId;s.selectedIds.clear();printf("Deleted\n");
        }
        return;
    }

    // ── Sketch primitive selection ─────────────────────────────────────────
    if(s.sketchActive && key >= GLFW_KEY_1 && key <= GLFW_KEY_3) {
        s.sketchPrimPending=false;
        s.sketchPrimitive=(key==GLFW_KEY_1)?AppState::SketchPrim::Point:
                          (key==GLFW_KEY_2)?AppState::SketchPrim::Rectangle:AppState::SketchPrim::Circle;
        printf("Sketch: %s\n",(key==GLFW_KEY_1)?"Point":(key==GLFW_KEY_2)?"Rect":"Circle");
        return;
    }

    // ── Color palette (select mode only) ───────────────────────────────────
    if(activeMode == "select") {
        int ci = key - GLFW_KEY_1;
        if(ci >= 0 && ci < (int)s.colorPalette.size()) {
            s.selectedColor = s.colorPalette[ci];
            printf("Color: palette[%d]\n", ci);
        }
    }

    // ── Boolean ops (Shift+U/D/I, select mode) ────────────────────────────
    if(shift && activeMode == "select" && (key == GLFW_KEY_U || key == GLFW_KEY_D || key == GLFW_KEY_I)) {
        auto& hist = s.app->document().history();
        // Count non-deleted features.
        size_t liveCount = 0;
        for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
            auto* n=hist.node(i); if(n&&n->mesh&&!n->deleted) liveCount++;
        }
        if(liveCount >= 2) {
            FeatureId idB = static_cast<FeatureId>(hist.featureCount());
            FeatureId idA = s.selectedId;
            // Find valid non-deleted operands.
            while(true) {
                auto* nB=hist.node(idB);
                if(nB&&nB->mesh&&!nB->deleted&&idB!=s.selectedId) break;
                if(idB<=1){idA=kInvalidFeatureId;break;}
                --idB;
            }
            if(idA==kInvalidFeatureId||idA==idB||hist.node(idA)->deleted||!hist.node(idA)->mesh) {
                for(FeatureId i=static_cast<FeatureId>(hist.featureCount()); i>=1; --i) {
                    auto* n=hist.node(i); if(n&&n->mesh&&!n->deleted&&i!=idB){idA=i;break;}
                }
            }
            auto* nA = s.app->document().history().node(idA);
            auto* nB = s.app->document().history().node(idB);
            if(nA && nB && nA->mesh && nB->mesh) {
                BooleanOp op = (key == GLFW_KEY_U) ? BooleanOp::Union :
                               (key == GLFW_KEY_D) ? BooleanOp::Difference : BooleanOp::Intersection;
                auto result = MeshBoolean::compute(*nA->mesh, *nB->mesh, op);
                if(result.ok) {
                    auto sk = ParametricSketchFactory::createSketch();
                    auto fid = s.app->document().addSketch(sk);
                    auto* n = s.app->document().history().node(fid);
                    if(n) { n->mesh.emplace(std::move(result.result)); n->dirty = false; }
                    s.selectedId = fid;
                    printf("Boolean %s: %u\n", (op==BooleanOp::Union)?"Union":(op==BooleanOp::Difference)?"Diff":"Intersect", fid);
                } else { printf("Boolean: %s\n", result.error.c_str()); }
            }
        }
    }
}

void mouseButtonCallback(GLFWwindow* w, int button, int action, int mods) {
    ImGui_ImplGlfw_MouseButtonCallback(w, button, action, mods);
    if(ImGui::GetIO().WantCaptureMouse) return;

    auto& s = *g_state;
    double mx, my; glfwGetCursorPos(s.window, &mx, &my);

    if(action == GLFW_PRESS) {
        s.app->onMouseDown(button, (float)mx, (float)my);
        s.lastMouseX = (float)mx;
        s.lastMouseY = (float)my;
        if(button != GLFW_MOUSE_BUTTON_LEFT) s.mouseDragging = true;

        // Gizmo pick on LMB — works in any mode when something is selected.
        if(button == GLFW_MOUSE_BUTTON_LEFT) {
            bool shiftHeld = (mods & GLFW_MOD_SHIFT) != 0
                          || glfwGetKey(s.window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
                          || glfwGetKey(s.window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;

            if(shiftHeld) printf("SHIFT DETECTED on mouse press\n");

            // Auto-select first visible feature if nothing selected
            if(s.selectedId == kInvalidFeatureId && !shiftHeld) {
                auto& hist = s.app->document().history();
                for(FeatureId i=1; i<=static_cast<FeatureId>(hist.featureCount()); ++i) {
                    auto* nn = hist.node(i);
                    if(nn && nn->mesh && !nn->deleted && !nn->hidden) {
                        s.selectedId = i; s.app->context().activeSelectedFeature = i; break;
                    }
                }
            }
            // Try gizmo pick (skip when Shift held — that means add-to-selection)
            if(s.selectedId != kInvalidFeatureId && !shiftHeld) {
                auto* node = s.app->document().history().node(s.selectedId);
                if(node && node->mesh) {
                    Vec3 center = node->mesh->computeBounds().center();
                    Vec3 rayOrigin, rayDir;
                    screenToWorldRay((float)mx, (float)my, rayOrigin, rayDir);
                    auto axis = s.gizmo.pickAxis(center, rayOrigin, rayDir);
                    if(axis != TransformGizmo::Axis::None) {
                        s.gizmoDragging = true;
                        s.activeGizmoAxis = axis;
                        s.gizmoWorldCenter = center;
                        // Project onto appropriate plane for this axis
                        Vec3 pn{0, 1, 0};
                        if (axis == TransformGizmo::Axis::Y) pn = {0, 0, 1};
                        float d = rayDir.dot(pn);
                        s.gizmoStartWorldPos = (std::fabs(d) > 1e-6f)
                            ? rayOrigin + rayDir * (-rayOrigin.dot(pn) / d)
                            : rayOrigin + rayDir * 5.f;
                        s.gizmoSaved.clear();
                        for(auto fid : s.selectedIds) {
                            auto* sn = s.app->document().history().node(fid);
                            if(sn && sn->mesh) s.gizmoSaved.push_back(*sn->mesh);
                        }
                        printf("Gizmo drag start (axis %d)\n", (int)axis);
                        return;
                    }
                }
            }
            // No gizmo hit: start box-select tracking (selection happens on release).
            s.gizmoDragging = false;
            s.boxSelecting = true;
            s.boxClickTime = glfwGetTime();
            s.shiftHeldAtClick = shiftHeld;
            s.boxStartX = (float)mx;
            s.boxStartY = (float)my;
            s.boxEndX = (float)mx;
            s.boxEndY = (float)my;
            return;
        }

        // Modeling mode: place primitive snapped to grid.
        if(s.app->orchestrator().activeModeId() == "modeling" && button == GLFW_MOUSE_BUTTON_LEFT) {
            // Route to modeling mode for placement.
        }
    } else {
        s.app->onMouseUp(button, (float)mx, (float)my);

            if(s.gizmoDragging) {
            s.gizmoDragging = false;
            s.activeGizmoAxis = TransformGizmo::Axis::None;
            for(size_t j = 0; j < s.gizmoSaved.size() && j < s.selectedIds.size(); ++j) {
                auto cmd = std::make_unique<nexus::cad::TransformCommand>(
                    s.selectedIds[j], std::move(s.gizmoSaved[j]));
                s.app->document().executeCommand(std::move(cmd));
            }
            s.gizmoSaved.clear();
            printf("Gizmo drag end\n");
            return;
        }

        if(s.boxSelecting) {
            s.boxSelecting = false;
            float dx = s.boxEndX - s.boxStartX;
            float dy = s.boxEndY - s.boxStartY;
            double dt = glfwGetTime() - s.boxClickTime;

            // A click is: small movement OR fast release (<300ms)
            bool isClick = (std::fabs(dx) <= 12.f && std::fabs(dy) <= 12.f) || dt < 0.3;

            if (!isClick) {
                // Box select
                float x1=std::min(s.boxStartX,s.boxEndX), x2=std::max(s.boxStartX,s.boxEndX);
                float y1=std::min(s.boxStartY,s.boxEndY), y2=std::max(s.boxStartY,s.boxEndY);
                if(!s.shiftHeldAtClick) s.selectedIds.clear();
                auto& hist=s.app->document().history();
                for(FeatureId i=1;i<=static_cast<FeatureId>(hist.featureCount());++i){
                    auto* n=hist.node(i); if(!n||!n->mesh||n->deleted||n->hidden) continue;
                    auto b=n->mesh->computeBounds();
                    float sx[8],sy[8];
                    Vec3 corners[8]={{b.min.x,b.min.y,b.min.z},{b.max.x,b.min.y,b.min.z},
                                     {b.min.x,b.max.y,b.min.z},{b.max.x,b.max.y,b.min.z},
                                     {b.min.x,b.min.y,b.max.z},{b.max.x,b.min.y,b.max.z},
                                     {b.min.x,b.max.y,b.max.z},{b.max.x,b.max.y,b.max.z}};
                    GLdouble mv[16],proj[16]; GLint vp[4];
                    glGetDoublev(GL_MODELVIEW_MATRIX,mv);
                    glGetDoublev(GL_PROJECTION_MATRIX,proj);
                    glGetIntegerv(GL_VIEWPORT,vp);
                    float sminX=1e9f,sminY=1e9f,smaxX=-1e9f,smaxY=-1e9f;
                    for(int c=0;c<8;++c){
                        GLdouble px,py,pz;
                        gluProject(corners[c].x,corners[c].y,corners[c].z,mv,proj,vp,&px,&py,&pz);
                        sx[c]=(float)px; sy[c]=(float)(s.height-py);
                        sminX=std::min(sminX,sx[c]); smaxX=std::max(smaxX,sx[c]);
                        sminY=std::min(sminY,sy[c]); smaxY=std::max(smaxY,sy[c]);
                    }
                    if(smaxX >= x1 && sminX <= x2 && smaxY >= y1 && sminY <= y2)
                        s.selectedIds.push_back(i);
                }
                s.selectedId=s.selectedIds.empty()?kInvalidFeatureId:s.selectedIds.back();
                s.app->context().activeSelectedFeature = s.selectedId;
            } else {
                // Click: ray-cast pick
                Vec3 rayOrigin, rayDir;
                screenToWorldRay((float)mx,(float)my,rayOrigin,rayDir);
                uint32_t faceIdx=~0u,vertIdx=~0u; Vec3 hit;
                auto fid=pickSubObject(s.app->document(),rayOrigin,rayDir,faceIdx,vertIdx,hit);
                if(fid == kInvalidFeatureId)
                    fid = coarsePick(s.app->document(), rayOrigin, rayDir);
                if(fid!=kInvalidFeatureId){
                    s.app->context().selectedFace=faceIdx;
                    s.app->context().selectedVertex=vertIdx;
                    s.app->context().hitPoint=hit;
                    if(s.shiftHeldAtClick){
                        auto it=std::find(s.selectedIds.begin(),s.selectedIds.end(),fid);
                        if(it!=s.selectedIds.end()) s.selectedIds.erase(it);
                        else s.selectedIds.push_back(fid);
                    }else{
                        s.selectedIds.clear(); s.selectedIds.push_back(fid);
                    }
                    s.selectedId=fid;
                    s.app->context().activeSelectedFeature = fid;
                } else if(!s.shiftHeldAtClick) {
                    s.selectedIds.clear();
                    s.selectedId = kInvalidFeatureId;
                    s.app->context().activeSelectedFeature = 0;
                }
            }
            return;
        }

        s.mouseDragging = false;

        // Sketch mode: add point or multi-click primitive.
        if(s.sketchActive && button == GLFW_MOUSE_BUTTON_LEFT) {
            auto& cwp = s.app->context().cursorWorldPos;
            double sx = cwp.x, sy = cwp.z;
            if(s.snapToGrid) { sx = std::round(sx); sy = std::round(sy); }
            switch(s.sketchPrimitive) {
                case AppState::SketchPrim::Point:
                    (void)s.sketcher->addPoint(sx, sy);
                    printf("Sketch point: (%.2f, %.2f)\n", sx, sy);
                    break;
                case AppState::SketchPrim::Rectangle:
                    if(!s.sketchPrimPending) {
                        s.sketchPrimPending = true;
                        s.sketchPrimX = sx; s.sketchPrimY = sy;
                        printf("Rect corner 1: (%.2f, %.2f) — click opposite corner\n", sx, sy);
                    } else {
                        double ox = s.sketchPrimX, oy = s.sketchPrimY;
                        double w = sx - ox, h = sy - oy;
                        if(std::fabs(w) < 0.1 && std::fabs(h) < 0.1) break;
                        (void)s.sketcher->addRectangle(ox, oy, w, h);
                        s.sketchPrimPending = false;
                        printf("Rect: (%.2f,%.2f) w=%.2f h=%.2f\n", ox, oy, w, h);
                    }
                    break;
                case AppState::SketchPrim::Circle:
                    if(!s.sketchPrimPending) {
                        s.sketchPrimPending = true;
                        s.sketchPrimX = sx; s.sketchPrimY = sy;
                        printf("Circle center: (%.2f, %.2f) — click edge\n", sx, sy);
                    } else {
                        double dx = sx - s.sketchPrimX, dy = sy - s.sketchPrimY;
                        double r = std::sqrt(dx*dx + dy*dy);
                        if(r < 0.1) break;
                        (void)s.sketcher->addCircle(s.sketchPrimX, s.sketchPrimY, r);
                        s.sketchPrimPending = false;
                        printf("Circle: cx=%.2f cy=%.2f r=%.2f\n", s.sketchPrimX, s.sketchPrimY, r);
                    }
                    break;
            }
        }
    }
}

void cursorPosCallback(GLFWwindow* w, double x, double y) {
    ImGui_ImplGlfw_CursorPosCallback(w, x, y);
    if(ImGui::GetIO().WantCaptureMouse) return;
    auto& s = *g_state;
    if(s.gizmoDragging) {
        Vec3 rayOrigin, rayDir;
        screenToWorldRay((float)x, (float)y, rayOrigin, rayDir);
        // Pick projection plane based on active axis
        Vec3 planeN{0, 1, 0}; // default XZ
        if (s.activeGizmoAxis == TransformGizmo::Axis::Y)
            planeN = {0, 0, 1}; // XY plane for vertical movement
        float denom = rayDir.dot(planeN);
        Vec3 curWorldPos = (std::fabs(denom) > 1e-6f)
            ? rayOrigin + rayDir * (-rayOrigin.dot(planeN) / denom)
            : rayOrigin + rayDir * 5.f;
        float amount = axisDisplacement(s.gizmoStartWorldPos, curWorldPos, s.activeGizmoAxis);
        if(std::fabs(amount) > 0.0005f) {
            switch(s.gizmo.mode()) {
                case TransformGizmo::Mode::Translate:
                    for(auto fid : s.selectedIds)
                        s.gizmo.translate(s.gizmoWorldCenter, s.activeGizmoAxis, amount,
                                          s.app->document(), fid, false);
                    break;
                case TransformGizmo::Mode::Scale: {
                    float factor = 1.f + amount * 0.3f;
                    if(factor < 0.01f) factor = 0.01f;
                    for(auto fid : s.selectedIds)
                        s.gizmo.scale(s.gizmoWorldCenter, s.activeGizmoAxis, factor,
                                      s.app->document(), fid, false);
                    break;
                }
                case TransformGizmo::Mode::Rotate: {
                    float angle = amount * 0.5f;
                    for(auto fid : s.selectedIds)
                        s.gizmo.rotate(s.gizmoWorldCenter, s.activeGizmoAxis, angle,
                                       s.app->document(), fid, false);
                    break;
                }
            }
            s.gizmoStartWorldPos = curWorldPos;
        }
        s.app->onMouseMove((float)x, (float)y);
        s.lastMouseX = (float)x; s.lastMouseY = (float)y;
        return;
    }
    if(s.boxSelecting) {
        s.boxEndX=(float)x; s.boxEndY=(float)y;
        s.app->onMouseMove((float)x, (float)y);
        s.lastMouseX=(float)x; s.lastMouseY=(float)y;
        return;
    }
    if(s.mouseDragging && (glfwGetMouseButton(s.window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS ||
                           glfwGetMouseButton(s.window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS)) {
        float dx = (float)(x - s.lastMouseX);
        float dy = (float)(y - s.lastMouseY);
        s.viewer->camera().orbit(dx * 3.f, dy * 3.f);
    }
    s.app->onMouseMove((float)x, (float)y);
    // Send MouseDrag instead of MouseMove when a button is held.
    if(s.mouseDragging) {
        InputEvent ev; ev.type=InputEventType::MouseDrag; ev.position={(float)x,(float)y,0};
        s.app->orchestrator().routeInput(ev);
    }
    s.lastMouseX = (float)x; s.lastMouseY = (float)y;

    // Compute world-space intersection with active working plane.
    {
        Vec3 origin, dir;
        screenToWorldRay((float)x, (float)y, origin, dir);
        // Intersect with the active working plane.
        Vec3 planeN{0,1,0}; // XZ default
        auto wp = s.app->context().workPlane;
        if(wp == AppContext::WorkPlane::XY) planeN = {0,0,1};
        else if(wp == AppContext::WorkPlane::YZ) planeN = {1,0,0};
        float denom = dir.dot(planeN);
        if(std::fabs(denom) > 1e-6f) {
            float t = -origin.dot(planeN) / denom;
            if(t > 0) {
                s.app->context().cursorWorldPos = origin + dir * t;
                bool ctrl = (glfwGetKey(s.window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                             glfwGetKey(s.window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
                if(ctrl) {
                    s.app->context().cursorWorldPos = snapToGeometry(
                        s.app->document(), s.app->context().cursorWorldPos, s.snapMode, 1.5f);
                }
                s.app->context().viewportWidth = (float)s.width;
                s.app->context().viewportHeight = (float)s.height;
            }
        }
    }
}

void scrollCallback(GLFWwindow* w, double xoff, double yoff) {
    ImGui_ImplGlfw_ScrollCallback(w, xoff, yoff);
    if(ImGui::GetIO().WantCaptureMouse) return;
    g_state->viewer->camera().zoom((float)yoff * 100.f);
}

void framebufferSizeCallback(GLFWwindow*, int w, int h) {
    g_state->width = w; g_state->height = h;
    glViewport(0, 0, w, h);
}

// ── Main ────────────────────────────────────────────────────────────────────

int main() {
    // Init GLFW.
    if(!glfwInit()) { fprintf(stderr, "GLFW init failed\n"); return 1; }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API); // simple OpenGL for now
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4); // 4x MSAA

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Nexus Modeling", nullptr, nullptr);
    if(!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    EditorUI::initialize(window);

    // Init application.
    ModelingApplication mdlApp;
    CadViewer viewerApp(mdlApp.document(), mdlApp.selection());
    CadAutoConstraintSketch sketchApp(mdlApp.document());

    AppState state;
    state.app = &mdlApp;
    state.viewer = &viewerApp;
    state.sketcher = &sketchApp;
    state.window = window;
    g_state = &state;

    if(!state.app->initialize()) { glfwTerminate(); return 1; }
    state.app->context().snapToGrid = state.snapToGrid;

    // Set callbacks.
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    glfwSetCursorPosCallback(window, cursorPosCallback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);

    printf("Nexus Modeling — Window opened. 1280x720\n");
    printf("  Keys: ESC=select  S=sketch  E=extrude  F=fillet\n");
    printf("  Mouse: drag=orbit  scroll=zoom  click=sketch point\n");

    // Render loop.
    while(!glfwWindowShouldClose(window)) {
        EditorUI::beginFrame();

        int nViews = state.quadView ? 4 : 1;
        int vpW[4], vpH[4], vpX[4], vpY[4];
        if(state.quadView) {
            int hw=state.width/2, hh=state.height/2;
            vpX[0]=0;    vpY[0]=hh;   vpW[0]=hw; vpH[0]=hh; // Top-left: Top
            vpX[1]=hw;   vpY[1]=hh;   vpW[1]=hw; vpH[1]=hh; // Top-right: Front
            vpX[2]=0;    vpY[2]=0;    vpW[2]=hw; vpH[2]=hh; // Bottom-left: Right
            vpX[3]=hw;   vpY[3]=0;    vpW[3]=hw; vpH[3]=hh; // Bottom-right: Perspective
        } else { vpX[0]=0; vpY[0]=0; vpW[0]=state.width; vpH[0]=state.height; }

        for(int vi=0; vi<nViews; ++vi) {
        glViewport(vpX[vi],vpY[vi],vpW[vi],vpH[vi]);
        glScissor(vpX[vi],vpY[vi],vpW[vi],vpH[vi]);
        glEnable(GL_SCISSOR_TEST);

        // 3D view setup.
        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_MULTISAMPLE); // 4x MSAA

        // Lighting setup.
        if(state.lighting) {
            glEnable(GL_LIGHTING);
            glEnable(GL_LIGHT0);
            GLfloat ambient[]  = {0.2f, 0.2f, 0.25f, 1.f};
            GLfloat diffuse[]  = {0.7f, 0.65f, 0.6f, 1.f};
            GLfloat lightPos[] = {5.f, 10.f, 8.f, 0.f}; // directional from upper-right
            glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
            glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
            glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
            GLfloat matAmb[] = {0.3f,0.3f,0.35f,1.f};
            GLfloat matDif[] = {0.7f,0.7f,0.7f,1.f};
            glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmb);
            glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDif);
            glShadeModel(GL_SMOOTH);
            glEnable(GL_NORMALIZE);
        } else {
            glDisable(GL_LIGHTING);
        }

        // Projection and modelview for the camera.
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)vpW[vi] / (float)vpH[vi];
        if(state.ortho) {
            float dist = state.viewer->camera().distance();
            float halfH = dist * 0.6f;
            float halfW = halfH * aspect;
            glOrtho(-halfW, halfW, -halfH, halfH, 0.1, 1000.0);
        } else {
            gluPerspective(45.0, aspect, 0.1, 1000.0);
        }

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        // Per-quadrant camera orientation.
        if(state.quadView && vi<3) {
            if(vi==0) state.viewer->camera().viewTop();
            else if(vi==1) state.viewer->camera().viewFront();
            else state.viewer->camera().viewRight();
        }
        auto& cam = state.viewer->camera();
        gluLookAt(cam.position().x, cam.position().y, cam.position().z,
                  cam.target().x,   cam.target().y,   cam.target().z,
                  cam.up().x,       cam.up().y,       cam.up().z);

        // Animation playback.
        if(state.animPlaying) {
            float now = (float)glfwGetTime();
            if(now - state.lastFrameTime > 0.05f) { // ~20fps
                state.lastFrameTime = now;
                state.animFrame++;
                if(state.animFrame > state.animMaxFrame) state.animFrame = 0;
                state.animTime = (float)state.animFrame;
            }
        }
        // Apply keyframe interpolation to meshes.
        auto& kfs = state.keyframes;
        for(auto& [fid, keys] : kfs) {
            if(keys.empty()) continue;
            auto* node = state.app->document().history().node(fid);
            if(!node || !node->mesh) continue;
            // Find bracketing keyframes.
            const decltype(keys)::value_type* prev = nullptr, *next = nullptr;
            for(auto& k : keys) {
                if(k.frame <= state.animFrame) prev = &k;
                if(k.frame >= state.animFrame && !next) next = &k;
            }
            if(!prev && !next) continue;
            if(!next) next = prev;
            if(!prev) prev = next;
            Vec3 target;
            if(prev == next) target = prev->position;
            else {
                float t = (prev->frame == next->frame) ? 0.f :
                    (float)(state.animFrame - prev->frame) / (float)(next->frame - prev->frame);
                target = prev->position + (next->position - prev->position) * std::clamp(t, 0.f, 1.f);
            }
            // Move mesh to target.
            auto currentCenter = node->mesh->computeBounds().center();
            Vec3 delta = target - currentCenter;
            auto pos = node->mesh->attributes().positions();
            for(auto& v : pos) { v.x += delta.x; v.y += delta.y; v.z += delta.z; }
            node->mesh->attributes().setPositions(std::move(pos));
        }
        // Draw grid and axes (no lighting).
        glDisable(GL_LIGHTING);
        GridOptions gridOpts;
        gridOpts.workPlane = (state.app->context().workPlane == AppContext::WorkPlane::XZ) ? kWorkPlaneXZ :
                             (state.app->context().workPlane == AppContext::WorkPlane::XY) ? kWorkPlaneXY : kWorkPlaneYZ;
        // Auto-scale grid extent with camera distance
        float camDist = state.viewer->camera().distance();
        float autoExtent = std::max(50.f, camDist * 6.f);
        float autoSpacing = std::pow(10.f, std::round(std::log10(std::max(0.1f, camDist * 0.05f))));
        gridOpts.spacing = std::max(autoSpacing, 0.1f);
        gridOpts.extent = autoExtent;
        ViewportGrid grid;
        grid.render(gridOpts);

        // Draw all document feature meshes with per-feature color + selection.
        for(size_t i=1; i<=state.app->document().history().featureCount(); ++i) {
            auto* node = state.app->document().history().node(static_cast<FeatureId>(i));
            if(!node || !node->mesh || node->deleted || node->hidden) continue;
            const auto& pos = node->mesh->attributes().positions();
            const auto& topo = node->mesh->topology();

            bool isSelected = (std::find(state.selectedIds.begin(), state.selectedIds.end(),
                                         static_cast<FeatureId>(i)) != state.selectedIds.end());
            Vec3 color = isSelected ? Vec3{1.f, 0.6f, 0.1f} : state.selectedColor;
            // Use per-feature material albedo for color when not selected.
            if(!isSelected) {
                color.x = node->material.albedo[0];
                color.y = node->material.albedo[1];
                color.z = node->material.albedo[2];
            }

            // Shaded fill with vertex arrays.
            glColor3f(color.x, color.y, color.z);
            if(state.lighting) {
                GLfloat matAmb[] = {color.x*0.4f, color.y*0.4f, color.z*0.4f, 1.f};
                GLfloat matDif[] = {color.x, color.y, color.z, 1.f};
                GLfloat matSpec[] = {node->material.roughness*0.5f, node->material.roughness*0.5f, node->material.roughness*0.5f, 1.f};
                glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, matAmb);
                glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, matDif);
                glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpec);
                glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, (1.f - node->material.roughness) * 128.f);
            }
            // Build vertex arrays (cached per frame).
            std::vector<float> varray, narray;
            size_t psz = pos.size();
            for(uint32_t fi=0; fi<topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                if(face.vertexCount() < 3) continue;
                for(size_t j=0; j+2<face.vertexCount(); ++j) {
                    uint32_t i0=face.indices[0], i1=face.indices[j+1], i2=face.indices[j+2];
                    if(i0>=psz||i1>=psz||i2>=psz) continue;
                    auto& v0=pos[i0], &v1=pos[i1], &v2=pos[i2];
                    varray.insert(varray.end(),{v0.x,v0.y,v0.z,v1.x,v1.y,v1.z,v2.x,v2.y,v2.z});
                    if(state.lighting) {
                        Vec3 fn = (v1-v0).cross(v2-v0).normalize();
                        narray.insert(narray.end(),{fn.x,fn.y,fn.z,fn.x,fn.y,fn.z,fn.x,fn.y,fn.z});
                    }
                }
            }
            if(!varray.empty()) {
                glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3, GL_FLOAT, 0, varray.data());
                if(state.lighting && !narray.empty()) {
                    glEnableClientState(GL_NORMAL_ARRAY);
                    glNormalPointer(GL_FLOAT, 0, narray.data());
                }
                glDrawArrays(GL_TRIANGLES, 0, (GLsizei)varray.size()/3);
                glDisableClientState(GL_VERTEX_ARRAY);
                if(state.lighting) glDisableClientState(GL_NORMAL_ARRAY);
            }

            // Wireframe overlay (offset to avoid z-fight with solid)
            if(!varray.empty()) {
                glEnable(GL_POLYGON_OFFSET_LINE);
                glPolygonOffset(-1.f, -1.f);
                glColor3f(0.05f, 0.05f, 0.08f);
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3, GL_FLOAT, 0, varray.data());
                glDrawArrays(GL_TRIANGLES, 0, (GLsizei)varray.size()/3);
                glDisableClientState(GL_VERTEX_ARRAY);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
                glDisable(GL_POLYGON_OFFSET_LINE);
            }

            // Selection highlight for ALL selected objects
            if(isSelected) {
                g_highlight.render(state.app->document(), static_cast<FeatureId>(i));
            }
            // Gizmo only on the primary selected object
            if(static_cast<FeatureId>(i) == state.selectedId && node && node->mesh) {
                state.gizmo.render(node->mesh->computeBounds().center());
            }
        }

        // Draw ghost preview mesh if set.
        auto& preview = state.app->context().previewMesh;
        if(preview.has_value() && preview->isValid()) {
            glDisable(GL_LIGHTING);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(0.8f, 0.6f, 0.2f, 0.35f);
            const auto& pp = preview->attributes().positions();
            const auto& pt = preview->topology();
            std::vector<float> gv;
            for(uint32_t fi=0; fi<pt.faceCount(); ++fi) {
                const auto& face = pt.face(fi);
                for(size_t j=0; j+2<face.vertexCount(); ++j) {
                    auto& v0=pp[face.indices[0]], &v1=pp[face.indices[j+1]], &v2=pp[face.indices[j+2]];
                    gv.insert(gv.end(),{v0.x,v0.y,v0.z,v1.x,v1.y,v1.z,v2.x,v2.y,v2.z});
                }
            }
            if(!gv.empty()) {
                glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(3,GL_FLOAT,0,gv.data());
                glDrawArrays(GL_TRIANGLES,0,(GLsizei)gv.size()/3);
                glDisableClientState(GL_VERTEX_ARRAY);
            }
            glDisable(GL_BLEND);
        }

        // Draw sketch points if active.
        if(state.sketchActive) {
            SketchPreview sketchPreview;
            sketchPreview.render(*state.sketcher);
        }

        // Draw dimension overlay with label.
        auto& dimCtx = state.app->context();
        if(dimCtx.dimActive) {
            auto& p1 = dimCtx.dimP1, &p2 = dimCtx.dimP2;
            // Screen-space label in the middle.
            {
                GLdouble mv[16],proj[16]; GLint vp[4];
                glGetDoublev(GL_MODELVIEW_MATRIX,mv);
                glGetDoublev(GL_PROJECTION_MATRIX,proj);
                glGetIntegerv(GL_VIEWPORT,vp);
                double sx1,sy1,sz1,sx2,sy2,sz2;
                gluProject(p1.x,p1.y,p1.z,mv,proj,vp,&sx1,&sy1,&sz1);
                gluProject(p2.x,p2.y,p2.z,mv,proj,vp,&sx2,&sy2,&sz2);
                if(sz1>0&&sz1<1&&sz2>0&&sz2<1){
                    float d=(p2-p1).length();
                    char buf[64];snprintf(buf,sizeof(buf),"%.2f",d);
                    float mx=(float)((sx1+sx2)*0.5),my=(float)(state.height-(sy1+sy2)*0.5);
                    ImDrawList* dl=ImGui::GetForegroundDrawList();
                    dl->AddText(ImVec2(mx-20,my-10),IM_COL32(255,230,50,255),buf);
                }
            }
            glDisable(GL_LIGHTING);
            glLineWidth(2.f);
            glColor3f(1.f, 0.9f, 0.25f);
            glBegin(GL_LINES);
            glVertex3f(p1.x, p1.y, p1.z);
            glVertex3f(p2.x, p2.y, p2.z);
            glEnd();
            float cr = 0.15f;
            glBegin(GL_LINES);
            glVertex3f(p1.x-cr,p1.y,p1.z); glVertex3f(p1.x+cr,p1.y,p1.z);
            glVertex3f(p1.x,p1.y-cr,p1.z); glVertex3f(p1.x,p1.y+cr,p1.z);
            glVertex3f(p1.x,p1.y,p1.z-cr); glVertex3f(p1.x,p1.y,p1.z+cr);
            glVertex3f(p2.x-cr,p2.y,p2.z); glVertex3f(p2.x+cr,p2.y,p2.z);
            glVertex3f(p2.x,p2.y-cr,p2.z); glVertex3f(p2.x,p2.y+cr,p2.z);
            glVertex3f(p2.x,p2.y,p2.z-cr); glVertex3f(p2.x,p2.y,p2.z+cr);
            glEnd();
            glLineWidth(1.f);
        }
        glDisable(GL_SCISSOR_TEST);

        } // end for (quad view loops)

        // Editor UI (menu bar, toolbar, panels).
        state.app->context().activeSelectedFeature = state.selectedId;
        EditorUI::renderMenuBar(state.app->context(), state.gizmo,
                                 state.app->orchestrator().viewport());
        if(state.showPanels) {
        EditorUI::renderToolbar(state.app->context(), state.gizmo);
        EditorUI::renderOutliner(state.app->context(), state.selectedId);
        EditorUI::renderProperties(state.app->context(), state.selectedId);
        EditorUI::renderContextMenu(state.app->context(), state.gizmo, state.selectedId);
        EditorUI::renderStatusBar(state.app->context(), state.snapToGrid,
                                   state.selectedId);
        EditorUI::renderTimeline(state.selectedId, state.animPlaying,
                                  state.animFrame, state.animMaxFrame);
        EditorUI::renderUndoHistory(state.app->context());
        }
        if(state.showKeys) EditorUI::renderKeybindings();

        // Perf overlay.
        if(state.showPerf) {
            float now = (float)glfwGetTime();
            state.perfFrameCount++;
            if(now - state.perfLastTime > 1.f) {
                state.perfFps = state.perfFrameCount / (now - state.perfLastTime);
                state.perfFrameCount = 0;
                state.perfLastTime = now;
            }
            // Count total verts.
            size_t totalVerts=0, totalTris=0;
            auto& hist=state.app->document().history();
            for(FeatureId i=1;i<=static_cast<FeatureId>(hist.featureCount());++i){
                auto* n=hist.node(i); if(n&&n->mesh&&!n->deleted&&!n->hidden) {
                    totalVerts+=n->mesh->attributes().vertexCount();
                    totalTris+=n->mesh->topology().faceCount();
                }
            }
            ImGui::Begin("Perf",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("%.0f FPS  |  %zu verts  |  %zu faces",state.perfFps,totalVerts,totalTris);
            // Grid:  [=Finer  ]=Coarser
    ImGui::Text("Group: Ctrl+G=Group  Ctrl+Shift+G=Ungroup");
    ImGui::Text("F1=Keys  F5=Perf  Ctrl+Q=QuadView  Ctrl+A=SelAll");
    ImGui::End();
        }

        // Box-select rectangle overlay.
        if(state.boxSelecting) {
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            float x1=std::min(state.boxStartX,state.boxEndX);
            float y1=std::min(state.boxStartY,state.boxEndY);
            float x2=std::max(state.boxStartX,state.boxEndX);
            float y2=std::max(state.boxStartY,state.boxEndY);
            dl->AddRect(ImVec2(x1,y1),ImVec2(x2,y2),IM_COL32(100,180,255,128),0.f,0,1.5f);
        }
        EditorUI::endFrame();

        glfwSwapBuffers(window);
        glfwPollEvents();

        // Update window title with modified state.
        {
            char title[256];
            bool mod = state.app->document().isModified();
            snprintf(title,sizeof(title),"Nexus Modeling%s", mod ? " *" : "");
            glfwSetWindowTitle(window, title);
        }
        // Auto-save every 5 minutes.
        float now = (float)glfwGetTime();
        if(now - state.lastAutoSave > 300.f) {
            state.lastAutoSave = now;
            auto data = state.app->document().serialize();
            FILE* f = fopen("autosave.nxm","wb");
            if(f){fwrite(data.data(),1,data.size(),f);fclose(f);}
        }
    }

    EditorUI::shutdown();
    glfwTerminate();
    return 0;
}
