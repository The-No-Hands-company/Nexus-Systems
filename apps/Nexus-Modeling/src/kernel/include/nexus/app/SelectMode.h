#pragma once
// Select mode — pick and selection operations

#include <nexus/app/AppMode.h>
#include <nexus/app/TransformGizmo.h>

namespace nexus::app {

class SelectMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;

    void onActivate(AppContext&) override;
    EventResult handleInput(AppContext&, const InputEvent&) override;
    void renderOverlay(AppContext&) override;
    bool executeAction(const std::string& actionId, AppContext&) override;
    [[nodiscard]] std::string statusText() const override;

private:
    enum class SelectionMode { Object, Face, Edge, Vertex } m_selMode = SelectionMode::Object;
    bool m_isDragging = false;
};

} // namespace nexus::app
