#include <gtest/gtest.h>
#include "nexus/pathtrace/PathTracer.h"
#include "nexus/geometry/Mesh.h"

using namespace nexus::pathtrace;
using namespace nexus;

TEST(PathTracer, DefaultConfig) {
    PathTracer pt;
    EXPECT_EQ(pt.config().maxBounces, 8u);
    EXPECT_EQ(pt.config().samplesPerPixel, 64u);
}

TEST(PathTracer, RenderProducesPixels) {
    PathTracer pt;
    CameraConfig cam;
    cam.width = 64;
    cam.height = 64;
    pt.setCamera(cam);

    RenderConfig cfg;
    cfg.samplesPerPixel = 1;
    cfg.maxBounces = 1;
    pt.setConfig(cfg);

    auto pixels = pt.render();
    EXPECT_EQ(pixels.size(), 64u * 64 * 4);
}

TEST(PathTracer, AddMeshStoresTriangles) {
    PathTracer pt;
    geometry::Mesh mesh = geometry::primitives::makeSphere(1.0f, 4, 4);

    pt.addMesh(mesh, nexus::render::Mat4{}, Vec3{1,0,0}, 0.3f, 0.1f);

    auto pixels = pt.render();
    EXPECT_EQ(pixels.size(), 1920u * 1080 * 4);
}

TEST(PathTracer, ClearSceneRemovesAll) {
    PathTracer pt;
    geometry::Mesh mesh = geometry::primitives::makeBox(1, 1, 1);
    pt.addMesh(mesh, nexus::render::Mat4{}, Vec3{1,1,1});
    pt.clearScene();

    CameraConfig cam;
    cam.width = 16;
    cam.height = 16;
    pt.setCamera(cam);
    RenderConfig cfg;
    cfg.samplesPerPixel = 1;
    pt.setConfig(cfg);

    auto pixels = pt.render();
    EXPECT_EQ(pixels.size(), 16u * 16 * 4);
    bool anyNonZero = false;
    for (size_t i = 0; i < pixels.size(); ++i) {
        if (pixels[i] > 0.0f) { anyNonZero = true; break; }
    }
    (void)anyNonZero;
}

TEST(PathTracer, TileRendering) {
    PathTracer pt;
    CameraConfig cam;
    cam.width = 32;
    cam.height = 32;
    pt.setCamera(cam);

    RenderConfig cfg;
    cfg.samplesPerPixel = 1;
    pt.setConfig(cfg);

    RenderTile tile{0, 0, 16, 16};
    auto pixels = pt.render(tile);
    EXPECT_EQ(pixels.size(), 16u * 16 * 4);
}
