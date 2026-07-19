#include <nexus/geometry/MeshVertexWeld.h>

#include <nexus/geometry/Tolerance.h>  // geometry::isFinite (non-finite rejection convention)

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

struct UnionFind {
    std::vector<uint32_t> parent;
    std::vector<uint32_t> rank;

    explicit UnionFind(uint32_t n) : parent(n), rank(n, 0) {
        for (uint32_t i = 0; i < n; ++i) parent[i] = i;
    }

    uint32_t find(uint32_t x) {
        while (parent[x] != x) {
            parent[x] = parent[parent[x]];
            x = parent[x];
        }
        return x;
    }

    void unite(uint32_t a, uint32_t b) {
        a = find(a);
        b = find(b);
        if (a == b) return;
        if (rank[a] < rank[b]) parent[a] = b;
        else if (rank[a] > rank[b]) parent[b] = a;
        else { parent[b] = a; rank[a]++; }
    }
};

} // namespace

Mesh MeshVertexWeld::weld(const Mesh& mesh, const WeldOptions& opts) {
    Mesh result = mesh;
    if (!result.isValid()) return result;

    // Reject non-finite input rather than propagate NaN/±Inf into the welded output
    // (welding compares squared distances — undefined with NaN). Matches the kernel's
    // pervasive non-finite-rejection convention (e.g. robustMeshBoolean returns empty).
    for (const auto& p : mesh.attributes().positions()) {
        if (!isFinite(p)) return Mesh{};
    }

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();
    if (V < 2) return mesh;

    float invTol = 1.f / std::max(1e-8f, opts.tolerance);
    float eps = opts.tolerance * 0.5f;

    auto cellCoord = [invTol, eps](const Vec3& p) -> std::tuple<int64_t, int64_t, int64_t> {
        return {
            static_cast<int64_t>(std::floor((p.x + eps) * invTol)),
            static_cast<int64_t>(std::floor((p.y + eps) * invTol)),
            static_cast<int64_t>(std::floor((p.z + eps) * invTol)),
        };
    };

    auto cellKey = [](int64_t cx, int64_t cy, int64_t cz) -> uint64_t {
        uint64_t hx = static_cast<uint64_t>(cx ^ (cx >> 32)) * 0x9e3779b97f4a7c15ULL;
        uint64_t hy = static_cast<uint64_t>(cy ^ (cy >> 32)) * 0x9e3779b97f4a7c15ULL;
        uint64_t hz = static_cast<uint64_t>(cz ^ (cz >> 32)) * 0x9e3779b97f4a7c15ULL;
        return (hx >> 16) ^ (hy) ^ (hz << 16);
    };

    std::unordered_map<uint64_t, std::vector<uint32_t>> grid;
    for (uint32_t vi = 0; vi < static_cast<uint32_t>(V); ++vi) {
        auto [cx, cy, cz] = cellCoord(pos[vi]);
        grid[cellKey(cx, cy, cz)].push_back(vi);
    }

    UnionFind uf(static_cast<uint32_t>(V));
    float tolSq = opts.tolerance * opts.tolerance;

    for (const auto& [ck, verts] : grid) {
        for (size_t a = 0; a < verts.size(); ++a) {
            for (size_t b = a + 1; b < verts.size(); ++b) {
                uint32_t va = verts[a], vb = verts[b];
                Vec3 diff = pos[va] - pos[vb];
                if (diff.lengthSq() < tolSq) {
                    uf.unite(va, vb);
                }
            }
        }

        auto [cx, cy, cz] = cellCoord(pos[verts[0]]);
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    uint64_t nKey = cellKey(cx + dx, cy + dy, cz + dz);
                    auto nit = grid.find(nKey);
                    if (nit == grid.end()) continue;
                    for (uint32_t va : verts) {
                        for (uint32_t vb : nit->second) {
                            Vec3 diff = pos[va] - pos[vb];
                            if (diff.lengthSq() < tolSq) {
                                uf.unite(va, vb);
                            }
                        }
                    }
                }
            }
        }
    }

    std::unordered_map<uint32_t, uint32_t> repToNew;
    std::vector<uint32_t> tmpMap(V);
    uint32_t newCount = 0;
    for (uint32_t vi = 0; vi < static_cast<uint32_t>(V); ++vi) {
        uint32_t root = uf.find(vi);
        auto it = repToNew.find(root);
        if (it == repToNew.end()) {
            repToNew[root] = newCount;
            tmpMap[vi] = newCount;
            newCount++;
        } else {
            tmpMap[vi] = it->second;
        }
    }

    std::vector<Vec3> newPos(newCount);
    std::vector<Vec3> newNorms;
    std::vector<Vec2> newUVs;
    std::vector<Vec4> newTangents;
    if (mesh.attributes().hasNormals()) newNorms.resize(newCount);
    if (mesh.attributes().hasUVs()) newUVs.resize(newCount);
    if (mesh.attributes().hasTangents()) newTangents.resize(newCount);

    for (uint32_t vi = 0; vi < static_cast<uint32_t>(V); ++vi) {
        uint32_t nvi = tmpMap[vi];
        newPos[nvi] = pos[vi];
        if (!newNorms.empty()) newNorms[nvi] = mesh.attributes().normals()[vi];
        if (!newUVs.empty()) newUVs[nvi] = mesh.attributes().uvs()[vi];
        if (!newTangents.empty()) newTangents[nvi] = mesh.attributes().tangents()[vi];
    }

    MeshTopology newTopo;
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& f = topo.face(fi);
        Face nf;
        for (uint32_t vi : f.indices) {
            nf.indices.push_back(tmpMap[vi]);
        }
        newTopo.addFace(std::move(nf));
    }

    result = Mesh();
    auto& resultTopo = result.topology();
    auto& resultAttr = result.attributes();

    for (size_t fi = 0; fi < newTopo.faceCount(); ++fi) {
        resultTopo.addFace(Face(newTopo.face(fi)));
    }

    resultAttr.setPositions(std::move(newPos));
    if (!newNorms.empty()) resultAttr.setNormals(std::move(newNorms));
    if (!newUVs.empty()) resultAttr.setUVs(std::move(newUVs));
    if (!newTangents.empty()) resultAttr.setTangents(std::move(newTangents));

    return result;
}

