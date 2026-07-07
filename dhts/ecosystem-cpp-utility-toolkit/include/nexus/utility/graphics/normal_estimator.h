#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <queue>
#include <utility>
#include <functional>
#include <algorithm>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Estimation and processing of surface normals for meshes and point clouds.
///
/// Point-cloud normals are estimated by principal component analysis of each
/// k-nearest-neighbour patch (the eigenvector of the smallest eigenvalue of the
/// local covariance). Orientation is made globally consistent with Hoppe's
/// minimum-spanning-tree propagation. For meshes, per-face normals are exact
/// cross products and per-vertex normals are area-weighted averages. Smoothing
/// utilities include uniform Laplacian, the Taubin λ|μ non-shrinking filter, and
/// direct Laplacian smoothing of a normal field.
class NormalEstimator {
public:
    using Vector3 = nexus::utility::math::Vector3;

    struct NormalResult {
        std::vector<Vector3> normals;
        std::vector<float> confidence;
    };

    // ── Point-cloud normal estimation ───────────────────────────────────

    /// @brief Estimate normals from a raw point cloud via local-patch PCA.
    static NormalResult estimateFromPoints(const std::vector<Vector3>& points,
                                           int kNeighbors) {
        NormalResult result;
        const int n = static_cast<int>(points.size());
        result.normals.assign(n, Vector3(0, 0, 1));
        result.confidence.assign(n, 0.0f);
        if (n == 0) return result;
        int k = std::max(3, std::min(kNeighbors, n));

        for (int i = 0; i < n; ++i) {
            std::vector<int> nbr = kNearest(points, i, k);
            Vector3 centroid;
            for (int idx : nbr) centroid += points[idx];
            centroid = centroid / static_cast<float>(nbr.size());

            float cov[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
            for (int idx : nbr) {
                Vector3 d = points[idx] - centroid;
                cov[0][0] += d.x * d.x; cov[0][1] += d.x * d.y; cov[0][2] += d.x * d.z;
                cov[1][1] += d.y * d.y; cov[1][2] += d.y * d.z; cov[2][2] += d.z * d.z;
            }
            cov[1][0] = cov[0][1]; cov[2][0] = cov[0][2]; cov[2][1] = cov[1][2];

            float eval[3];
            Vector3 evec[3];
            jacobiEigen(cov, eval, evec);
            // eval sorted ascending: evec[0] = smallest eigenvalue = normal.
            result.normals[i] = evec[0].normalized();
            float sum = eval[0] + eval[1] + eval[2];
            result.confidence[i] =
                (sum > 1e-12f) ? (eval[1] - eval[0]) / (eval[2] + 1e-12f) : 0.0f;
            result.confidence[i] = std::clamp(result.confidence[i], 0.0f, 1.0f);
        }
        return result;
    }

    /// @brief Estimate and globally orient point-cloud normals (Hoppe MST).
    static NormalResult autoOrientNormals(const std::vector<Vector3>& points,
                                          int kNeighbors) {
        NormalResult result = estimateFromPoints(points, kNeighbors);
        const int n = static_cast<int>(points.size());
        if (n == 0) return result;
        int k = std::max(3, std::min(kNeighbors, n));

        std::vector<std::vector<int>> adj(n);
        for (int i = 0; i < n; ++i) adj[i] = kNearest(points, i, k);

        // Seed: the extreme (max-Z) point, oriented to face +Z.
        int seed = 0;
        for (int i = 1; i < n; ++i)
            if (points[i].z > points[seed].z) seed = i;
        if (result.normals[seed].z < 0.0f) result.normals[seed] = result.normals[seed] * -1.0f;

        std::vector<uint8_t> visited(n, 0);
        struct Edge {
            float cost;
            int from, to;
            bool operator>(const Edge& o) const { return cost > o.cost; }
        };
        std::priority_queue<Edge, std::vector<Edge>, std::greater<Edge>> pq;
        auto pushEdges = [&](int u) {
            for (int v : adj[u]) {
                if (visited[v]) continue;
                float c = 1.0f - std::abs(dot(result.normals[u], result.normals[v]));
                pq.push({c, u, v});
            }
        };
        visited[seed] = 1;
        pushEdges(seed);

        int processed = 1;
        while (!pq.empty() && processed < n) {
            Edge e = pq.top();
            pq.pop();
            if (visited[e.to]) continue;
            visited[e.to] = 1;
            ++processed;
            if (dot(result.normals[e.from], result.normals[e.to]) < 0.0f)
                result.normals[e.to] = result.normals[e.to] * -1.0f;
            pushEdges(e.to);
        }
        return result;
    }

    // ── Mesh normals ────────────────────────────────────────────────────

    /// @brief Per-face normals (normalized cross products).
    static std::vector<Vector3> computeFaceNormals(const std::vector<Vector3>& verts,
                                                   const std::vector<unsigned>& indices) {
        std::vector<Vector3> normals(indices.size() / 3);
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            const Vector3& a = verts[indices[t]];
            const Vector3& b = verts[indices[t + 1]];
            const Vector3& c = verts[indices[t + 2]];
            normals[t / 3] = cross(b - a, c - a).normalized();
        }
        return normals;
    }

