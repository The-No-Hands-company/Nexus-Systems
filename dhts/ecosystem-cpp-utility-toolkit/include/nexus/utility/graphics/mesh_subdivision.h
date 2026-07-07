#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <utility>
#include <unordered_map>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Subdivision surfaces: Catmull-Clark (polygon meshes) and Loop
/// (triangle meshes).
///
/// Catmull-Clark computes a face point per face, an edge point per edge (blended
/// from endpoints and adjacent face points), repositions the original vertices,
/// and emits a quad per (face, corner). Loop inserts an edge vertex per edge
/// using 3/8-1/8 weights, repositions the originals with valence-dependent Loop
/// weights, and splits every triangle into four. Boundary edges/vertices use the
/// standard crease rules so open meshes remain well-behaved.
class MeshSubdivision {
public:
    using Vector3 = nexus::utility::math::Vector3;

    enum Method { CatmullClark, Loop };

    struct SubdivResult {
        std::vector<Vector3> verts;
        std::vector<unsigned> indices;
    };

    // ── Catmull-Clark ───────────────────────────────────────────────────

    /// @brief One Catmull-Clark step. Input faces are flat groups of
    /// `verticesPerFace` indices (3 = triangles, 4 = quads). Output is quads.
    SubdivResult catmullClark(const std::vector<Vector3>& verts,
                              const std::vector<unsigned>& indices,
                              unsigned verticesPerFace = 3) {
        std::vector<std::vector<unsigned>> faces =
            chunkFaces(indices, verticesPerFace);
        std::vector<Vector3> outVerts;
        std::vector<std::vector<unsigned>> outFaces;
        catmullClarkFaces(verts, faces, outVerts, outFaces);

        result_.verts = std::move(outVerts);
        result_.indices = flatten(outFaces);
        lastValence_ = 4;
        return result_;
    }

    // ── Loop ────────────────────────────────────────────────────────────

    /// @brief One Loop subdivision step on a triangle mesh (flat triples).
    SubdivResult loopSubdivision(const std::vector<Vector3>& verts,
                                 const std::vector<unsigned>& indices) {
        std::vector<Vector3> outVerts;
        std::vector<unsigned> outIndices;
        loopStep(verts, indices, outVerts, outIndices);

        result_.verts = std::move(outVerts);
        result_.indices = std::move(outIndices);
        lastValence_ = 3;
        return result_;
    }

    // ── Driver ──────────────────────────────────────────────────────────

    /// @brief Apply `iterations` rounds of the chosen scheme.
    SubdivResult subdivide(const std::vector<Vector3>& verts,
                           const std::vector<unsigned>& indices,
                           Method method, int iterations,
                           unsigned verticesPerFace = 3) {
        if (method == Loop) {
            std::vector<Vector3> v = verts;
            std::vector<unsigned> idx = indices;
            for (int it = 0; it < iterations; ++it) {
                std::vector<Vector3> nv;
                std::vector<unsigned> ni;
                loopStep(v, idx, nv, ni);
                v = std::move(nv);
                idx = std::move(ni);
            }
            result_.verts = std::move(v);
            result_.indices = std::move(idx);
            lastValence_ = 3;
            return result_;
        }

        std::vector<Vector3> v = verts;
        std::vector<std::vector<unsigned>> faces = chunkFaces(indices, verticesPerFace);
        for (int it = 0; it < iterations; ++it) {
            std::vector<Vector3> nv;
            std::vector<std::vector<unsigned>> nf;
            catmullClarkFaces(v, faces, nv, nf);
            v = std::move(nv);
            faces = std::move(nf);
        }
        result_.verts = std::move(v);
        result_.indices = flatten(faces);
        lastValence_ = (iterations > 0) ? 4u : verticesPerFace;
        return result_;
    }

    // ── Queries ─────────────────────────────────────────────────────────

    size_t getVertexCount() const { return result_.verts.size(); }
    size_t getFaceCount() const {
        return lastValence_ ? result_.indices.size() / lastValence_ : 0;
    }

private:
    SubdivResult result_;
    unsigned lastValence_ = 3;

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static uint64_t edgeKey(unsigned a, unsigned b) {
        if (a > b) std::swap(a, b);
        return (static_cast<uint64_t>(a) << 32) | static_cast<uint64_t>(b);
    }

