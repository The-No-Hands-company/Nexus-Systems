#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sim — Simulation / SceneGraph coupling contract
//
//  Defines a small deterministic bridge between simulation state snapshots and
//  render scene graph nodes.  This is the first Track C coupling API for
//  connecting solver output to visible scene state.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/render/SceneGraph.h>
#include <nexus/sim/SimulationCore.h>
#include <span>
#include <vector>

namespace nexus::sim {

/// Binds a simulation body to a scene graph node so solver state can update
/// the visible transform.
struct SimulationSceneBinding {
    BodyId                   simBodyId    = kInvalidBodyId;
    render::Node::NodeID     sceneNodeId  = render::Node::kInvalidNode;
};

/// Lightweight coupling helper for deterministic state application.
class SimulationSceneCoupling {
public:
    explicit SimulationSceneCoupling(render::SceneGraph& scene) noexcept;
    ~SimulationSceneCoupling();

    void setBindings(std::span<const SimulationSceneBinding> bindings) noexcept;

    /// Applies each bound body's position and orientation from the snapshot to
    /// its scene node's local transform. Unbound bodies and missing nodes are
    /// skipped. Scale is left untouched (the solver does not model it).
    void applyState(const SimState& state) noexcept;
    bool isBindingValid(const SimulationSceneBinding& binding) const noexcept;

    [[nodiscard]] bool hasBindings() const noexcept { return !m_bindings.empty(); }

private:
    render::SceneGraph* m_scene = nullptr;
    std::vector<SimulationSceneBinding> m_bindings;
};

} // namespace nexus::sim