    /// @brief Per-vertex normals as an area-weighted average of face normals.
    static std::vector<Vector3> computeVertexNormals(const std::vector<Vector3>& verts,
                                                     const std::vector<unsigned>& indices) {
        std::vector<Vector3> normals(verts.size(), Vector3());
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            unsigned ia = indices[t], ib = indices[t + 1], ic = indices[t + 2];
            Vector3 fn = cross(verts[ib] - verts[ia], verts[ic] - verts[ia]);  // area-weighted
            normals[ia] += fn;
            normals[ib] += fn;
            normals[ic] += fn;
        }
        for (auto& nrm : normals) nrm = nrm.normalized();
        return normals;
    }

    // ── Normal / position smoothing ─────────────────────────────────────

    /// @brief Laplacian smoothing of a per-vertex normal field.
    static std::vector<Vector3> smoothNormals(const std::vector<Vector3>& verts,
                                              const std::vector<unsigned>& indices,
                                              const std::vector<Vector3>& normals,
                                              int iterations) {
        std::vector<std::vector<int>> adj = buildAdjacency(verts.size(), indices);
        std::vector<Vector3> cur = normals;
        std::vector<Vector3> next(cur.size());
        for (int it = 0; it < iterations; ++it) {
            for (size_t i = 0; i < cur.size(); ++i) {
                Vector3 sum = cur[i];
                for (int nb : adj[i]) sum += cur[nb];
                next[i] = sum.normalized();
            }
            cur.swap(next);
        }
        return cur;
    }

    /// @brief Uniform Laplacian smoothing of vertex positions.
    static std::vector<Vector3> laplacianSmooth(const std::vector<Vector3>& verts,
                                               const std::vector<unsigned>& indices,
                                               int iterations, float factor) {
        std::vector<std::vector<int>> adj = buildAdjacency(verts.size(), indices);
        std::vector<Vector3> cur = verts;
        std::vector<Vector3> next(cur.size());
        for (int it = 0; it < iterations; ++it) {
            for (size_t i = 0; i < cur.size(); ++i) {
                if (adj[i].empty()) { next[i] = cur[i]; continue; }
                Vector3 avg;
                for (int nb : adj[i]) avg += cur[nb];
                avg = avg / static_cast<float>(adj[i].size());
                next[i] = cur[i] + (avg - cur[i]) * factor;
            }
            cur.swap(next);
        }
        return cur;
    }

    /// @brief Taubin λ|μ smoothing (shrink-free) of vertex positions.
    static std::vector<Vector3> taubinSmooth(const std::vector<Vector3>& verts,
                                             const std::vector<unsigned>& indices,
                                             float lambda, float mu, int iterations) {
        std::vector<std::vector<int>> adj = buildAdjacency(verts.size(), indices);
        std::vector<Vector3> cur = verts;
        for (int it = 0; it < iterations; ++it) {
            laplacianPass(cur, adj, lambda);
            laplacianPass(cur, adj, mu);
        }
        return cur;
    }

    /// @brief Flip normals so each faces towards `viewpoint`.
    static std::vector<Vector3> flipNormalsTowardsViewpoint(const std::vector<Vector3>& verts,
                                                           const std::vector<Vector3>& normals,
                                                           const Vector3& viewpoint) {
        std::vector<Vector3> out = normals;
        for (size_t i = 0; i < out.size() && i < verts.size(); ++i) {
            Vector3 toView = viewpoint - verts[i];
            if (dot(out[i], toView) < 0.0f) out[i] = out[i] * -1.0f;
        }
        return out;
    }