size_t MeshVertexWeld::countUnique(const Mesh& mesh, float tolerance) {
    if (!mesh.isValid()) return 0;

    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();

    float invTol = 1.f / std::max(1e-8f, tolerance);
    float eps = tolerance * 0.5f;

    auto cellCoord = [invTol, eps](const Vec3& p) -> std::tuple<int64_t, int64_t, int64_t> {
        return {
            static_cast<int64_t>(std::floor((p.x + eps) * invTol)),
            static_cast<int64_t>(std::floor((p.y + eps) * invTol)),
            static_cast<int64_t>(std::floor((p.z + eps) * invTol)),
        };
    };

    auto cellKey = [](int64_t cx, int64_t cy, int64_t cz) -> uint64_t {
        uint64_t hx = static_cast<uint64_t>(cx ^ (cx >> 32)) * 0x9e3779b97f4a7c15ULL;
        uint64_t hy = static_cast<uint64_t>(cy ^ (cy >> 32)) * 0x9e3779b97f4a7c15ULL;
        uint64_t hz = static_cast<uint64_t>(cz ^ (cz >> 32)) * 0x9e3779b97f4a7c15ULL;
        return (hx >> 16) ^ hy ^ (hz << 16);
    };

    std::unordered_map<uint64_t, std::vector<Vec3>> grid;
    for (size_t vi = 0; vi < V; ++vi) {
        auto [cx, cy, cz] = cellCoord(pos[vi]);
        grid[cellKey(cx, cy, cz)].push_back(pos[vi]);
    }

    size_t uniqueCount = 0;
    float tolSq = tolerance * tolerance;

    for (auto& [ck, verts] : grid) {
        std::vector<Vec3> uniques;
        for (const Vec3& p : verts) {
            bool found = false;
            for (const Vec3& u : uniques) {
                Vec3 d = p - u;
                if (d.lengthSq() < tolSq) { found = true; break; }
            }
            if (!found) {
                uniques.push_back(p);
                uniqueCount++;
            }
        }
    }

    return uniqueCount;
}

} // namespace nexus::geometry
