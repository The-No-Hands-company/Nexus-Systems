#include <nexus/geometry/MeshRepair.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/MeshVertexWeld.h>
#include <nexus/geometry/Tolerance.h>  // geometry::isFinite (non-finite rejection convention)

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nexus::geometry {

namespace {

bool isDegenerate(const Face& f, const std::vector<nexus::render::Vec3>& positions) {
    if (f.indices.size() < 3) return true;
    if (!f.indicesInBounds(positions.size())) return true;
    for (size_t i = 0; i < f.indices.size(); ++i) {
        for (size_t j = i + 1; j < f.indices.size(); ++j) {
            if (f.indices[i] == f.indices[j]) return true;
        }
    }
    if (f.indices.size() == 3 && positions.size() > f.indices[0] &&
        positions.size() > f.indices[1] && positions.size() > f.indices[2]) {
        const auto& a = positions[f.indices[0]];
        const auto& b = positions[f.indices[1]];
        const auto& c = positions[f.indices[2]];
        float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
        float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
        float areaSq = (e1y * e2z - e1z * e2y) * (e1y * e2z - e1z * e2y)
                     + (e1z * e2x - e1x * e2z) * (e1z * e2x - e1x * e2z)
                     + (e1x * e2y - e1y * e2x) * (e1x * e2y - e1y * e2x);
        if (areaSq < 1e-20f) return true;
    }
    return false;
}

float faceAreaSq(const Face& f, const std::vector<nexus::render::Vec3>& positions) {
    if (!f.indicesInBounds(positions.size())) return 0.f;
    if (f.indices.size() < 3) return 0.f;
    const auto& v0 = positions[f.indices[0]];
    nexus::render::Vec3 accumulatedCross{0, 0, 0};
    for (size_t i = 2; i < f.indices.size(); ++i) {
        const auto& a = positions[f.indices[i - 1]];
        const auto& b = positions[f.indices[i]];
        float e1x = a.x - v0.x, e1y = a.y - v0.y, e1z = a.z - v0.z;
        float e2x = b.x - v0.x, e2y = b.y - v0.y, e2z = b.z - v0.z;
        accumulatedCross.x += e1y * e2z - e1z * e2y;
        accumulatedCross.y += e1z * e2x - e1x * e2z;
        accumulatedCross.z += e1x * e2y - e1y * e2x;
    }
    return accumulatedCross.lengthSq();
}

} // anonymous namespace

// --- removeDuplicateVertices ---------------------------------------------------

bool MeshRepair::removeDuplicateVertices(Mesh& mesh) {
    if (!mesh.isValid()) return false;

    size_t oldVertexCount = mesh.attributes().vertexCount();
    Mesh welded = MeshVertexWeld::weld(mesh);
    if (!welded.isValid()) return false;

    size_t newVertexCount = welded.attributes().vertexCount();
    if (newVertexCount >= oldVertexCount) return false;

    mesh = std::move(welded);
    return true;
}

// --- removeZeroAreaFaces -------------------------------------------------------

bool MeshRepair::removeZeroAreaFaces(Mesh& mesh, float minArea) {
    if (!mesh.isValid()) return false;
    if (minArea <= 0.f) return false;

    auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    size_t nFaces = topo.faceCount();
    if (nFaces == 0) return false;

    float minAreaSq = minArea * minArea;

    std::vector<Face> keptFaces;
    keptFaces.reserve(nFaces);
    bool removed = false;

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        if (!f.indicesInBounds(pos.size())) {
            removed = true;
            continue;
        }
        if (f.indices.size() < 3) {
            removed = true;
            continue;
        }

        std::set<uint32_t> uniq(f.indices.begin(), f.indices.end());
        if (uniq.size() != f.indices.size()) {
            removed = true;
            continue;
        }

        float areaSq = faceAreaSq(f, pos);
        if (areaSq < minAreaSq * 4.f) {
            removed = true;
            continue;
        }

        keptFaces.push_back(f);
    }

    if (!removed) return false;

    topo.clearFaces();
    for (auto& f : keptFaces) {
        topo.addFace(std::move(f));
    }
    return true;
}

// --- removeIsolatedVertices ----------------------------------------------------

