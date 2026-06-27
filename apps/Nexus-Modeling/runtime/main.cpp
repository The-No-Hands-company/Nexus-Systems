// ────────────────────────────────────────────────────────────────────────────
//  Nexus Runtime Viewer — Standalone model viewer
//
//  Loads .nxm files, orbital camera, fullscreen toggle.
//  No editor UI — pure model viewing.
// ────────────────────────────────────────────────────────────────────────────

#include <GLFW/glfw3.h>
#include <GL/glu.h>

#include <nexus/geometry/NexusFormat.h>
#include <nexus/render/Camera.h>

#include <cstdio>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace nexus::geometry;
using namespace nexus::render;

struct ViewerState {
    GLFWwindow* window = nullptr;
    int width = 1280, height = 720;
    Camera camera;
    Vec3 eye{0,0,10};
    Vec3 target{0,0,0};
    Vec3 up{0,1,0};
    std::vector<Mesh> meshes;
    bool wireframe = false;
    bool lighting = true;
    bool fullscreen = false;
    int windowedX = 100, windowedY = 100;
    int windowedW = 1280, windowedH = 720;
    double lastMouseX = 0, lastMouseY = 0;
    bool mouseDragging = false;
    float fps = 0;
    int frameCount = 0;
    double lastFpsTime = 0;
};

static ViewerState g_state;

static void framebufferSizeCallback(GLFWwindow*, int w, int h) {
    g_state.width = w; g_state.height = h;
    glViewport(0, 0, w, h);
}

static void keyCallback(GLFWwindow* w, int key, int /*scancode*/, int action, int /*mods*/) {
    if(action != GLFW_PRESS) return;
    if(key == GLFW_KEY_ESCAPE) {
        glfwSetWindowShouldClose(w, 1);
    }
    if(key == GLFW_KEY_F11) {
        g_state.fullscreen = !g_state.fullscreen;
        if(g_state.fullscreen) {
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            const GLFWvidmode* mode = glfwGetVideoMode(monitor);
            glfwGetWindowPos(w, &g_state.windowedX, &g_state.windowedY);
            glfwGetWindowSize(w, &g_state.windowedW, &g_state.windowedH);
            glfwSetWindowMonitor(w, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        } else {
            glfwSetWindowMonitor(w, nullptr, g_state.windowedX, g_state.windowedY,
                                 g_state.windowedW, g_state.windowedH, 0);
        }
    }
    if(key == GLFW_KEY_W) g_state.wireframe = !g_state.wireframe;
    if(key == GLFW_KEY_L) g_state.lighting = !g_state.lighting;
    if(key == GLFW_KEY_HOME) {
        Aabb total{{1e10,1e10,1e10},{-1e10,-1e10,-1e10}};
        bool hasMesh = false;
        for(auto& m : g_state.meshes) {
            auto b = m.computeBounds();
            total.min.x = std::min(total.min.x, b.min.x);
            total.min.y = std::min(total.min.y, b.min.y);
            total.min.z = std::min(total.min.z, b.min.z);
            total.max.x = std::max(total.max.x, b.max.x);
            total.max.y = std::max(total.max.y, b.max.y);
            total.max.z = std::max(total.max.z, b.max.z);
            hasMesh = true;
        }
        if(hasMesh) {
            auto e = total.extents();
            float d = std::max({e.x, e.y, e.z}) * 3.f + 5.f;
            if(d > 1e8) d = 10.f;
            g_state.eye = total.center() + Vec3{0,0,d};
            g_state.target = total.center();
            g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up);
        }
    }
    // Standard views.
    if(key == GLFW_KEY_KP_0) { g_state.eye = Vec3{1,1,1}.normalize() * 10.f; g_state.target = {0,0,0}; g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up); }
    if(key == GLFW_KEY_KP_1) { g_state.eye = {0,0,10}; g_state.target = {0,0,0}; g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up); }
    if(key == GLFW_KEY_KP_3) { g_state.eye = {10,0,0}; g_state.target = {0,0,0}; g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up); }
    if(key == GLFW_KEY_KP_7) { g_state.eye = {0,10,0}; g_state.target = {0,0,0}; g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up); }
    // F = frame selected / frame all
    if(key == GLFW_KEY_F) {
        Aabb total{{1e10,1e10,1e10},{-1e10,-1e10,-1e10}};
        for(auto& m : g_state.meshes) {
            auto b = m.computeBounds();
            total.min.x = std::min(total.min.x, b.min.x);
            total.min.y = std::min(total.min.y, b.min.y);
            total.min.z = std::min(total.min.z, b.min.z);
            total.max.x = std::max(total.max.x, b.max.x);
            total.max.y = std::max(total.max.y, b.max.y);
            total.max.z = std::max(total.max.z, b.max.z);
        }
        auto e = total.extents();
        float d = std::max({e.x, e.y, e.z}) * 3.f + 5.f;
        if(d > 1e8) d = 10.f;
        g_state.eye = total.center() + Vec3{0,0,d};
        g_state.target = total.center();
        g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up);
    }
}

