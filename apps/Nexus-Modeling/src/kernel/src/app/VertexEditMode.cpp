#include <nexus/app/VertexEditMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <cmath>

namespace nexus::app {

std::string VertexEditMode::modeId() const { return "vertex-edit"; }
std::string VertexEditMode::displayName() const { return "Vertex Edit"; }

std::vector<ActionDescriptor> VertexEditMode::actions() const {
    return {
        {"vertex.move", "Move", "G", "modify", "Vertex"},
        {"vertex.soft_select", "Soft Select", "O", "modify", "Vertex"},
        {"vertex.falloff_up", "Increase Falloff", "]", "modify", "Vertex"},
        {"vertex.falloff_down", "Decrease Falloff", "[", "modify", "Vertex"},
    };
}

EventResult VertexEditMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (event.type == InputEventType::KeyDown) {
        if (event.key == 'O') { m_softSelect = !m_softSelect; return EventResult::Consumed; }
        if (event.key == ']') { m_falloffRadius = std::min(m_falloffRadius * 1.2f, 100.0f); return EventResult::Consumed; }
        if (event.key == '[') { m_falloffRadius = std::max(m_falloffRadius * 0.8f, 0.1f); return EventResult::Consumed; }
        return EventResult::Unconsumed;
    }

    if (event.type != InputEventType::MouseDown || event.button != 0)
        return EventResult::Unconsumed;
    if (!ctx.document || ctx.selectedVertex == ~0u) return EventResult::Unconsumed;

    auto target = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(target);
    if (!node || !node->mesh || node->deleted) return EventResult::Unconsumed;

    float r2 = m_falloffRadius * m_falloffRadius;

    auto hem = geometry::HalfEdgeMesh::fromMesh(*node->mesh);
    if (!hem) return EventResult::Unconsumed;

    auto positions = node->mesh->attributes().positions();
    auto srcPositions = positions;

    auto& targetPos = positions[ctx.selectedVertex];
    targetPos = ctx.cursorWorldPos;

    if (m_softSelect) {
        for (uint32_t v = 0; v < hem->vertexCount(); ++v) {
            if (v == ctx.selectedVertex) continue;
            float dx = srcPositions[v].x - srcPositions[ctx.selectedVertex].x;
            float dy = srcPositions[v].y - srcPositions[ctx.selectedVertex].y;
            float dz = srcPositions[v].z - srcPositions[ctx.selectedVertex].z;
            float d2 = dx*dx + dy*dy + dz*dz;
            if (d2 < r2) {
                float weight = 1.0f - std::sqrt(d2) / m_falloffRadius;
                weight = std::max(weight, 0.0f);
                float dxMove = positions[ctx.selectedVertex].x - srcPositions[ctx.selectedVertex].x;
                float dyMove = positions[ctx.selectedVertex].y - srcPositions[ctx.selectedVertex].y;
                float dzMove = positions[ctx.selectedVertex].z - srcPositions[ctx.selectedVertex].z;
                positions[v].x += dxMove * weight;
                positions[v].y += dyMove * weight;
                positions[v].z += dzMove * weight;
            }
        }
    }

    node->mesh->attributes().setPositions(std::move(positions));

    return EventResult::Consumed;
}

std::string VertexEditMode::statusText() const {
    return std::string("Vertex:") + (m_softSelect ? " soft r=" : " hard") +
           std::to_string(m_falloffRadius).substr(0, 4);
}

} // namespace nexus::app
