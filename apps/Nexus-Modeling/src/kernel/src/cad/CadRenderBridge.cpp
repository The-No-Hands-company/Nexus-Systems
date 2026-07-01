#include <nexus/cad/CadRenderBridge.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/render/SceneGraph.h>

namespace nexus::cad {

uint32_t CadRenderBridge::populateScene(
    const CadDocument& doc,
    nexus::render::SceneGraph& scene) noexcept
{
    uint32_t nodeCount = 0;
    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (!node || !node->mesh || node->deleted || node->hidden) continue;

        auto name = node->name.empty()
            ? "Feature_" + std::to_string(i)
            : node->name;

        auto* sceneNode = scene.createNode(name);

        if (sceneNode) {
            sceneNode->parametricBindingId = static_cast<parametric::ParametricEntityId>(i);
            sceneNode->markDirty();
            nodeCount++;
        }
    }
    return nodeCount;
}

void CadRenderBridge::updateScene(
    const CadDocument& doc,
    nexus::render::SceneGraph& scene) noexcept
{
    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (!node || !node->mesh || node->deleted || node->hidden) continue;

        // Update bounds on the scene node for frustum culling
        auto name = node->name.empty()
            ? "Feature_" + std::to_string(i)
            : node->name;

        auto* sn = scene.findNode(name);
        if (sn) {
            auto bounds = node->mesh->computeBounds();
            if (bounds.min != bounds.max) {
                sn->setLocalBounds(bounds);
            }
            sn->markDirty();
        }
    }
}

void CadRenderBridge::applySelection(
    const CadSelection& selection,
    nexus::render::SceneGraph& scene) noexcept
{
    if (selection.empty()) return;

    // Mark all known scene nodes for render update when selection changes
    for (const auto& item : selection.items()) {
        // Find the feature name from selection item
        auto name = "Feature_" + std::to_string(item.index);
        auto* sn = scene.findNode(name);
        if (sn) {
            sn->markDirty();
        }
    }
}

} // namespace nexus::cad
