#include <nexus/app/TransformGizmo.h>
#include <nexus/cad/CadCommand.h>

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

void TransformGizmo::render(const Vec3& center) const noexcept {
#ifndef NEXUS_HEADLESS
    GLdouble mv[16], proj[16]; GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, vp);

    // Constant 100 screen pixels regardless of zoom
    GLdouble cx, cy, cz, ux, uy, uz;
    gluProject(center.x, center.y, center.z, mv, proj, vp, &cx, &cy, &cz);
    gluProject(center.x + 1.f, center.y, center.z, mv, proj, vp, &ux, &uy, &uz);
    float pixelScale = std::max(std::fabs((float)(ux - cx)), 0.001f);
    float s = 100.f / pixelScale; // constant 100 screen pixels
    float h = s * 0.25f, w = s * 0.12f;

    if (m_mode == Mode::Translate) {
        // Three colored arrows with cone tips
        glLineWidth(3.f);
        glBegin(GL_LINES);
        glColor3f(1,0,0); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x+s,center.y,center.z);
        glColor3f(0,1,0); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x,center.y+s,center.z);
        glColor3f(0,0,1); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x,center.y,center.z+s);
        glEnd();

        glBegin(GL_TRIANGLES);
        glColor3f(1,0,0);
        glVertex3f(center.x+s,center.y,center.z); glVertex3f(center.x+s-h,center.y+w,center.z); glVertex3f(center.x+s-h,center.y-w,center.z);
        glColor3f(0,1,0);
        glVertex3f(center.x,center.y+s,center.z); glVertex3f(center.x+w,center.y+s-h,center.z); glVertex3f(center.x-w,center.y+s-h,center.z);
        glColor3f(0,0,1);
        glVertex3f(center.x,center.y,center.z+s); glVertex3f(center.x+w,center.y,center.z+s-h); glVertex3f(center.x-w,center.y,center.z+s-h);
        glEnd();
    } else if (m_mode == Mode::Rotate) {
        // Three colored circle rings (rotate handles)
        float r = s * 0.75f;
        int segments = 48;
        glLineWidth(3.f);
        // X circle (YZ plane)
        glColor3f(1,0,0);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float a = (float)i / (float)segments * 6.2831853f;
            glVertex3f(center.x, center.y + r * cosf(a), center.z + r * sinf(a));
        }
        glEnd();
        // Y circle (XZ plane)
        glColor3f(0,1,0);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float a = (float)i / (float)segments * 6.2831853f;
            glVertex3f(center.x + r * cosf(a), center.y, center.z + r * sinf(a));
        }
        glEnd();
        // Z circle (XY plane)
        glColor3f(0,0,1);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < segments; ++i) {
            float a = (float)i / (float)segments * 6.2831853f;
            glVertex3f(center.x + r * cosf(a), center.y + r * sinf(a), center.z);
        }
        glEnd();
    } else { // Scale
        // Three colored lines with cube tips
        glLineWidth(3.f);
        glBegin(GL_LINES);
        glColor3f(1,0,0); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x+s,center.y,center.z);
        glColor3f(0,1,0); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x,center.y+s,center.z);
        glColor3f(0,0,1); glVertex3f(center.x,center.y,center.z); glVertex3f(center.x,center.y,center.z+s);
        glEnd();

        float b = s * 0.08f; // box half-size
        glBegin(GL_QUADS);
        // X box tip (at end of X arrow)
        glColor3f(1,0,0);
        float x = center.x + s;
        glVertex3f(x-b,center.y-b,center.z-b); glVertex3f(x+b,center.y-b,center.z-b);
        glVertex3f(x+b,center.y+b,center.z-b); glVertex3f(x-b,center.y+b,center.z-b);
        glVertex3f(x-b,center.y-b,center.z+b); glVertex3f(x+b,center.y-b,center.z+b);
        glVertex3f(x+b,center.y+b,center.z+b); glVertex3f(x-b,center.y+b,center.z+b);
        // Y box tip
        glColor3f(0,1,0);
        float y = center.y + s;
        glVertex3f(center.x-b,y,center.z-b); glVertex3f(center.x+b,y,center.z-b);
        glVertex3f(center.x+b,y,center.z+b); glVertex3f(center.x-b,y,center.z+b);
        // Z box tip
        glColor3f(0,0,1);
        float z = center.z + s;
        glVertex3f(center.x-b,center.y-b,z); glVertex3f(center.x+b,center.y-b,z);
        glVertex3f(center.x+b,center.y+b,z); glVertex3f(center.x-b,center.y+b,z);
        glEnd();
    }
    glLineWidth(1.f);
#else
    (void)center;
#endif
}

