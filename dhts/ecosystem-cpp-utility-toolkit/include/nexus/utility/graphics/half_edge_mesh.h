#pragma once

#include <vector>
#include <unordered_map>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Half-edge data structure for mesh traversal and topology operations.
/// Provides O(1) access to adjacent vertices, edges, and faces.
/// Essential for edge collapse, subdivision, and mesh repair.

class HalfEdgeMesh {
public:
    using Vector3 = nexus::utility::math::Vector3;

    struct Vertex { uint32_t he; Vector3 pos; };
    struct HalfEdge { uint32_t vert; uint32_t pair; uint32_t face; uint32_t next; uint32_t prev; };
    struct Face { uint32_t he; };

    /// @brief Add a triangle face. Returns face index.
    uint32_t addFace(const Vector3& v0, const Vector3& v1, const Vector3& v2) {
        uint32_t vi0 = addVertex(v0);
        uint32_t vi1 = addVertex(v1);
        uint32_t vi2 = addVertex(v2);

        uint32_t he0 = addHalfEdge(vi0, 0, 0);
        uint32_t he1 = addHalfEdge(vi1, 0, 0);
        uint32_t he2 = addHalfEdge(vi2, 0, 0);

        // Link next/prev
        hes_[he0].next = he1; hes_[he1].next = he2; hes_[he2].next = he0;
        hes_[he0].prev = he2; hes_[he1].prev = he0; hes_[he2].prev = he1;

        uint32_t fi = static_cast<uint32_t>(faces_.size());
        faces_.push_back({he0});

        for (uint32_t he : {he0, he1, he2}) hes_[he].face = fi;

        // Link pairs by searching for opposite edge
        linkPair(he0, vi1, vi0);
        linkPair(he1, vi2, vi1);
        linkPair(he2, vi0, vi2);

        return fi;
    }

    // ── Traversal ───────────────────────────────────────────────────────

    [[nodiscard]] std::vector<uint32_t> vertexOneRing(uint32_t vi) const {
        std::vector<uint32_t> neighbors;
        uint32_t he = verts_[vi].he;
        uint32_t cur = he;
        do {
            neighbors.push_back(hes_[cur].vert);
            cur = hes_[hes_[cur].pair].next;
        } while (cur != he && cur != 0);
        return neighbors;
    }

    [[nodiscard]] std::vector<uint32_t> faceVertices(uint32_t fi) const {
        std::vector<uint32_t> verts;
        uint32_t he = faces_[fi].he;
        uint32_t cur = he;
        do {
            verts.push_back(hes_[cur].vert);
            cur = hes_[cur].next;
        } while (cur != he);
        return verts;
    }

    [[nodiscard]] float edgeLength(uint32_t he) const {
        uint32_t v0 = hes_[hes_[he].prev].vert;
        uint32_t v1 = hes_[he].vert;
        auto d = verts_[v1].pos - verts_[v0].pos;
        return std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    }

    [[nodiscard]] bool isBoundary(uint32_t he) const { return hes_[he].pair == 0; }

    // ── Queries ─────────────────────────────────────────────────────────

    [[nodiscard]] size_t vertexCount() const { return verts_.size() - 1; }
    [[nodiscard]] size_t faceCount() const { return faces_.size(); }
    [[nodiscard]] size_t edgeCount() const { return (hes_.size() - 1) / 2; }

    [[nodiscard]] const Vertex& vertex(uint32_t i) const { return verts_[i]; }
    [[nodiscard]] const HalfEdge& halfEdge(uint32_t i) const { return hes_[i]; }
    [[nodiscard]] const Face& face(uint32_t i) const { return faces_[i]; }

    /// @brief Detect boundary (open) edges.
    [[nodiscard]] std::vector<uint32_t> boundaryEdges() const {
        std::vector<uint32_t> result;
        for (size_t i = 1; i < hes_.size(); ++i)
            if (hes_[i].pair == 0) result.push_back(static_cast<uint32_t>(i));
        return result;
    }

    /// @brief Count non-manifold vertices (valence != expected for boundary/interior).
    [[nodiscard]] size_t nonManifoldVertices() const {
        size_t count = 0;
        for (size_t i = 1; i < verts_.size(); ++i) {
            auto ring = vertexOneRing(static_cast<uint32_t>(i));
            // Simple heuristic: fewer than 2 neighbors is suspicious
            if (ring.size() < 2) ++count;
        }
        return count;
    }

    void clear() { verts_.clear(); hes_.clear(); faces_.clear(); edgeMap_.clear(); }

private:
    std::vector<Vertex> verts_{{}};   // 1-indexed for sentinel
    std::vector<HalfEdge> hes_{{}};   // 1-indexed
    std::vector<Face> faces_;
    std::unordered_map<uint64_t, uint32_t> edgeMap_;

    uint32_t addVertex(const Vector3& pos) {
        // Simple dedup by position (for demo; use spatial index in production)
        for (size_t i = 1; i < verts_.size(); ++i) {
            auto d = verts_[i].pos - pos;
            if (std::abs(d.x) < 1e-6f && std::abs(d.y) < 1e-6f && std::abs(d.z) < 1e-6f)
                return static_cast<uint32_t>(i);
        }
        uint32_t idx = static_cast<uint32_t>(verts_.size());
        verts_.push_back({0, pos});
        return idx;
    }

    uint32_t addHalfEdge(uint32_t vert, uint32_t pair, uint32_t face) {
        uint32_t idx = static_cast<uint32_t>(hes_.size());
        hes_.push_back({vert, pair, face, 0, 0});
        verts_[vert].he = idx;
        return idx;
    }

    uint64_t makeEdgeKey(uint32_t a, uint32_t b) {
        if (a > b) std::swap(a, b);
        return (static_cast<uint64_t>(a) << 32) | b;
    }

    void linkPair(uint32_t he, uint32_t from, uint32_t to) {
        uint64_t key = makeEdgeKey(from, to);
        auto it = edgeMap_.find(key);
        if (it != edgeMap_.end()) {
            hes_[he].pair = it->second;
            hes_[it->second].pair = he;
            edgeMap_.erase(it);
        } else {
            edgeMap_[key] = he;
        }
    }
};

} // namespace nexus::utility::graphics
