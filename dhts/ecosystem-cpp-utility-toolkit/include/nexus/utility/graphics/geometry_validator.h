#pragma once

#include <vector>
#include <utility>
#include <cstddef>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <cstdint>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Geometry validation utilities for CAD / 3D modeling pipelines.
class GeometryValidator {
public:
    enum class Winding { CW, CCW, Degenerate };

    struct NormalReport {
        bool consistent = true;       // all faces wind consistently
        std::size_t flippedCount = 0; // faces opposing the majority orientation
        double avgDeviation = 0.0;    // avg angular deviation (deg) from mesh mean normal
    };

    /**
     * @brief Normalized cross-product face normal for a triangle.
     */
    static Vector3 computeFaceNormal(const Vector3& v0, const Vector3& v1, const Vector3& v2) {
        const Vector3 n = cross(v1 - v0, v2 - v0);
        const float len = n.length();
        if (len < 1e-8f) return {0.0f, 0.0f, 0.0f};
        return n * (1.0f / len);
    }

    /**
     * @brief Triangle area via half the cross-product magnitude.
     */
    static float computeFaceArea(const Vector3& v0, const Vector3& v1, const Vector3& v2) {
        return 0.5f * cross(v1 - v0, v2 - v0).length();
    }

    /**
     * @brief True if the triangle has (near) zero area / collinear vertices.
     */
    static bool isDegenerate(const Vector3& v0, const Vector3& v1, const Vector3& v2,
                             float epsilon = 1e-6f) {
        return computeFaceArea(v0, v1, v2) <= epsilon;
    }

    /**
     * @brief Winding order of a triangle as seen from a given view direction.
     *
     * If the face normal points toward the viewer (opposite viewDir) the
     * triangle is front-facing / CCW; otherwise CW.
     */
    static Winding checkWindingOrder(const Vector3& v0, const Vector3& v1, const Vector3& v2,
                                     const Vector3& viewDir) {
        const Vector3 n = cross(v1 - v0, v2 - v0);
        if (n.length() < 1e-8f) return Winding::Degenerate;
        const float d = dot(n, viewDir);
        if (std::abs(d) < 1e-8f) return Winding::Degenerate; // edge-on
        return (d < 0.0f) ? Winding::CCW : Winding::CW;
    }

    /**
     * @brief Validates normal consistency across an indexed triangle mesh.
     */
    static NormalReport validateNormals(const std::vector<Vector3>& vertices,
                                        const std::vector<unsigned>& indices) {
        NormalReport report;
        const std::size_t triCount = indices.size() / 3;
        if (triCount == 0) return report;

        Vector3 mean{0.0f, 0.0f, 0.0f};
        std::vector<Vector3> normals;
        normals.reserve(triCount);

        for (std::size_t t = 0; t < triCount; ++t) {
            Vector3 v0, v1, v2;
            if (!fetch(vertices, indices, t, v0, v1, v2)) {
                normals.push_back({0.0f, 0.0f, 0.0f});
                continue;
            }
            const Vector3 n = computeFaceNormal(v0, v1, v2);
            normals.push_back(n);
            mean = mean + n;
        }

        const float meanLen = mean.length();
        if (meanLen < 1e-8f) {
            // Normals cancel out -> strongly inconsistent orientation.
            report.consistent = false;
            report.flippedCount = triCount / 2;
            report.avgDeviation = 90.0;
            return report;
        }
        const Vector3 meanN = mean * (1.0f / meanLen);

        double deviationSum = 0.0;
        std::size_t valid = 0;
        for (const Vector3& n : normals) {
            if (n.lengthSquared() < 1e-12f) continue;
            const float c = std::clamp(dot(n, meanN), -1.0f, 1.0f);
            const double devDeg = std::acos(c) * (180.0 / 3.14159265358979323846);
            deviationSum += devDeg;
            ++valid;
            if (devDeg > 90.0) ++report.flippedCount;
        }
        report.avgDeviation = valid ? deviationSum / static_cast<double>(valid) : 0.0;
        report.consistent = (report.flippedCount == 0);
        return report;
    }

