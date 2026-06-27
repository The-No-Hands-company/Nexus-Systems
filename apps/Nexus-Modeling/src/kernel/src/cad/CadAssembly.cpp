#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadDocument.h>
#include <nexus/parametric/FeatureHistory.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace nexus::cad {

using namespace nexus::geometry;
using Vec3 = nexus::render::Vec3;

// ── CadPart ───────────────────────────────────────────────────────────────────

CadPart::CadPart(std::string name) : m_name(std::move(name)) {}

geometry::Mesh CadPart::combinedMesh() const noexcept
{
    geometry::Mesh combined;
    for (size_t i = 1; i <= m_document.history().featureCount(); ++i) {
        auto* node = m_document.history().node(static_cast<parametric::FeatureId>(i));
        if (node && node->mesh) {
            if (!combined.appendMesh(*node->mesh)) { combined = *node->mesh; }
        }
    }
    return combined;
}

// ── CadAssembly ───────────────────────────────────────────────────────────────

uint32_t CadAssembly::addPart(std::shared_ptr<CadPart> part)
{
    m_parts.push_back(std::move(part));
    return static_cast<uint32_t>(m_parts.size()) - 1;
}

CadPart* CadAssembly::part(uint32_t index) noexcept
{
    return (index < m_parts.size()) ? m_parts[index].get() : nullptr;
}

const CadPart* CadAssembly::part(uint32_t index) const noexcept
{
    return (index < m_parts.size()) ? m_parts[index].get() : nullptr;
}

void CadAssembly::addConstraint(const AssemblyConstraint& c)
{
    m_constraints.push_back(c);
}

const std::vector<AssemblyConstraint>& CadAssembly::constraints() const noexcept { return m_constraints; }

// ── CadMeasure ───────────────────────────────────────────────────────────────

namespace {
    [[nodiscard]] float computeFaceArea(const Mesh& mesh, uint32_t faceIndex) noexcept {
        const auto& topo = mesh.topology();
        const auto& pos = mesh.attributes().positions();
        if(faceIndex >= topo.faceCount()) return 0.f;
        const auto& face = topo.face(faceIndex);
        if(face.vertexCount() < 3 || !face.indicesInBounds(pos.size())) return 0.f;
        Vec3 n{};
        for(size_t j=0; j+2 < face.vertexCount(); ++j)
            n = n + (pos[face.indices[j+1]] - pos[face.indices[0]])
                    .cross(pos[face.indices[j+2]] - pos[face.indices[0]]);
        return n.length() * 0.5f;
    }
}

MeasureResult CadMeasure::distanceBetween(const CadPart& part,
                                           uint32_t faceA, uint32_t faceB) noexcept
{
    MeasureResult r{};
    Mesh mesh = part.combinedMesh();
    if(!mesh.isValid()) return r;

    const auto& pos = mesh.attributes().positions();
    const auto& topo = mesh.topology();
    if(faceA >= topo.faceCount() || faceB >= topo.faceCount()) return r;

    const auto& fa = topo.face(faceA);
    const auto& fb = topo.face(faceB);
    if(fa.indices.empty() || fb.indices.empty()) return r;

    float minDist = std::numeric_limits<float>::max();
    const size_t nVerts = pos.size();
    for(auto ia : fa.indices) { if(ia >= nVerts) continue;
        for(auto ib : fb.indices) { if(ib >= nVerts) continue;
            float d = (pos[ia] - pos[ib]).length();
            if(d < minDist) minDist = d;
        }
    }
    r.distance = (minDist < std::numeric_limits<float>::max()) ? static_cast<double>(minDist) : 0.0;
    r.valid = true;
    return r;
}

MeasureResult CadMeasure::angleBetween(const CadPart& part,
                                         uint32_t faceA, uint32_t faceB) noexcept
{
    MeasureResult r{};
    Mesh mesh = part.combinedMesh();
    if(!mesh.isValid()) return r;

    const auto& pos = mesh.attributes().positions();
    const auto& topo = mesh.topology();
    if(faceA >= topo.faceCount() || faceB >= topo.faceCount()) return r;

    const auto& fa = topo.face(faceA);
    const auto& fb = topo.face(faceB);
    if(fa.vertexCount() < 3 || fb.vertexCount() < 3) return r;
    if(!fa.indicesInBounds(pos.size()) || !fb.indicesInBounds(pos.size())) return r;

    Vec3 nA{}, nB{};
    for(size_t j=0; j+2<fa.vertexCount(); ++j)
        nA = nA + (pos[fa.indices[j+1]] - pos[fa.indices[0]]).cross(pos[fa.indices[j+2]] - pos[fa.indices[0]]);
    for(size_t j=0; j+2<fb.vertexCount(); ++j)
        nB = nB + (pos[fb.indices[j+1]] - pos[fb.indices[0]]).cross(pos[fb.indices[j+2]] - pos[fb.indices[0]]);
    nA = nA.normalize(); nB = nB.normalize();
    float dotVal = std::clamp(nA.dot(nB), -1.f, 1.f);
    r.angle = std::acos(dotVal) * 180.0 / 3.14159265358979323846;
    r.valid = true;
    return r;
}

MeasureResult CadMeasure::faceArea(const CadPart& part, uint32_t faceIndex) noexcept
{
    MeasureResult r{};
    Mesh mesh = part.combinedMesh();
    if(!mesh.isValid()) return r;
    r.area = static_cast<double>(computeFaceArea(mesh, faceIndex));
    r.valid = true;
    return r;
}

MeasureResult CadMeasure::solidVolume(const CadPart& part) noexcept
{
    MeasureResult r{};
    Mesh mesh = part.combinedMesh();
    if(!mesh.isValid()) return r;
    auto props = MeshMassProperties::compute(mesh);
    r.volume = props.volume;
    r.valid = true;
    return r;
}

} // namespace nexus::cad
