#include <nexus/parametric/ParametricSceneBridge.h>
#include <algorithm>
#include <vector>

namespace nexus::parametric {

void applySolutionToNodePositions(const ConstraintGraph& graph,
                                  nexus::render::SceneGraph& scene) noexcept
{
    std::vector<ParametricEntityId> updatedNodes;
    std::vector<ParametricEntityId> failedNodes;

    scene.traverse([&](nexus::render::Node& node, const nexus::render::Mat4&) {
        if (node.parametricBindingId == 0) return;

        const ParametricPoint3* pt = graph.point(node.parametricBindingId);
        if (!pt) {
            failedNodes.push_back(node.parametricBindingId);
            return;
        }

        nexus::render::Transform& tf = node.localTransform();
        tf.translation.x = static_cast<float>(pt->x);
        tf.translation.y = static_cast<float>(pt->y);
        tf.translation.z = static_cast<float>(pt->z);
        node.markDirty();
        updatedNodes.push_back(node.parametricBindingId);
    });
}

bool applySolutionToNodeTransform(const ConstraintGraph& graph,
                                    nexus::render::SceneGraph& scene,
                                    ParametricEntityId entityId) noexcept
{
    bool updated = false;
    scene.traverse([&](nexus::render::Node& node, const nexus::render::Mat4&) {
        if (node.parametricBindingId != entityId) return;

        const ParametricPoint3* pt = graph.point(entityId);
        if (!pt) return;

        nexus::render::Transform& tf = node.localTransform();
        tf.translation.x = static_cast<float>(pt->x);
        tf.translation.y = static_cast<float>(pt->y);
        tf.translation.z = static_cast<float>(pt->z);
        node.markDirty();
        updated = true;
    });
    return updated;
}

void applySolutionToNodeScale(const ConstraintGraph& graph,
                                nexus::render::SceneGraph& scene) noexcept
{
    scene.traverse([&](nexus::render::Node& node, const nexus::render::Mat4&) {
        if (node.parametricBindingId == 0) return;

        const ParametricPoint3* pt = graph.point(node.parametricBindingId);
        if (!pt) return;

        nexus::render::Transform& tf = node.localTransform();
        float scaleX = static_cast<float>(pt->x);
        float scaleY = static_cast<float>(pt->y);
        float scaleZ = static_cast<float>(pt->z);

        if (scaleX > 1e-10f && scaleY > 1e-10f && scaleZ > 1e-10f) {
            tf.scale.x = scaleX;
            tf.scale.y = scaleY;
            tf.scale.z = scaleZ;
            node.markDirty();
        }
    });
}

} // namespace nexus::parametric