    /**
     * @brief True if every edge appears exactly twice with opposite orientation.
     *
     * This is the standard closed-manifold (watertight, orientable) condition.
     */
    static bool isManifold(const std::vector<unsigned>& indices) {
        const std::size_t triCount = indices.size() / 3;
        if (triCount == 0) return false;

        // Map undirected edge -> net orientation balance and usage count.
        std::unordered_map<std::uint64_t, int> directed; // +1 forward, -1 reverse net
        std::unordered_map<std::uint64_t, int> usage;
        directed.reserve(triCount * 3);
        usage.reserve(triCount * 3);

        auto addEdge = [&](unsigned a, unsigned b) {
            const std::uint64_t key = edgeKey(a, b);
            ++usage[key];
            directed[key] += (a < b) ? 1 : -1;
        };

        for (std::size_t t = 0; t < triCount; ++t) {
            const unsigned i0 = indices[t * 3 + 0];
            const unsigned i1 = indices[t * 3 + 1];
            const unsigned i2 = indices[t * 3 + 2];
            addEdge(i0, i1);
            addEdge(i1, i2);
            addEdge(i2, i0);
        }

        for (const auto& kv : usage) {
            if (kv.second != 2) return false;               // not shared by exactly 2 faces
            if (directed[kv.first] != 0) return false;       // not opposite orientation
        }
        return true;
    }

    /**
     * @brief Brute-force detection of intersecting triangle pairs (debug tool).
     * @return list of (triA, triB) index pairs whose triangles intersect.
     */
    static std::vector<std::pair<std::size_t, std::size_t>>
    detectSelfIntersections(const std::vector<Vector3>& verts,
                            const std::vector<unsigned>& idx) {
        std::vector<std::pair<std::size_t, std::size_t>> hits;
        const std::size_t triCount = idx.size() / 3;
        for (std::size_t a = 0; a < triCount; ++a) {
            Vector3 a0, a1, a2;
            if (!fetch(verts, idx, a, a0, a1, a2)) continue;
            for (std::size_t b = a + 1; b < triCount; ++b) {
                if (sharesVertex(idx, a, b)) continue; // adjacent faces skip
                Vector3 b0, b1, b2;
                if (!fetch(verts, idx, b, b0, b1, b2)) continue;
                if (trianglesIntersect(a0, a1, a2, b0, b1, b2)) {
                    hits.emplace_back(a, b);
                }
            }
        }
        return hits;
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

    static std::uint64_t edgeKey(unsigned a, unsigned b) {
        if (a > b) std::swap(a, b);
        return (static_cast<std::uint64_t>(a) << 32) | static_cast<std::uint64_t>(b);
    }

    static bool fetch(const std::vector<Vector3>& verts, const std::vector<unsigned>& idx,
                      std::size_t tri, Vector3& v0, Vector3& v1, Vector3& v2) {
        const unsigned i0 = idx[tri * 3 + 0];
        const unsigned i1 = idx[tri * 3 + 1];
        const unsigned i2 = idx[tri * 3 + 2];
        if (i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) return false;
        v0 = verts[i0];
        v1 = verts[i1];
        v2 = verts[i2];
        return true;
    }

    static bool sharesVertex(const std::vector<unsigned>& idx, std::size_t a, std::size_t b) {
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (idx[a * 3 + i] == idx[b * 3 + j]) return true;
            }
        }
        return false;
    }

    // --- Moeller triangle/triangle intersection (interval overlap on planes) ---

    static bool trianglesIntersect(const Vector3& a0, const Vector3& a1, const Vector3& a2,
                                   const Vector3& b0, const Vector3& b1, const Vector3& b2) {
        // Edges of B tested against triangle A and vice versa via segment/triangle tests.
        if (segmentIntersectsTriangle(b0, b1, a0, a1, a2)) return true;
        if (segmentIntersectsTriangle(b1, b2, a0, a1, a2)) return true;
        if (segmentIntersectsTriangle(b2, b0, a0, a1, a2)) return true;
        if (segmentIntersectsTriangle(a0, a1, b0, b1, b2)) return true;
        if (segmentIntersectsTriangle(a1, a2, b0, b1, b2)) return true;
        if (segmentIntersectsTriangle(a2, a0, b0, b1, b2)) return true;
        return false;
    }

    static bool segmentIntersectsTriangle(const Vector3& p, const Vector3& q,
                                          const Vector3& v0, const Vector3& v1,
                                          const Vector3& v2) {
        const Vector3 dir = q - p;
        const Vector3 e1 = v1 - v0;
        const Vector3 e2 = v2 - v0;
        const Vector3 pv = cross(dir, e2);
        const float det = dot(e1, pv);
        if (std::abs(det) < 1e-8f) return false;
        const float invDet = 1.0f / det;

        const Vector3 tv = p - v0;
        const float u = dot(tv, pv) * invDet;
        if (u < 0.0f || u > 1.0f) return false;

        const Vector3 qv = cross(tv, e1);
        const float v = dot(dir, qv) * invDet;
        if (v < 0.0f || u + v > 1.0f) return false;

        const float t = dot(e2, qv) * invDet;
        return t >= 0.0f && t <= 1.0f; // within the segment span
    }
};

} // namespace nexus::utility::graphics
