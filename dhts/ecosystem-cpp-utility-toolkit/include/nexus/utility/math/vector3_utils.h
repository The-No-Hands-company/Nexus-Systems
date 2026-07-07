#pragma once

#include <cmath>
#include <algorithm>

namespace nexus::utility::math {

/**
 * @brief 3D vector utilities.
 */
struct Vector3 {
    float x, y, z;
    
    constexpr Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    
    Vector3 operator+(const Vector3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vector3 operator-(const Vector3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vector3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vector3 operator/(float s) const { return {x / s, y / s, z / s}; }
    
    Vector3& operator+=(const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vector3& operator-=(const Vector3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    
    float lengthSquared() const { return x * x + y * y + z * z; }
    float length() const { return std::sqrt(lengthSquared()); }
    
    Vector3 normalized() const {
        float len = length();
        return len > 0.00001f ? (*this / len) : Vector3();
    }
    
    static constexpr Vector3 Zero() { return {0, 0, 0}; }
    static constexpr Vector3 One() { return {1, 1, 1}; }
    static constexpr Vector3 Up() { return {0, 1, 0}; }
    static constexpr Vector3 Right() { return {1, 0, 0}; }
    static constexpr Vector3 Forward() { return {0, 0, 1}; }
};

class Vector3Utils {
public:
    /**
     * @brief Dot product.
     */
    static float dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    
    /**
     * @brief Cross product.
     */
    static Vector3 cross(const Vector3& a, const Vector3& b) {
        return Vector3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        );
    }
    
    /**
     * @brief Distance between two points.
     */
    static float distance(const Vector3& a, const Vector3& b) {
        return (b - a).length();
    }
    
    /**
     * @brief Angle between vectors in radians.
     */
    static float angle(const Vector3& a, const Vector3& b) {
        float d = dot(a.normalized(), b.normalized());
        return std::acos(std::clamp(d, -1.0f, 1.0f));
    }
    
    /**
     * @brief Linear interpolation.
     */
    static Vector3 lerp(const Vector3& a, const Vector3& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return a + (b - a) * t;
    }
    
    /**
     * @brief Projects vector a onto b.
     */
    static Vector3 project(const Vector3& a, const Vector3& b) {
        float d = dot(b, b);
        if (d < 0.00001f) return Vector3::Zero();
        return b * (dot(a, b) / d);
    }
    
    /**
     * @brief Reflects vector off a surface with given normal.
     */
    static Vector3 reflect(const Vector3& v, const Vector3& normal) {
        return v - normal * (2.0f * dot(v, normal));
    }
    
    /**
     * @brief Clamps vector magnitude.
     */
    static Vector3 clampMagnitude(const Vector3& v, float maxLength) {
        float len = v.length();
        if (len > maxLength && len > 0.00001f) {
            return v * (maxLength / len);
        }
        return v;
    }
};

} // namespace nexus::utility::math
