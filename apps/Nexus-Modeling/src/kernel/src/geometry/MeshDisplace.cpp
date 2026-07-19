#include <nexus/geometry/MeshDisplace.h>
#include <nexus/geometry/Tolerance.h>  // geometry::isFinite (non-finite rejection convention)

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

std::vector<Vec3> computeAreaWeightedNormals(const Mesh& mesh) {
    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    std::vector<Vec3> vnorms(pos.size(), Vec3{0.f, 0.f, 0.f});

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        if (!face.indicesInBounds(pos.size())) continue;
        if (face.indices.size() < 3) continue;

        Vec3 fn = {0.f, 0.f, 0.f};
        const Vec3& a = pos[face.indices[0]];
        for (size_t vi = 0; vi + 2 < face.indices.size(); ++vi) {
            const Vec3& b = pos[face.indices[vi + 1]];
            const Vec3& c = pos[face.indices[vi + 2]];
            Vec3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
            Vec3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
            fn = fn + e1.cross(e2);
        }

        Vec3 contrib = {fn.x * 0.5f, fn.y * 0.5f, fn.z * 0.5f};

        for (size_t vi = 0; vi < face.indices.size(); ++vi) {
            uint32_t vidx = face.indices[vi];
            vnorms[vidx] = vnorms[vidx] + contrib;
        }
    }

    for (auto& n : vnorms) {
        n = n.normalize();
    }

    return vnorms;
}

} // namespace

Mesh MeshDisplace::displaceByScalar(
    const Mesh& mesh,
    const std::function<float(const Vec3&)>& scalarField,
    const DisplaceOptions& opts) {

    Mesh result = mesh;
    if (!result.isValid()) return result;
    for (const auto& p : mesh.attributes().positions())
        if (!isFinite(p)) return Mesh{};  // reject non-finite input (convention)

    std::vector<Vec3> positions = result.attributes().positions();
    std::vector<Vec3> vnorms = computeAreaWeightedNormals(result);

    for (size_t i = 0; i < positions.size(); ++i) {
        float s = scalarField(positions[i]) * opts.magnitude;
        positions[i].x += vnorms[i].x * s;
        positions[i].y += vnorms[i].y * s;
        positions[i].z += vnorms[i].z * s;
    }

    result.attributes().setPositions(std::move(positions));

    if (opts.recomputeNormals) {
        if (!result.computeVertexNormals()) return result;
    } else {
        std::vector<Vec3> smoothed = computeAreaWeightedNormals(result);
        result.attributes().setNormals(std::move(smoothed));
    }

    result.rebuildStableElementIds();
    return result;
}

Mesh MeshDisplace::displaceByVector(
    const Mesh& mesh,
    const std::function<Vec3(const Vec3&)>& vectorField,
    const DisplaceOptions& opts) {

    Mesh result = mesh;
    if (!result.isValid()) return result;
    for (const auto& p : mesh.attributes().positions())
        if (!isFinite(p)) return Mesh{};  // reject non-finite input (convention)

    std::vector<Vec3> positions = result.attributes().positions();

    for (size_t i = 0; i < positions.size(); ++i) {
        Vec3 d = vectorField(positions[i]);
        positions[i].x += d.x * opts.magnitude;
        positions[i].y += d.y * opts.magnitude;
        positions[i].z += d.z * opts.magnitude;
    }

    result.attributes().setPositions(std::move(positions));

    if (opts.recomputeNormals) {
        if (!result.computeVertexNormals()) return result;
    } else {
        result.attributes().clearNormals();
    }

    result.rebuildStableElementIds();
    return result;
}

} // namespace nexus::geometry
