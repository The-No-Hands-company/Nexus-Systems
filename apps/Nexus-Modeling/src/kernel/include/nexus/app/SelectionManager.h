#pragma once
// SelectionManager — ray-cast picking, geometry snapping, selection state

#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::app {

using Vec3 = nexus::render::Vec3;
using FeatureId = nexus::parametric::FeatureId;

enum class SnapMode : uint8_t { Vertex, EdgeMidpoint, FaceCenter };

class SelectionManager {
public:
    void setDocument(nexus::cad::CadDocument* doc) { m_doc = doc; }

    // Ray-cast pick against all document features
    FeatureId pick(const Vec3& rayOrigin, const Vec3& rayDir,
                   uint32_t& outFace, uint32_t& outVertex, Vec3& outHit) const;

    // Snap cursor to nearest geometry element
    Vec3 snap(const Vec3& pos, SnapMode mode, float radius = 1.5f) const;

    // Selection state
    FeatureId primary() const { return m_primary; }
    void setPrimary(FeatureId id) { m_primary = id; }
    const std::vector<FeatureId>& all() const { return m_all; }
    std::vector<FeatureId>& all() { return m_all; }
    void add(FeatureId id);
    void remove(FeatureId id);
    void toggle(FeatureId id);
    void clearAll() { m_all.clear(); m_primary = nexus::parametric::kInvalidFeatureId; }
    bool empty() const { return m_all.empty(); }
    size_t count() const { return m_all.size(); }

private:
    nexus::cad::CadDocument* m_doc = nullptr;
    FeatureId m_primary = nexus::parametric::kInvalidFeatureId;
    std::vector<FeatureId> m_all;
};

} // namespace nexus::app
