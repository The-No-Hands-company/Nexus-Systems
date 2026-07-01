#pragma once
// Extrude mode — drag to set height; supports sketch extrude and face extrude

#include <nexus/app/AppMode.h>
#include <nexus/parametric/FeatureHistory.h>

#include <string>

namespace nexus::app {

class ExtrudeMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    bool onDeactivate(AppContext& ctx) override;
    void renderOverlay(AppContext& ctx) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    [[nodiscard]] std::string statusText() const override;
    bool executeAction(const std::string& actionId, AppContext& ctx) override;

private:
    parametric::FeatureId m_sketchId = parametric::kInvalidFeatureId;
    bool m_dragging = false;
    float m_startY = 0;
    float m_currentHeight = 0;
};

} // namespace nexus::app
