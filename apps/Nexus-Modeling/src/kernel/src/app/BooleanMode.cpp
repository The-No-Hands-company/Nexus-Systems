#include <nexus/app/BooleanMode.h>

#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/parametric/FeatureHistory.h>

namespace nexus::app {

std::string BooleanMode::modeId() const { return "boolean"; }

std::string BooleanMode::displayName() const { return "Boolean"; }

std::vector<ActionDescriptor> BooleanMode::actions() const {
    return {
        {"boolean.union", "Union", "U", "", "Boolean"},
        {"boolean.difference", "Difference", "D", "", "Boolean"},
        {"boolean.intersection", "Intersection", "I", "", "Boolean"},
        {"boolean.execute", "Execute", "Enter", "", "Boolean"},
        {"boolean.cancel", "Cancel", "Esc", "", "Boolean"},
    };
}

void BooleanMode::onActivate(AppContext& ctx) {
    m_state = State::PickBodyA;
    m_bodyA = 0;
    m_bodyB = 0;
    (void)ctx;
}

EventResult BooleanMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (event.type != InputEventType::MouseDown || event.button != 0) {
        return EventResult::Unconsumed;
    }

    if (!ctx.document) return EventResult::Unconsumed;

    switch (m_state) {
        case State::PickBodyA: {
            if (ctx.activeSelectedFeature > 0) {
                m_bodyA = ctx.activeSelectedFeature;
                m_state = State::PickBodyB;
            }
            break;
        }
        case State::PickBodyB: {
            if (ctx.activeSelectedFeature > 0 && ctx.activeSelectedFeature != m_bodyA) {
                m_bodyB = ctx.activeSelectedFeature;
                m_state = State::Confirm;
            }
            break;
        }
        case State::Confirm: {
            // Execute on third click
            auto& history = ctx.document->history();
            auto* nodeA = history.node(static_cast<nexus::parametric::FeatureId>(m_bodyA));
            auto* nodeB = history.node(static_cast<nexus::parametric::FeatureId>(m_bodyB));

            if (nodeA && nodeA->mesh && nodeB && nodeB->mesh) {
                // ANALYTIC first, when both operands actually are analytic solids.
                //
                // The two paths are not two implementations of one thing. The mesh boolean
                // cuts triangles against triangles and welds the result; the B-rep boolean
                // intersects the real surfaces — planes, cylinders, spheres — imprints the
                // seams onto both solids, classifies whole faces and sews them. What comes
                // out the second way is still a solid, with its faces still lying on the
                // surfaces they came from, so it can be fed to the NEXT operation exactly
                // as it is. That is what makes a chain of operations stay exact instead of
                // degrading a little at each step.
                //
                // It is attempted rather than assumed. booleanToBody holds itself to
                // watertight-or-empty and returns a clean empty body for configurations it
                // cannot sew — tangencies, most of all, where the true answer is not a
                // manifold solid at all. An empty result here is not an error to report; it
                // is the analytic path declining, and the mesh path below still gives the
                // user their operation.
                bool done = false;
                if (nodeA->body && nodeB->body) {
                    const auto op = m_op == Op::Union         ? geometry::brep::BooleanOp::Union
                                    : m_op == Op::Difference  ? geometry::brep::BooleanOp::Difference
                                                              : geometry::brep::BooleanOp::Intersection;
                    geometry::brep::Body sewn =
                        geometry::brep::booleanToBody(*nodeA->body, *nodeB->body, op);
                    if (sewn.faceCount() > 0 && sewn.isClosed() && sewn.checkIntegrity().ok) {
                        // `kBooleanDisplaySubdivisions` refines the curved faces for DISPLAY
                        // only; the body keeps the exact surfaces regardless of how finely
                        // it is drawn here.
                        constexpr uint32_t kBooleanDisplaySubdivisions = 2;
                        geometry::Mesh shown = sewn.toMesh(kBooleanDisplaySubdivisions);
                        if (shown.isValid()) {
                            (void)shown.computeVertexNormals();
                            nodeA->mesh.emplace(std::move(shown));
                            nodeA->body.emplace(std::move(sewn));
                            done = true;
                        }
                    }
                }

                if (!done) {
                    geometry::Mesh result;
                    auto report = geometry::BooleanOperation::compute(*nodeA->mesh, *nodeB->mesh,
                        m_op == Op::Union ? geometry::BooleanOperationType::Union :
                        m_op == Op::Difference ? geometry::BooleanOperationType::Difference :
                        geometry::BooleanOperationType::Intersection,
                        geometry::BooleanOperationOptions{}, result);
                    if (report.valid) {
                        nodeA->mesh.emplace(std::move(result));
                        // The analytic body described the solid BEFORE this operation, and
                        // the mesh path cannot update it. Leaving it attached would hand a
                        // later operation a body that no longer matches what is on screen,
                        // and it would silently model the wrong shape. Drop it: the feature
                        // continues as a mesh, which is exactly what it now is.
                        nodeA->body.reset();
                    }
                }
            }

            m_state = State::PickBodyA;
            m_bodyA = 0;
            m_bodyB = 0;
            break;
        }
    }

    return EventResult::Consumed;
}

void BooleanMode::renderToolVisuals(AppContext& ctx) {
    (void)ctx;
}

bool BooleanMode::executeAction(const std::string& actionId, AppContext& ctx) {
    if (actionId == "boolean.union")        { m_op = Op::Union; return true; }
    if (actionId == "boolean.difference")    { m_op = Op::Difference; return true; }
    if (actionId == "boolean.intersection")  { m_op = Op::Intersection; return true; }
    if (actionId == "boolean.cancel") {
        m_state = State::PickBodyA;
        m_bodyA = 0;
        m_bodyB = 0;
        return true;
    }
    (void)ctx;
    return false;
}

std::string BooleanMode::statusText() const {
    switch (m_state) {
        case State::PickBodyA: return "Boolean: click body A";
        case State::PickBodyB: return "Boolean: click body B";
        case State::Confirm: {
            const char* op = m_op == Op::Union ? "Union" : m_op == Op::Difference ? "Diff" : "Isect";
            return std::string("Boolean: ") + op + " — click to execute";
        }
    }
    return "Boolean";
}

} // namespace nexus::app
