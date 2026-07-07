#pragma once

#include <vector>
#include <array>
#include <queue>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Surface simplification using the Garland-Heckbert quadric error metric.
///
/// Each vertex accumulates a quadric (the sum of squared distances to the planes
/// of its adjacent faces). Edge collapses are ordered by the quadric error at the
/// optimal collapse position and applied greedily via a lazily-updated min-heap
/// until the target triangle count is reached. Collapses that would flip a face
/// normal are rejected to preserve mesh quality.
class MeshDecimator {
public:
    using Vector3 = nexus::utility::math::Vector3;

    /// @brief Symmetric 4x4 quadric matrix stored as its upper triangle.
    ///
    /// Layout (row-major upper triangle):
    ///   [ a0 a1 a2 a3 ]
    ///   [ a1 a4 a5 a6 ]
    ///   [ a2 a5 a7 a8 ]
    ///   [ a3 a6 a8 a9 ]
    struct QEM {
        float a[10];

        QEM() { for (int i = 0; i < 10; ++i) a[i] = 0.0f; }

        /// @brief Build a quadric from a plane (nx, ny, nz, d) with unit normal.
        static QEM fromPlane(float nx, float ny, float nz, float d) {
            QEM q;
            q.a[0] = nx * nx; q.a[1] = nx * ny; q.a[2] = nx * nz; q.a[3] = nx * d;
            q.a[4] = ny * ny; q.a[5] = ny * nz; q.a[6] = ny * d;
            q.a[7] = nz * nz; q.a[8] = nz * d;
            q.a[9] = d * d;
            return q;
        }

        QEM operator+(const QEM& o) const {
            QEM r;
            for (int i = 0; i < 10; ++i) r.a[i] = a[i] + o.a[i];
            return r;
        }
        QEM& operator+=(const QEM& o) {
            for (int i = 0; i < 10; ++i) a[i] += o.a[i];
            return *this;
        }
    };

    // ── Setup ───────────────────────────────────────────────────────────

    /// @brief Load an indexed triangle mesh to be simplified.
    void setMesh(const std::vector<Vector3>& vertices,
                 const std::vector<unsigned>& indices) {
        positions_ = vertices;
        faces_.clear();
        faceValid_.clear();
        const size_t triCount = indices.size() / 3;
        faces_.reserve(triCount);
        faceValid_.reserve(triCount);
        for (size_t t = 0; t < triCount; ++t) {
            int i0 = static_cast<int>(indices[t * 3 + 0]);
            int i1 = static_cast<int>(indices[t * 3 + 1]);
            int i2 = static_cast<int>(indices[t * 3 + 2]);
            if (i0 == i1 || i1 == i2 || i0 == i2) continue;
            faces_.push_back({i0, i1, i2});
            faceValid_.push_back(true);
        }
        validFaceCount_ = faces_.size();

        const size_t n = positions_.size();
        vertexValid_.assign(n, true);
        version_.assign(n, 0);
        vertexFaces_.assign(n, {});
        for (int f = 0; f < static_cast<int>(faces_.size()); ++f) {
            for (int i = 0; i < 3; ++i) vertexFaces_[faces_[f][i]].push_back(f);
        }

        quadrics_.assign(n, QEM());
        for (size_t v = 0; v < n; ++v) quadrics_[v] = computeQEM(static_cast<int>(v));
    }

    // ── Quadric operations ──────────────────────────────────────────────

    /// @brief Accumulate the quadric of all faces adjacent to a vertex.
    QEM computeQEM(int vertexIndex) const {
        QEM q;
        if (vertexIndex < 0 || vertexIndex >= static_cast<int>(vertexFaces_.size()))
            return q;
        for (int f : vertexFaces_[vertexIndex]) {
            if (!faceValid_[f]) continue;
            const auto& face = faces_[f];
            const Vector3& v0 = positions_[face[0]];
            const Vector3& v1 = positions_[face[1]];
            const Vector3& v2 = positions_[face[2]];
            Vector3 n = cross(v1 - v0, v2 - v0);
            float len = n.length();
            if (len < 1e-12f) continue;
            n = n / len;
            float d = -dot(n, v0);
            q += QEM::fromPlane(n.x, n.y, n.z, d);
        }
        return q;
    }

    /// @brief Evaluate the quadric error v^T Q v at a point.
    static float evaluate(const QEM& q, const Vector3& p) {
        const float x = p.x, y = p.y, z = p.z;
        return q.a[0] * x * x + 2.0f * q.a[1] * x * y + 2.0f * q.a[2] * x * z +
               2.0f * q.a[3] * x + q.a[4] * y * y + 2.0f * q.a[5] * y * z +
               2.0f * q.a[6] * y + q.a[7] * z * z + 2.0f * q.a[8] * z + q.a[9];
    }

