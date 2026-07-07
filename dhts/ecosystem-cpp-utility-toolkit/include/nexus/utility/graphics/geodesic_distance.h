#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <utility>
#include <algorithm>
#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/graphics/surface_curvature.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Geodesic distances on triangle meshes via the Heat Method
/// (Crane, Weischedel, Wardetzky 2013).
///
/// The algorithm proceeds in three stages: (1) integrate the heat equation for a
/// short time t by solving the backward-Euler system (M + tL)u = u0 where L is
/// the cotangent Laplace-Beltrami operator and M the lumped mass matrix;
/// (2) evaluate the normalized negative gradient X of the resulting temperature
/// field per face; (3) recover distances by solving the Poisson problem
/// Lφ = ∇·X. Both linear systems are solved with Jacobi iteration so the module
/// stays dependency-free. Cotangent weights follow the curvature module's
/// convention.
class GeodesicDistance {
public:
    using Vector3 = nexus::utility::math::Vector3;

    /// @brief Compute geodesic distance from a single source vertex.
    std::vector<float> compute(const std::vector<Vector3>& verts,
                               const std::vector<unsigned>& indices,
                               int sourceVertex) {
        return computeMultiSource(verts, indices, {sourceVertex});
    }

    /// @brief Compute geodesic distance from a set of source vertices.
    std::vector<float> computeMultiSource(const std::vector<Vector3>& verts,
                                          const std::vector<unsigned>& indices,
                                          const std::vector<int>& sourceVertices) {
        verts_ = verts;
        indices_ = indices;
        const int V = static_cast<int>(verts.size());
        distances_.assign(V, 0.0f);
        if (V == 0 || indices.empty() || sourceVertices.empty()) return distances_;

        buildLaplacian();

        // Time step: t = mean(edge length)^2.
        float t = avgEdgeLength_ * avgEdgeLength_;
        if (t < 1e-12f) t = 1.0f;

        // Stage 1: heat diffusion (M + tL) u = M u0, u0 = source indicator.
        std::vector<uint8_t> isSource(V, 0);
        for (int s : sourceVertices)
            if (s >= 0 && s < V) isSource[s] = 1;

        std::vector<float> b(V, 0.0f);
        for (int i = 0; i < V; ++i)
            if (isSource[i]) b[i] = mass_[i];

        std::vector<float> u(V, 0.0f);
        jacobiHeat(u, b, t);

        // Stage 2: normalized negative gradient per face.
        const size_t triCount = indices.size() / 3;
        std::vector<Vector3> X(triCount);
        for (size_t f = 0; f < triCount; ++f) {
            int a = static_cast<int>(indices[f * 3 + 0]);
            int bIdx = static_cast<int>(indices[f * 3 + 1]);
            int c = static_cast<int>(indices[f * 3 + 2]);
            const Vector3& pa = verts[a];
            const Vector3& pb = verts[bIdx];
            const Vector3& pc = verts[c];
            Vector3 fn = cross(pb - pa, pc - pa);
            float area2 = fn.length();
            if (area2 < 1e-12f) { X[f] = Vector3(); continue; }
            Vector3 N = fn / area2;
            Vector3 grad = (cross(N, pc - pb) * u[a] +
                            cross(N, pa - pc) * u[bIdx] +
                            cross(N, pb - pa) * u[c]) / area2;
            float glen = grad.length();
            X[f] = (glen > 1e-12f) ? (grad * (-1.0f / glen)) : Vector3();
        }

        // Stage 2b: integrated divergence of X at vertices.
        std::vector<float> div(V, 0.0f);
        for (size_t f = 0; f < triCount; ++f) {
            int idx[3] = {static_cast<int>(indices[f * 3 + 0]),
                          static_cast<int>(indices[f * 3 + 1]),
                          static_cast<int>(indices[f * 3 + 2])};
            const Vector3& Xf = X[f];
            for (int e = 0; e < 3; ++e) {
                int i = idx[e];
                int j = idx[(e + 1) % 3];
                int k = idx[(e + 2) % 3];
                Vector3 e1 = verts[j] - verts[i];
                Vector3 e2 = verts[k] - verts[i];
                float cotK = cotangent(verts[i] - verts[k], verts[j] - verts[k]);
                float cotJ = cotangent(verts[i] - verts[j], verts[k] - verts[j]);
                div[i] += 0.5f * (cotK * dot(e1, Xf) + cotJ * dot(e2, Xf));
            }
        }

        // Stage 3: Poisson solve Lφ = div, with sources pinned to 0.
        std::vector<float> phi(V, 0.0f);
        jacobiPoisson(phi, div, isSource);

        // Normalize: shift so sources are at zero distance, keep non-negative.
        float base = 0.0f;
        int scount = 0;
        for (int s : sourceVertices)
            if (s >= 0 && s < V) { base += phi[s]; ++scount; }
        if (scount) base /= static_cast<float>(scount);
        for (int i = 0; i < V; ++i) distances_[i] = phi[i] - base;

        double mean = 0.0;
        for (float d : distances_) mean += d;
        if (mean < 0.0)
            for (float& d : distances_) d = -d;

        float minD = *std::min_element(distances_.begin(), distances_.end());
        for (float& d : distances_) d -= minD;

        return distances_;
    }

