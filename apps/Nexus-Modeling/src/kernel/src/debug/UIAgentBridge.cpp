// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Debug — UI Agent Bridge Implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/debug/UIAgentBridge.h>

#include <GL/gl.h>

#include <cstdio>
#include <cstring>
#include <sstream>

namespace nexus::debug {

UIAgentBridge::UIAgentBridge()
{
    m_agentLoop.setConfig(nexus::agent::AgentLoopConfig{});
}

UIAgentBridge::~UIAgentBridge() = default;

bool UIAgentBridge::captureSnapshot(const UISnapshot& snapshot)
{
    bool changed = (m_latest.activeMode != snapshot.activeMode ||
                    m_latest.activeSelectedFeature != snapshot.activeSelectedFeature ||
                    m_latest.selectedFeatureIds != snapshot.selectedFeatureIds ||
                    m_latest.timestamp != snapshot.timestamp);

    m_latest = snapshot;
    return changed;
}

std::string UIAgentBridge::exportSnapshotJson() const
{
    const auto& s = m_latest;
    std::ostringstream js;

    auto writeVec3 = [&](const char* key, float x, float y, float z) {
        js << "\"" << key << "\":[" << x << "," << y << "," << z << "]";
    };

    auto writeUintList = [&](const char* key, const std::vector<uint32_t>& v) {
        js << "\"" << key << "\":[";
        for (size_t i = 0; i < v.size(); ++i) {
            if (i > 0) js << ",";
            js << v[i];
        }
        js << "]";
    };

    auto writeBool = [&](const char* key, bool val) {
        js << "\"" << key << "\":" << (val ? "true" : "false");
    };

    js << "{";

    js << "\"viewport\":{" << "\"width\":" << s.viewportWidth
       << ",\"height\":" << s.viewportHeight << "},";

    js << "\"camera\":{";
    writeVec3("position", s.cameraPosX, s.cameraPosY, s.cameraPosZ); js << ",";
    writeVec3("target",   s.cameraTargetX, s.cameraTargetY, s.cameraTargetZ); js << ",";
    writeVec3("up",       s.cameraUpX, s.cameraUpY, s.cameraUpZ); js << ",";
    js << "\"distance\":" << s.cameraDistance;
    js << "},";

    js << "\"mode\":{";
    js << "\"active\":\"" << s.activeMode << "\",";
    writeBool("sketchActive", s.sketchActive); js << ",";
    writeBool("ortho", s.ortho); js << ",";
    writeBool("lighting", s.lighting); js << ",";
    writeBool("quadView", s.quadView); js << ",";
    writeBool("snapToGrid", s.snapToGrid); js << ",";
    js << "\"gridSpacing\":" << s.gridSpacing;
    js << "},";

    js << "\"selection\":{";
    writeUintList("featureIds", s.selectedFeatureIds); js << ",";
    js << "\"activeFeature\":" << s.activeSelectedFeature << ",";
    writeUintList("faces", s.selectedFaces); js << ",";
    writeUintList("edges", s.selectedEdges); js << ",";
    js << "\"vertex\":" << s.selectedVertex << ",";
    js << "\"subMode\":" << static_cast<int>(s.selSubMode);
    js << "},";

    js << "\"cursor\":{";
    writeVec3("world", s.cursorWorldX, s.cursorWorldY, s.cursorWorldZ);
    js << "},";

    js << "\"inventory\":{";
    js << "\"totalFeatures\":" << s.totalFeatures << ",";
    js << "\"visibleFeatures\":" << s.visibleFeatures << ",";
    js << "\"totalVertices\":" << s.totalVertices << ",";
    js << "\"totalTriangles\":" << s.totalTriangles << ",";
    writeUintList("hiddenFeatureIds", s.hiddenFeatureIds);
    js << "},";

    js << "\"timestamp\":" << s.timestamp;

    js << "}";

    return js.str();
}

std::vector<uint8_t> UIAgentBridge::captureViewportPixels(int width, int height) const
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    return pixels;
}

bool UIAgentBridge::saveScreenshot(const std::string& path, int width, int height) const
{
    auto pixels = captureViewportPixels(width, height);

    std::string json = exportSnapshotJson();

    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;

    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; --y)
        fwrite(&pixels[static_cast<size_t>(y) * width * 3], 1, static_cast<size_t>(width) * 3, f);

    fclose(f);

    std::string metaPath = path + ".json";
    FILE* fm = fopen(metaPath.c_str(), "w");
    if (fm) {
        fwrite(json.data(), 1, json.size(), fm);
        fclose(fm);
    }

    return true;
}

void UIAgentBridge::updateCamera(const nexus::render::Camera& camera)
{
    m_visualBridge.setCamera(camera);
}

} // namespace nexus::debug
