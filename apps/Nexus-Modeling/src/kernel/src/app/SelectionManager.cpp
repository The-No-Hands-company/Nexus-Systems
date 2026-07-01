#include <nexus/app/SelectionManager.h>
#include <nexus/app/ViewportManager.h>

#include <algorithm>

namespace nexus::app {

FeatureId SelectionManager::pick(const Vec3& ro, const Vec3& rd,
    uint32_t& outFace, uint32_t& outVertex, Vec3& outHit) const {
    if (!m_doc) return nexus::parametric::kInvalidFeatureId;

    FeatureId best = nexus::parametric::kInvalidFeatureId;
    outFace = ~0u; outVertex = ~0u;
    float bestT = 1e30f;

    size_t fc = m_doc->history().featureCount();
    for (FeatureId i = 1; i <= static_cast<FeatureId>(fc); ++i) {
        auto* node = m_doc->history().node(i);
        if (!node || !node->mesh || node->deleted || node->hidden) continue;
        const auto& pos = node->mesh->attributes().positions();
        const auto& topo = node->mesh->topology();
        for (uint32_t fi = 0; fi < topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            if (face.vertexCount() < 3) continue;
            for (size_t j = 0; j + 2 < face.vertexCount(); ++j) {
                uint32_t i0 = face.indices[0], i1 = face.indices[j + 1], i2 = face.indices[j + 2];
                if (i0 >= pos.size() || i1 >= pos.size() || i2 >= pos.size()) continue;
                float t;
                if (ViewportManager::rayTriangleIntersect(ro, rd, pos[i0], pos[i1], pos[i2], t) && t < bestT) {
                    best = i; bestT = t; outFace = fi;
                    outHit = ro + rd * t;
                    outVertex = face.indices[0];
                    float bestVDist = (pos[outVertex] - outHit).lengthSq();
                    for (size_t k = 1; k < face.vertexCount(); ++k) {
                        float d2 = (pos[face.indices[k]] - outHit).lengthSq();
                        if (d2 < bestVDist) { bestVDist = d2; outVertex = face.indices[k]; }
                    }
                }
            }
        }
    }
    return best;
}

Vec3 SelectionManager::snap(const Vec3& pos, SnapMode mode, float radius) const {
    Vec3 best = pos;
    float bestDist = radius;
    if (!m_doc) return best;

    size_t fc = m_doc->history().featureCount();
    for (FeatureId i = 1; i <= static_cast<FeatureId>(fc); ++i) {
        auto* node = m_doc->history().node(i);
        if (!node || !node->mesh || node->deleted || node->hidden) continue;
        const auto& verts = node->mesh->attributes().positions();

        if (mode == SnapMode::Vertex) {
            for (const auto& v : verts) {
                float d = (v - pos).length();
                if (d < bestDist) { bestDist = d; best = v; }
            }
        } else if (mode == SnapMode::FaceCenter) {
            const auto& topo = node->mesh->topology();
            for (uint32_t fi = 0; fi < topo.faceCount(); ++fi) {
                const auto& face = topo.face(fi);
                Vec3 center{};
                for (size_t j = 0; j < face.vertexCount(); ++j) center = center + verts[face.indices[j]];
                center = center * (1.f / static_cast<float>(face.vertexCount()));
                float d = (center - pos).length();
                if (d < bestDist) { bestDist = d; best = center; }
            }
        }
    }
    return best;
}

void SelectionManager::add(FeatureId id) {
    if (std::find(m_all.begin(), m_all.end(), id) == m_all.end())
        m_all.push_back(id);
}

void SelectionManager::remove(FeatureId id) {
    auto it = std::find(m_all.begin(), m_all.end(), id);
    if (it != m_all.end()) m_all.erase(it);
}

void SelectionManager::toggle(FeatureId id) {
    auto it = std::find(m_all.begin(), m_all.end(), id);
    if (it != m_all.end()) m_all.erase(it);
    else m_all.push_back(id);
}

} // namespace nexus::app
