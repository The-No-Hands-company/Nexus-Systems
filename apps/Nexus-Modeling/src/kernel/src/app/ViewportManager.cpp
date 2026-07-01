#include <nexus/app/ViewportManager.h>

#ifndef NEXUS_HEADLESS
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#endif

#include <cmath>

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

void ViewportManager::initialize(int width, int height) {
    m_state.width = width;
    m_state.height = height;
    m_camera.lookAt({0, 0, 0}, 20.f);
}

void ViewportManager::resize(int width, int height) {
    m_state.width = width;
    m_state.height = height;
}

void ViewportManager::beginFrame(int vpX, int vpY, int vpW, int vpH) {
#ifndef NEXUS_HEADLESS
    glViewport(vpX, vpY, vpW, vpH);
    glScissor(vpX, vpY, vpW, vpH);
    glEnable(GL_SCISSOR_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (vpH <= 0) vpH = 1;
    float aspect = (float)vpW / (float)vpH;
    if (ortho) {
        float dist = m_camera.distance();
        float halfH = dist * 0.6f;
        glOrtho(-halfH * aspect, halfH * aspect, -halfH, halfH, 0.1, 1000.0);
    } else {
        gluPerspective(45.0, aspect, 0.1, 1000.0);
    }

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(m_camera.position().x, m_camera.position().y, m_camera.position().z,
              m_camera.target().x,   m_camera.target().y,   m_camera.target().z,
              m_camera.up().x,       m_camera.up().y,       m_camera.up().z);
#endif
}

void ViewportManager::endFrame() {
#ifndef NEXUS_HEADLESS
    glDisable(GL_SCISSOR_TEST);
#endif
}

void ViewportManager::screenToWorldRay(float mx, float my,
    Vec3& origin, Vec3& direction) const {
#ifndef NEXUS_HEADLESS
    GLdouble mv[16], proj[16]; GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, vp);
    double winY = vp[3] - my;
    double nx, ny, nz, fx, fy, fz;
    gluUnProject(mx, winY, 0.0, mv, proj, vp, &nx, &ny, &nz);
    gluUnProject(mx, winY, 1.0, mv, proj, vp, &fx, &fy, &fz);
    origin = Vec3{(float)nx, (float)ny, (float)nz};
    direction = Vec3{(float)(fx - nx), (float)(fy - ny), (float)(fz - nz)};
    float len = std::sqrt(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z);
    if (len > 1e-6f) direction = direction * (1.f / len);
#else
    (void)mx; (void)my; origin = {}; direction = {0, 0, 1};
#endif
}

void ViewportManager::updateCursorWorldPos(float mx, float my) {
    Vec3 origin, dir;
    screenToWorldRay(mx, my, origin, dir);
    Vec3 planeN{0, 1, 0};
    if (m_state.workPlane == WorkPlane::XY) planeN = {0, 0, 1};
    else if (m_state.workPlane == WorkPlane::YZ) planeN = {1, 0, 0};
    float denom = dir.dot(planeN);
    if (std::fabs(denom) > 1e-6f) {
        float t = -origin.dot(planeN) / denom;
        if (t > 0) m_state.cursorWorldPos = origin + dir * t;
    }
}

bool ViewportManager::rayTriangleIntersect(const Vec3& ro, const Vec3& rd,
    const Vec3& v0, const Vec3& v1, const Vec3& v2, float& t) {
    Vec3 e1 = v1 - v0, e2 = v2 - v0;
    Vec3 h = rd.cross(e2);
    float a = e1.dot(h);
    if (std::fabs(a) < 1e-7f) return false;
    float f = 1.f / a;
    Vec3 s = ro - v0;
    float u = f * s.dot(h);
    if (u < 0.f || u > 1.f) return false;
    Vec3 q = s.cross(e1);
    float v = f * rd.dot(q);
    if (v < 0.f || u + v > 1.f) return false;
    float tt = f * e2.dot(q);
    if (tt < 1e-4f) return false;
    t = tt;
    return true;
}

} // namespace nexus::app
