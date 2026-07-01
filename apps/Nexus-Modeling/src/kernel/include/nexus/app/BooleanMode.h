#pragma once
// Boolean mode — union, difference, intersection operations

#include <nexus/app/AppMode.h>

namespace nexus::app {

class BooleanMode : public AppMode {
public:
    [[nodiscard]] std::string modeId() const override;
    [[nodiscard]] std::string displayName() const override;
    [[nodiscard]] std::vector<ActionDescriptor> actions() const override;

    void onActivate(AppContext&) override;
    EventResult handleInput(AppContext&, const InputEvent&) override;
    void renderToolVisuals(AppContext&) override;
    bool executeAction(const std::string& actionId, AppContext&) override;
    [[nodiscard]] std::string statusText() const override;

private:
    enum class Op { Union, Difference, Intersection } m_op = Op::Union;
    enum class State { PickBodyA, PickBodyB, Confirm } m_state = State::PickBodyA;
    uint32_t m_bodyA = 0;
    uint32_t m_bodyB = 0;
};

} // namespace nexus::app
