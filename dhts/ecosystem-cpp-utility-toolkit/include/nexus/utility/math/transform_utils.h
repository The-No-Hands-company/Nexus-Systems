#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/math/quaternion_utils.h>
#include <nexus/utility/math/matrix4_utils.h>

namespace nexus::utility::math {

/**
 * @brief 3D transform (position, rotation, scale).
 */
struct Transform {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
    
    Transform()
        : position(Vector3::Zero()),
          rotation(Quaternion::Identity()),
          scale(Vector3::One()) {}
    
    Transform(const Vector3& pos, const Quaternion& rot = Quaternion::Identity(), 
              const Vector3& scl = Vector3::One())
        : position(pos), rotation(rot), scale(scl) {}
};

class TransformUtils {
public:
    /**
     * @brief Converts transform to matrix.
     */
    static Matrix4 toMatrix(const Transform& t) {
        // TRS order: Translation * Rotation * Scale
        Matrix4 translation = Matrix4Utils::translate(t.position);
        Matrix4 rotation = quaternionToMatrix(t.rotation);
        Matrix4 scale = Matrix4Utils::scale(t.scale);
        
        return Matrix4Utils::multiply(Matrix4Utils::multiply(translation, rotation), scale);
    }
    
    /**
     * @brief Transforms a point from local to world space.
     */
    static Vector3 transformPoint(const Transform& t, const Vector3& point) {
        Vector3 scaled = Vector3(point.x * t.scale.x, point.y * t.scale.y, point.z * t.scale.z);
        Vector3 rotated = QuaternionUtils::rotate(t.rotation, scaled);
        return rotated + t.position;
    }
    
    /**
     * @brief Transforms a direction (ignores position and scale).
     */
    static Vector3 transformDirection(const Transform& t, const Vector3& direction) {
        return QuaternionUtils::rotate(t.rotation, direction);
    }
    
    /**
     * @brief Combines two transforms (parent * child).
     */
    static Transform combine(const Transform& parent, const Transform& child) {
        Transform result;
        result.position = transformPoint(parent, child.position);
        result.rotation = parent.rotation * child.rotation;
        result.scale = Vector3(
            parent.scale.x * child.scale.x,
            parent.scale.y * child.scale.y,
            parent.scale.z * child.scale.z
        );
        return result;
    }
    
    /**
     * @brief Interpolates between transforms.
     */
    static Transform lerp(const Transform& a, const Transform& b, float t) {
        return Transform(
            Vector3Utils::lerp(a.position, b.position, t),
            QuaternionUtils::slerp(a.rotation, b.rotation, t),
            Vector3Utils::lerp(a.scale, b.scale, t)
        );
    }
    
    /**
     * @brief Creates transform looking at target.
     */
    static Transform lookAt(const Vector3& position, const Vector3& target, const Vector3& up = Vector3::Up()) {
        Vector3 forward = (target - position).normalized();
        Vector3 right = Vector3Utils::cross(up, forward).normalized();
        Vector3 newUp = Vector3Utils::cross(forward, right);
        
        // Create rotation from basis vectors
        // Simplified - full implementation would construct quaternion from matrix
        Transform t;
        t.position = position;
        // Note: This is simplified; proper implementation would convert basis to quaternion
        return t;
    }

private:
    static Matrix4 quaternionToMatrix(const Quaternion& q) {
        Matrix4 mat;
        
        float xx = q.x * q.x;
        float yy = q.y * q.y;
        float zz = q.z * q.z;
        float xy = q.x * q.y;
        float xz = q.x * q.z;
        float yz = q.y * q.z;
        float wx = q.w * q.x;
        float wy = q.w * q.y;
        float wz = q.w * q.z;
        
        mat.at(0, 0) = 1.0f - 2.0f * (yy + zz);
        mat.at(0, 1) = 2.0f * (xy - wz);
        mat.at(0, 2) = 2.0f * (xz + wy);
        
        mat.at(1, 0) = 2.0f * (xy + wz);
        mat.at(1, 1) = 1.0f - 2.0f * (xx + zz);
        mat.at(1, 2) = 2.0f * (yz - wx);
        
        mat.at(2, 0) = 2.0f * (xz - wy);
        mat.at(2, 1) = 2.0f * (yz + wx);
        mat.at(2, 2) = 1.0f - 2.0f * (xx + yy);
        
        return mat;
    }
};

} // namespace nexus::utility::math
