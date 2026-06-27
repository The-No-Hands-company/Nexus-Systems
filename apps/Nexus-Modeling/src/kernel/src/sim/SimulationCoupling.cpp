#include <nexus/sim/SimulationCoupling.h>
#include <algorithm>
#include <cmath>

namespace nexus::sim {

SimulationSceneCoupling::SimulationSceneCoupling(render::SceneGraph& scene) noexcept
    : m_scene(&scene)
{
}

SimulationSceneCoupling::~SimulationSceneCoupling() = default;

void SimulationSceneCoupling::setBindings(std::span<const SimulationSceneBinding> bindings) noexcept
{
    m_bindings.assign(bindings.begin(), bindings.end());
}

void SimulationSceneCoupling::applyState(const SimState& state) noexcept
{
    if (!m_scene) return;

    for (const auto& body : state.bodies) {
        const auto it = std::find_if(m_bindings.begin(), m_bindings.end(),
            [&](const SimulationSceneBinding& binding) {
                return binding.simBodyId == body.id;
            });

        if (it == m_bindings.end() || it->sceneNodeId == render::Node::kInvalidNode) continue;

        if (render::Node* node = m_scene->findNode(it->sceneNodeId)) {
            render::Transform& xf = node->localTransform();
            xf.translation.x = body.position.x;
            xf.translation.y = body.position.y;
            xf.translation.z = body.position.z;
            xf.rotation.x    = body.orientation.x;
            xf.rotation.y    = body.orientation.y;
            xf.rotation.z    = body.orientation.z;
            xf.rotation.w    = body.orientation.w;
            node->markDirty();
        }
    }
}

bool SimulationSceneCoupling::isBindingValid(const SimulationSceneBinding& binding) const noexcept
{
    if (!m_scene) return false;
    if (binding.sceneNodeId == render::Node::kInvalidNode) return false;
    return m_scene->findNode(binding.sceneNodeId) != nullptr;
}

} // namespace nexus::sim
