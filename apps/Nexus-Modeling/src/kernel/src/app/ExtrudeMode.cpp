#include <nexus/app/ExtrudeMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/CurveExtrude.h>

namespace nexus::app {

std::string ExtrudeMode::modeId() const { return "extrude"; }

std::string ExtrudeMode::displayName() const { return "Extrude"; }

std::vector<ActionDescriptor> ExtrudeMode::actions() const {
    return {
        {"extrude.create", "Extrude", "E", "modify", "Extrude"},
        {"extrude.cancel", "Cancel", "Esc", "modify", "Extrude"},
    };
}

EventResult ExtrudeMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (!ctx.document) return EventResult::Unconsumed;

    // Key handlers
    if (event.type == InputEventType::KeyDown && event.key == 27) {
        m_dragging = false;
        m_sketchId = parametric::kInvalidFeatureId;
        ctx.previewMesh.reset();
        return EventResult::Consumed;
    }

    // Find sketch if not already set
    auto& hist = ctx.document->history();
    if (m_sketchId == parametric::kInvalidFeatureId) {
        size_t fc = hist.featureCount();
        for (parametric::FeatureId i = static_cast<parametric::FeatureId>(fc); i >= 1; --i) {
            auto* n = hist.node(i);
            if (n && n->kind == parametric::FeatureKind::Sketch && !n->deleted) {
                m_sketchId = i;
                break;
            }
        }
    }

    bool fromSketch = (m_sketchId != parametric::kInvalidFeatureId);

    // Mouse drag to set extrude height
    if (event.type == InputEventType::MouseDown && event.button == 0) {
        m_dragging = true;
        m_startY = event.position.y;
        m_currentHeight = 0;
        return EventResult::Consumed;
    }

    if (event.type == InputEventType::MouseDrag && m_dragging) {
        float dy = event.position.y - m_startY;
        m_currentHeight = std::max(0.1f, -dy * 0.02f);
        return EventResult::Consumed;
    }

    if (event.type == InputEventType::MouseUp && m_dragging) {
        m_dragging = false;
        ctx.previewMesh.reset();

        if (fromSketch) {
            geometry::CurveExtrudeDesc desc;
            desc.direction = {0, 0, 1};
            desc.height = m_currentHeight;
            desc.capEnds = true;
            auto fid = ctx.document->addExtrude(m_sketchId, desc);
            if (fid != parametric::kInvalidFeatureId) {
                hist.rebuild();
            }
        }

        m_sketchId = parametric::kInvalidFeatureId;
        m_currentHeight = 0;
        return EventResult::Consumed;
    }

    return EventResult::Unconsumed;
}

bool ExtrudeMode::onDeactivate(AppContext&) {
    m_dragging = false;
    m_sketchId = parametric::kInvalidFeatureId;
    m_currentHeight = 0;
    return true;
}

void ExtrudeMode::renderOverlay(AppContext& ctx) {
    (void)ctx;
}

std::string ExtrudeMode::statusText() const {
    if (m_dragging) {
        return "Extrude: height " + std::to_string(m_currentHeight).substr(0, 4);
    }
    if (m_sketchId != parametric::kInvalidFeatureId) {
        return "Extrude: sketch ready — drag to set height, Esc to cancel";
    }
    return "Extrude: no sketch selected";
}

bool ExtrudeMode::executeAction(const std::string& actionId, AppContext& ctx) {
    if (actionId == "extrude.cancel") {
        m_dragging = false;
        m_sketchId = parametric::kInvalidFeatureId;
        ctx.previewMesh.reset();
        return true;
    }
    return false;
}

} // namespace nexus::app
