#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <cmath>

namespace nexus::utility::math {

/**
 * @brief Quaternion for rotation representation.
 */
struct Quaternion {
    float x, y, z, w;
    
    constexpr Quaternion(float x = 0, float y = 0, float z = 0, float w = 1)
        : x(x), y(y), z(z), w(w) {}
    
    Quaternion operator*(const Quaternion& q) const {
        return Quaternion(
            w * q.x + x * q.w + y * q.z - z * q.y,
            w * q.y - x * q.z + y * q.w + z * q.x,
            w * q.z + x * q.y - y * q.x + z * q.w,
            w * q.w - x * q.x - y * q.y - z * q.z
        );
    }
    
    float lengthSquared() const { return x * x + y * y + z * z + w * w; }
    float length() const { return std::sqrt(lengthSquared()); }
    
    Quaternion normalized() const {
        float len = length();
        return len > 0.00001f ? Quaternion(x / len, y / len, z / len, w / len) : Quaternion();
    }
    
    static constexpr Quaternion Identity() { return {0, 0, 0, 1}; }
};

class QuaternionUtils {
public:
    /**
     * @brief Creates quaternion from axis and angle.
     */
    static Quaternion fromAxisAngle(const Vector3& axis, float radians) {
        float halfAngle = radians * 0.5f;
        float s = std::sin(halfAngle);
        Vector3 normalized = axis.normalized();
        
        return Quaternion(
            normalized.x * s,
            normalized.y * s,
            normalized.z * s,
            std::cos(halfAngle)
        );
    }
    
    /**
     * @brief Creates quaternion from Euler angles (pitch, yaw, roll).
     */
    static Quaternion fromEuler(float pitch, float yaw, float roll) {
        float cy = std::cos(yaw * 0.5f);
        float sy = std::sin(yaw * 0.5f);
        float cp = std::cos(pitch * 0.5f);
        float sp = std::sin(pitch * 0.5f);
        float cr = std::cos(roll * 0.5f);
        float sr = std::sin(roll * 0.5f);
        
        return Quaternion(
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy
        );
    }
    
    /**
     * @brief Converts quaternion to Euler angles.
     */
    static Vector3 toEuler(const Quaternion& q) {
        Vector3 euler;
        
        // Roll (x-axis rotation)
        float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
        float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
        euler.x = std::atan2(sinr_cosp, cosr_cosp);
        
        // Pitch (y-axis rotation)
        float sinp = 2.0f * (q.w * q.y - q.z * q.x);
        if (std::abs(sinp) >= 1)
            euler.y = std::copysign(M_PI / 2, sinp);
        else
            euler.y = std::asin(sinp);
        
        // Yaw (z-axis rotation)
        float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
        float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
        euler.z = std::atan2(siny_cosp, cosy_cosp);
        
        return euler;
    }
    
    /**
     * @brief Spherical linear interpolation.
     */
    static Quaternion slerp(const Quaternion& a, const Quaternion& b, float t) {
        Quaternion qa = a.normalized();
        Quaternion qb = b.normalized();
        
        float dot = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
        
        // If dot < 0, negate one quaternion to take shorter path
        if (dot < 0.0f) {
            qb = Quaternion(-qb.x, -qb.y, -qb.z, -qb.w);
            dot = -dot;
        }
        
        if (dot > 0.9995f) {
            // Linear interpolation for very close quaternions
            return Quaternion(
                qa.x + t * (qb.x - qa.x),
                qa.y + t * (qb.y - qa.y),
                qa.z + t * (qb.z - qa.z),
                qa.w + t * (qb.w - qa.w)
            ).normalized();
        }
        
        float theta = std::acos(dot);
        float sinTheta = std::sin(theta);
        float wa = std::sin((1.0f - t) * theta) / sinTheta;
        float wb = std::sin(t * theta) / sinTheta;
        
        return Quaternion(
            qa.x * wa + qb.x * wb,
            qa.y * wa + qb.y * wb,
            qa.z * wa + qb.z * wb,
            qa.w * wa + qb.w * wb
        );
    }
    
    /**
     * @brief Rotates a vector by quaternion.
     */
    static Vector3 rotate(const Quaternion& q, const Vector3& v) {
        Vector3 u(q.x, q.y, q.z);
        float s = q.w;
        
        return u * (2.0f * Vector3Utils::dot(u, v))
             + v * (s * s - Vector3Utils::dot(u, u))
             + Vector3Utils::cross(u, v) * (2.0f * s);
    }
};

} // namespace nexus::utility::math
