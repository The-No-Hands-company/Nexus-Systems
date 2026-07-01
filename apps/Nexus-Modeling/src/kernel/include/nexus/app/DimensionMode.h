#pragma once
// Dimension mode — measure and annotate distances, angles, and radii

#include <nexus/app/AppMode.h>

namespace nexus::app {

class DimensionMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    void renderOverlay(AppContext& ctx) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    [[nodiscard]] std::string statusText() const override;

private:
    Vec3 m_p1{}, m_p2{};
    bool m_firstSet = false;
    float m_distance = 0;
};

} // namespace nexus::app
