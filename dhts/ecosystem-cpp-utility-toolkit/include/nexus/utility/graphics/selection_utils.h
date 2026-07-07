#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <array>
#include <vector>
#include <optional>
#include <cstddef>
#include <cmath>
#include <algorithm>

namespace nexus::utility::graphics {

/**
 * @brief A ray for picking: origin plus a (normalized-or-not) direction.
 */
struct SelectionRay {
    using Vector3 = nexus::utility::math::Vector3;
    Vector3 origin;
    Vector3 direction;

    SelectionRay() = default;
    SelectionRay(const Vector3& o, const Vector3& d) : origin(o), direction(d) {}
};

/**
 * @brief A 4-component plane {a, b, c, d} such that ax + by + cz + d = 0.
 *        The (a,b,c) normal points toward the frustum interior.
 */
struct Vector4Plane {
    float a = 0, b = 0, c = 0, d = 0;

    Vector4Plane() = default;
    Vector4Plane(float a_, float b_, float c_, float d_) : a(a_), b(b_), c(c_), d(d_) {}
};

/**
 * @brief View frustum: 6 planes (left, right, bottom, top, near, far).
 */
using Frustum = std::array<Vector4Plane, 6>;

/**
 * @brief Selection / picking utilities for 3D viewport interaction.
 */
class SelectionUtils {
public:
    using Vector3 = nexus::utility::math::Vector3;
    using Ray = SelectionRay;

    /**
     * @brief Moeller-Trumbore ray/triangle intersection.
     * @return the hit distance along the ray (>= 0), or nullopt if no hit.
     */
    static std::optional<float> rayTriangleIntersect(const Ray& ray,
                                                     const Vector3& v0,
                                                     const Vector3& v1,
                                                     const Vector3& v2) {
        using V = nexus::utility::math::Vector3Utils;
        const Vector3 e1 = v1 - v0;
        const Vector3 e2 = v2 - v0;
        const Vector3 p = V::cross(ray.direction, e2);
        const float det = V::dot(e1, p);
        if (std::abs(det) < 1e-8f) return std::nullopt; // parallel
        const float invDet = 1.0f / det;

        const Vector3 tvec = ray.origin - v0;
        const float u = V::dot(tvec, p) * invDet;
        if (u < 0.0f || u > 1.0f) return std::nullopt;

        const Vector3 q = V::cross(tvec, e1);
        const float v = V::dot(ray.direction, q) * invDet;
        if (v < 0.0f || u + v > 1.0f) return std::nullopt;

        const float t = V::dot(e2, q) * invDet;
        if (t < 0.0f) return std::nullopt; // behind ray origin
        return t;
    }

    /**
     * @brief Slab-based ray/AABB intersection.
     * @return distance to the first entry point (>= 0), or nullopt if no hit.
     */
    static std::optional<float> rayBoxIntersect(const Ray& ray,
                                                const Vector3& boxMin,
                                                const Vector3& boxMax) {
        float tMin = 0.0f;
        float tMax = std::numeric_limits<float>::max();
        for (int i = 0; i < 3; ++i) {
            const float o = (&ray.origin.x)[i];
            const float d = (&ray.direction.x)[i];
            const float lo = (&boxMin.x)[i];
            const float hi = (&boxMax.x)[i];
            if (std::abs(d) < 1e-8f) {
                if (o < lo || o > hi) return std::nullopt;
                continue;
            }
            const float invD = 1.0f / d;
            float t0 = (lo - o) * invD;
            float t1 = (hi - o) * invD;
            if (invD < 0.0f) std::swap(t0, t1);
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMax < tMin) return std::nullopt;
        }
        return tMin;
    }

