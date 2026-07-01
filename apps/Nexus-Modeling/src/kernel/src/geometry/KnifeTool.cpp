#include <nexus/geometry/KnifeTool.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace nexus::geometry {
using Vec3 = nexus::render::Vec3;

KnifeCutResult KnifeTool::cut(Mesh& mesh,
    const std::vector<Vec3>& cutPoints, float snapDistance) {
    KnifeCutResult result;
    if (cutPoints.size() < 2) return result;

    const auto& positions = mesh.attributes().positions();
    const auto& topology = mesh.topology();
    size_t nFaces = topology.faceCount();

    if (nFaces == 0 || positions.empty()) return result;

    auto dist2 = [](const Vec3& a, const Vec3& b) {
        return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z);
    };

    float snap2 = snapDistance * snapDistance;

    // Snap cut line endpoints to nearby vertices
    std::vector<Vec3> snappedPoints = cutPoints;
    for (auto& pt : snappedPoints) {
        float bestDist2 = snap2;
        Vec3 bestPt = pt;
        for (const auto& v : positions) {
            float d2 = dist2(pt, v);
            if (d2 < bestDist2) {
                bestDist2 = d2;
                bestPt = v;
            }
        }
        pt = bestPt;
    }

    std::unordered_map<uint64_t, std::vector<Vec3>> faceHits;
    const float eps = 1e-4f;

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& face = topology.face(fi);
        if (face.indices.size() < 3) continue;

        for (size_t si = 0; si + 1 < snappedPoints.size(); ++si) {
            const Vec3& a = snappedPoints[si];
            const Vec3& b = snappedPoints[si + 1];

            Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
            float abLen = std::sqrt(ab.x * ab.x + ab.y * ab.y + ab.z * ab.z);
            if (abLen < eps) continue;

            // For each edge in the face, test segment-segment intersection with [a,b]
            for (size_t vi = 0; vi < face.indices.size(); ++vi) {
                uint32_t v0 = face.indices[vi];
                uint32_t v1 = face.indices[(vi + 1) % face.indices.size()];
                if (v0 >= positions.size() || v1 >= positions.size()) continue;

                const Vec3& c = positions[v0];
                const Vec3& d = positions[v1];

                Vec3 cd{d.x - c.x, d.y - c.y, d.z - c.z};
                float cdLen = std::sqrt(cd.x * cd.x + cd.y * cd.y + cd.z * cd.z);
                if (cdLen < eps) continue;

                Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};

                Vec3 crossAB_CD{
                    ab.y * cd.z - ab.z * cd.y,
                    ab.z * cd.x - ab.x * cd.z,
                    ab.x * cd.y - ab.y * cd.x,
                };
                float denom = crossAB_CD.x * crossAB_CD.x + crossAB_CD.y * crossAB_CD.y + crossAB_CD.z * crossAB_CD.z;
                if (denom < 1e-12f) continue;

                float invDenom = 1.0f / std::sqrt(denom);
                Vec3 n{crossAB_CD.x * invDenom, crossAB_CD.y * invDenom, crossAB_CD.z * invDenom};

                float t = (ac.y * cd.z - ac.z * cd.y) * n.x + (ac.z * cd.x - ac.x * cd.z) * n.y + (ac.x * cd.y - ac.y * cd.x) * n.z;
                t = t * invDenom;

                if (t < -eps || t > abLen + eps) continue;

                Vec3 crossAC_CD{
                    ac.y * cd.z - ac.z * cd.y,
                    ac.z * cd.x - ac.x * cd.z,
                    ac.x * cd.y - ac.y * cd.x,
                };
                float u = (crossAC_CD.x * crossAB_CD.x + crossAC_CD.y * crossAB_CD.y + crossAC_CD.z * crossAB_CD.z) / denom;

                if (u < -eps || u > cdLen + eps) continue;

                Vec3 hit{a.x + ab.x * (t / abLen), a.y + ab.y * (t / abLen), a.z + ab.z * (t / abLen)};
                faceHits[fi].push_back(hit);
            }
        }
    }

    // Apply hits: for each face, create split vertices if it intersects
    std::vector<Vec3> newPositions = positions;
    std::vector<Face> newFaces;

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& face = topology.face(fi);
        auto it = faceHits.find(fi);

        if (it == faceHits.end() || it->second.empty()) {
            newFaces.push_back(face);
            continue;
        }

        // Retriangulate the face including cut points
        std::vector<uint32_t> faceVerts = face.indices;

        for (const auto& hit : it->second) {
            uint32_t newIdx = static_cast<uint32_t>(newPositions.size());
            newPositions.push_back(hit);
            faceVerts.push_back(newIdx);
            result.newVertices++;
        }

        // Fan triangulate
        for (size_t i = 1; i + 1 < faceVerts.size(); ++i) {
            Face nf;
            nf.indices = {faceVerts[0], faceVerts[i], faceVerts[i + 1]};
            newFaces.push_back(nf);
        }

        result.edgesSplit += static_cast<uint32_t>(it->second.size());
        result.facesSplit++;
    }

    mesh.attributes().setPositions(newPositions);
    mesh.topology().clearFaces();
    for (auto& f : newFaces) {
        mesh.topology().addFace(f);
    }

    result.success = true;
    return result;
}

KnifeCutResult KnifeTool::cutLine(Mesh& mesh,
    const Vec3& start, const Vec3& end, float snapDistance) {
    return cut(mesh, {start, end}, snapDistance);
}

} // namespace nexus::geometry
