#include <nexus/app/SketchPreview.h>
#include <nexus/parametric/ConstraintGraph.h>

#ifndef NEXUS_HEADLESS
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

namespace nexus::app {

void SketchPreview::render(nexus::cad::CadAutoConstraintSketch& sketch) const noexcept {
    const auto& profile = sketch.profile();
    if(profile.points.empty()) return;

#ifndef NEXUS_HEADLESS
    glPointSize(6.f);
    glColor3f(0.2f, 0.8f, 1.0f);
    glBegin(GL_POINTS);
    std::vector<std::pair<double,double>> pts2d;
    for(auto id : profile.points) {
        auto* pt = profile.graph.point(id);
        if(pt) { glVertex3d(pt->x, pt->y, pt->z); pts2d.push_back({pt->x, pt->z}); }
    }
    glEnd();
    glPointSize(1.f);
    // Draw connecting lines if 2+ points.
    if(pts2d.size() >= 2) {
        glColor3f(1.f, 1.f, 0.3f);
        glLineWidth(1.5f);
        glBegin(GL_LINE_STRIP);
        for(auto& p : pts2d) glVertex3d(p.first, 0, p.second);
        glEnd();
        glLineWidth(1.f);
    }
#endif
}

} // namespace nexus::app
