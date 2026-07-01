#include <nexus/app/SelectMode.h>

#include <nexus/cad/CadDocument.h>
#include <nexus/parametric/FeatureHistory.h>

namespace nexus::app {

std::string SelectMode::modeId() const { return "select"; }

std::string SelectMode::displayName() const { return "Select"; }

std::vector<ActionDescriptor> SelectMode::actions() const {
    return {
        {"select.object", "Object", "Tab", "", "Select"},
        {"select.face", "Face", "1", "", "Select"},
        {"select.edge", "Edge", "2", "", "Select"},
        {"select.vertex", "Vertex", "3", "", "Select"},
        {"select.all", "Select All", "A", "", "Select"},
        {"select.none", "Deselect All", "Alt+A", "", "Select"},
    };
}

void SelectMode::onActivate(AppContext& ctx) {
    m_selMode = SelectionMode::Object;
    (void)ctx;
}

EventResult SelectMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (event.type != InputEventType::MouseDown || event.button != 0) {
        return EventResult::Unconsumed;
    }

    if (!ctx.document || !ctx.selection) return EventResult::Unconsumed;

    switch (m_selMode) {
        case SelectionMode::Object: {
            auto count = ctx.document->history().featureCount();
            if (count > 0 && ctx.activeSelectedFeature > 0 && ctx.activeSelectedFeature <= count) {
                if (event.modifiers & 2) { // Shift: toggle
                    ctx.activeSelectedFeature = 0;
                }
            }
            break;
        }
        case SelectionMode::Face: {
            ctx.selection->clear();
            if (ctx.selectedFace != ~0u) {
                ctx.selection->addFace(ctx.selectedFace);
            }
            break;
        }
        case SelectionMode::Edge: {
            ctx.selection->clear();
            if (ctx.selectedEdge != ~0u) {
                ctx.selection->addEdge(ctx.selectedEdge);
            }
            break;
        }
        case SelectionMode::Vertex: {
            ctx.selection->clear();
            if (ctx.selectedVertex != ~0u) {
                ctx.selection->addVertex(ctx.selectedVertex);
            }
            break;
        }
    }

    return EventResult::Consumed;
}

void SelectMode::renderOverlay(AppContext& ctx) {
    (void)ctx;
}

bool SelectMode::executeAction(const std::string& actionId, AppContext& ctx) {
    if (actionId == "select.object") { m_selMode = SelectionMode::Object; return true; }
    if (actionId == "select.face")   { m_selMode = SelectionMode::Face;   return true; }
    if (actionId == "select.edge")   { m_selMode = SelectionMode::Edge;   return true; }
    if (actionId == "select.vertex") { m_selMode = SelectionMode::Vertex; return true; }

    if (actionId == "select.all") {
        if (ctx.document) {
            auto count = ctx.document->history().featureCount();
            ctx.activeSelectedFeature = count > 0 ? 1 : 0;
        }
        return true;
    }

    if (actionId == "select.none") {
        if (ctx.selection) ctx.selection->clear();
        ctx.activeSelectedFeature = 0;
        ctx.selectedFace = ~0u;
        ctx.selectedEdge = ~0u;
        ctx.selectedVertex = ~0u;
        return true;
    }

    return false;
}

std::string SelectMode::statusText() const {
    switch (m_selMode) {
        case SelectionMode::Object: return "Object Select";
        case SelectionMode::Face:   return "Face Select";
        case SelectionMode::Edge:   return "Edge Select";
        case SelectionMode::Vertex: return "Vertex Select";
    }
    return "";
}

} // namespace nexus::app
