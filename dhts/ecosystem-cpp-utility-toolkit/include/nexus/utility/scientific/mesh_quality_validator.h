#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <cstddef>
#include <algorithm>
#include <limits>
#include <unordered_map>
#include <cstdint>

namespace nexus::utility::scientific {

/**
 * @brief Complete triangle mesh quality analysis for 3D modeling / CAD.
 *
 * Header-only, no external dependencies. Accepts a raw vertex/index buffer
 * and exposes per-triangle and whole-mesh quality metrics: angles, aspect
 * ratio, jacobian, skewness, degeneracy, inversion and watertightness.
 */
class MeshQualityValidator {
public:
    struct Vector3 {
        float x = 0.0f, y = 0.0f, z = 0.0f;

        Vector3() = default;
        Vector3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

        Vector3 operator+(const Vector3& v) const { return {x + v.x, y + v.y, z + v.z}; }
        Vector3 operator-(const Vector3& v) const { return {x - v.x, y - v.y, z - v.z}; }
        Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }

        float lengthSquared() const { return x * x + y * y + z * z; }
        float length() const { return std::sqrt(lengthSquared()); }
    };

    struct Stats {
        double minAngle = 0.0;          // smallest triangle angle in the mesh (deg)
        double maxAngle = 0.0;          // largest triangle angle in the mesh (deg)
        double avgAngle = 60.0;         // average interior angle (deg)
        double minAspectRatio = 0.0;    // best aspect ratio (closest to 1)
        double maxAspectRatio = 0.0;    // worst aspect ratio
        double degeneratePercent = 0.0; // percentage of zero-area triangles
        double invertedPercent = 0.0;   // percentage of negative-jacobian triangles
    };

    MeshQualityValidator() = default;

    /**
     * @brief Sets the working mesh from a flat position buffer and index buffer.
     * @param vertexCount number of vertices (positions holds vertexCount*3 floats).
     * @param positions   pointer to xyz float triples (may be null if vertexCount==0).
     * @param triCount     number of triangles (indices holds triCount*3 entries).
     * @param indices      pointer to triangle vertex indices.
     */
    void setMesh(std::size_t vertexCount, const float* positions,
                 std::size_t triCount, const unsigned* indices) {
        vertices_.clear();
        indices_.clear();
        vertices_.reserve(vertexCount);
        indices_.reserve(triCount * 3);

        if (positions) {
            for (std::size_t i = 0; i < vertexCount; ++i) {
                vertices_.emplace_back(positions[i * 3 + 0],
                                       positions[i * 3 + 1],
                                       positions[i * 3 + 2]);
            }
        }
        if (indices) {
            for (std::size_t i = 0; i < triCount * 3; ++i) {
                indices_.push_back(indices[i]);
            }
        }
        computeReferenceNormal();
    }

    std::size_t getTriangleCount() const { return indices_.size() / 3; }
    std::size_t getVertexCount() const { return vertices_.size(); }

    /**
     * @brief Smallest interior angle of a triangle, in degrees.
     */
    double computeMinAngle(std::size_t triIndex) const {
        double a, b, c;
        if (!triangleAngles(triIndex, a, b, c)) return 0.0;
        return std::min({a, b, c});
    }

    /**
     * @brief Largest interior angle of a triangle, in degrees.
     */
    double computeMaxAngle(std::size_t triIndex) const {
        double a, b, c;
        if (!triangleAngles(triIndex, a, b, c)) return 0.0;
        return std::max({a, b, c});
    }

    /**
     * @brief Aspect ratio: longest edge / shortest edge (>= 1). 0 if degenerate.
     */
    double computeAspectRatio(std::size_t triIndex) const {
        Vector3 v0, v1, v2;
        if (!triangle(triIndex, v0, v1, v2)) return 0.0;
        const double e0 = (v1 - v0).length();
        const double e1 = (v2 - v1).length();
        const double e2 = (v0 - v2).length();
        const double lo = std::min({e0, e1, e2});
        const double hi = std::max({e0, e1, e2});
        if (lo <= kEpsilon) return 0.0;
        return hi / lo;
    }

