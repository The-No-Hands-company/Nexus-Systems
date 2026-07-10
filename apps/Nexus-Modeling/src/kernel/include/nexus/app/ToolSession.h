#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <nexus/geometry/Mesh.h>

namespace nexus::app {

enum class ToolState {
    Idle,            // No active tool
    AwaitingTarget,   // Tool selected, waiting for user to click an object/face
    Previewing,      // Target selected, user is dragging gizmo for real-time update
    Committing       // Finalizing the operation
};

struct ToolSession {
    std::string toolId;
    ToolState state = ToolState::Idle;
    
    // Store the original mesh before modification for the preview loop
    std::optional<nexus::geometry::Mesh> originalMesh;
    
    // Current preview mesh being rendered in the viewport
    std::optional<nexus::geometry::Mesh> previewMesh;
    
    // Parameters for the operation (e.g., distance, radius, taper)
    struct Params {
        float value = 0.0f;
        float min = -100.0f;
        float max = 100.0f;
        float step = 0.1f;
        std::string label = "Value";
    } param;

    uint32_t targetFeatureId = 0;
    std::vector<uint32_t> targetFaces;

    void reset() {
        toolId = "";
        state = ToolState::Idle;
        originalMesh.reset();
        previewMesh.reset();
        targetFeatureId = 0;
        targetFaces.clear();
    }
};

} // namespace nexus::app
