#include <nexus/cad/CadDrawing.h>
#include <nexus/geometry/MeshAnalysis.h>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <unordered_map>

namespace nexus::cad {

std::vector<DrawingView> CadDrawing::generateStandardViews(const CadDocument& doc) noexcept {
    std::vector<DrawingView> views;

    Vec3 docCenter{};
    float docSize = 10.0f;
    size_t featureCount = doc.history().featureCount();
    if (featureCount > 0) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(1));
        if (node && node->mesh) {
            auto bounds = node->mesh->computeBounds();
            docCenter = {(bounds.min.x + bounds.max.x) * 0.5f,
                         (bounds.min.y + bounds.max.y) * 0.5f,
                         (bounds.min.z + bounds.max.z) * 0.5f};
            float dx = bounds.max.x - bounds.min.x;
            float dy = bounds.max.y - bounds.min.y;
            float dz = bounds.max.z - bounds.min.z;
            docSize = std::max({dx, dy, dz, 1.0f}) * 1.5f;
        }
    }

    views.push_back({DrawingViewType::Front,
                     {docCenter.x, docCenter.y, docCenter.z + docSize},
                     docCenter, {0, 1, 0}, 1.0f, "Front", {}});
    views.push_back({DrawingViewType::Top,
                     {docCenter.x, docCenter.y + docSize, docCenter.z},
                     docCenter, {0, 0, -1}, 1.0f, "Top", {}});
    views.push_back({DrawingViewType::Right,
                     {docCenter.x + docSize, docCenter.y, docCenter.z},
                     docCenter, {0, 1, 0}, 1.0f, "Right", {}});
    views.push_back({DrawingViewType::Isometric,
                     {docCenter.x + docSize * 0.7f, docCenter.y + docSize * 0.5f, docCenter.z + docSize * 0.7f},
                     docCenter, {0, 1, 0}, 1.0f, "Isometric", {}});
    return views;
}

DrawingView CadDrawing::generateSectionView(const CadDocument& doc, const Vec3& planePoint,
                                              const Vec3& planeNormal) noexcept {
    DrawingView view;
    view.type = DrawingViewType::Section;

    float nx = planeNormal.x, ny = planeNormal.y, nz = planeNormal.z;
    float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nl < 1e-10f) { nx = 0; ny = 0; nz = 1; }
    else { nx /= nl; ny /= nl; nz /= nl; }

    view.cameraPosition = {planePoint.x + nx * 10.0f,
                            planePoint.y + ny * 10.0f,
                            planePoint.z + nz * 10.0f};
    view.target = planePoint;

    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (node && !node->deleted) {
            view.featureIds.push_back(static_cast<uint32_t>(i));
        }
    }

    view.label = "A-A";
    return view;
}

geometry::Mesh CadDrawing::exportViewsAsMesh(const std::vector<DrawingView>& views) noexcept {
    geometry::Mesh combined;
    if (views.empty()) return combined;

    std::vector<Vec3> allPositions;

    for (size_t vi = 0; vi < views.size(); ++vi) {
        const auto& view = views[vi];
        float offsetX = static_cast<float>(vi) * 15.0f;

        float halfW = 5.0f;
        float halfH = 5.0f;

        Vec3 center{view.target.x + offsetX, view.target.y, view.target.z};
        Vec3 right, up;
        Vec3 forward{view.cameraPosition.x - view.target.x,
                     view.cameraPosition.y - view.target.y,
                     view.cameraPosition.z - view.target.z};
        float fl = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
        if (fl > 1e-10f) forward = Vec3{forward.x / fl, forward.y / fl, forward.z / fl};

        right = Vec3{forward.y * view.up.z - forward.z * view.up.y,
                     forward.z * view.up.x - forward.x * view.up.z,
                     forward.x * view.up.y - forward.y * view.up.x};
        float rl = std::sqrt(right.x * right.x + right.y * right.y + right.z * right.z);
        if (rl > 1e-10f) right = Vec3{right.x / rl, right.y / rl, right.z / rl};

        up = Vec3{right.y * forward.z - right.z * forward.y,
                  right.z * forward.x - right.x * forward.z,
                  right.x * forward.y - right.y * forward.x};

        uint32_t base = static_cast<uint32_t>(allPositions.size());
        allPositions.push_back(Vec3{center.x - right.x * halfW - up.x * halfH,
                                     center.y - right.y * halfW - up.y * halfH,
                                     center.z - right.z * halfW - up.z * halfH});
        allPositions.push_back(Vec3{center.x + right.x * halfW - up.x * halfH,
                                     center.y + right.y * halfW - up.y * halfH,
                                     center.z + right.z * halfW - up.z * halfH});
        allPositions.push_back(Vec3{center.x + right.x * halfW + up.x * halfH,
                                     center.y + right.y * halfW + up.y * halfH,
                                     center.z + right.z * halfW + up.z * halfH});
        allPositions.push_back(Vec3{center.x - right.x * halfW + up.x * halfH,
                                     center.y - right.y * halfW + up.y * halfH,
                                     center.z - right.z * halfW + up.z * halfH});

        geometry::Face f1, f2;
        f1.indices = {base, base + 1, base + 2};
        f2.indices = {base, base + 2, base + 3};
        combined.topology().addFace(f1);
        combined.topology().addFace(f2);
    }

    combined.attributes().setPositions(allPositions);
    return combined;
}