    /// @brief Vertex position minimizing the quadric (solves the 3x3 system).
    ///
    /// Falls back to the origin when the quadric matrix is singular; use the
    /// internal solve for a midpoint fallback during collapse costing.
    static Vector3 optimalPoint(const QEM& q) {
        Vector3 out;
        if (solveOptimal(q, out)) return out;
        return Vector3();
    }

    // ── Simplification ──────────────────────────────────────────────────

    /// @brief Collapse edges greedily until at most targetFaces remain.
    void simplify(size_t targetFaces) {
        std::priority_queue<Edge, std::vector<Edge>, EdgeGreater> heap;

        // Seed the heap with every unique edge.
        std::unordered_set<uint64_t> seen;
        seen.reserve(faces_.size() * 3);
        for (int f = 0; f < static_cast<int>(faces_.size()); ++f) {
            if (!faceValid_[f]) continue;
            const auto& face = faces_[f];
            addEdgeKey(seen, heap, face[0], face[1]);
            addEdgeKey(seen, heap, face[1], face[2]);
            addEdgeKey(seen, heap, face[2], face[0]);
        }

        while (validFaceCount_ > targetFaces && !heap.empty()) {
            Edge e = heap.top();
            heap.pop();

            if (!vertexValid_[e.v0] || !vertexValid_[e.v1]) continue;
            if (e.ver0 != version_[e.v0] || e.ver1 != version_[e.v1]) continue;
            if (!shareFace(e.v0, e.v1)) continue;
            if (causesFlip(e.v0, e.v1, e.optimal)) continue;

            collapse(e.v0, e.v1, e.optimal, heap);
        }
    }

    // ── Queries / extraction ────────────────────────────────────────────

    size_t getVertexCount() const {
        size_t count = 0;
        for (bool v : vertexValid_) if (v) ++count;
        return count;
    }

    size_t getFaceCount() const { return validFaceCount_; }

    /// @brief Extract the simplified mesh with compacted vertex indices.
    void getMesh(std::vector<Vector3>& outVertices,
                 std::vector<unsigned>& outIndices) const {
        outVertices.clear();
        outIndices.clear();
        std::vector<int> remap(positions_.size(), -1);
        for (int v = 0; v < static_cast<int>(positions_.size()); ++v) {
            if (!vertexValid_[v]) continue;
            remap[v] = static_cast<int>(outVertices.size());
            outVertices.push_back(positions_[v]);
        }
        for (int f = 0; f < static_cast<int>(faces_.size()); ++f) {
            if (!faceValid_[f]) continue;
            const auto& face = faces_[f];
            outIndices.push_back(static_cast<unsigned>(remap[face[0]]));
            outIndices.push_back(static_cast<unsigned>(remap[face[1]]));
            outIndices.push_back(static_cast<unsigned>(remap[face[2]]));
        }
    }

private:
    struct Edge {
        float cost;
        int v0, v1;
        int ver0, ver1;
        Vector3 optimal;
    };
    struct EdgeGreater {
        bool operator()(const Edge& a, const Edge& b) const { return a.cost > b.cost; }
    };

