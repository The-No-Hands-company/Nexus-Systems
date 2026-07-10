// ─────────────────────────────────────────────────────────────────────────────
//  nxrender — headless software-rasterizer preview for the AI visual dev-loop
//
//  Renders kernel meshes (built-in primitives or a loaded .nxm) to a PNG using
//  the CPU SoftwareRasterizer — no GPU, no window. The camera auto-frames the
//  scene bounds. Supports industry-standard selection visualisation: an
//  object-ID buffer feeds a Jump-Flood silhouette outline with hover / selected
//  / active styling (see softrast/SelectionOverlay.h).
//
//  Usage:
//    nxrender [--prim box|sphere|plane|cylinder|cone|triangle|capsule]
//             [--in FILE.nxm] [--out FILE.png]
//             [--view iso|front|back|top|bottom|left|right]
//             [--mode flat|gouraud|wireframe]
//             [--select none|hover|selected|active]   (single-object state)
//             [--demo selection]                      (4-up state comparison)
//             [--size S] [--width W] [--height H]
//
//  Examples:
//    nxrender --prim sphere --select active --out /tmp/sel.png
//    nxrender --demo selection --out /tmp/states.png
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NexusFormat.h>
#include <nexus/render/Camera.h>

#include "softrast/ImageWriter.h"
#include "softrast/SelectionOverlay.h"
#include "softrast/SoftwareRasterizer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using nexus::geometry::Mesh;
using nexus::render::Aabb;
using nexus::render::Camera;
using nexus::render::Mat4;
using nexus::render::Vec3;
namespace prims = nexus::geometry::primitives;
namespace sr = nexus::softrast;

namespace {

struct Options {
    std::string prim = "box";
    std::string in;
    std::string out = "render.png";
    std::string view = "iso";
    std::string mode = "gouraud";
    std::string select = "none";
    std::string demo;
    float size = 1.0f;
    uint32_t width = 900;
    uint32_t height = 675;
};

// One drawable: a mesh placed by `model`, tagged with an id and a selection state.
struct SceneObject {
    Mesh mesh;
    Mat4 model = Mat4::identity();
    uint32_t id = 0;
    sr::SelState state = sr::SelState::None;
};

[[nodiscard]] const char* argValue(int argc, char** argv, int& i) {
    if (i + 1 >= argc) {
        std::fprintf(stderr, "nxrender: missing value for %s\n", argv[i]);
        std::exit(2);
    }
    return argv[++i];
}

[[nodiscard]] Options parseArgs(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "--prim")        o.prim   = argValue(argc, argv, i);
        else if (a == "--in")     o.in     = argValue(argc, argv, i);
        else if (a == "--out")    o.out    = argValue(argc, argv, i);
        else if (a == "--view")   o.view   = argValue(argc, argv, i);
        else if (a == "--mode")   o.mode   = argValue(argc, argv, i);
        else if (a == "--select") o.select = argValue(argc, argv, i);
        else if (a == "--demo")   o.demo   = argValue(argc, argv, i);
        else if (a == "--size")   o.size   = std::strtof(argValue(argc, argv, i), nullptr);
        else if (a == "--width")  o.width  = static_cast<uint32_t>(std::atoi(argValue(argc, argv, i)));
        else if (a == "--height") o.height = static_cast<uint32_t>(std::atoi(argValue(argc, argv, i)));
        else {
            std::fprintf(stderr, "nxrender: unknown argument '%s'\n", argv[i]);
            std::exit(2);
        }
    }
    if (o.width == 0) o.width = 1;
    if (o.height == 0) o.height = 1;
    return o;
}

[[nodiscard]] Mesh buildPrimitive(const std::string& kind, float s) {
    if (kind == "box")      return prims::makeBox(s, s, s);
    if (kind == "sphere")   return prims::makeSphere(s * 0.5f, 24, 24);
    if (kind == "plane")    return prims::makePlane(s, s, 4, 4);
    if (kind == "cylinder") return prims::makeCylinder(s * 0.5f, s, 24);
    if (kind == "cone")     return prims::makeCone(s * 0.5f, s, 24);
    if (kind == "capsule")  return prims::makeCapsule(s * 0.4f, s, 20, 8);
    if (kind == "triangle") return prims::makeTriangle(s);
    std::fprintf(stderr, "nxrender: unknown primitive '%s', using box\n", kind.c_str());
    return prims::makeBox(s, s, s);
}

[[nodiscard]] sr::SelState selState(const std::string& s) {
    if (s == "hover")    return sr::SelState::Hover;
    if (s == "selected") return sr::SelState::Selected;
    if (s == "active")   return sr::SelState::Active;
    return sr::SelState::None;
}

[[nodiscard]] Mat4 translate(float x, float y, float z) {
    Mat4 m = Mat4::identity();
    m.m[0][3] = x; m.m[1][3] = y; m.m[2][3] = z;
    return m;
}

// Union of object bounds in world space (each transformed by its model matrix).
[[nodiscard]] Aabb sceneBounds(const std::vector<SceneObject>& scene) {
    bool any = false;
    Aabb box{};
    for (const SceneObject& o : scene) {
        const Aabb local = o.mesh.computeBounds();
        if (local.min == local.max) continue;
        const Aabb b = local.transformed(o.model);
        if (!any) { box = b; any = true; continue; }
        box.min = {std::min(box.min.x, b.min.x), std::min(box.min.y, b.min.y), std::min(box.min.z, b.min.z)};
        box.max = {std::max(box.max.x, b.max.x), std::max(box.max.y, b.max.y), std::max(box.max.z, b.max.z)};
    }
    if (!any) box = Aabb{{-1, -1, -1}, {1, 1, 1}};
    return box;
}

