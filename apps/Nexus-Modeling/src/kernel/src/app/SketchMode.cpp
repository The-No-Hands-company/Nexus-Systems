#include <nexus/app/SketchMode.h>

#include <nexus/cad/CadDocument.h>
#include <nexus/parametric/ParametricSketchProfile.h>
#include <nexus/parametric/ParametricSamples.h>

namespace nexus::app {

std::string SketchMode::modeId() const { return "sketch"; }

std::string SketchMode::displayName() const { return "Sketch"; }

std::vector<ActionDescriptor> SketchMode::actions() const {
    return {
        {"sketch.line", "Line", "L", "draw", "Sketch"},
        {"sketch.rectangle", "Rectangle", "R", "draw", "Sketch"},
        {"sketch.circle", "Circle", "C", "draw", "Sketch"},
        {"sketch.arc", "Arc", "A", "draw", "Sketch"},
        {"sketch.polyline", "Polyline", "P", "draw", "Sketch"},
    };
}

EventResult SketchMode::handleInput(AppContext& ctx, const InputEvent& event) {
    if (event.type == InputEventType::KeyDown) {
        if (event.key == 27) { // Esc: cancel sketch
            m_points.clear();
            m_started = false;
            ctx.previewMesh.reset();
            return EventResult::Consumed;
        }
        if (event.key == 13 && m_points.size() >= 2) { // Enter: complete
            if (ctx.document) {
                nexus::parametric::SketchProfile profile;
                for (size_t i = 0; i < m_points.size(); ++i) {
                    (void)nexus::parametric::ParametricSketchFactory::addSketchPoint(
                        profile, static_cast<double>(m_points[i].x), static_cast<double>(m_points[i].y));
                }
                (void)ctx.document->addSketch(profile);
            }
            m_points.clear();
            m_started = false;
            return EventResult::Consumed;
        }
        return EventResult::Unconsumed;
    }

    if (event.type == InputEventType::MouseDown && event.button == 0) {
        m_points.push_back(ctx.cursorWorldPos);
        m_started = true;

        // Generate preview mesh for line segments
        if (m_points.size() >= 2 && ctx.document) {
            auto& preview = ctx.previewMesh.emplace();
            preview.attributes().setPositions(m_points);
            for (size_t i = 0; i + 1 < m_points.size(); ++i) {
                nexus::geometry::Face f;
                f.indices = {static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1), static_cast<uint32_t>(i + 1)};
                preview.topology().addFace(f);
            }
        }

        return EventResult::Consumed;
    }

    return EventResult::Unconsumed;
}

void SketchMode::renderOverlay(AppContext& ctx) {
    (void)ctx;
}

std::string SketchMode::statusText() const {
    if (m_started) {
        return "Sketch: " + std::to_string(m_points.size()) + " points — Enter to complete, Esc to cancel";
    }
    return "Sketch: click to add points";
}

} // namespace nexus::app
