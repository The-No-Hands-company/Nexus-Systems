#include <nexus/app/DimensionMode.h>
#include <cmath>

namespace nexus::app {

std::string DimensionMode::modeId() const { return "dimension"; }
std::string DimensionMode::displayName() const { return "Dimension"; }

std::vector<ActionDescriptor> DimensionMode::actions() const {
    return {
        {"dimension.linear", "Linear", "L", "measure", "Dimension"},
        {"dimension.clear", "Clear", "Esc", "measure", "Dimension"},
    };
}

EventResult DimensionMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (event.type == InputEventType::KeyDown && event.key == 27) {
        m_firstSet = false;
        m_distance = 0;
        return EventResult::Consumed;
    }

    if (event.type != InputEventType::MouseDown || event.button != 0)
        return EventResult::Unconsumed;

    if (!m_firstSet) {
        m_p1 = ctx.cursorWorldPos;
        m_firstSet = true;
        return EventResult::Consumed;
    }

    m_p2 = ctx.cursorWorldPos;
    float dx = m_p2.x - m_p1.x;
    float dy = m_p2.y - m_p1.y;
    float dz = m_p2.z - m_p1.z;
    m_distance = std::sqrt(dx*dx + dy*dy + dz*dz);
    m_firstSet = false;

    ctx.dimActive = true;
    ctx.dimP1 = m_p1;
    ctx.dimP2 = m_p2;

    return EventResult::Consumed;
}

void DimensionMode::renderOverlay(AppContext& ctx) {
    (void)ctx;
}

std::string DimensionMode::statusText() const {
    if (m_distance > 0) {
        return "Dimension: " + std::to_string(m_distance).substr(0, 5);
    }
    return m_firstSet ? "Dimension: click second point" : "Dimension: click first point";
}

} // namespace nexus::app