bool MeshRepair::removeIsolatedVertices(Mesh& mesh) {
    if (!mesh.isValid()) return false;

    auto& topo = mesh.topology();
    auto& attr = mesh.attributes();
    size_t nVerts = attr.vertexCount();
    size_t nFaces = topo.faceCount();

    if (nVerts == 0) return false;
    if (nFaces == 0) {
        attr.setPositions({});
        attr.clearNormals();
        attr.clearUVs();
        attr.clearTangents();
        attr.clearSkinning();
        return true;
    }

    std::vector<bool> referenced(nVerts, false);
    for (size_t fi = 0; fi < nFaces; ++fi) {
        for (uint32_t idx : topo.face(fi).indices) {
            if (idx < nVerts) referenced[idx] = true;
        }
    }

    bool hasIsolated = false;
    for (size_t vi = 0; vi < nVerts; ++vi) {
        if (!referenced[vi]) {
            hasIsolated = true;
            break;
        }
    }
    if (!hasIsolated) return false;

    std::vector<uint32_t> remap(nVerts);
    uint32_t newCount = 0;
    for (size_t vi = 0; vi < nVerts; ++vi) {
        if (referenced[vi]) {
            remap[vi] = newCount++;
        } else {
            remap[vi] = 0xFFFFFFFFu;
        }
    }

    auto remapFaceIndices = [&](const Face& f) {
        Face nf;
        for (uint32_t idx : f.indices) {
            nf.indices.push_back(remap[idx]);
        }
        return nf;
    };

    std::vector<Face> newFaces;
    newFaces.reserve(nFaces);
    for (size_t fi = 0; fi < nFaces; ++fi) {
        newFaces.push_back(remapFaceIndices(topo.face(fi)));
    }

    topo.clearFaces();
    for (auto& f : newFaces) {
        topo.addFace(std::move(f));
    }

    const auto& oldPos = attr.positions();
    std::vector<nexus::render::Vec3> newPos(newCount);
    for (size_t vi = 0; vi < nVerts; ++vi) {
        if (referenced[vi]) newPos[remap[vi]] = oldPos[vi];
    }
    attr.setPositions(std::move(newPos));

    if (attr.hasNormals()) {
        const auto& oldNormals = attr.normals();
        std::vector<nexus::render::Vec3> newNormals(newCount);
        for (size_t vi = 0; vi < nVerts; ++vi) {
            if (referenced[vi]) newNormals[remap[vi]] = oldNormals[vi];
        }
        attr.setNormals(std::move(newNormals));
    }

    if (attr.hasUVs()) {
        const auto& oldUVs = attr.uvs();
        std::vector<Vec2> newUVs(newCount);
        for (size_t vi = 0; vi < nVerts; ++vi) {
            if (referenced[vi]) newUVs[remap[vi]] = oldUVs[vi];
        }
        attr.setUVs(std::move(newUVs));
    }

    if (attr.hasTangents()) {
        const auto& oldTangents = attr.tangents();
        std::vector<Vec4> newTangents(newCount);
        for (size_t vi = 0; vi < nVerts; ++vi) {
            if (referenced[vi]) newTangents[remap[vi]] = oldTangents[vi];
        }
        attr.setTangents(std::move(newTangents));
    }

    if (attr.hasSkinning()) {
        const auto& oldJoints = attr.jointIndices();
        const auto& oldWeights = attr.jointWeights();
        std::vector<JointIndex4> newJoints(newCount);
        std::vector<JointWeight4> newWeights(newCount);
        for (size_t vi = 0; vi < nVerts; ++vi) {
            if (referenced[vi]) {
                newJoints[remap[vi]] = oldJoints[vi];
                newWeights[remap[vi]] = oldWeights[vi];
            }
        }
        attr.setSkinning(std::move(newJoints), std::move(newWeights));
    }

    return true;
}

// --- fixFaceOrientation --------------------------------------------------------

