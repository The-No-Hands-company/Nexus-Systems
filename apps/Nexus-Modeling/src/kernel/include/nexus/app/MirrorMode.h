#pragma once
// Mirror mode — mirror selected features across X/Y/Z plane with instance option

#include <nexus/app/AppMode.h>

namespace nexus::app {

class MirrorMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    EventResult handleInput(AppContext& ctx, const InputEvent& event) override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;
    [[nodiscard]] std::string statusText() const override;

private:
    enum class Axis { X, Y, Z } m_axis = Axis::X;
    bool m_instance = false;
};

} // namespace nexus::app
