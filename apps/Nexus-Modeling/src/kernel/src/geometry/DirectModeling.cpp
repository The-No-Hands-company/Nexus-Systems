#include <nexus/geometry/DirectModeling.h>

#include <algorithm>
#include <unordered_set>
#include <cmath>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

DirectModelingReport DirectModeling::pushFaces(
    HalfEdgeMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float distance) noexcept
{
    DirectModelingReport report;
    if (faceIndices.empty() || std::abs(distance) < 1e-10f) return report;

    std::unordered_set<uint32_t> vertSet;
    for (uint32_t fi : faceIndices) {
        if (fi >= mesh.faceCount()) continue;
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;
        uint32_t startHe = he;
        do {
            vertSet.insert(mesh.edge(he).src);
            he = mesh.edge(he).next;
        } while (he != startHe);
    }

    Vec3 avgNormal{};
    for (uint32_t fi : faceIndices) {
        if (fi >= mesh.faceCount()) continue;
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;
        Vec3 a = mesh.positions()[mesh.edge(he).src];
        Vec3 b = mesh.positions()[mesh.edge(mesh.edge(he).next).src];
        Vec3 c = mesh.positions()[mesh.edge(mesh.edge(mesh.edge(he).next).next).src];
        Vec3 n{(b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y),
               (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z),
               (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)};
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 1e-10f) avgNormal = Vec3{avgNormal.x + n.x / len, avgNormal.y + n.y / len, avgNormal.z + n.z / len};
    }
    float normLen = std::sqrt(avgNormal.x * avgNormal.x + avgNormal.y * avgNormal.y + avgNormal.z * avgNormal.z);
    if (normLen > 1e-10f) avgNormal = Vec3{avgNormal.x / normLen, avgNormal.y / normLen, avgNormal.z / normLen};

    for (uint32_t v : vertSet) {
        if (v < mesh.vertexCount())
            mesh.positions()[v] = Vec3{mesh.positions()[v].x + avgNormal.x * distance,
                                        mesh.positions()[v].y + avgNormal.y * distance,
                                        mesh.positions()[v].z + avgNormal.z * distance};
    }

    report.valid = true;
    report.facesMoved = static_cast<uint32_t>(faceIndices.size());
    report.verticesAdded = 0;
    return report;
}

DirectModelingReport DirectModeling::pushFacesPerFace(
    HalfEdgeMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float distance) noexcept
{
    DirectModelingReport report;
    if (faceIndices.empty() || std::abs(distance) < 1e-10f) return report;

    for (uint32_t fi : faceIndices) {
        if (fi >= mesh.faceCount()) continue;

        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;

        Vec3 a = mesh.positions()[mesh.edge(he).src];
        Vec3 b = mesh.positions()[mesh.edge(mesh.edge(he).next).src];
        Vec3 c = mesh.positions()[mesh.edge(mesh.edge(mesh.edge(he).next).next).src];
        Vec3 n{(b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y),
               (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z),
               (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)};
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len < 1e-10f) continue;
        n = Vec3{n.x / len, n.y / len, n.z / len};

        uint32_t startHe = he;
        do {
            uint32_t v = mesh.edge(he).src;
            if (v < mesh.vertexCount())
                mesh.positions()[v] = Vec3{mesh.positions()[v].x + n.x * distance,
                                            mesh.positions()[v].y + n.y * distance,
                                            mesh.positions()[v].z + n.z * distance};
            he = mesh.edge(he).next;
        } while (he != startHe);
    }

    report.valid = true;
    report.facesMoved = static_cast<uint32_t>(faceIndices.size());
    report.verticesAdded = 0;
    return report;
}

DirectModelingReport DirectModeling::pushFacesWithDraft(
    HalfEdgeMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float distance,
    float draftAngleDeg) noexcept
{
    DirectModelingReport report;
    if (faceIndices.empty() || std::abs(distance) < 1e-10f) return report;

    float draftRad = draftAngleDeg * 3.1415926535f / 180.0f;
    float draftFactor = std::tan(draftRad);

    std::unordered_set<uint32_t> vertSet;
    for (uint32_t fi : faceIndices) {
        if (fi >= mesh.faceCount()) continue;
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;
        uint32_t startHe = he;
        do {
            vertSet.insert(mesh.edge(he).src);
            he = mesh.edge(he).next;
        } while (he != startHe);
    }

    Vec3 avgNormal{};
    for (uint32_t fi : faceIndices) {
        if (fi >= mesh.faceCount()) continue;
        uint32_t he = mesh.face(fi).edge;
        if (he == HalfEdgeMesh::kInvalid) continue;
        Vec3 a = mesh.positions()[mesh.edge(he).src];
        Vec3 b = mesh.positions()[mesh.edge(mesh.edge(he).next).src];
        Vec3 c = mesh.positions()[mesh.edge(mesh.edge(mesh.edge(he).next).next).src];
        Vec3 n{(b.y - a.y) * (c.z - a.z) - (b.z - a.z) * (c.y - a.y),
               (b.z - a.z) * (c.x - a.x) - (b.x - a.x) * (c.z - a.z),
               (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)};
        float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 1e-10f) avgNormal = Vec3{avgNormal.x + n.x / len, avgNormal.y + n.y / len, avgNormal.z + n.z / len};
    }
    float normLen = std::sqrt(avgNormal.x * avgNormal.x + avgNormal.y * avgNormal.y + avgNormal.z * avgNormal.z);
    if (normLen > 1e-10f) avgNormal = Vec3{avgNormal.x / normLen, avgNormal.y / normLen, avgNormal.z / normLen};

    Vec3 pullDir{0, 0, 1};

    for (uint32_t v : vertSet) {
        if (v >= mesh.vertexCount()) continue;
        float lateralOffset = std::abs(distance) * draftFactor;
        Vec3 lateral{avgNormal.y * pullDir.z - avgNormal.z * pullDir.y,
                      avgNormal.z * pullDir.x - avgNormal.x * pullDir.z,
                      avgNormal.x * pullDir.y - avgNormal.y * pullDir.x};
        float lLen = std::sqrt(lateral.x * lateral.x + lateral.y * lateral.y + lateral.z * lateral.z);
        if (lLen > 1e-10f) lateral = Vec3{lateral.x / lLen, lateral.y / lLen, lateral.z / lLen};

        mesh.positions()[v] = Vec3{
            mesh.positions()[v].x + avgNormal.x * distance + lateral.x * lateralOffset,
            mesh.positions()[v].y + avgNormal.y * distance + lateral.y * lateralOffset,
            mesh.positions()[v].z + avgNormal.z * distance + lateral.z * lateralOffset,
        };
    }

    report.valid = true;
    report.facesMoved = static_cast<uint32_t>(faceIndices.size());
    report.verticesAdded = 0;
    return report;
}

DirectModelingReport DirectModeling::pullFaces(
    HalfEdgeMesh& mesh,
    const std::vector<uint32_t>& faceIndices,
    float distance) noexcept
{
    return pushFaces(mesh, faceIndices, -distance);
}

} // namespace nexus::geometry
