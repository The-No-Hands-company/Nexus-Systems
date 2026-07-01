#include <nexus/app/BooleanMode.h>

#include <nexus/cad/CadDocument.h>
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
                geometry::Mesh result;
                auto report = geometry::BooleanOperation::compute(*nodeA->mesh, *nodeB->mesh,
                    m_op == Op::Union ? geometry::BooleanOperationType::Union :
                    m_op == Op::Difference ? geometry::BooleanOperationType::Difference :
                    geometry::BooleanOperationType::Intersection,
                    geometry::BooleanOperationOptions{}, result);
                if (report.valid) {
                    nodeA->mesh.emplace(std::move(result));
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
