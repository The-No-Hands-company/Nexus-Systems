#include <nexus/app/EdgeEditMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>

namespace nexus::app {

std::string EdgeEditMode::modeId() const { return "edge-edit"; }

std::string EdgeEditMode::displayName() const { return "Edge Edit"; }

std::vector<ActionDescriptor> EdgeEditMode::actions() const {
    return {
        {"edge.bevel", "Bevel", "B", "modify", "Edge"},
        {"edge.slide", "Slide", "G", "modify", "Edge"},
        {"edge.loop_cut", "Loop Cut", "Ctrl+R", "modify", "Edge"},
        {"edge.collapse", "Collapse", "X", "modify", "Edge"},
    };
}

EventResult EdgeEditMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (!ctx.document) return EventResult::Unconsumed;

    auto target = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(target);
    if (!node || !node->mesh || node->deleted) return EventResult::Unconsumed;

    if (event.type == InputEventType::KeyDown) {
        if (event.key == 'B') {
            // Bevel using HEM for proper vertex mitering
            auto hemOpt = geometry::HalfEdgeMesh::fromMesh(*node->mesh);
            if (hemOpt) {
                geometry::Mesh saved = *node->mesh;

                // Collect all edges for beveling
                std::vector<uint32_t> allEdges;
                for (uint32_t ei = 0; ei < hemOpt->edgeCount(); ++ei) {
                    if (hemOpt->edge(ei).face != geometry::HalfEdgeMesh::kInvalid &&
                        hemOpt->edge(ei).twin != geometry::HalfEdgeMesh::kInvalid) {
                        allEdges.push_back(ei);
                    }
                }

                if (!allEdges.empty() && hemOpt->bevelEdgesHEM(allEdges, 0.1f)) {
                    node->mesh.emplace(hemOpt->toMesh());
                    auto cmd = std::make_unique<nexus::cad::TransformCommand>(target, std::move(saved));
                    ctx.document->executeCommand(std::move(cmd));
                }
            }
            return EventResult::Consumed;
        }

        if (event.key == 'G') {
            // Toggle edge slide mode
            m_slideMode = !m_slideMode;
            return EventResult::Consumed;
        }
    }

    if (event.type == InputEventType::MouseDown && event.button == 0) {
        if (m_slideMode && ctx.selectedEdge != ~0u) {
            // Edge slide: move selected edge along its incident faces
            auto hemOpt = geometry::HalfEdgeMesh::fromMesh(*node->mesh);
            if (hemOpt) {
                uint32_t he = hemOpt->findEdge(ctx.selectedEdge, 0);
                for (uint32_t ei = 0; ei < hemOpt->edgeCount(); ++ei) {
                    if (hemOpt->edge(ei).src == ctx.selectedEdge) {
                        he = ei;
                        break;
                    }
                }
                if (he != geometry::HalfEdgeMesh::kInvalid) {
                    geometry::Mesh saved = *node->mesh;
                    hemOpt->slideEdgeLoop(he, 0.3f);
                    node->mesh.emplace(hemOpt->toMesh());
                    auto cmd = std::make_unique<nexus::cad::TransformCommand>(target, std::move(saved));
                    ctx.document->executeCommand(std::move(cmd));
                }
            }
            m_slideMode = false;
            return EventResult::Consumed;
        }

        if (event.modifiers & 2 && ctx.selectedEdge != ~0u) {
            // Ctrl+Click: insert edge loop
            auto hemOpt = geometry::HalfEdgeMesh::fromMesh(*node->mesh);
            if (hemOpt) {
                for (uint32_t ei = 0; ei < hemOpt->edgeCount(); ++ei) {
                    if (hemOpt->edge(ei).src == ctx.selectedEdge) {
                        geometry::Mesh saved = *node->mesh;
                        hemOpt->insertEdgeLoop(ei, 0.5f);
                        node->mesh.emplace(hemOpt->toMesh());
                        auto cmd = std::make_unique<nexus::cad::TransformCommand>(target, std::move(saved));
                        ctx.document->executeCommand(std::move(cmd));
                        break;
                    }
                }
            }
            return EventResult::Consumed;
        }
    }

    return EventResult::Unconsumed;
}

std::string EdgeEditMode::statusText() const {
    if (m_slideMode) return "Edge Slide: click destination";
    return "Edge: B=Bevel G=Slide Ctrl+Click=LoopCut";
}

} // namespace nexus::app
