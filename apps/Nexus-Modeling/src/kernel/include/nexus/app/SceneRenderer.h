#pragma once
// SceneRenderer — renders CAD features, grid, gizmo, previews, overlays

#include <nexus/cad/CadDocument.h>
#include <nexus/app/ViewportManager.h>
#include <nexus/app/SelectionManager.h>
#include <nexus/app/TransformGizmo.h>

#include <cstdint>

namespace nexus::app {

using FeatureId = nexus::parametric::FeatureId;

class SceneRenderer {
public:
    void setDocument(nexus::cad::CadDocument* doc) { m_doc = doc; }

    // Render all visible features, selection highlights, and gizmo
    void renderFeatures(const ViewportManager& vp, const SelectionManager& sel,
                        const TransformGizmo& gizmo) const;

    // Render ghost/preview mesh
    void renderPreview(const ViewportManager& vp) const;

private:
    nexus::cad::CadDocument* m_doc = nullptr;
    void renderSingleFeature(FeatureId id, bool selected, bool lighting,
                             const Vec3& selColor) const;
};

} // namespace nexus::app
