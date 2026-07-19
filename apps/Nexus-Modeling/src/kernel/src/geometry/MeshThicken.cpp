#include <nexus/geometry/MeshThicken.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Tolerance.h>  // geometry::isFinite (non-finite rejection convention)

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh MeshThicken::solidify(const Mesh& mesh, const ThickenOptions& opts) {
    Mesh result;
    if (!mesh.isValid() || opts.thickness <= 0.f) return result;
    for (const auto& p : mesh.attributes().positions())
        if (!isFinite(p)) return result;  // reject non-finite input (convention)

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    const size_t vc = pos.size();
    const size_t fc = topo.faceCount();

    std::vector<Vec3> vnorms(vc, Vec3{0.f, 0.f, 0.f});

    for (size_t fi = 0; fi < fc; ++fi) {
        const Face& face = topo.face(fi);
        if (!face.indicesInBounds(vc)) continue;
        if (face.indices.size() < 3) continue;

        for (size_t vi = 0; vi + 2 < face.indices.size(); ++vi) {
            const Vec3& a = pos[face.indices[0]];
            const Vec3& b = pos[face.indices[vi + 1]];
            const Vec3& c = pos[face.indices[vi + 2]];
            Vec3 e1 = {b.x - a.x, b.y - a.y, b.z - a.z};
            Vec3 e2 = {c.x - a.x, c.y - a.y, c.z - a.z};
            Vec3 fn = e1.cross(e2);
            for (size_t j = 0; j < 3; ++j) {
                uint32_t vidx = face.indices[vi + j];
                vnorms[vidx].x += fn.x;
                vnorms[vidx].y += fn.y;
                vnorms[vidx].z += fn.z;
            }
        }
    }

    for (auto& n : vnorms) {
        n = n.normalize();
    }

    std::map<uint64_t, int32_t> edgeCount;
    for (size_t fi = 0; fi < fc; ++fi) {
        const Face& face = topo.face(fi);
        if (!face.indicesInBounds(vc)) continue;
        if (face.indices.size() < 3) continue;
        for (size_t vi = 0; vi < face.indices.size(); ++vi) {
            uint32_t a = face.indices[vi];
            uint32_t b = face.indices[(vi + 1) % face.indices.size()];
            uint32_t v0 = std::min(a, b);
            uint32_t v1 = std::max(a, b);
            uint64_t key = (static_cast<uint64_t>(v0) << 32) | v1;
            ++edgeCount[key];
        }
    }

    std::vector<std::pair<uint32_t, uint32_t>> boundaryEdges;
    for (const auto& [key, cnt] : edgeCount) {
        if (cnt == 1) {
            uint32_t v0 = static_cast<uint32_t>(key >> 32);
            uint32_t v1 = static_cast<uint32_t>(key & 0xFFFFFFFF);
            boundaryEdges.push_back({v0, v1});
        }
    }

    std::vector<Vec3> allPos;
    allPos.reserve(vc * 2);
    for (size_t i = 0; i < vc; ++i) {
        allPos.push_back(pos[i]);
    }
    for (size_t i = 0; i < vc; ++i) {
        allPos.push_back({
            pos[i].x + vnorms[i].x * opts.thickness,
            pos[i].y + vnorms[i].y * opts.thickness,
            pos[i].z + vnorms[i].z * opts.thickness,
        });
    }

    auto& resTopo = result.topology();
    auto& resAttr = result.attributes();

    for (size_t fi = 0; fi < fc; ++fi) {
        const Face& face = topo.face(fi);
        if (face.indices.size() < 3) {
            resTopo.addFace(face);
            continue;
        }

        resTopo.addFace(face);
        if (opts.closedSolid) {
            Face inner = face;
            std::reverse(inner.indices.begin(), inner.indices.end());
            for (auto& idx : inner.indices) {
                idx += static_cast<uint32_t>(vc);
            }
            resTopo.addFace(inner);
        }
    }

    for (const auto& [v0, v1] : boundaryEdges) {
        uint32_t i0 = v0;
        uint32_t i1 = v1;
        uint32_t i2 = v1 + static_cast<uint32_t>(vc);
        uint32_t i3 = v0 + static_cast<uint32_t>(vc);

        resTopo.addFace(Face{{i0, i1, i2}});
        resTopo.addFace(Face{{i0, i2, i3}});
    }

    resAttr.setPositions(std::move(allPos));
    if (!result.computeVertexNormals()) return result;
    result.rebuildStableElementIds();
    return result;
}

} // namespace nexus::geometry
