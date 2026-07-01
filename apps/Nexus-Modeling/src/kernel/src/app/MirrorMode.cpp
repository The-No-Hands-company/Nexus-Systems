#include <nexus/app/MirrorMode.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>

namespace nexus::app {

std::string MirrorMode::modeId() const { return "mirror"; }
std::string MirrorMode::displayName() const { return "Mirror"; }

std::vector<ActionDescriptor> MirrorMode::actions() const {
    return {
        {"mirror.x", "Mirror X", "X", "modify", "Mirror"},
        {"mirror.y", "Mirror Y", "Y", "modify", "Mirror"},
        {"mirror.z", "Mirror Z", "Z", "modify", "Mirror"},
        {"mirror.instance", "Toggle Instance", "I", "modify", "Mirror"},
    };
}

EventResult MirrorMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (event.type == InputEventType::KeyDown) {
        if (event.key == 'X') { m_axis = Axis::X; return EventResult::Consumed; }
        if (event.key == 'Y') { m_axis = Axis::Y; return EventResult::Consumed; }
        if (event.key == 'Z') { m_axis = Axis::Z; return EventResult::Consumed; }
        if (event.key == 'I') { m_instance = !m_instance; return EventResult::Consumed; }
        return EventResult::Unconsumed;
    }

    if (event.type != InputEventType::MouseDown || event.button != 0)
        return EventResult::Unconsumed;
    if (!ctx.document) return EventResult::Unconsumed;

    auto fid = static_cast<parametric::FeatureId>(ctx.activeSelectedFeature);
    auto* node = ctx.document->history().node(fid);
    if (!node || !node->mesh || node->deleted) return EventResult::Unconsumed;

    geometry::Mesh saved = *node->mesh;
    auto positions = node->mesh->attributes().positions();

    for (auto& p : positions) {
        switch (m_axis) {
            case Axis::X: p.x = -p.x; break;
            case Axis::Y: p.y = -p.y; break;
            case Axis::Z: p.z = -p.z; break;
        }
    }

    node->mesh->attributes().setPositions(std::move(positions));

    (void)node->mesh->computeVertexNormals();

    if (!m_instance) {
        auto cmd = std::make_unique<nexus::cad::TransformCommand>(fid, std::move(saved));
        ctx.document->executeCommand(std::move(cmd));
    }

    return EventResult::Consumed;
}

std::string MirrorMode::statusText() const {
    const char* axis = m_axis == Axis::X ? "X" : m_axis == Axis::Y ? "Y" : "Z";
    return std::string("Mirror: ") + axis + (m_instance ? " (instance)" : "");
}

} // namespace nexus::app
