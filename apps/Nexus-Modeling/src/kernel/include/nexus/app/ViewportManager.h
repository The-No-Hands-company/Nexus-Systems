#pragma once
// ViewportManager — camera orbit/zoom, screen→world ray, working plane, grid

#include <nexus/cad/CadViewer.h>

#include <cstdint>
#include <functional>

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

enum class WorkPlane : uint8_t { XZ, XY, YZ };

struct ViewportState {
    int width = 1280, height = 720;
    WorkPlane workPlane = WorkPlane::XZ;
    Vec3 cursorWorldPos{};
    bool snapToGrid = true;
    float gridSpacing = 1.0f;
    float gridExtent = 10.0f;
    bool lighting = false;
};

class ViewportManager {
public:
    void initialize(int width, int height);
    void resize(int width, int height);

    nexus::cad::ViewportCamera& camera() { return m_camera; }
    const nexus::cad::ViewportCamera& camera() const { return m_camera; }
    bool ortho = false;

    void beginFrame(int vpX, int vpY, int vpW, int vpH);
    void endFrame();

    void screenToWorldRay(float mx, float my, Vec3& origin, Vec3& direction) const;
    Vec3 cursorWorldPos() const { return m_state.cursorWorldPos; }
    void updateCursorWorldPos(float mx, float my);

    ViewportState& state() { return m_state; }
    const ViewportState& state() const { return m_state; }

    static bool rayTriangleIntersect(const Vec3& ro, const Vec3& rd,
        const Vec3& v0, const Vec3& v1, const Vec3& v2, float& t);

private:
    nexus::cad::ViewportCamera m_camera;
    ViewportState m_state;
};

} // namespace nexus::app