    std::vector<Vector3> positions_;
    std::vector<QEM> quadrics_;
    std::vector<std::array<int, 3>> faces_;
    std::vector<char> faceValid_;
    std::vector<char> vertexValid_;
    std::vector<int> version_;
    std::vector<std::vector<int>> vertexFaces_;
    size_t validFaceCount_ = 0;

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }

    static uint64_t edgeKey(int a, int b) {
        if (a > b) std::swap(a, b);
        return (static_cast<uint64_t>(static_cast<uint32_t>(a)) << 32) |
               static_cast<uint32_t>(b);
    }

    static bool solveOptimal(const QEM& q, Vector3& out) {
        // Solve A x = -b where A is the 3x3 upper-left block, b the (a3,a6,a8).
        const float m00 = q.a[0], m01 = q.a[1], m02 = q.a[2];
        const float m11 = q.a[4], m12 = q.a[5];
        const float m22 = q.a[7];
        const float det =
            m00 * (m11 * m22 - m12 * m12) -
            m01 * (m01 * m22 - m12 * m02) +
            m02 * (m01 * m12 - m11 * m02);
        if (std::abs(det) < 1e-10f) return false;
        const float invDet = 1.0f / det;
        // Cofactor inverse of the symmetric matrix.
        const float c00 = (m11 * m22 - m12 * m12);
        const float c01 = -(m01 * m22 - m12 * m02);
        const float c02 = (m01 * m12 - m11 * m02);
        const float c11 = (m00 * m22 - m02 * m02);
        const float c12 = -(m00 * m12 - m01 * m02);
        const float c22 = (m00 * m11 - m01 * m01);
        const float bx = -q.a[3], by = -q.a[6], bz = -q.a[8];
        out.x = (c00 * bx + c01 * by + c02 * bz) * invDet;
        out.y = (c01 * bx + c11 * by + c12 * bz) * invDet;
        out.z = (c02 * bx + c12 * by + c22 * bz) * invDet;
        return true;
    }

    void addEdgeKey(std::unordered_set<uint64_t>& seen,
                    std::priority_queue<Edge, std::vector<Edge>, EdgeGreater>& heap,
                    int a, int b) {
        uint64_t key = edgeKey(a, b);
        if (!seen.insert(key).second) return;
        heap.push(makeEdge(a, b));
    }

    Edge makeEdge(int a, int b) const {
        QEM q = quadrics_[a] + quadrics_[b];
        Vector3 opt;
        if (!solveOptimal(q, opt)) opt = (positions_[a] + positions_[b]) * 0.5f;
        Edge e;
        e.v0 = a; e.v1 = b;
        e.ver0 = version_[a]; e.ver1 = version_[b];
        e.optimal = opt;
        e.cost = evaluate(q, opt);
        return e;
    }

    bool shareFace(int a, int b) const {
        for (int f : vertexFaces_[a]) {
            if (!faceValid_[f]) continue;
            const auto& face = faces_[f];
            if (face[0] == b || face[1] == b || face[2] == b) return true;
        }
        return false;
    }

    bool causesFlip(int v0, int v1, const Vector3& newPos) const {
        for (int vv : {v0, v1}) {
            for (int f : vertexFaces_[vv]) {
                if (!faceValid_[f]) continue;
                const auto& face = faces_[f];
                bool hasV0 = face[0] == v0 || face[1] == v0 || face[2] == v0;
                bool hasV1 = face[0] == v1 || face[1] == v1 || face[2] == v1;
                if (hasV0 && hasV1) continue; // face removed by the collapse
                Vector3 p[3];
                for (int i = 0; i < 3; ++i) {
                    int idx = face[i];
                    p[i] = (idx == v0 || idx == v1) ? newPos : positions_[idx];
                }
                Vector3 oldN = cross(positions_[face[1]] - positions_[face[0]],
                                     positions_[face[2]] - positions_[face[0]]);
                Vector3 newN = cross(p[1] - p[0], p[2] - p[0]);
                if (newN.lengthSquared() < 1e-16f) return true; // degenerate
                if (dot(oldN, newN) < 0.0f) return true;         // flipped
            }
        }
        return false;
    }

    void collapse(int v0, int v1, const Vector3& newPos,
                  std::priority_queue<Edge, std::vector<Edge>, EdgeGreater>& heap) {
        positions_[v0] = newPos;
        quadrics_[v0] += quadrics_[v1];

        // Redirect v1's incident faces onto v0.
        for (int f : vertexFaces_[v1]) {
            if (!faceValid_[f]) continue;
            auto& face = faces_[f];
            for (int i = 0; i < 3; ++i) if (face[i] == v1) face[i] = v0;
            if (face[0] == face[1] || face[1] == face[2] || face[0] == face[2]) {
                faceValid_[f] = false;
                --validFaceCount_;
            } else {
                vertexFaces_[v0].push_back(f);
            }
        }
        vertexValid_[v1] = false;
        vertexFaces_[v1].clear();

        // Compact v0's face list (drop invalid + duplicates).
        std::vector<int> compact;
        compact.reserve(vertexFaces_[v0].size());
        std::unordered_set<int> uniq;
        for (int f : vertexFaces_[v0]) {
            if (!faceValid_[f]) continue;
            if (uniq.insert(f).second) compact.push_back(f);
        }
        vertexFaces_[v0] = std::move(compact);

        // Gather the one-ring neighbors of v0.
        std::unordered_set<int> neighbors;
        for (int f : vertexFaces_[v0]) {
            const auto& face = faces_[f];
            for (int i = 0; i < 3; ++i)
                if (face[i] != v0) neighbors.insert(face[i]);
        }

        // Bump versions so stale heap entries are discarded.
        ++version_[v0];
        for (int n : neighbors) ++version_[n];

        // Refresh incident edge costs.
        for (int n : neighbors) heap.push(makeEdge(v0, n));
    }
};

} // namespace nexus::utility::graphics
