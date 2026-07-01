#pragma once
// ViewportController — camera orbit/zoom, screen→world ray, working plane, grid

#include <nexus/render/Camera.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

struct ViewportCamera {
    nexus::render::Camera cam;
    bool ortho = false;

    void orbit(float dx, float dy) { cam.orbit(dx, dy); }
    void zoom(float amount) { cam.zoom(amount); }
    Vec3 position() const { return cam.position(); }
    Vec3 target() const { return cam.target(); }
    Vec3 up() const { return cam.up(); }
    float distance() const { return cam.distance(); }
    void lookAt(const Vec3& t, float d) { cam.lookAt(t, d); }
    void viewTop() { cam.viewTop(); }
    void viewFront() { cam.viewFront(); }
    void viewRight() { cam.viewRight(); }
    void viewIsometric() { cam.viewIsometric(); }
};

enum class WorkPlane : uint8_t { XZ, XY, YZ };

struct ViewportState {
    int width = 1280, height = 720;
    WorkPlane workPlane = WorkPlane::XZ;
    Vec3 cursorWorldPos{};
    bool snapToGrid = true;
    float gridSpacing = 1.0f;
    float gridExtent = 10.0f;
    bool lighting = false;
    ViewportCamera camera;
};

class ViewportController {
public:
    void initialize(int width, int height);
    void resize(int width, int height);

    void beginFrame(int vpX, int vpY, int vpW, int vpH);
    void endFrame();

    void screenToWorldRay(float mx, float my, Vec3& origin, Vec3& direction) const;
    void updateCursorWorldPos(float mx, float my);
    void updateCursorWorldPosWithSnap(float mx, float my,
        std::function<Vec3(const Vec3&)> snapFn);

    ViewportState& state() { return m_state; }
    const ViewportState& state() const { return m_state; }

    static bool rayTriangleIntersect(const Vec3& ro, const Vec3& rd,
        const Vec3& v0, const Vec3& v1, const Vec3& v2, float& t);

private:
    ViewportState m_state;
};

} // namespace nexus::app