    /// @brief Extract iso-distance line segments for `numLines` evenly spaced levels.
    ///
    /// Each returned polyline is a flat list of segment endpoints (pairs of
    /// points) for one iso-level.
    std::vector<std::vector<Vector3>> computeIsoLines(const std::vector<float>& distances,
                                                      int numLines) const {
        std::vector<std::vector<Vector3>> result;
        if (distances.empty() || numLines < 1 || indices_.empty()) return result;

        float minD = std::numeric_limits<float>::max();
        float maxD = -std::numeric_limits<float>::max();
        for (float d : distances) { minD = std::min(minD, d); maxD = std::max(maxD, d); }
        if (maxD - minD < 1e-12f) return result;

        const size_t triCount = indices_.size() / 3;
        for (int l = 1; l <= numLines; ++l) {
            float level = minD + (maxD - minD) * static_cast<float>(l) /
                                     static_cast<float>(numLines + 1);
            std::vector<Vector3> seg;
            for (size_t f = 0; f < triCount; ++f) {
                int i0 = static_cast<int>(indices_[f * 3 + 0]);
                int i1 = static_cast<int>(indices_[f * 3 + 1]);
                int i2 = static_cast<int>(indices_[f * 3 + 2]);
                std::array<int, 3> tri{i0, i1, i2};
                std::vector<Vector3> pts;
                for (int e = 0; e < 3; ++e) {
                    int a = tri[e];
                    int b = tri[(e + 1) % 3];
                    if (a >= static_cast<int>(distances.size()) ||
                        b >= static_cast<int>(distances.size())) continue;
                    float da = distances[a] - level;
                    float db = distances[b] - level;
                    if ((da <= 0.0f && db > 0.0f) || (da > 0.0f && db <= 0.0f)) {
                        float t = da / (da - db);
                        pts.push_back(verts_[a] + (verts_[b] - verts_[a]) * t);
                    }
                }
                if (pts.size() == 2) {
                    seg.push_back(pts[0]);
                    seg.push_back(pts[1]);
                }
            }
            if (!seg.empty()) result.push_back(std::move(seg));
        }
        return result;
    }

    // ── Queries ─────────────────────────────────────────────────────────

    float getMaxDistance() const {
        float mx = 0.0f;
        for (float d : distances_) mx = std::max(mx, d);
        return mx;
    }
    float getAverageDistance() const {
        if (distances_.empty()) return 0.0f;
        double sum = 0.0;
        for (float d : distances_) sum += d;
        return static_cast<float>(sum / static_cast<double>(distances_.size()));
    }

    const std::vector<float>& distances() const { return distances_; }

private:
    std::vector<Vector3> verts_;
    std::vector<unsigned> indices_;
    std::vector<float> distances_;