bool CadConfiguration::apply(const Configuration& config, CadDocument& doc) const noexcept {
    if (config.values.empty()) return true;

    for (size_t i = 0; i < std::min(config.values.size(), m_params.size()); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i + 1));
        if (!node || !node->mesh) continue;

        double value = config.values[i].second;
        const auto& param = m_params[i];

        if (param.name.find("height") != std::string::npos ||
            param.name.find("depth") != std::string::npos) {
            doc.history().setHeight(static_cast<parametric::FeatureId>(i + 1),
                                    static_cast<float>(value));
        } else if (param.name.find("draft") != std::string::npos) {
            doc.history().setDraftAngle(static_cast<parametric::FeatureId>(i + 1),
                                        static_cast<float>(value));
        }
    }

    return true;
}

void CadRenderModes::applyStyle(const CadRenderStyle& style, CadDocument& doc) noexcept {
    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (!node) continue;

        switch (style.mode) {
            case CadRenderMode::Wireframe:
                node->displayMode = parametric::FeatureNode::DisplayMode::Wireframe;
                break;
            case CadRenderMode::Shaded:
            case CadRenderMode::ShadedWithEdges:
                node->displayMode = parametric::FeatureNode::DisplayMode::Solid;
                break;
            case CadRenderMode::XRay:
                node->displayMode = parametric::FeatureNode::DisplayMode::Wireframe;
                break;
            default:
                break;
        }
    }
}

geometry::Mesh CadRenderModes::extractEdges(const CadDocument& doc) noexcept {
    geometry::Mesh edgeMesh;
    std::vector<Vec3> positions;
    std::unordered_map<uint64_t, uint32_t> edgeCount;

    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (!node || !node->mesh || node->deleted || node->hidden) continue;

        const auto& meshPos = node->mesh->attributes().positions();
        const auto& topo = node->mesh->topology();

        edgeCount.clear();
        for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            for (size_t vi = 0; vi < face.indices.size(); ++vi) {
                uint32_t a = face.indices[vi];
                uint32_t b = face.indices[(vi + 1) % face.indices.size()];
                uint64_t key = (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
                edgeCount[key]++;
            }
        }

        for (const auto& [key, count] : edgeCount) {
            if (count == 1) {
                uint32_t a = static_cast<uint32_t>(key >> 32);
                uint32_t b = static_cast<uint32_t>(key & 0xFFFFFFFF);
                if (a < meshPos.size() && b < meshPos.size()) {
                    uint32_t base = static_cast<uint32_t>(positions.size());
                    positions.push_back(meshPos[a]);
                    positions.push_back(meshPos[b]);
                    geometry::Face f;
                    f.indices = {base, base + 1, base + 1};
                    edgeMesh.topology().addFace(f);
                }
            }
        }
    }

    edgeMesh.attributes().setPositions(positions);
    return edgeMesh;
}

geometry::Mesh CadRenderModes::generateHiddenLine(const CadDocument& doc,
                                                    const Vec3& viewDirection) noexcept {
    geometry::Mesh hiddenMesh;
    std::vector<Vec3> positions;

    float vx = viewDirection.x, vy = viewDirection.y, vz = viewDirection.z;
    float vl = std::sqrt(vx * vx + vy * vy + vz * vz);
    if (vl > 1e-10f) { vx /= vl; vy /= vl; vz /= vl; }

    for (size_t i = 1; i <= doc.history().featureCount(); ++i) {
        auto* node = doc.history().node(static_cast<parametric::FeatureId>(i));
        if (!node || !node->mesh || node->deleted || node->hidden) continue;

        const auto& meshPos = node->mesh->attributes().positions();
        const auto& topo = node->mesh->topology();

        for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
            const auto& face = topo.face(fi);
            if (face.indices.size() < 3) continue;

            uint32_t i0 = face.indices[0], i1 = face.indices[1], i2 = face.indices[2];
            if (i0 >= meshPos.size() || i1 >= meshPos.size() || i2 >= meshPos.size()) continue;

            Vec3 e1{meshPos[i1].x - meshPos[i0].x,
                    meshPos[i1].y - meshPos[i0].y,
                    meshPos[i1].z - meshPos[i0].z};
            Vec3 e2{meshPos[i2].x - meshPos[i0].x,
                    meshPos[i2].y - meshPos[i0].y,
                    meshPos[i2].z - meshPos[i0].z};
            float fnx = e1.y * e2.z - e1.z * e2.y;
            float fny = e1.z * e2.x - e1.x * e2.z;
            float fnz = e1.x * e2.y - e1.y * e2.x;

            float dot = fnx * vx + fny * vy + fnz * vz;
            if (dot <= 0) continue;

            for (size_t vi = 0; vi < face.indices.size(); ++vi) {
                uint32_t a = face.indices[vi];
                uint32_t b = face.indices[(vi + 1) % face.indices.size()];
                if (a >= meshPos.size() || b >= meshPos.size()) continue;

                uint32_t base = static_cast<uint32_t>(positions.size());
                positions.push_back(meshPos[a]);
                positions.push_back(meshPos[b]);
                geometry::Face f;
                f.indices = {base, base + 1, base + 1};
                hiddenMesh.topology().addFace(f);
            }
        }
    }

    hiddenMesh.attributes().setPositions(positions);
    return hiddenMesh;
}

}
