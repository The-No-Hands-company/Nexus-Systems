#pragma once
// Edge edit mode — bevel, slide, and loop-cut on selected edges

#include <nexus/app/AppMode.h>

namespace nexus::app {

class EdgeEditMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    [[nodiscard]] std::string statusText() const override;

private:
    bool m_slideMode = false;
    float m_slideAmount = 0.5f;
};

} // namespace nexus::app