    std::vector<std::vector<std::pair<int, float>>> weights_;  // (neighbor, w_ij)
    std::vector<float> diag_;   // L_ii = sum_j w_ij
    std::vector<float> mass_;   // lumped vertex areas
    float avgEdgeLength_ = 1.0f;

    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }
    // Cotangent of the angle between two vectors (stable form).
    static float cotangent(const Vector3& a, const Vector3& b) {
        float d = dot(a, b);
        float cl = cross(a, b).length();
        if (cl < 1e-12f) return 0.0f;
        return d / cl;
    }

    void buildLaplacian() {
        const int V = static_cast<int>(verts_.size());
        std::vector<std::vector<std::pair<int, float>>> adjMap(V);
        // Accumulate via linear search per vertex (meshes are locally small).
        auto addWeight = [&](int i, int j, float w) {
            auto& row = adjMap[i];
            for (auto& pr : row)
                if (pr.first == j) { pr.second += w; return; }
            row.emplace_back(j, w);
        };

        mass_.assign(V, 0.0f);
        diag_.assign(V, 0.0f);

        const size_t triCount = indices_.size() / 3;
        double edgeSum = 0.0;
        size_t edgeCnt = 0;
        for (size_t t = 0; t < triCount; ++t) {
            int a = static_cast<int>(indices_[t * 3 + 0]);
            int b = static_cast<int>(indices_[t * 3 + 1]);
            int c = static_cast<int>(indices_[t * 3 + 2]);
            const Vector3& pa = verts_[a];
            const Vector3& pb = verts_[b];
            const Vector3& pc = verts_[c];

            float cotA = cotangent(pb - pa, pc - pa);
            float cotB = cotangent(pa - pb, pc - pb);
            float cotC = cotangent(pa - pc, pb - pc);

            // Edge (b,c) opposite a, edge (a,c) opposite b, edge (a,b) opposite c.
            addWeight(b, c, 0.5f * cotA); addWeight(c, b, 0.5f * cotA);
            addWeight(a, c, 0.5f * cotB); addWeight(c, a, 0.5f * cotB);
            addWeight(a, b, 0.5f * cotC); addWeight(b, a, 0.5f * cotC);

            float area = 0.5f * cross(pb - pa, pc - pa).length();
            mass_[a] += area / 3.0f;
            mass_[b] += area / 3.0f;
            mass_[c] += area / 3.0f;

            edgeSum += (pb - pa).length();
            edgeSum += (pc - pb).length();
            edgeSum += (pa - pc).length();
            edgeCnt += 3;
        }

        weights_ = std::move(adjMap);
        for (int i = 0; i < V; ++i) {
            float s = 0.0f;
            for (const auto& pr : weights_[i]) s += pr.second;
            diag_[i] = s;
            if (mass_[i] < 1e-12f) mass_[i] = 1e-12f;
        }
        avgEdgeLength_ = edgeCnt ? static_cast<float>(edgeSum / static_cast<double>(edgeCnt)) : 1.0f;
    }

    // Solve (M + tL) u = b via Jacobi. Diagonal = M_i + t*L_ii; off-diagonal
    // entries are -t*w_ij.
    void jacobiHeat(std::vector<float>& u, const std::vector<float>& b, float t) {
        const int V = static_cast<int>(u.size());
        std::vector<float> next(V);
        for (int iter = 0; iter < 400; ++iter) {
            float maxDelta = 0.0f;
            for (int i = 0; i < V; ++i) {
                float diag = mass_[i] + t * diag_[i];
                float acc = b[i];
                for (const auto& pr : weights_[i]) acc += t * pr.second * u[pr.first];
                float val = (diag > 1e-12f) ? acc / diag : 0.0f;
                maxDelta = std::max(maxDelta, std::abs(val - u[i]));
                next[i] = val;
            }
            u.swap(next);
            if (maxDelta < 1e-7f) break;
        }
    }

    // Solve Lφ = rhs via Jacobi, pinning pinned vertices to 0 (Dirichlet).
    void jacobiPoisson(std::vector<float>& phi, const std::vector<float>& rhs,
                       const std::vector<uint8_t>& pinned) {
        const int V = static_cast<int>(phi.size());
        std::vector<float> next(V);
        for (int iter = 0; iter < 2000; ++iter) {
            float maxDelta = 0.0f;
            for (int i = 0; i < V; ++i) {
                if (pinned[i]) { next[i] = 0.0f; continue; }
                float diag = diag_[i];
                if (diag < 1e-12f) { next[i] = phi[i]; continue; }
                float acc = rhs[i];
                for (const auto& pr : weights_[i]) acc += pr.second * phi[pr.first];
                float val = acc / diag;
                maxDelta = std::max(maxDelta, std::abs(val - phi[i]));
                next[i] = val;
            }
            phi.swap(next);
            if (maxDelta < 1e-7f) break;
        }
    }
};

} // namespace nexus::utility::graphics
