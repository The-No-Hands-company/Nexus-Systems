#include <nexus/geometry/MeshLaplacian.h>
#include <nexus/geometry/Tolerance.h>  // geometry::isFinite (non-finite rejection convention)

#include <cmath>
#include <map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

static constexpr float kCotEpsilon = 1e-10f;

float cotan(const Vec3& a, const Vec3& b) {
    float dotAB = a.dot(b);
    float crossLen = a.cross(b).length();
    if (crossLen < kCotEpsilon) return 0.f;
    return dotAB / crossLen;
}

} // namespace

Mesh MeshLaplacian::smooth(const Mesh& mesh, const SmoothOptions& opts) {
    Mesh result = mesh;
    if (!result.isValid()) return result;
    for (const auto& p : mesh.attributes().positions())
        if (!isFinite(p)) return Mesh{};  // reject non-finite input (convention)

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();

    std::map<uint64_t, float> cotWeights;
    std::map<uint64_t, int> edgeFaceCount;
    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() != 3) continue;
        for (size_t vi = 0; vi < f.indices.size(); ++vi) {
            uint32_t v0 = f.indices[vi];
            uint32_t v1 = f.indices[(vi + 1) % f.indices.size()];
            uint32_t v2 = f.indices[(vi + 2) % f.indices.size()];
            Vec3 e0 = pos[v1] - pos[v0];
            Vec3 e1 = pos[v2] - pos[v0];
            float cot0 = cotan(e0 * -1.f, e1 * -1.f);
            uint64_t key12 = (static_cast<uint64_t>(std::min(v1, v2)) << 32) | std::max(v1, v2);
            cotWeights[key12] += cot0;
            ++edgeFaceCount[key12];
        }
    }

    std::vector<std::vector<std::pair<uint32_t, float>>> vertEdges(V);
    for (const auto& [key, w] : cotWeights) {
        uint32_t a = static_cast<uint32_t>(key >> 32);
        uint32_t b = static_cast<uint32_t>(key & 0xFFFFFFFF);
        vertEdges[a].emplace_back(b, w);
        vertEdges[b].emplace_back(a, w);
    }

    std::vector<bool> isBoundary(V, false);
    if (opts.fixBoundary) {
        for (size_t i = 0; i < V; ++i) {
            for (const auto& [j, w] : vertEdges[i]) {
                uint64_t key = (static_cast<uint64_t>(std::min(static_cast<uint32_t>(i), j)) << 32) | std::max(static_cast<uint32_t>(i), j);
                if (edgeFaceCount.count(key) && edgeFaceCount[key] == 1) {
                    isBoundary[i] = true;
                    break;
                }
            }
        }
    }

    std::vector<Vec3> curPos = pos;
    for (uint32_t iter = 0; iter < opts.iterations; ++iter) {
        std::vector<Vec3> nextPos = curPos;
        for (size_t i = 0; i < V; ++i) {
            if (vertEdges[i].empty()) continue;
            if (opts.fixBoundary && isBoundary[i]) continue;
            Vec3 weightedSum = {0, 0, 0};
            float totalW = 0.f;
            for (const auto& [j, w] : vertEdges[i]) {
                weightedSum = weightedSum + (curPos[j] - curPos[i]) * w;
                totalW += w;
            }
            if (totalW > kCotEpsilon) {
                nextPos[i] = curPos[i] + (weightedSum * (opts.lambda / totalW));
            }
        }
        curPos = std::move(nextPos);
    }

    result.attributes().setPositions(std::move(curPos));
    if (opts.recomputeNormals && mesh.attributes().hasNormals()) {
        if (!result.computeVertexNormals()) return mesh;
    }
    return result;
}

std::vector<float> MeshLaplacian::meanCurvature(const Mesh& mesh) {
    std::vector<float> result;
    if (!mesh.isValid()) return result;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();
    result.resize(V, 0.f);

    std::map<uint64_t, float> cotWeights;
    std::vector<float> vertexAreas(V, 0.f);

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const auto& f = topo.face(fi);
        if (f.indices.size() != 3) continue;
        uint32_t v0 = f.indices[0];
        uint32_t v1 = f.indices[1];
        uint32_t v2 = f.indices[2];
        Vec3 p0 = pos[v0], p1 = pos[v1], p2 = pos[v2];
        Vec3 e01 = p1 - p0;
        Vec3 e02 = p2 - p0;
        Vec3 e12 = p2 - p1;
        float triArea = e01.cross(e02).length() * 0.5f;

        bool obtuse0 = (e01.dot(e02) < 0.f);
        bool obtuse1 = ((p0 - p1).dot(e12) < 0.f);
        bool obtuse2 = ((p0 - p2).dot(p1 - p2) < 0.f);

        float e01sq = e01.dot(e01);
        float e02sq = e02.dot(e02);
        float e12sq = e12.dot(e12);

        if (obtuse0) {
            vertexAreas[v0] += triArea * 0.5f;
        } else if (obtuse1 || obtuse2) {
            vertexAreas[v0] += triArea * 0.25f;
        } else {
            float cot1 = cotan(p0 - p1, e12);
            float cot2 = cotan(p0 - p2, p1 - p2);
            vertexAreas[v0] += (e02sq * cot1 + e01sq * cot2) * 0.125f;
        }

        if (obtuse1) {
            vertexAreas[v1] += triArea * 0.5f;
        } else if (obtuse0 || obtuse2) {
            vertexAreas[v1] += triArea * 0.25f;
        } else {
            float cot0 = cotan(e01, e02);
            float cot2 = cotan(p0 - p2, p1 - p2);
            vertexAreas[v1] += (e12sq * cot0 + e01sq * cot2) * 0.125f;
        }

        if (obtuse2) {
            vertexAreas[v2] += triArea * 0.5f;
        } else if (obtuse0 || obtuse1) {
            vertexAreas[v2] += triArea * 0.25f;
        } else {
            float cot0 = cotan(e01, e02);
            float cot1 = cotan(p0 - p1, e12);
            vertexAreas[v2] += (e12sq * cot0 + e02sq * cot1) * 0.125f;
        }

        for (size_t vi = 0; vi < f.indices.size(); ++vi) {
            uint32_t vc0 = f.indices[vi];
            uint32_t vc1 = f.indices[(vi + 1) % f.indices.size()];
            uint32_t vc2 = f.indices[(vi + 2) % f.indices.size()];
            Vec3 ev1 = pos[vc1] - pos[vc0];
            Vec3 ev2 = pos[vc2] - pos[vc0];
            float cot0 = cotan(ev1 * -1.f, ev2 * -1.f);
            uint64_t key12 = (static_cast<uint64_t>(std::min(vc1, vc2)) << 32) | std::max(vc1, vc2);
            cotWeights[key12] += cot0;
        }
    }

    std::vector<Vec3> laplacians(V, {0, 0, 0});
    for (const auto& [key, w] : cotWeights) {
        uint32_t a = static_cast<uint32_t>(key >> 32);
        uint32_t b = static_cast<uint32_t>(key & 0xFFFFFFFF);
        Vec3 delta = (pos[b] - pos[a]) * w;
        laplacians[a] = laplacians[a] + delta;
        laplacians[b] = laplacians[b] + (delta * -1.f);
    }

    for (size_t i = 0; i < V; ++i) {
        if (vertexAreas[i] > kCotEpsilon) {
            result[i] = 0.5f * laplacians[i].length() / vertexAreas[i];
        }
    }

    return result;
}

} // namespace nexus::geometry
