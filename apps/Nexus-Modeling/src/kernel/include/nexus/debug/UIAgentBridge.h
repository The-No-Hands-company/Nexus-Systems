#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Debug — UI Agent Bridge
//
//  Bridges the AI agent infrastructure (AgentLoop, AgentVisualBridge) with the
//  live application UI.  Provides:
//    - UI state snapshots (mode, selection, camera, inventory)
//    - Programmatic viewport screenshot capture
//    - JSON serialization of state for AI consumption
//    - Integrated AgentLoop with live camera + mesh data
//
//  Call updateSnapshot() each frame after rendering to keep agent state
//  synchronized with the application.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/agent/AgentLoop.h>
#include <nexus/agent/AgentVisualBridge.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::debug {

// ── UI State Snapshot ─────────────────────────────────────────────────────

struct UISnapshot {
    int viewportWidth  = 0;
    int viewportHeight = 0;

    float cameraPosX = 0, cameraPosY = 0, cameraPosZ = 0;
    float cameraTargetX = 0, cameraTargetY = 0, cameraTargetZ = 0;
    float cameraUpX = 0, cameraUpY = 0, cameraUpZ = 0;
    float cameraDistance = 0;

    std::string activeMode;
    bool sketchActive = false;
    bool ortho        = false;
    bool lighting     = false;
    bool quadView     = false;
    bool snapToGrid   = true;
    float gridSpacing = 1.0f;

    std::vector<uint32_t> selectedFeatureIds;
    uint32_t activeSelectedFeature = 0;
    std::vector<uint32_t> selectedFaces;
    std::vector<uint32_t> selectedEdges;
    uint32_t selectedVertex = ~0u;
    uint8_t  selSubMode    = 0;

    float cursorWorldX = 0, cursorWorldY = 0, cursorWorldZ = 0;

    size_t totalFeatures   = 0;
    size_t visibleFeatures = 0;
    size_t totalVertices   = 0;
    size_t totalTriangles  = 0;

    std::vector<uint32_t> hiddenFeatureIds;

    double timestamp = 0;
};

// ── UI Agent Bridge ───────────────────────────────────────────────────────

class UIAgentBridge {
public:
    UIAgentBridge();
    ~UIAgentBridge();

    UIAgentBridge(const UIAgentBridge&) = delete;
    UIAgentBridge& operator=(const UIAgentBridge&) = delete;

    void enable(bool on) noexcept { m_enabled = on; }
    [[nodiscard]] bool enabled() const noexcept { return m_enabled; }

    bool captureSnapshot(const UISnapshot& snapshot);
    [[nodiscard]] const UISnapshot& latestSnapshot() const noexcept { return m_latest; }

    [[nodiscard]] std::string exportSnapshotJson() const;

    [[nodiscard]] std::vector<uint8_t> captureViewportPixels(int width, int height) const;
    bool saveScreenshot(const std::string& path, int width, int height) const;

    [[nodiscard]] nexus::agent::AgentLoop& agentLoop() noexcept { return m_agentLoop; }
    [[nodiscard]] const nexus::agent::AgentLoop& agentLoop() const noexcept { return m_agentLoop; }
    [[nodiscard]] nexus::agent::AgentVisualBridge& visualBridge() noexcept { return m_visualBridge; }

    void updateCamera(const nexus::render::Camera& camera);

private:
    nexus::agent::AgentLoop       m_agentLoop;
    nexus::agent::AgentVisualBridge m_visualBridge;
    UISnapshot                     m_latest;
    bool                           m_enabled = false;
};

} // namespace nexus::debug
