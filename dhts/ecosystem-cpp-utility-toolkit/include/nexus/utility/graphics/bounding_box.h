#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <array>
#include <algorithm>
#include <limits>

namespace nexus::utility::graphics {

/**
 * @brief A ray defined by an origin and a (not necessarily normalized) direction.
 */
struct Ray {
    using Vector3 = nexus::utility::math::Vector3;
    Vector3 origin;
    Vector3 direction;

    Ray() = default;
    Ray(const Vector3& o, const Vector3& d) : origin(o), direction(d) {}
};

/**
 * @brief A view frustum expressed as 6 planes (ax + by + cz + d = 0).
 *
 * Each plane is stored as {a, b, c, d}; the (a,b,c) normal is expected to point
 * toward the interior of the frustum so that pointInside() >= 0 means inside.
 * Plane order: left, right, bottom, top, near, far.
 */
struct FrustumPlanes {
    struct Plane { float a = 0, b = 0, c = 0, d = 0; };
    std::array<Plane, 6> planes{};
};

/**
 * @brief Axis-aligned bounding box.
 */
struct BoundingBox {
    using Vector3 = nexus::utility::math::Vector3;
    Vector3 min;
    Vector3 max;
    
    BoundingBox()
        : min(std::numeric_limits<float>::max(), 
              std::numeric_limits<float>::max(), 
              std::numeric_limits<float>::max()),
          max(std::numeric_limits<float>::lowest(), 
              std::numeric_limits<float>::lowest(), 
              std::numeric_limits<float>::lowest()) {}
    
    BoundingBox(const Vector3& min, const Vector3& max)
        : min(min), max(max) {}
    
    /**
     * @brief Expands box to include point.
     */
    void expand(const Vector3& point) {
        min.x = std::min(min.x, point.x);
        min.y = std::min(min.y, point.y);
        min.z = std::min(min.z, point.z);
        max.x = std::max(max.x, point.x);
        max.y = std::max(max.y, point.y);
        max.z = std::max(max.z, point.z);
    }
    
    /**
     * @brief Expands box to include another box.
     */
    void expand(const BoundingBox& other) {
        expand(other.min);
        expand(other.max);
    }
    
    /**
     * @brief Gets center point.
     */
    Vector3 center() const {
        return (min + max) * 0.5f;
    }
    
    /**
     * @brief Gets size (extents).
     */
    Vector3 size() const {
        return max - min;
    }
    
    /**
     * @brief Checks if point is inside box.
     */
    bool contains(const Vector3& point) const {
        return point.x >= min.x && point.x <= max.x &&
               point.y >= min.y && point.y <= max.y &&
               point.z >= min.z && point.z <= max.z;
    }
    
    /**
     * @brief Checks if boxes intersect.
     */
    bool intersects(const BoundingBox& other) const {
        return !(max.x < other.min.x || min.x > other.max.x ||
                 max.y < other.min.y || min.y > other.max.y ||
                 max.z < other.min.z || min.z > other.max.z);
    }
    
    /**
     * @brief Ray-box intersection test.
     */
    bool intersectsRay(const Vector3& origin, const Vector3& direction, float& tMin, float& tMax) const {
        tMin = 0.0f;
        tMax = std::numeric_limits<float>::max();
        
        for (int i = 0; i < 3; ++i) {
            float invD = 1.0f / (&direction.x)[i];
            float t0 = ((&min.x)[i] - (&origin.x)[i]) * invD;
            float t1 = ((&max.x)[i] - (&origin.x)[i]) * invD;
            
            if (invD < 0.0f) std::swap(t0, t1);
            
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            
            if (tMax < tMin) return false;
        }
        
        return true;
    }
    
    /**
     * @brief Expands box to include a point (alias of expand).
     */
    void expandToInclude(const Vector3& point) {
        expand(point);
    }

    /**
     * @brief Expands box to include another box (alias of expand).
     */
    void expandToInclude(const BoundingBox& other) {
        expand(other);
    }

    /**
     * @brief True if the box holds no volume (never expanded / inverted).
     */
    bool isEmpty() const {
        return min.x > max.x || min.y > max.y || min.z > max.z;
    }

    /**
     * @brief Slab-based ray/box intersection.
     * @param ray  the ray to test.
     * @param tMin [out] entry distance along the ray (clamped to >= 0).
     * @param tMax [out] exit distance along the ray.
     * @return true if the ray intersects the box in the forward direction.
     */
    bool rayIntersects(const Ray& ray, float& tMin, float& tMax) const {
        tMin = 0.0f;
        tMax = std::numeric_limits<float>::max();

        for (int i = 0; i < 3; ++i) {
            const float o = (&ray.origin.x)[i];
            const float d = (&ray.direction.x)[i];
            const float lo = (&min.x)[i];
            const float hi = (&max.x)[i];

            if (std::abs(d) < 1e-8f) {
                // Ray parallel to slab: no hit if origin outside slab.
                if (o < lo || o > hi) return false;
                continue;
            }
            const float invD = 1.0f / d;
            float t0 = (lo - o) * invD;
            float t1 = (hi - o) * invD;
            if (invD < 0.0f) std::swap(t0, t1);
            tMin = std::max(tMin, t0);
            tMax = std::min(tMax, t1);
            if (tMax < tMin) return false;
        }
        return true;
    }

    /**
     * @brief Sphere/box intersection using the closest-point distance test.
     */
    bool intersectsSphere(const Vector3& center, float radius) const {
        float distSq = 0.0f;
        for (int i = 0; i < 3; ++i) {
            const float c = (&center.x)[i];
            const float lo = (&min.x)[i];
            const float hi = (&max.x)[i];
            if (c < lo) { const float d = lo - c; distSq += d * d; }
            else if (c > hi) { const float d = c - hi; distSq += d * d; }
        }
        return distSq <= radius * radius;
    }

    /**
     * @brief Frustum/box intersection using the p-vertex (positive vertex) test.
     * @return true if the box is inside or intersecting the frustum.
     */
    bool intersectsFrustum(const FrustumPlanes& frustum) const {
        for (const auto& p : frustum.planes) {
            // Pick the box corner farthest along the plane normal.
            const float px = (p.a >= 0.0f) ? max.x : min.x;
            const float py = (p.b >= 0.0f) ? max.y : min.y;
            const float pz = (p.c >= 0.0f) ? max.z : min.z;
            const float dist = p.a * px + p.b * py + p.c * pz + p.d;
            if (dist < 0.0f) return false; // fully outside this plane
        }
        return true;
    }

    /**
     * @brief Gets corner vertices (8 corners).
     */
    std::array<Vector3, 8> getCorners() const {
        return {
            Vector3(min.x, min.y, min.z),
            Vector3(max.x, min.y, min.z),
            Vector3(min.x, max.y, min.z),
            Vector3(max.x, max.y, min.z),
            Vector3(min.x, min.y, max.z),
            Vector3(max.x, min.y, max.z),
            Vector3(min.x, max.y, max.z),
            Vector3(max.x, max.y, max.z)
        };
    }
};

} // namespace nexus::utility::graphics