bool MeshRepair::fixFaceOrientation(Mesh& mesh) {
    if (!mesh.isValid()) return false;

    auto& topo = mesh.topology();
    size_t nFaces = topo.faceCount();
    if (nFaces == 0) return false;

    std::map<uint64_t, std::vector<std::pair<uint32_t, bool>>> edgeFaceDir;

    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        for (size_t vi = 0; vi < f.indices.size(); ++vi) {
            uint32_t v0 = f.indices[vi];
            uint32_t v1 = f.indices[(vi + 1) % f.indices.size()];
            uint32_t lo = std::min(v0, v1);
            uint32_t hi = std::max(v0, v1);
            uint64_t key = (static_cast<uint64_t>(hi) << 32) | lo;
            bool dir = (v0 == lo);
            edgeFaceDir[key].push_back({static_cast<uint32_t>(fi), dir});
        }
    }

    std::vector<std::vector<uint32_t>> neighbors(nFaces);
    std::map<std::pair<uint32_t, uint32_t>, bool> needFlipTable;

    for (auto& [key, entries] : edgeFaceDir) {
        for (size_t i = 0; i < entries.size(); ++i) {
            for (size_t j = i + 1; j < entries.size(); ++j) {
                auto [fi, dirI] = entries[i];
                auto [fj, dirJ] = entries[j];
                if (fi == fj) continue;
                neighbors[fi].push_back(fj);
                neighbors[fj].push_back(fi);
                bool sameDir = (dirI == dirJ);
                needFlipTable[{fi, fj}] = sameDir;
                needFlipTable[{fj, fi}] = sameDir;
            }
        }
    }

    std::vector<bool> visited(nFaces, false);
    std::vector<bool> flipped(nFaces, false);
    std::queue<uint32_t> q;

    for (uint32_t seed = 0; seed < static_cast<uint32_t>(nFaces); ++seed) {
        if (visited[seed]) continue;
        visited[seed] = true;
        q.push(seed);

        while (!q.empty()) {
            uint32_t cur = q.front();
            q.pop();

            for (uint32_t nb : neighbors[cur]) {
                if (visited[nb]) continue;

                auto it = needFlipTable.find({cur, nb});
                if (it == needFlipTable.end()) continue;

                bool baseNeedFlip = it->second;
                flipped[nb] = baseNeedFlip ^ flipped[cur];
                visited[nb] = true;
                q.push(nb);
            }
        }
    }

    bool anyFlipped = false;
    for (bool fl : flipped) {
        if (fl) { anyFlipped = true; break; }
    }
    if (!anyFlipped) return false;

    std::vector<Face> newFaces;
    newFaces.reserve(nFaces);
    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        Face nf;
        if (flipped[fi]) {
            nf.indices.assign(f.indices.rbegin(), f.indices.rend());
        } else {
            nf.indices = f.indices;
        }
        newFaces.push_back(std::move(nf));
    }

    topo.clearFaces();
    for (auto& f : newFaces) {
        topo.addFace(std::move(f));
    }

    return true;
}

// --- fillHoles -----------------------------------------------------------------

bool MeshRepair::fillHoles(Mesh& mesh, uint32_t maxHoleSize) {
    if (!mesh.isValid()) return false;

    auto hemOpt = HalfEdgeMesh::fromMesh(mesh);
    if (!hemOpt) return false;

    auto& hem = *hemOpt;
    auto loops = hem.boundaryLoops();
    if (loops.empty()) return false;

    auto& topo = mesh.topology();
    auto positions = mesh.attributes().positions();
    uint32_t nextVertexBase = static_cast<uint32_t>(positions.size());
    std::vector<nexus::render::Vec3> newVerts;
    uint32_t filled = 0;

    for (auto& loop : loops) {
        if (loop.size() > maxHoleSize || loop.size() < 3) continue;

        nexus::render::Vec3 centroid{0, 0, 0};
        for (uint32_t vi : loop) {
            const auto& v = positions[vi];
            centroid.x += v.x;
            centroid.y += v.y;
            centroid.z += v.z;
        }
        float inv = 1.0f / static_cast<float>(loop.size());
        centroid.x *= inv;
        centroid.y *= inv;
        centroid.z *= inv;

        uint32_t cidx = nextVertexBase + static_cast<uint32_t>(newVerts.size());
        newVerts.push_back(centroid);

        for (size_t i = 0; i < loop.size(); ++i) {
            uint32_t v0 = loop[i];
            uint32_t v1 = loop[(i + 1) % loop.size()];
            Face f;
            f.indices = {v0, v1, cidx};
            topo.addFace(std::move(f));
        }

        ++filled;
    }

    if (filled == 0) return false;

    auto& attrs = mesh.attributes();
    auto existingPos = attrs.positions();
    existingPos.insert(existingPos.end(), newVerts.begin(), newVerts.end());
    attrs.setPositions(std::move(existingPos));

    return true;
}

// --- repairAll -----------------------------------------------------------------