[[nodiscard]] Vec3 viewDir(const std::string& view) {
    if (view == "front")  return {0, 0, 1};
    if (view == "back")   return {0, 0, -1};
    if (view == "right")  return {1, 0, 0};
    if (view == "left")   return {-1, 0, 0};
    if (view == "top")    return {0, 1, 0.0001f};
    if (view == "bottom") return {0, -1, 0.0001f};
    return Vec3{1.0f, 0.8f, 1.0f}.normalize();  // iso
}

[[nodiscard]] sr::ShadingMode shadingMode(const std::string& mode) {
    if (mode == "flat")      return sr::ShadingMode::Flat;
    if (mode == "wireframe") return sr::ShadingMode::Wireframe;
    return sr::ShadingMode::Gouraud;
}

// Left→right: unselected, hover, selected, active — so all four states are
// directly comparable in one image.
[[nodiscard]] std::vector<SceneObject> buildSelectionDemo() {
    const sr::SelState states[4] = {sr::SelState::None, sr::SelState::Hover,
                                    sr::SelState::Selected, sr::SelState::Active};
    std::vector<SceneObject> scene;
    for (int i = 0; i < 4; ++i) {
        SceneObject o;
        o.mesh = prims::makeBox(1.3f, 1.3f, 1.3f);
        o.model = translate(static_cast<float>(i) * 2.1f - 3.15f, 0.f, 0.f);
        o.id = static_cast<uint32_t>(i + 1);
        o.state = states[i];
        scene.push_back(std::move(o));
    }
    return scene;
}

}  // namespace

int main(int argc, char** argv) {
    const Options opt = parseArgs(argc, argv);

    // ── Build the scene ───────────────────────────────────────────────────────
    std::vector<SceneObject> scene;
    if (opt.demo == "selection") {
        scene = buildSelectionDemo();
    } else if (!opt.in.empty()) {
        std::vector<Mesh> loaded = nexus::geometry::NexusFormat::loadFromFile(opt.in);
        if (loaded.empty()) {
            std::fprintf(stderr, "nxrender: failed to load or empty .nxm: %s\n", opt.in.c_str());
            return 1;
        }
        uint32_t id = 1;
        for (Mesh& m : loaded) {
            SceneObject o; o.mesh = std::move(m); o.id = id++;
            o.state = selState(opt.select);   // apply the state to every loaded mesh
            scene.push_back(std::move(o));
        }
    } else {
        SceneObject o; o.mesh = buildPrimitive(opt.prim, opt.size); o.id = 1;
        o.state = selState(opt.select);
        scene.push_back(std::move(o));
    }

    // ── Auto-frame the camera on the scene bounds ─────────────────────────────
    const Aabb bounds = sceneBounds(scene);
    const Vec3 center = bounds.center();
    const Vec3 half = bounds.extents();
    const float radius = std::max(0.001f, std::sqrt(half.x*half.x + half.y*half.y + half.z*half.z));

    constexpr float kFovYDeg = 45.0f;
    const float aspect = static_cast<float>(opt.width) / static_cast<float>(opt.height);
    const float fovYRad = kFovYDeg * 3.14159265f / 180.0f;
    const float fitFov = (aspect < 1.0f) ? (2.0f * std::atan(std::tan(fovYRad * 0.5f) * aspect)) : fovYRad;
    const float dist = (radius / std::sin(fitFov * 0.5f)) * 1.35f;

    Camera camera;
    camera.setPerspective(kFovYDeg, aspect, std::max(0.01f, dist - radius * 2.0f), dist + radius * 4.0f);
    const Vec3 eye = center + viewDir(opt.view) * dist;
    camera.lookAt(eye, center, Vec3{0, 1, 0});

    // ── Rasterize the scene into shared colour / depth / object-ID buffers ────
    sr::PixelBuffer buf(opt.width, opt.height);
    sr::RasterizerConfig cfg;
    cfg.mode = shadingMode(opt.mode);
    buf.clear(cfg.background);

    const size_t px = static_cast<size_t>(opt.width) * opt.height;
    std::vector<float> depth(px, -std::numeric_limits<float>::infinity());
    std::vector<uint32_t> idBuf(px, 0u);

    sr::SoftwareRasterizer raster;
    uint32_t maxId = 0;
    for (const SceneObject& o : scene) {
        raster.renderShared(buf, depth, &idBuf, o.id, o.mesh, camera, cfg, o.model);
        maxId = std::max(maxId, o.id);
    }

    // ── Composite the selection outline (hover / selected / active) ───────────
    bool anySel = false;
    std::vector<sr::SelState> stateByObject(maxId, sr::SelState::None);
    for (const SceneObject& o : scene) {
        if (o.id >= 1 && o.id <= maxId) stateByObject[o.id - 1] = o.state;
        if (o.state != sr::SelState::None) anySel = true;
    }
    if (anySel) {
        sr::applySelectionOutline(buf, idBuf, opt.width, opt.height, stateByObject);
    }

    if (!sr::writePNG(opt.out, buf)) {
        std::fprintf(stderr, "nxrender: failed to write %s\n", opt.out.c_str());
        return 1;
    }

    std::printf("nxrender: wrote %s  (%ux%u, %zu obj, view=%s, mode=%s%s)\n",
                opt.out.c_str(), opt.width, opt.height, scene.size(),
                opt.view.c_str(), opt.mode.c_str(),
                opt.demo == "selection" ? ", demo=selection" : "");
    return 0;
}
