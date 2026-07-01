#pragma once
// Vertex edit mode — move vertices with soft selection and transform constraints

#include <nexus/app/AppMode.h>

namespace nexus::app {

class VertexEditMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    [[nodiscard]] std::string statusText() const override;

private:
    float m_falloffRadius = 1.0f;
    bool m_softSelect = false;
};

} // namespace nexus::app