RepairReport MeshRepair::repairAll(Mesh& mesh) {
    RepairReport report;
    report.ok = true;

    if (!mesh.isValid()) {
        report.ok = false;
        report.error = "Input mesh is not valid";
        return report;
    }
    for (const auto& p : mesh.attributes().positions()) {
        if (!isFinite(p)) {  // decline non-finite geometry; leave the mesh unchanged
            report.ok = false;
            report.error = "Input mesh has non-finite positions";
            return report;
        }
    }

    report.duplicateVerticesRemoved = removeDuplicateVertices(mesh);
    report.zeroAreaFacesRemoved = removeZeroAreaFaces(mesh) ? 1 : 0;
    report.isolatedVerticesRemoved = removeIsolatedVertices(mesh) ? 1 : 0;
    report.faceOrientationFixed = fixFaceOrientation(mesh);
    report.holesFilledOp = fillHoles(mesh);

    return report;
}

// --- repair (legacy) -----------------------------------------------------------

RepairReport MeshRepair::repair(Mesh& mesh, const RepairOptions& opts) {
    RepairReport report;
    report.ok = true;

    if (!mesh.isValid()) {
        report.ok = false;
        report.error = "Input mesh is not valid";
        return report;
    }
    for (const auto& p : mesh.attributes().positions()) {
        if (!isFinite(p)) {  // decline non-finite geometry; leave the mesh unchanged
            report.ok = false;
            report.error = "Input mesh has non-finite positions";
            return report;
        }
    }

    auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    const size_t nVerts = pos.size();
    const size_t nFaces = topo.faceCount();

    if (nFaces == 0 || nVerts == 0) {
        report.ok = true;
        return report;
    }

    for (size_t fi = 0; fi < nFaces; ++fi) {
        if (isDegenerate(topo.face(fi), pos)) {
            ++report.degenerateFaces;
        }
    }

    std::map<uint64_t, uint32_t> edgeFaceCount;
    for (size_t fi = 0; fi < nFaces; ++fi) {
        const auto& f = topo.face(fi);
        if (!f.indicesInBounds(nVerts)) continue;
        for (size_t vi = 0; vi < f.indices.size(); ++vi) {
            uint32_t v0 = f.indices[vi];
            uint32_t v1 = f.indices[(vi + 1) % f.indices.size()];
            uint32_t lo = std::min(v0, v1);
            uint32_t hi = std::max(v0, v1);
            uint64_t key = (static_cast<uint64_t>(hi) << 32) | lo;
            ++edgeFaceCount[key];
        }
    }
    for (auto& [key, count] : edgeFaceCount) {
        if (count > 2) ++report.nonManifoldEdges;
    }

    if (opts.fillHoles) {
        auto hemOpt = HalfEdgeMesh::fromMesh(mesh);
        if (!hemOpt) {
            report.error = "Failed to build half-edge mesh for hole detection";
            return report;
        }

        auto& hem = *hemOpt;
        auto loops = hem.boundaryLoops();

        if (loops.empty()) {
            report.ok = true;
            return report;
        }

        auto positions = hem.positions();

        uint32_t nextVertexBase = static_cast<uint32_t>(positions.size());
        std::vector<nexus::render::Vec3> newVerts;

        for (auto& loop : loops) {
            if (loop.size() > opts.maxHoleEdges || loop.size() < 3) continue;

            nexus::render::Vec3 centroid{0, 0, 0};
            for (uint32_t vi : loop) {
                const auto& v = positions[vi];
                centroid.x += v.x;
                centroid.y += v.y;
                centroid.z += v.z;
            }
            float inv = 1.0f / static_cast<float>(loop.size());
            centroid.x *= inv;
            centroid.y *= inv;
            centroid.z *= inv;

            uint32_t cidx = nextVertexBase + static_cast<uint32_t>(newVerts.size());
            newVerts.push_back(centroid);

            for (size_t i = 0; i < loop.size(); ++i) {
                uint32_t v0 = loop[i];
                uint32_t v1 = loop[(i + 1) % loop.size()];
                Face f;
                f.indices = {v0, v1, cidx};
                topo.addFace(std::move(f));
            }

            ++report.holesFilled;
        }

        auto& attrs = mesh.attributes();
        auto existingPos = attrs.positions();
        existingPos.insert(existingPos.end(), newVerts.begin(), newVerts.end());
        attrs.setPositions(std::move(existingPos));
    }

    if (opts.resolveNonManifold) {
    }

    report.ok = true;
    return report;
}

} // namespace nexus::geometry