private:
    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }

    static std::vector<int> kNearest(const std::vector<Vector3>& pts, int query, int k) {
        const int n = static_cast<int>(pts.size());
        std::vector<std::pair<float, int>> dists;
        dists.reserve(n);
        for (int i = 0; i < n; ++i) {
            if (i == query) continue;
            dists.emplace_back((pts[i] - pts[query]).lengthSquared(), i);
        }
        int kk = std::min(k, static_cast<int>(dists.size()));
        std::partial_sort(dists.begin(), dists.begin() + kk, dists.end());
        std::vector<int> out;
        out.reserve(kk + 1);
        out.push_back(query);
        for (int i = 0; i < kk; ++i) out.push_back(dists[i].second);
        return out;
    }

    static std::vector<std::vector<int>> buildAdjacency(size_t vertexCount,
                                                        const std::vector<unsigned>& indices) {
        std::vector<std::vector<int>> adj(vertexCount);
        auto addEdge = [&](unsigned a, unsigned b) {
            auto& row = adj[a];
            for (int v : row) if (v == static_cast<int>(b)) return;
            row.push_back(static_cast<int>(b));
        };
        for (size_t t = 0; t + 2 < indices.size(); t += 3) {
            unsigned a = indices[t], b = indices[t + 1], c = indices[t + 2];
            addEdge(a, b); addEdge(b, a);
            addEdge(b, c); addEdge(c, b);
            addEdge(c, a); addEdge(a, c);
        }
        return adj;
    }

    static void laplacianPass(std::vector<Vector3>& verts,
                              const std::vector<std::vector<int>>& adj, float weight) {
        std::vector<Vector3> delta(verts.size());
        for (size_t i = 0; i < verts.size(); ++i) {
            if (adj[i].empty()) { delta[i] = Vector3(); continue; }
            Vector3 avg;
            for (int nb : adj[i]) avg += verts[nb];
            avg = avg / static_cast<float>(adj[i].size());
            delta[i] = (avg - verts[i]) * weight;
        }
        for (size_t i = 0; i < verts.size(); ++i) verts[i] += delta[i];
    }

    // Symmetric 3x3 eigen-decomposition via cyclic Jacobi rotations.
    // Outputs eigenvalues ascending with matching eigenvectors.
    static void jacobiEigen(const float in[3][3], float eval[3], Vector3 evec[3]) {
        float a[3][3];
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) a[i][j] = in[i][j];
        float v[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

        for (int sweep = 0; sweep < 32; ++sweep) {
            float off = std::abs(a[0][1]) + std::abs(a[0][2]) + std::abs(a[1][2]);
            if (off < 1e-12f) break;
            for (int p = 0; p < 2; ++p) {
                for (int q = p + 1; q < 3; ++q) {
                    if (std::abs(a[p][q]) < 1e-14f) continue;
                    float theta = (a[q][q] - a[p][p]) / (2.0f * a[p][q]);
                    float t = (theta >= 0.0f ? 1.0f : -1.0f) /
                              (std::abs(theta) + std::sqrt(theta * theta + 1.0f));
                    float cA = 1.0f / std::sqrt(t * t + 1.0f);
                    float sA = t * cA;
                    for (int i = 0; i < 3; ++i) {
                        float aip = a[i][p], aiq = a[i][q];
                        a[i][p] = cA * aip - sA * aiq;
                        a[i][q] = sA * aip + cA * aiq;
                    }
                    for (int i = 0; i < 3; ++i) {
                        float api = a[p][i], aqi = a[q][i];
                        a[p][i] = cA * api - sA * aqi;
                        a[q][i] = sA * api + cA * aqi;
                    }
                    for (int i = 0; i < 3; ++i) {
                        float vip = v[i][p], viq = v[i][q];
                        v[i][p] = cA * vip - sA * viq;
                        v[i][q] = sA * vip + cA * viq;
                    }
                }
            }
        }

        std::array<std::pair<float, int>, 3> order = {
            std::make_pair(a[0][0], 0), std::make_pair(a[1][1], 1),
            std::make_pair(a[2][2], 2)};
        std::sort(order.begin(), order.end(),
                  [](const auto& x, const auto& y) { return x.first < y.first; });
        for (int i = 0; i < 3; ++i) {
            int c = order[i].second;
            eval[i] = order[i].first;
            evec[i] = Vector3(v[0][c], v[1][c], v[2][c]);
        }
    }
};

} // namespace nexus::utility::graphics