    /**
     * @brief Ratio of the longest to shortest edge across the entire mesh.
     */
    double computeEdgeLengthRatio() const {
        double lo = std::numeric_limits<double>::max();
        double hi = 0.0;
        const std::size_t tris = getTriangleCount();
        for (std::size_t t = 0; t < tris; ++t) {
            Vector3 v0, v1, v2;
            if (!triangle(t, v0, v1, v2)) continue;
            const double edges[3] = {
                (v1 - v0).length(),
                (v2 - v1).length(),
                (v0 - v2).length()
            };
            for (double e : edges) {
                if (e <= kEpsilon) continue;
                lo = std::min(lo, e);
                hi = std::max(hi, e);
            }
        }
        if (lo == std::numeric_limits<double>::max() || lo <= kEpsilon) return 0.0;
        return hi / lo;
    }

    /**
     * @brief Signed jacobian determinant of the triangle's affine map.
     *
     * Computed as 2 * signed area, where the sign is taken relative to the
     * mesh's average orientation. Negative values indicate an inverted (flipped)
     * triangle relative to the rest of the surface.
     */
    double computeJacobian(std::size_t triIndex) const {
        Vector3 v0, v1, v2;
        if (!triangle(triIndex, v0, v1, v2)) return 0.0;
        const Vector3 n = cross(v1 - v0, v2 - v0);
        const double area2 = n.length();          // == 2 * area
        const double s = dot(n, referenceNormal_); // orientation sign
        return (s < 0.0) ? -area2 : area2;
    }

    /**
     * @brief Equiangle skewness (0 = perfect equilateral, 1 = fully degenerate).
     *
     * Deviation of the worst angle from the ideal 60 degrees, normalized.
     */
    double computeSkew(std::size_t triIndex) const {
        double a, b, c;
        if (!triangleAngles(triIndex, a, b, c)) return 1.0;
        const double maxA = std::max({a, b, c});
        const double minA = std::min({a, b, c});
        const double ideal = 60.0;
        const double skewMax = (maxA - ideal) / (180.0 - ideal);
        const double skewMin = (ideal - minA) / ideal;
        return std::clamp(std::max(skewMax, skewMin), 0.0, 1.0);
    }

    /**
     * @brief Count of triangles with (near) zero area.
     */
    std::size_t getDegenerateCount() const {
        std::size_t count = 0;
        const std::size_t tris = getTriangleCount();
        for (std::size_t t = 0; t < tris; ++t) {
            if (isDegenerate(t)) ++count;
        }
        return count;
    }

    /**
     * @brief Count of triangles whose jacobian is negative (flipped winding).
     */
    std::size_t getInvertedCount() const {
        std::size_t count = 0;
        const std::size_t tris = getTriangleCount();
        for (std::size_t t = 0; t < tris; ++t) {
            if (computeJacobian(t) < 0.0) ++count;
        }
        return count;
    }

    /**
     * @brief True if every mesh edge is shared by exactly two triangles.
     */
    bool isWatertight() const {
        const std::size_t tris = getTriangleCount();
        if (tris == 0) return false;
        std::unordered_map<std::uint64_t, int> edgeCount;
        edgeCount.reserve(tris * 3);
        for (std::size_t t = 0; t < tris; ++t) {
            const unsigned i0 = indices_[t * 3 + 0];
            const unsigned i1 = indices_[t * 3 + 1];
            const unsigned i2 = indices_[t * 3 + 2];
            ++edgeCount[edgeKey(i0, i1)];
            ++edgeCount[edgeKey(i1, i2)];
            ++edgeCount[edgeKey(i2, i0)];
        }
        for (const auto& kv : edgeCount) {
            if (kv.second != 2) return false;
        }
        return true;
    }