static void mouseButtonCallback(GLFWwindow* w, int button, int action, int /*mods*/) {
    if(button == GLFW_MOUSE_BUTTON_LEFT) {
        if(action == GLFW_PRESS) {
            g_state.mouseDragging = true;
            glfwGetCursorPos(w, &g_state.lastMouseX, &g_state.lastMouseY);
        } else {
            g_state.mouseDragging = false;
        }
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
        g_state.eye = {0,0,10};
        g_state.target = {0,0,0};
        g_state.up = {0,1,0};
        g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up);
    }
}

static void cursorPosCallback(GLFWwindow* /*w*/, double x, double y) {
    if(g_state.mouseDragging) {
        double dx = x - g_state.lastMouseX;
        double dy = y - g_state.lastMouseY;
        // Orbit around target.
        Vec3 dir = g_state.eye - g_state.target;
        float dist = dir.length();
        if(dist > 0.001f) {
            Vec3 fwd = dir * (1.f/dist);
            Vec3 right = fwd.cross(g_state.up).normalize();
            Vec3 worldUp = right.cross(fwd).normalize();
            float ay = dx * 0.005f;
            float ax = dy * 0.005f;
            // Horizontal orbit around world up.
            float cosY = std::cos(ay), sinY = std::sin(ay);
            Vec3 newDir;
            newDir.x = fwd.x * cosY + right.x * sinY;
            newDir.y = fwd.y * cosY + right.y * sinY;
            newDir.z = fwd.z * cosY + right.z * sinY;
            // Vertical orbit around right.
            float cosX = std::cos(ax), sinX = std::sin(ax);
            fwd = newDir.normalize();
            Vec3 upNew = worldUp;
            newDir.x = fwd.x * cosX - upNew.x * sinX;
            newDir.y = fwd.y * cosX - upNew.y * sinX;
            newDir.z = fwd.z * cosX - upNew.z * sinX;
            g_state.eye = g_state.target + newDir.normalize() * dist;
        }
        g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up);
        g_state.lastMouseX = x;
        g_state.lastMouseY = y;
    }
}

static void scrollCallback(GLFWwindow*, double /*xoff*/, double yoff) {
    Vec3 dir = g_state.eye - g_state.target;
    float dist = dir.length();
    float newDist = dist - yoff * 5.f;
    if(newDist < 0.5f) newDist = 0.5f;
    if(newDist > 1000.f) newDist = 1000.f;
    g_state.eye = g_state.target + dir * (newDist / dist);
    g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up);
}

