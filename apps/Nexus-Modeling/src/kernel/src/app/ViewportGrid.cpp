#include <nexus/app/ViewportGrid.h>

#ifndef NEXUS_HEADLESS
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

namespace nexus::app {

void ViewportGrid::renderXZGrid(float extent, float spacing) const noexcept {
#ifndef NEXUS_HEADLESS
    if (spacing <= 0) spacing = 1.f;
    int n = static_cast<int>(extent / spacing);
    if (n < 1) n = 1;
    glColor3f(0.25f, 0.25f, 0.28f);
    glBegin(GL_LINES);
    for(int i = -n; i <= n; ++i) {
        float x = static_cast<float>(i) * spacing;
        glVertex3f(x, 0, -extent);
        glVertex3f(x, 0,  extent);
        glVertex3f(-extent, 0, x);
        glVertex3f( extent, 0, x);
    }
    glEnd();
#endif
}

void ViewportGrid::renderXYGrid(float extent, float spacing) const noexcept {
#ifndef NEXUS_HEADLESS
    glColor3f(0.28f, 0.28f, 0.25f);
    glBegin(GL_LINES);
    for(int i = -static_cast<int>(extent); i <= static_cast<int>(extent); ++i) {
        float v = static_cast<float>(i) * spacing;
        glVertex3f(v, -extent, 0); glVertex3f(v, extent, 0);
        glVertex3f(-extent, v, 0); glVertex3f(extent, v, 0);
    }
    glEnd();
#endif
}

void ViewportGrid::renderYZGrid(float extent, float spacing) const noexcept {
#ifndef NEXUS_HEADLESS
    glColor3f(0.25f, 0.28f, 0.28f);
    glBegin(GL_LINES);
    for(int i = -static_cast<int>(extent); i <= static_cast<int>(extent); ++i) {
        float v = static_cast<float>(i) * spacing;
        glVertex3f(0, v, -extent); glVertex3f(0, v, extent);
        glVertex3f(0, -extent, v); glVertex3f(0, extent, v);
    }
    glEnd();
#endif
}

void ViewportGrid::render(const GridOptions& opts) const noexcept {
    if(opts.drawGrid) {
        if(opts.workPlane == kWorkPlaneXY) renderXYGrid(opts.extent, opts.spacing);
        else if(opts.workPlane == kWorkPlaneYZ) renderYZGrid(opts.extent, opts.spacing);
        else renderXZGrid(opts.extent, opts.spacing);
    }

    if(opts.drawAxes) {
#ifndef NEXUS_HEADLESS
        glLineWidth(2.f);
        glBegin(GL_LINES);
        glColor3f(1,0,0); glVertex3f(0,0,0); glVertex3f(opts.axisLength,0,0);
        glColor3f(0,1,0); glVertex3f(0,0,0); glVertex3f(0,opts.axisLength,0);
        glColor3f(0,0,1); glVertex3f(0,0,0); glVertex3f(0,0,opts.axisLength);
        glEnd();
        glLineWidth(1.f);
#endif
    }
}

} // namespace nexus::app
