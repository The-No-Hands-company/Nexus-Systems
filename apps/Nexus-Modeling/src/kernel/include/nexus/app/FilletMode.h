#pragma once
// Fillet mode — click to fillet between selected faces with variable radius

#include <nexus/app/AppMode.h>

namespace nexus::app {

class FilletMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    [[nodiscard]] std::string statusText() const override;

private:
    float m_radius = 0.1f;
    bool m_variableRadius = false;
};

} // namespace nexus::app
