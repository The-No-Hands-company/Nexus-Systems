#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/graphics/bounding_box.h>
#include <optional>
#include <vector>

namespace nexus::utility::physics {

/**
 * @brief Ray casting utilities for spatial queries.
 */
class RayCaster {
public:
    using Vector3 = nexus::utility::math::Vector3;
    using Vector3Utils = nexus::utility::math::Vector3Utils;
    using BoundingBox = nexus::utility::graphics::BoundingBox;
    struct Ray {
        Vector3 origin;
        Vector3 direction; // Should be normalized
        
        Ray(const Vector3& origin, const Vector3& direction)
            : origin(origin), direction(direction.normalized()) {}
        
        Vector3 pointAt(float t) const {
            return origin + direction * t;
        }
    };

    struct RayHit {
        bool hit;
        float distance;
        Vector3 point;
        Vector3 normal;
        void* userData; // Can store object pointer
        
        RayHit() : hit(false), distance(0), userData(nullptr) {}
    };

    /**
     * @brief Casts ray against AABB.
     */
    static RayHit castAABB(const Ray& ray, const BoundingBox& box) {
        RayHit result;
        float tMin, tMax;
        
        if (box.intersectsRay(ray.origin, ray.direction, tMin, tMax)) {
            result.hit = true;
            result.distance = tMin;
            result.point = ray.pointAt(tMin);
            
            // Calculate normal (simplified - which face was hit)
            Vector3 center = box.center();
            Vector3 localHit = result.point - center;
            Vector3 size = box.size();
            
            // Find dominant axis
            float absX = std::abs(localHit.x / size.x);
            float absY = std::abs(localHit.y / size.y);
            float absZ = std::abs(localHit.z / size.z);
            
            if (absX > absY && absX > absZ) {
                result.normal = Vector3(localHit.x > 0 ? 1 : -1, 0, 0);
            } else if (absY > absZ) {
                result.normal = Vector3(0, localHit.y > 0 ? 1 : -1, 0);
            } else {
                result.normal = Vector3(0, 0, localHit.z > 0 ? 1 : -1);
            }
        }
        
        return result;
    }

    /**
     * @brief Casts ray against sphere.
     */
    static RayHit castSphere(const Ray& ray, const Vector3& center, float radius) {
        RayHit result;
        
        Vector3 oc = ray.origin - center;
        float a = Vector3Utils::dot(ray.direction, ray.direction);
        float b = 2.0f * Vector3Utils::dot(oc, ray.direction);
        float c = Vector3Utils::dot(oc, oc) - radius * radius;
        float discriminant = b * b - 4 * a * c;
        
        if (discriminant >= 0) {
            float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
            if (t >= 0) {
                result.hit = true;
                result.distance = t;
                result.point = ray.pointAt(t);
                result.normal = (result.point - center).normalized();
            }
        }
        
        return result;
    }

    /**
     * @brief Casts ray against triangle.
     */
    static RayHit castTriangle(const Ray& ray, const Vector3& v0, const Vector3& v1, const Vector3& v2) {
        RayHit result;
        
        // Möller–Trumbore intersection algorithm
        Vector3 edge1 = v1 - v0;
        Vector3 edge2 = v2 - v0;
        Vector3 h = Vector3Utils::cross(ray.direction, edge2);
        float a = Vector3Utils::dot(edge1, h);
        
        if (std::abs(a) < 0.00001f) {
            return result; // Ray parallel to triangle
        }
        
        float f = 1.0f / a;
        Vector3 s = ray.origin - v0;
        float u = f * Vector3Utils::dot(s, h);
        
        if (u < 0.0f || u > 1.0f) {
            return result;
        }
        
        Vector3 q = Vector3Utils::cross(s, edge1);
        float v = f * Vector3Utils::dot(ray.direction, q);
        
        if (v < 0.0f || u + v > 1.0f) {
            return result;
        }
        
        float t = f * Vector3Utils::dot(edge2, q);
        
        if (t > 0.00001f) {
            result.hit = true;
            result.distance = t;
            result.point = ray.pointAt(t);
            result.normal = Vector3Utils::cross(edge1, edge2).normalized();
        }
        
        return result;
    }

    /**
     * @brief Finds closest hit from multiple hits.
     */
    static RayHit findClosest(const std::vector<RayHit>& hits) {
        RayHit closest;
        float minDist = std::numeric_limits<float>::max();
        
        for (const auto& hit : hits) {
            if (hit.hit && hit.distance < minDist) {
                minDist = hit.distance;
                closest = hit;
            }
        }
        
        return closest;
    }
};

} // namespace nexus::utility::physics