    /**
     * @brief Aggregate statistics across the whole mesh.
     */
    Stats computeStats() const {
        Stats s;
        const std::size_t tris = getTriangleCount();
        if (tris == 0) return s;

        double minAngle = std::numeric_limits<double>::max();
        double maxAngle = 0.0;
        double angleSum = 0.0;
        std::size_t angleSamples = 0;
        double minAspect = std::numeric_limits<double>::max();
        double maxAspect = 0.0;
        std::size_t degenerate = 0;
        std::size_t inverted = 0;

        for (std::size_t t = 0; t < tris; ++t) {
            if (isDegenerate(t)) {
                ++degenerate;
                continue;
            }
            if (computeJacobian(t) < 0.0) ++inverted;

            double a, b, c;
            if (triangleAngles(t, a, b, c)) {
                minAngle = std::min({minAngle, a, b, c});
                maxAngle = std::max({maxAngle, a, b, c});
                angleSum += a + b + c;
                angleSamples += 3;
            }
            const double ar = computeAspectRatio(t);
            if (ar > 0.0) {
                minAspect = std::min(minAspect, ar);
                maxAspect = std::max(maxAspect, ar);
            }
        }

        s.minAngle = (minAngle == std::numeric_limits<double>::max()) ? 0.0 : minAngle;
        s.maxAngle = maxAngle;
        s.avgAngle = angleSamples ? angleSum / static_cast<double>(angleSamples) : 0.0;
        s.minAspectRatio = (minAspect == std::numeric_limits<double>::max()) ? 0.0 : minAspect;
        s.maxAspectRatio = maxAspect;
        s.degeneratePercent = 100.0 * static_cast<double>(degenerate) / static_cast<double>(tris);
        s.invertedPercent = 100.0 * static_cast<double>(inverted) / static_cast<double>(tris);
        return s;
    }

private:
    static constexpr double kEpsilon = 1e-9;

    static double dot(const Vector3& a, const Vector3& b) {
        return static_cast<double>(a.x) * b.x + static_cast<double>(a.y) * b.y +
               static_cast<double>(a.z) * b.z;
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

    bool triangle(std::size_t triIndex, Vector3& v0, Vector3& v1, Vector3& v2) const {
        if (triIndex >= getTriangleCount()) return false;
        const unsigned i0 = indices_[triIndex * 3 + 0];
        const unsigned i1 = indices_[triIndex * 3 + 1];
        const unsigned i2 = indices_[triIndex * 3 + 2];
        if (i0 >= vertices_.size() || i1 >= vertices_.size() || i2 >= vertices_.size())
            return false;
        v0 = vertices_[i0];
        v1 = vertices_[i1];
        v2 = vertices_[i2];
        return true;
    }

    bool isDegenerate(std::size_t triIndex) const {
        Vector3 v0, v1, v2;
        if (!triangle(triIndex, v0, v1, v2)) return true;
        const double area2 = cross(v1 - v0, v2 - v0).length();
        return area2 <= kEpsilon;
    }

    bool triangleAngles(std::size_t triIndex, double& a, double& b, double& c) const {
        Vector3 v0, v1, v2;
        if (!triangle(triIndex, v0, v1, v2)) return false;
        const Vector3 e01 = v1 - v0;
        const Vector3 e02 = v2 - v0;
        const Vector3 e12 = v2 - v1;
        const double l01 = e01.length();
        const double l02 = e02.length();
        const double l12 = e12.length();
        if (l01 <= kEpsilon || l02 <= kEpsilon || l12 <= kEpsilon) return false;

        a = angleBetween(e01, e02);                 // at v0
        b = angleBetween(v0 - v1, e12);             // at v1
        c = 180.0 - a - b;                          // at v2
        return true;
    }

    static double angleBetween(const Vector3& a, const Vector3& b) {
        const double la = a.length();
        const double lb = b.length();
        if (la <= kEpsilon || lb <= kEpsilon) return 0.0;
        double cosv = dot(a, b) / (la * lb);
        cosv = std::clamp(cosv, -1.0, 1.0);
        return std::acos(cosv) * (180.0 / 3.14159265358979323846);
    }

    void computeReferenceNormal() {
        referenceNormal_ = {0.0f, 0.0f, 1.0f};
        Vector3 accum{0.0f, 0.0f, 0.0f};
        const std::size_t tris = getTriangleCount();
        for (std::size_t t = 0; t < tris; ++t) {
            Vector3 v0, v1, v2;
            if (!triangle(t, v0, v1, v2)) continue;
            accum = accum + cross(v1 - v0, v2 - v0);
        }
        const float len = accum.length();
        if (len > 1e-6f) referenceNormal_ = accum * (1.0f / len);
    }

    std::vector<Vector3> vertices_;
    std::vector<unsigned> indices_;
    Vector3 referenceNormal_{0.0f, 0.0f, 1.0f};
};

} // namespace nexus::utility::scientific
