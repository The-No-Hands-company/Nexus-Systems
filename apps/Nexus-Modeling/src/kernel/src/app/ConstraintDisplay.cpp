// ─────────────────────────────────────────────────────────────────────────────
//  Nexus App Constraint Display — Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/app/ConstraintDisplay.h>

#ifndef NEXUS_HEADLESS
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

#include <cmath>

namespace nexus::app {

void ConstraintDisplay::render(const nexus::parametric::SketchProfile& profile,
                                const nexus::parametric::ConstraintGraph& graph) {
    (void)profile;
#ifndef NEXUS_HEADLESS
    renderCoincident(graph);
    renderHorizontal(graph);
    renderVertical(graph);
    renderDistance(graph);
    renderAngle(graph);
    renderParallel(graph);
    renderTangent(graph);
    renderMidpoint(graph);
#endif
}

void ConstraintDisplay::renderCoincident(const nexus::parametric::ConstraintGraph& graph) {
    for (const auto& c : graph.coincidentConstraints()) {
        auto* pa = graph.point(c.entityA);
        auto* pb = graph.point(c.entityB);
        if (!pa || !pb) continue;
        float cx = (float)((pa->x + pb->x) * 0.5);
        float cy = (float)((pa->y + pb->y) * 0.5);
        float cz = (float)((pa->z + pb->z) * 0.5);
        glPointSize(8.f);
        glColor3f(1.0f, 0.4f, 0.0f);
        glBegin(GL_POINTS);
        glVertex3f(cx, cy, cz);
        glEnd();
        glPointSize(1.f);
    }
}

void ConstraintDisplay::renderHorizontal(const nexus::parametric::ConstraintGraph& graph) {
    for (const auto& c : graph.axisAlignedDistanceConstraints()) {
        if (c.axis != nexus::parametric::Axis::Y) continue;
        auto* pa = graph.point(c.entityA);
        auto* pb = graph.point(c.entityB);
        if (!pa || !pb) continue;
        float mx = (float)((pa->x + pb->x) * 0.5);
        float my = (float)(pa->y);
        float mz = (float)((pa->z + pb->z) * 0.5);
        float s = 0.25f;
        glColor3f(0.2f, 0.6f, 1.0f);
        glLineWidth(2.f);
        glBegin(GL_LINES);
        glVertex3f(mx - s, my, mz); glVertex3f(mx + s, my, mz);
        glVertex3f(mx - s, my - s * 0.3f, mz); glVertex3f(mx - s, my + s * 0.3f, mz);
        glVertex3f(mx + s, my - s * 0.3f, mz); glVertex3f(mx + s, my + s * 0.3f, mz);
        glEnd();
        glLineWidth(1.f);
    }
}

void ConstraintDisplay::renderVertical(const nexus::parametric::ConstraintGraph& graph) {
    for (const auto& c : graph.axisAlignedDistanceConstraints()) {
        if (c.axis != nexus::parametric::Axis::X) continue;
        auto* pa = graph.point(c.entityA);
        auto* pb = graph.point(c.entityB);
        if (!pa || !pb) continue;
        float mx = (float)(pa->x);
        float my = (float)((pa->y + pb->y) * 0.5);
        float mz = (float)((pa->z + pb->z) * 0.5);
        float s = 0.25f;
        glColor3f(0.2f, 0.6f, 1.0f);
        glLineWidth(2.f);
        glBegin(GL_LINES);
        glVertex3f(mx, my - s, mz); glVertex3f(mx, my + s, mz);
        glVertex3f(mx - s * 0.3f, my - s, mz); glVertex3f(mx + s * 0.3f, my - s, mz);
        glVertex3f(mx - s * 0.3f, my + s, mz); glVertex3f(mx + s * 0.3f, my + s, mz);
        glEnd();
        glLineWidth(1.f);
    }
}

void ConstraintDisplay::renderDistance(const nexus::parametric::ConstraintGraph& graph) {
    for (const auto& c : graph.distanceConstraints()) {
        auto* pa = graph.point(c.entityA);
        auto* pb = graph.point(c.entityB);
        if (!pa || !pb) continue;
        float ax = (float)pa->x, ay = (float)pa->y, az = (float)pa->z;
        float bx = (float)pb->x, by = (float)pb->y, bz = (float)pb->z;
        glColor3f(1.0f, 0.9f, 0.25f);
        glLineWidth(1.5f);
        glLineStipple(1, 0x0F0F);
        glEnable(GL_LINE_STIPPLE);
        glBegin(GL_LINES);
        glVertex3f(ax, ay, az); glVertex3f(bx, by, bz);
        glEnd();
        glDisable(GL_LINE_STIPPLE);
        glLineWidth(1.f);
    }
}

void ConstraintDisplay::renderAngle(const nexus::parametric::ConstraintGraph& graph) {
    for (const auto& c : graph.angleConstraints()) {
        auto* pa = graph.point(c.entityA);
        auto* pb = graph.point(c.entityB);
        auto* pc = graph.point(c.entityC);
        if (!pa || !pb || !pc) continue;
        float bx = (float)pb->x, by = (float)pb->y, bz = (float)pb->z;
        float ax = (float)pa->x - bx, ay = (float)pa->y - by, az = (float)pa->z - bz;
        float cx = (float)pc->x - bx, cy = (float)pc->y - by, cz = (float)pc->z - bz;
        float la = std::sqrt(ax*ax + ay*ay + az*az);
        float lc = std::sqrt(cx*cx + cy*cy + cz*cz);
        if (la < 1e-10f || lc < 1e-10f) continue;
        float r = 0.3f;
        ax /= la; ay /= la; az /= la;
        cx /= lc; cy /= lc; cz /= lc;
        glColor3f(1.0f, 0.6f, 0.8f);
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i <= 16; ++i) {
            float t = (float)i / 16.0f;
            float nx = ax * (1.f - t) + cx * t;
            float ny = ay * (1.f - t) + cy * t;
            float nz = az * (1.f - t) + cz * t;
            float nl = std::sqrt(nx*nx + ny*ny + nz*nz);
            if (nl > 1e-10f) { nx /= nl; ny /= nl; nz /= nl; }
            glVertex3f(bx + nx * r, by + ny * r, bz + nz * r);
        }
        glEnd();
        glLineWidth(1.f);
    }
}

