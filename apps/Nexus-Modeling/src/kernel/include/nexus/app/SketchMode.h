#pragma once
// Sketch mode — 2D sketch creation (line, rectangle, circle, arc, polyline)

#include <nexus/app/AppMode.h>

#include <vector>

namespace nexus::app {

class SketchMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    void renderOverlay(AppContext& ctx) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    [[nodiscard]] std::string statusText() const override;

private:
    std::vector<Vec3> m_points;
    bool m_started = false;
};

} // namespace nexus::app