static void renderMeshes() {
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.12f, 0.12f, 0.14f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Camera.
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    float aspect = (float)g_state.width / (float)g_state.height;
    gluPerspective(45.0, aspect, 0.1, 1000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(g_state.eye.x, g_state.eye.y, g_state.eye.z,
              g_state.target.x, g_state.target.y, g_state.target.z,
              g_state.up.x, g_state.up.y, g_state.up.z);

    // Lighting.
    if(g_state.lighting) {
        glEnable(GL_LIGHTING);
        glEnable(GL_LIGHT0);
        glEnable(GL_NORMALIZE);
        GLfloat ambient[]  = {0.2f, 0.2f, 0.25f, 1.f};
        GLfloat diffuse[]  = {0.8f, 0.75f, 0.7f, 1.f};
        GLfloat lightPos[] = {5.f, 10.f, 8.f, 0.f};
        glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
        glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
        glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    } else {
        glDisable(GL_LIGHTING);
    }

    if(g_state.wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    else glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    for(auto& mesh : g_state.meshes) {
        const auto& pos = mesh.attributes().positions();
        const auto& topo = mesh.topology();
        if(pos.empty() || topo.faceCount() == 0) continue;

        std::vector<float> varray, narray;
        size_t psz = pos.size();
        for(uint32_t fi = 0; fi < topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            if(face.vertexCount() < 3) continue;
            for(size_t j = 0; j + 2 < face.vertexCount(); ++j) {
                uint32_t i0 = face.indices[0], i1 = face.indices[j+1], i2 = face.indices[j+2];
                if(i0 >= psz || i1 >= psz || i2 >= psz) continue;
                auto& v0 = pos[i0], &v1 = pos[i1], &v2 = pos[i2];
                varray.insert(varray.end(), {v0.x, v0.y, v0.z, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z});
                if(g_state.lighting) {
                    Vec3 fn = (v1 - v0).cross(v2 - v0).normalize();
                    narray.insert(narray.end(), {fn.x, fn.y, fn.z, fn.x, fn.y, fn.z, fn.x, fn.y, fn.z});
                }
            }
        }

        if(!varray.empty()) {
            glColor3f(0.7f, 0.7f, 0.75f);
            glEnableClientState(GL_VERTEX_ARRAY);
            glVertexPointer(3, GL_FLOAT, 0, varray.data());
            if(g_state.lighting && !narray.empty()) {
                glEnableClientState(GL_NORMAL_ARRAY);
                glNormalPointer(GL_FLOAT, 0, narray.data());
            }
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)varray.size() / 3);
            glDisableClientState(GL_VERTEX_ARRAY);
            if(g_state.lighting) glDisableClientState(GL_NORMAL_ARRAY);
        }
    }

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    // FPS in window title.
    {
        char title[128];
        snprintf(title, sizeof(title), "Nexus Runtime Viewer  |  %.0f FPS  |  %zu meshes  |  F11=FS  W=Wire  L=Light",
                 g_state.fps, g_state.meshes.size());
        glfwSetWindowTitle(g_state.window, title);
    }
}

int main(int argc, char** argv) {
    if(!glfwInit()) {
        fprintf(stderr, "GLFW init failed\n");
        return 1;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    g_state.window = glfwCreateWindow(1280, 720, "Nexus Runtime Viewer", nullptr, nullptr);
    if(!g_state.window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(g_state.window);
    glfwSwapInterval(1);

    glEnable(GL_MULTISAMPLE);

    // Load .nxm file.
    std::string path = "scene.nxm";
    if(argc > 1) path = argv[1];

    printf("Nexus Runtime Viewer\n");
    printf("  Loading: %s\n", path.c_str());

    g_state.meshes = NexusFormat::loadFromFile(path);
    if(g_state.meshes.empty()) {
        printf("  No meshes found (or file missing). Running with empty scene.\n");
    } else {
        printf("  Loaded %zu meshes\n", g_state.meshes.size());
        // Frame all.
        Aabb total{{1e10,1e10,1e10},{-1e10,-1e10,-1e10}};
        for(auto& m : g_state.meshes) {
            auto b = m.computeBounds();
            total.min.x = std::min(total.min.x, b.min.x);
            total.min.y = std::min(total.min.y, b.min.y);
            total.min.z = std::min(total.min.z, b.min.z);
            total.max.x = std::max(total.max.x, b.max.x);
            total.max.y = std::max(total.max.y, b.max.y);
            total.max.z = std::max(total.max.z, b.max.z);
        }
        auto e = total.extents();
        float d = std::max({e.x, e.y, e.z}) * 3.f + 5.f;
        if(d > 1e8) d = 10.f;
        g_state.eye = total.center() + Vec3{0,0,d};
        g_state.target = total.center();
        g_state.camera.lookAt(g_state.eye, g_state.target, g_state.up);
    }

    glfwSetKeyCallback(g_state.window, keyCallback);
    glfwSetMouseButtonCallback(g_state.window, mouseButtonCallback);
    glfwSetCursorPosCallback(g_state.window, cursorPosCallback);
    glfwSetScrollCallback(g_state.window, scrollCallback);
    glfwSetFramebufferSizeCallback(g_state.window, framebufferSizeCallback);

    g_state.lastFpsTime = glfwGetTime();

    while(!glfwWindowShouldClose(g_state.window)) {
        renderMeshes();
        glfwSwapBuffers(g_state.window);
        glfwPollEvents();

        // FPS counter.
        g_state.frameCount++;
        double now = glfwGetTime();
        if(now - g_state.lastFpsTime >= 1.0) {
            g_state.fps = (float)(g_state.frameCount / (now - g_state.lastFpsTime));
            g_state.frameCount = 0;
            g_state.lastFpsTime = now;
        }
    }

    glfwTerminate();
    return 0;
}