    static std::vector<std::vector<unsigned>> chunkFaces(
            const std::vector<unsigned>& indices, unsigned k) {
        std::vector<std::vector<unsigned>> faces;
        if (k < 3) k = 3;
        faces.reserve(indices.size() / k);
        for (size_t i = 0; i + k <= indices.size(); i += k) {
            std::vector<unsigned> f(indices.begin() + i, indices.begin() + i + k);
            faces.push_back(std::move(f));
        }
        return faces;
    }

    static std::vector<unsigned> flatten(const std::vector<std::vector<unsigned>>& faces) {
        std::vector<unsigned> out;
        for (const auto& f : faces)
            for (unsigned v : f) out.push_back(v);
        return out;
    }

    // ── Catmull-Clark core ──────────────────────────────────────────────

    static void catmullClarkFaces(const std::vector<Vector3>& verts,
                                  const std::vector<std::vector<unsigned>>& faces,
                                  std::vector<Vector3>& outVerts,
                                  std::vector<std::vector<unsigned>>& outFaces) {
        const size_t V = verts.size();
        const size_t F = faces.size();

        // Face points.
        std::vector<Vector3> facePoints(F);
        for (size_t f = 0; f < F; ++f) {
            Vector3 sum;
            for (unsigned vi : faces[f]) sum += verts[vi];
            facePoints[f] = sum / static_cast<float>(faces[f].size());
        }

        // Edges: unique id, endpoints, adjacent faces.
        struct EdgeInfo {
            unsigned v0 = 0, v1 = 0;
            int faceCount = 0;
            int f0 = -1, f1 = -1;
        };
        std::unordered_map<uint64_t, unsigned> edgeId;
        std::vector<EdgeInfo> edges;
        auto getEdge = [&](unsigned a, unsigned b, int face) -> unsigned {
            uint64_t k = edgeKey(a, b);
            auto it = edgeId.find(k);
            unsigned id;
            if (it == edgeId.end()) {
                id = static_cast<unsigned>(edges.size());
                edgeId.emplace(k, id);
                EdgeInfo e;
                e.v0 = a; e.v1 = b;
                edges.push_back(e);
            } else {
                id = it->second;
            }
            EdgeInfo& e = edges[id];
            ++e.faceCount;
            if (e.f0 < 0) e.f0 = face; else e.f1 = face;
            return id;
        };

        std::vector<std::vector<unsigned>> faceEdges(F);
        for (size_t f = 0; f < F; ++f) {
            const auto& vs = faces[f];
            size_t m = vs.size();
            faceEdges[f].resize(m);
            for (size_t i = 0; i < m; ++i)
                faceEdges[f][i] = getEdge(vs[i], vs[(i + 1) % m], static_cast<int>(f));
        }
        const size_t E = edges.size();

        // Edge points.
        std::vector<Vector3> edgePoints(E);
        std::vector<bool> edgeBoundary(E, false);
        for (size_t e = 0; e < E; ++e) {
            const EdgeInfo& info = edges[e];
            Vector3 mid = (verts[info.v0] + verts[info.v1]) * 0.5f;
            if (info.faceCount >= 2) {
                Vector3 fp = facePoints[info.f0] + facePoints[info.f1];
                edgePoints[e] = (verts[info.v0] + verts[info.v1] + fp) * 0.25f;
            } else {
                edgePoints[e] = mid;
                edgeBoundary[e] = true;
            }
        }

        // Per-vertex accumulation for repositioning.
        std::vector<Vector3> faceSum(V);
        std::vector<int> faceN(V, 0);
        std::vector<Vector3> edgeMidSum(V);
        std::vector<int> edgeN(V, 0);
        std::vector<bool> vertBoundary(V, false);
        std::vector<std::array<unsigned, 2>> boundaryNbr(V, {UINT32_MAX, UINT32_MAX});

        for (size_t f = 0; f < F; ++f)
            for (unsigned vi : faces[f]) {
                faceSum[vi] += facePoints[f];
                ++faceN[vi];
            }

        for (size_t e = 0; e < E; ++e) {
            const EdgeInfo& info = edges[e];
            Vector3 mid = (verts[info.v0] + verts[info.v1]) * 0.5f;
            edgeMidSum[info.v0] += mid; ++edgeN[info.v0];
            edgeMidSum[info.v1] += mid; ++edgeN[info.v1];
            if (edgeBoundary[e]) {
                addBoundaryNeighbor(vertBoundary, boundaryNbr, info.v0, info.v1);
                addBoundaryNeighbor(vertBoundary, boundaryNbr, info.v1, info.v0);
            }
        }

        // Repositioned original vertices.
        std::vector<Vector3> newOrig(V);
        for (size_t v = 0; v < V; ++v) {
            const Vector3& P = verts[v];
            if (vertBoundary[v]) {
                unsigned n0 = boundaryNbr[v][0];
                unsigned n1 = boundaryNbr[v][1];
                if (n0 != UINT32_MAX && n1 != UINT32_MAX)
                    newOrig[v] = P * 0.75f + (verts[n0] + verts[n1]) * 0.125f;
                else
                    newOrig[v] = P;
            } else if (edgeN[v] > 0 && faceN[v] > 0) {
                float n = static_cast<float>(edgeN[v]);
                Vector3 F_avg = faceSum[v] / static_cast<float>(faceN[v]);
                Vector3 R_avg = edgeMidSum[v] / static_cast<float>(edgeN[v]);
                newOrig[v] = (F_avg + R_avg * 2.0f + P * (n - 3.0f)) / n;
            } else {
                newOrig[v] = P;
            }
        }

        // Assemble output vertices: [orig][facePoints][edgePoints].
        outVerts.clear();
        outVerts.reserve(V + F + E);
        for (size_t v = 0; v < V; ++v) outVerts.push_back(newOrig[v]);
        unsigned faceBase = static_cast<unsigned>(V);
        for (size_t f = 0; f < F; ++f) outVerts.push_back(facePoints[f]);
        unsigned edgeBase = faceBase + static_cast<unsigned>(F);
        for (size_t e = 0; e < E; ++e) outVerts.push_back(edgePoints[e]);

        // Emit one quad per (face, corner).
        outFaces.clear();
        for (size_t f = 0; f < F; ++f) {
            const auto& vs = faces[f];
            size_t m = vs.size();
            unsigned fp = faceBase + static_cast<unsigned>(f);
            for (size_t i = 0; i < m; ++i) {
                unsigned cur = vs[i];
                unsigned eNext = edgeBase + faceEdges[f][i];
                unsigned ePrev = edgeBase + faceEdges[f][(i + m - 1) % m];
                outFaces.push_back({cur, eNext, fp, ePrev});
            }
        }
    }

