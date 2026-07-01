#include <nexus/app/SceneRenderer.h>
#include <nexus/app/SelectionHighlight.h>
#include <nexus/app/TransformGizmo.h>

#ifndef NEXUS_HEADLESS
#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#endif

namespace nexus::app {

using Vec3 = nexus::render::Vec3;

void SceneRenderer::renderSingleFeature(FeatureId id, bool selected,
    bool lighting, const Vec3& selColor) const {
    if (!m_doc) return;
    auto* node = m_doc->history().node(id);
    if (!node || !node->mesh || node->deleted || node->hidden) return;

    const auto& pos = node->mesh->attributes().positions();
    const auto& topo = node->mesh->topology();

    std::vector<float> varray, narray;
    for (uint32_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& face = topo.face(fi);
        for (size_t j = 0; j + 2 < face.vertexCount(); ++j) {
            auto& v0 = pos[face.indices[0]], &v1 = pos[face.indices[j+1]], &v2 = pos[face.indices[j+2]];
            varray.insert(varray.end(), {v0.x,v0.y,v0.z, v1.x,v1.y,v1.z, v2.x,v2.y,v2.z});
            if (lighting) {
                Vec3 n = (v1 - v0).cross(v2 - v0).normalize();
                for (int k = 0; k < 3; ++k) narray.insert(narray.end(), {n.x, n.y, n.z});
            }
        }
    }

    if (varray.empty()) return;

    if (selected)
        glColor3fv(&selColor.x);
    else
        glColor3f(0.75f, 0.75f, 0.78f);

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, varray.data());
    if (lighting && !narray.empty()) {
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, 0, narray.data());
    }
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)varray.size() / 3);
    glDisableClientState(GL_VERTEX_ARRAY);
    if (lighting) glDisableClientState(GL_NORMAL_ARRAY);

    // Wireframe overlay
    glColor3f(0.1f, 0.1f, 0.15f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, varray.data());
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)varray.size() / 3);
    glDisableClientState(GL_VERTEX_ARRAY);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

void SceneRenderer::renderFeatures(const ViewportManager& vp,
    const SelectionManager& sel, const TransformGizmo& gizmo) const {
    if (!m_doc) return;

    auto& hist = m_doc->history();
    for (FeatureId i = 1; i <= static_cast<FeatureId>(hist.featureCount()); ++i) {
        bool selected = (sel.primary() == i);
        renderSingleFeature(i, selected, vp.state().lighting,
            {0.4f, 0.55f, 0.7f});

        // Selection highlight + gizmo for selected feature
        if (selected) {
            SelectionHighlight hl;
            hl.render(*m_doc, sel.primary());
            auto* node = m_doc->history().node(sel.primary());
            if (node && node->mesh) {
                gizmo.render(node->mesh->computeBounds().center());
            }
        }
    }
}

void SceneRenderer::renderPreview(const ViewportManager& vp) const {
    // Preview mesh is stored in AppContext::previewMesh — handled in main loop for now.
    (void)vp;
}

} // namespace nexus::app