void ConstraintDisplay::renderParallel(const nexus::parametric::ConstraintGraph& graph) {
    renderAngle(graph);
}

void ConstraintDisplay::renderTangent(const nexus::parametric::ConstraintGraph& graph) {
    for (const auto& c : graph.equalDistanceConstraints()) {
        auto* pa = graph.point(c.entityA);
        auto* pb = graph.point(c.entityB);
        if (!pa || !pb) continue;
        float mx = (float)((pa->x + pb->x) * 0.5);
        float my = (float)((pa->y + pb->y) * 0.5);
        float mz = (float)((pa->z + pb->z) * 0.5);
        float s = 0.15f;
        glColor3f(0.5f, 1.0f, 0.5f);
        glLineWidth(1.5f);
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 24; ++i) {
            float a = (float)i * 0.2618f;
            glVertex3f(mx + std::cos(a) * s, my, mz + std::sin(a) * s);
        }
        glEnd();
        glLineWidth(1.f);
    }
}

void ConstraintDisplay::renderMidpoint(const nexus::parametric::ConstraintGraph& graph) {
    for (const auto& c : graph.pointOnLineConstraints()) {
        auto* pp = graph.point(c.entityP);
        if (!pp) continue;
        float s = 0.15f;
        glColor3f(0.8f, 0.8f, 0.2f);
        glPointSize(6.f);
        glBegin(GL_POINTS);
        glVertex3f(pp->x, pp->y, pp->z);
        glEnd();
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 4; ++i) {
            float a = (float)i * 1.5708f + 0.7854f;
            glVertex3f(pp->x + std::cos(a) * s, pp->y, pp->z + std::sin(a) * s);
        }
        glEnd();
        glPointSize(1.f);
    }
}

} // namespace nexus::app