    static void addBoundaryNeighbor(std::vector<bool>& isB,
                                    std::vector<std::array<unsigned, 2>>& nbr,
                                    unsigned v, unsigned other) {
        isB[v] = true;
        if (nbr[v][0] == UINT32_MAX) nbr[v][0] = other;
        else if (nbr[v][1] == UINT32_MAX && other != nbr[v][0]) nbr[v][1] = other;
    }

    // ── Loop core ───────────────────────────────────────────────────────

    static void loopStep(const std::vector<Vector3>& verts,
                         const std::vector<unsigned>& indices,
                         std::vector<Vector3>& outVerts,
                         std::vector<unsigned>& outIndices) {
        const size_t V = verts.size();
        const size_t triCount = indices.size() / 3;

        struct EdgeInfo {
            unsigned v0 = 0, v1 = 0;
            int faceCount = 0;
            int opp0 = -1, opp1 = -1;  // opposite vertices of adjacent triangles
        };
        std::unordered_map<uint64_t, unsigned> edgeId;
        std::vector<EdgeInfo> edges;
        auto getEdge = [&](unsigned a, unsigned b, unsigned opp) -> unsigned {
            uint64_t k = edgeKey(a, b);
            auto it = edgeId.find(k);
            unsigned id;
            if (it == edgeId.end()) {
                id = static_cast<unsigned>(edges.size());
                edgeId.emplace(k, id);
                EdgeInfo e;
                e.v0 = a; e.v1 = b;
                edges.push_back(e);
            } else {
                id = it->second;
            }
            EdgeInfo& e = edges[id];
            if (e.faceCount == 0) e.opp0 = static_cast<int>(opp);
            else e.opp1 = static_cast<int>(opp);
            ++e.faceCount;
            return id;
        };

        std::vector<std::array<unsigned, 3>> triEdges(triCount);
        for (size_t t = 0; t < triCount; ++t) {
            unsigned a = indices[t * 3 + 0];
            unsigned b = indices[t * 3 + 1];
            unsigned c = indices[t * 3 + 2];
            triEdges[t][0] = getEdge(a, b, c);
            triEdges[t][1] = getEdge(b, c, a);
            triEdges[t][2] = getEdge(c, a, b);
        }
        const size_t E = edges.size();

        // Vertex adjacency & boundary detection.
        std::vector<std::vector<unsigned>> neighbors(V);
        std::vector<bool> vertBoundary(V, false);
        std::vector<std::array<unsigned, 2>> boundaryNbr(V, {UINT32_MAX, UINT32_MAX});
        for (size_t e = 0; e < E; ++e) {
            const EdgeInfo& info = edges[e];
            neighbors[info.v0].push_back(info.v1);
            neighbors[info.v1].push_back(info.v0);
            if (info.faceCount == 1) {
                addBoundaryNeighbor(vertBoundary, boundaryNbr, info.v0, info.v1);
                addBoundaryNeighbor(vertBoundary, boundaryNbr, info.v1, info.v0);
            }
        }

        // Edge points.
        std::vector<Vector3> edgePoints(E);
        for (size_t e = 0; e < E; ++e) {
            const EdgeInfo& info = edges[e];
            const Vector3& a = verts[info.v0];
            const Vector3& b = verts[info.v1];
            if (info.faceCount >= 2 && info.opp0 >= 0 && info.opp1 >= 0) {
                const Vector3& c = verts[info.opp0];
                const Vector3& d = verts[info.opp1];
                edgePoints[e] = (a + b) * (3.0f / 8.0f) + (c + d) * (1.0f / 8.0f);
            } else {
                edgePoints[e] = (a + b) * 0.5f;
            }
        }

        // Repositioned original vertices.
        std::vector<Vector3> newOrig(V);
        for (size_t v = 0; v < V; ++v) {
            const Vector3& P = verts[v];
            if (vertBoundary[v]) {
                unsigned n0 = boundaryNbr[v][0];
                unsigned n1 = boundaryNbr[v][1];
                if (n0 != UINT32_MAX && n1 != UINT32_MAX)
                    newOrig[v] = P * 0.75f + (verts[n0] + verts[n1]) * 0.125f;
                else
                    newOrig[v] = P;
            } else {
                int n = static_cast<int>(neighbors[v].size());
                if (n == 0) { newOrig[v] = P; continue; }
                float fn = static_cast<float>(n);
                float beta = (n == 3) ? (3.0f / 16.0f) : (3.0f / (8.0f * fn));
                Vector3 sum;
                for (unsigned nb : neighbors[v]) sum += verts[nb];
                newOrig[v] = P * (1.0f - fn * beta) + sum * beta;
            }
        }

        // Assemble output vertices: [orig][edgePoints].
        outVerts.clear();
        outVerts.reserve(V + E);
        for (size_t v = 0; v < V; ++v) outVerts.push_back(newOrig[v]);
        unsigned edgeBase = static_cast<unsigned>(V);
        for (size_t e = 0; e < E; ++e) outVerts.push_back(edgePoints[e]);

        // Split each triangle into four.
        outIndices.clear();
        outIndices.reserve(indices.size() * 4);
        for (size_t t = 0; t < triCount; ++t) {
            unsigned a = indices[t * 3 + 0];
            unsigned b = indices[t * 3 + 1];
            unsigned c = indices[t * 3 + 2];
            unsigned eab = edgeBase + triEdges[t][0];
            unsigned ebc = edgeBase + triEdges[t][1];
            unsigned eca = edgeBase + triEdges[t][2];
            pushTri(outIndices, a, eab, eca);
            pushTri(outIndices, eab, b, ebc);
            pushTri(outIndices, eca, ebc, c);
            pushTri(outIndices, eab, ebc, eca);
        }
    }

    static void pushTri(std::vector<unsigned>& out, unsigned a, unsigned b, unsigned c) {
        out.push_back(a);
        out.push_back(b);
        out.push_back(c);
    }
};

} // namespace nexus::utility::graphics