    /**
     * @brief Ray/sphere intersection.
     * @return nearest non-negative hit distance, or nullopt if no hit.
     */
    static std::optional<float> raySphereIntersect(const Ray& ray,
                                                   const Vector3& center,
                                                   float radius) {
        using V = nexus::utility::math::Vector3Utils;
        const Vector3 oc = ray.origin - center;
        const float a = V::dot(ray.direction, ray.direction);
        if (a < 1e-12f) return std::nullopt;
        const float b = 2.0f * V::dot(oc, ray.direction);
        const float c = V::dot(oc, oc) - radius * radius;
        const float disc = b * b - 4.0f * a * c;
        if (disc < 0.0f) return std::nullopt;
        const float sq = std::sqrt(disc);
        float t0 = (-b - sq) / (2.0f * a);
        float t1 = (-b + sq) / (2.0f * a);
        if (t0 > t1) std::swap(t0, t1);
        if (t0 >= 0.0f) return t0;
        if (t1 >= 0.0f) return t1; // origin inside sphere
        return std::nullopt;
    }

    /**
     * @brief Returns indices of points inside the axis-aligned selection box.
     */
    static std::vector<std::size_t> boxSelectPoints(const std::vector<Vector3>& points,
                                                    const Vector3& boxMin,
                                                    const Vector3& boxMax) {
        const float minX = std::min(boxMin.x, boxMax.x);
        const float minY = std::min(boxMin.y, boxMax.y);
        const float minZ = std::min(boxMin.z, boxMax.z);
        const float maxX = std::max(boxMin.x, boxMax.x);
        const float maxY = std::max(boxMin.y, boxMax.y);
        const float maxZ = std::max(boxMin.z, boxMax.z);

        std::vector<std::size_t> result;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const Vector3& p = points[i];
            if (p.x >= minX && p.x <= maxX &&
                p.y >= minY && p.y <= maxY &&
                p.z >= minZ && p.z <= maxZ) {
                result.push_back(i);
            }
        }
        return result;
    }

    /**
     * @brief Returns indices of points that lie inside all 6 frustum planes.
     */
    static std::vector<std::size_t> frustumSelectPoints(const std::vector<Vector3>& points,
                                                        const Frustum& f) {
        std::vector<std::size_t> result;
        for (std::size_t i = 0; i < points.size(); ++i) {
            if (pointInsideFrustum(points[i], f)) result.push_back(i);
        }
        return result;
    }

    /**
     * @brief Projects a point onto the (infinite) line defined by the ray.
     */
    static Vector3 nearestPointOnRay(const Vector3& point, const Ray& ray) {
        using V = nexus::utility::math::Vector3Utils;
        const float dd = V::dot(ray.direction, ray.direction);
        if (dd < 1e-12f) return ray.origin;
        float t = V::dot(point - ray.origin, ray.direction) / dd;
        if (t < 0.0f) t = 0.0f; // clamp to ray (not full line)
        return ray.origin + ray.direction * t;
    }

    /**
     * @brief Shortest distance between two infinite lines.
     * @param p1,d1 point and direction of line 1.
     * @param p2,d2 point and direction of line 2.
     */
    static float lineLineDistance(const Vector3& p1, const Vector3& d1,
                                  const Vector3& p2, const Vector3& d2) {
        using V = nexus::utility::math::Vector3Utils;
        const Vector3 n = V::cross(d1, d2);
        const float nLen = n.length();
        if (nLen < 1e-8f) {
            // Parallel lines: distance from p2 to line 1.
            const Vector3 diff = p2 - p1;
            const float d1sq = V::dot(d1, d1);
            if (d1sq < 1e-12f) return diff.length();
            const Vector3 proj = d1 * (V::dot(diff, d1) / d1sq);
            return (diff - proj).length();
        }
        return std::abs(V::dot(p2 - p1, n)) / nLen;
    }

private:
    static bool pointInsideFrustum(const Vector3& p, const Frustum& f) {
        for (const auto& pl : f) {
            const float dist = pl.a * p.x + pl.b * p.y + pl.c * p.z + pl.d;
            if (dist < 0.0f) return false;
        }
        return true;
    }
};

} // namespace nexus::utility::graphics
