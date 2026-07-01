#include <nexus/app/FilletMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/FaceFillet.h>
#include <nexus/geometry/Mesh.h>

namespace nexus::app {

std::string FilletMode::modeId() const { return "fillet"; }
std::string FilletMode::displayName() const { return "Fillet"; }

std::vector<ActionDescriptor> FilletMode::actions() const {
    return {
        {"fillet.constant", "Constant Radius", "F", "modify", "Fillet"},
        {"fillet.variable", "Variable Radius", "Shift+F", "modify", "Fillet"},
        {"fillet.radius_up", "Increase Radius", "]", "modify", "Fillet"},
        {"fillet.radius_down", "Decrease Radius", "[", "modify", "Fillet"},
    };
}

EventResult FilletMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (event.type == InputEventType::KeyDown) {
        if (event.key == ']') { m_radius = std::min(m_radius * 1.2f, 10.0f); return EventResult::Consumed; }
        if (event.key == '[') { m_radius = std::max(m_radius * 0.8f, 0.01f); return EventResult::Consumed; }
        if (event.key == 'F' && (event.modifiers & 1)) {
            m_variableRadius = !m_variableRadius;
            return EventResult::Consumed;
        }
        return EventResult::Unconsumed;
    }

    if (event.type != InputEventType::MouseDown || event.button != 0)
        return EventResult::Unconsumed;
    if (!ctx.document) return EventResult::Unconsumed;

    auto fid = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(fid);
    if (!node || !node->mesh || node->deleted) return EventResult::Unconsumed;

    geometry::FaceFilletOptions opts;
    opts.radius = m_radius;
    opts.constantRadius = !m_variableRadius;
    opts.segments = 8;

    // Iterate adjacent face pairs for filleting
    auto hem = geometry::HalfEdgeMesh::fromMesh(*node->mesh);
    if (hem) {
        for (uint32_t fi = 0; fi < hem->faceCount(); ++fi) {
            uint32_t start = hem->face(fi).edge;
            if (start == geometry::HalfEdgeMesh::kInvalid) continue;
            uint32_t e = start;
            uint32_t safety = 0;
            do {
                uint32_t t = hem->edge(e).twin;
                if (t != geometry::HalfEdgeMesh::kInvalid && t < hem->edgeCount()) {
                    uint32_t adjFace = hem->edge(t).face;
                    if (adjFace != geometry::HalfEdgeMesh::kInvalid && adjFace != fi) {
                        geometry::Mesh result = faceFillet(*node->mesh, fi, adjFace, opts);
                        if (result.isValid()) {
                            node->mesh.emplace(std::move(result));
                            return EventResult::Consumed;
                        }
                        break;
                    }
                }
                uint32_t nxt = hem->edge(e).next;
                if (nxt == geometry::HalfEdgeMesh::kInvalid || nxt >= hem->edgeCount()) break;
                e = nxt;
                if (++safety > 256) break;
            } while (e != start);
            break;
        }
    }

    return EventResult::Unconsumed;
}

std::string FilletMode::statusText() const {
    return std::string("Fillet: r=") + std::to_string(m_radius).substr(0, 4) +
           (m_variableRadius ? " variable" : " constant");
}

} // namespace nexus::app
