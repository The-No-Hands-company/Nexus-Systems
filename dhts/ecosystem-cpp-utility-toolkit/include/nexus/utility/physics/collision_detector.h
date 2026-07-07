#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/graphics/bounding_box.h>
#include <optional>

namespace nexus::utility::physics {

/**
 * @brief Collision detection utilities.
 */
class CollisionDetector {
public:
    using Vector3 = nexus::utility::math::Vector3;
    using Vector3Utils = nexus::utility::math::Vector3Utils;
    using BoundingBox = nexus::utility::graphics::BoundingBox;
    struct CollisionResult {
        bool collided;
        Vector3 contactPoint;
        Vector3 contactNormal;
        float penetrationDepth;
    };

    /**
     * @brief Sphere-sphere collision.
     */
    static bool sphereSphere(const Vector3& center1, float radius1,
                            const Vector3& center2, float radius2) {
        float distance = Vector3Utils::distance(center1, center2);
        return distance < (radius1 + radius2);
    }

    /**
     * @brief Sphere-sphere collision with details.
     */
    static CollisionResult sphereSphereDetailed(const Vector3& center1, float radius1,
                                               const Vector3& center2, float radius2) {
        CollisionResult result;
        Vector3 delta = center2 - center1;
        float distance = delta.length();
        float radiusSum = radius1 + radius2;
        
        result.collided = distance < radiusSum;
        if (result.collided) {
            result.penetrationDepth = radiusSum - distance;
            result.contactNormal = delta.normalized();
            result.contactPoint = center1 + result.contactNormal * radius1;
        }
        
        return result;
    }

    /**
     * @brief AABB-AABB collision.
     */
    static bool aabbAabb(const BoundingBox& box1, const BoundingBox& box2) {
        return box1.intersects(box2);
    }

    /**
     * @brief Point-AABB collision.
     */
    static bool pointAabb(const Vector3& point, const BoundingBox& box) {
        return box.contains(point);
    }

    /**
     * @brief Ray-sphere intersection.
     */
    static std::optional<float> raySphere(const Vector3& rayOrigin, const Vector3& rayDir,
                                         const Vector3& sphereCenter, float sphereRadius) {
        Vector3 oc = rayOrigin - sphereCenter;
        float a = Vector3Utils::dot(rayDir, rayDir);
        float b = 2.0f * Vector3Utils::dot(oc, rayDir);
        float c = Vector3Utils::dot(oc, oc) - sphereRadius * sphereRadius;
        float discriminant = b * b - 4 * a * c;
        
        if (discriminant < 0) {
            return std::nullopt;
        }
        
        float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
        return t >= 0 ? std::optional<float>(t) : std::nullopt;
    }

    /**
     * @brief Ray-plane intersection.
     */
    static std::optional<float> rayPlane(const Vector3& rayOrigin, const Vector3& rayDir,
                                        const Vector3& planePoint, const Vector3& planeNormal) {
        float denom = Vector3Utils::dot(rayDir, planeNormal);
        if (std::abs(denom) < 0.00001f) {
            return std::nullopt; // Ray parallel to plane
        }
        
        float t = Vector3Utils::dot(planePoint - rayOrigin, planeNormal) / denom;
        return t >= 0 ? std::optional<float>(t) : std::nullopt;
    }

    /**
     * @brief Capsule-capsule collision (simplified).
     */
    static bool capsuleCapsule(const Vector3& a1, const Vector3& a2, float radiusA,
                              const Vector3& b1, const Vector3& b2, float radiusB) {
        // Simplified: distance between line segments
        float dist = distanceBetweenSegments(a1, a2, b1, b2);
        return dist < (radiusA + radiusB);
    }

private:
    static float distanceBetweenSegments(const Vector3& a1, const Vector3& a2,
                                        const Vector3& b1, const Vector3& b2) {
        Vector3 d1 = a2 - a1;
        Vector3 d2 = b2 - b1;
        Vector3 r = a1 - b1;
        
        float a = Vector3Utils::dot(d1, d1);
        float e = Vector3Utils::dot(d2, d2);
        float f = Vector3Utils::dot(d2, r);
        
        float s, t;
        
        if (a <= 0.00001f && e <= 0.00001f) {
            return r.length();
        }
        
        if (a <= 0.00001f) {
            s = 0.0f;
            t = std::clamp(f / e, 0.0f, 1.0f);
        } else {
            float c = Vector3Utils::dot(d1, r);
            if (e <= 0.00001f) {
                t = 0.0f;
                s = std::clamp(-c / a, 0.0f, 1.0f);
            } else {
                float b = Vector3Utils::dot(d1, d2);
                float denom = a * e - b * b;
                
                if (denom != 0.0f) {
                    s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
                } else {
                    s = 0.0f;
                }
                
                t = (b * s + f) / e;
                
                if (t < 0.0f) {
                    t = 0.0f;
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                } else if (t > 1.0f) {
                    t = 1.0f;
                    s = std::clamp((b - c) / a, 0.0f, 1.0f);
                }
            }
        }
        
        Vector3 c1 = a1 + d1 * s;
        Vector3 c2 = b1 + d2 * t;
        return (c1 - c2).length();
    }
};

} // namespace nexus::utility::physics