TransformGizmo::Axis TransformGizmo::pickAxis(
    const Vec3& center, const Vec3& rayOrigin,
    const Vec3& rayDirection) const noexcept {
#ifndef NEXUS_HEADLESS
    GLdouble mv[16], proj[16]; GLint vp[4];
    glGetDoublev(GL_MODELVIEW_MATRIX, mv);
    glGetDoublev(GL_PROJECTION_MATRIX, proj);
    glGetIntegerv(GL_VIEWPORT, vp);

    auto screenDist = [&](const Vec3& worldPos) -> float {
        GLdouble sx, sy, sz;
        gluProject(worldPos.x, worldPos.y, worldPos.z, mv, proj, vp, &sx, &sy, &sz);
        if(sz < 0.0 || sz > 1.0) return 1e9f;

        GLdouble cx, cy, cz;
        gluProject(rayOrigin.x, rayOrigin.y, rayOrigin.z, mv, proj, vp, &cx, &cy, &cz);

        float dx = (float)(sx - cx);
        float dy = (float)(sy - cy);
        return std::sqrt(dx*dx + dy*dy) / (float)vp[2];
    };

    // Check distance to each axis LINE (not just the tip)
    GLdouble px, py, pz, pux, puy, puz;
    gluProject(center.x, center.y, center.z, mv, proj, vp, &px, &py, &pz);
    gluProject(center.x + 1.f, center.y, center.z, mv, proj, vp, &pux, &puy, &puz);
    float pickPixelScale = std::max(std::fabs((float)(pux - px)), 0.001f);
    float s = 100.f / pickPixelScale; // constant 100 screen pixels
    auto axisDist = [&](const Vec3& dir) -> float {
        // Project cursor world position onto this axis to find the closest point
        Vec3 ro = rayOrigin;
        Vec3 rd = rayDirection;
        // Find closest point on axis to the ray
        Vec3 w = ro - center;
        float a = rd.dot(rd);
        float b = rd.dot(dir);
        float c = dir.dot(dir);
        float d = rd.dot(w);
        float e = dir.dot(w);
        float denom = a * c - b * b;
        if (std::fabs(denom) < 1e-10f) return 1e9f;
        float t = (b * e - c * d) / denom;
        float u = e + b * t;
        Vec3 closestOnAxis = center + dir * std::clamp(u, 0.0f, s);
        return screenDist(closestOnAxis);
    };

    float dX = axisDist({1,0,0});
    float dY = axisDist({0,1,0});
    float dZ = axisDist({0,0,1});

    const float threshold = 0.08f;
    if(dX < threshold && dX < dY && dX < dZ) return Axis::X;
    if(dY < threshold && dY < dX && dY < dZ) return Axis::Y;
    if(dZ < threshold && dZ < dX && dZ < dY) return Axis::Z;
#else
    (void)center; (void)rayOrigin; (void)rayDirection;
#endif
    return Axis::None;
}

void TransformGizmo::translate(const Vec3& /*center*/, Axis axis, float amount,
                                 nexus::cad::CadDocument& doc,
                                 nexus::parametric::FeatureId fid, bool pushUndo) const noexcept {
    auto* node = doc.history().node(fid);
    if(!node || !node->mesh) return;

    // Save mesh for potential undo.
    geometry::Mesh saved = *node->mesh;

    Vec3 offset{};
    if(axis == Axis::X) offset.x = amount;
    else if(axis == Axis::Y) offset.y = amount;
    else if(axis == Axis::Z) offset.z = amount;
    else return;

    auto pos = node->mesh->attributes().positions();
    for(auto& v : pos) { v.x += offset.x; v.y += offset.y; v.z += offset.z; }
    node->mesh->attributes().setPositions(std::move(pos));

    if (pushUndo) {
        auto cmd = std::make_unique<nexus::cad::TransformCommand>(fid, std::move(saved));
        (void)doc.executeCommand(std::move(cmd));
    }
}

void TransformGizmo::scale(const Vec3& center, Axis axis, float factor,
                             nexus::cad::CadDocument& doc,
                             nexus::parametric::FeatureId fid, bool pushUndo) const noexcept {
    auto* node = doc.history().node(fid);
    if(!node || !node->mesh) return;

    geometry::Mesh saved = *node->mesh;

    auto pos = node->mesh->attributes().positions();
    Vec3 scaleVec{1,1,1};
    if(axis == Axis::X) scaleVec.x = factor;
    else if(axis == Axis::Y) scaleVec.y = factor;
    else if(axis == Axis::Z) scaleVec.z = factor;
    else { scaleVec.x = factor; scaleVec.y = factor; scaleVec.z = factor; }
    for(auto& v : pos) {
        v.x = center.x + (v.x - center.x) * scaleVec.x;
        v.y = center.y + (v.y - center.y) * scaleVec.y;
        v.z = center.z + (v.z - center.z) * scaleVec.z;
    }
    node->mesh->attributes().setPositions(std::move(pos));

    if (pushUndo) {
        auto cmd = std::make_unique<nexus::cad::TransformCommand>(fid, std::move(saved));
        (void)doc.executeCommand(std::move(cmd));
    }
}

void TransformGizmo::rotate(const Vec3& center, Axis axis, float angleRad,
                              nexus::cad::CadDocument& doc,
                              nexus::parametric::FeatureId fid, bool pushUndo) const noexcept {
    auto* node = doc.history().node(fid);
    if(!node || !node->mesh) return;

    geometry::Mesh saved = *node->mesh;

    float c = std::cos(angleRad), s = std::sin(angleRad);
    auto pos = node->mesh->attributes().positions();
    for(auto& v : pos) {
        float rx = v.x - center.x;
        float ry = v.y - center.y;
        float rz = v.z - center.z;
        if(axis == Axis::X) { float ny = ry*c - rz*s; float nz = ry*s + rz*c; v.y = center.y+ny; v.z = center.z+nz; }
        else if(axis == Axis::Y) { float nx = rx*c + rz*s; float nz = -rx*s + rz*c; v.x = center.x+nx; v.z = center.z+nz; }
        else if(axis == Axis::Z) { float nx = rx*c - ry*s; float ny = rx*s + ry*c; v.x = center.x+nx; v.y = center.y+ny; }
    }
    node->mesh->attributes().setPositions(std::move(pos));

    if (pushUndo) {
        auto cmd = std::make_unique<nexus::cad::TransformCommand>(fid, std::move(saved));
        (void)doc.executeCommand(std::move(cmd));
    }
}

} // namespace nexus::app
