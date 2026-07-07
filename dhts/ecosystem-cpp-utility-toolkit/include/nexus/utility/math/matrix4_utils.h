#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <array>
#include <cmath>

namespace nexus::utility::math {

/**
 * @brief 4x4 matrix for 3D transformations (column-major).
 */
struct Matrix4 {
    std::array<float, 16> m;
    
    Matrix4() : m{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    } {}
    
    float& operator[](size_t i) { return m[i]; }
    const float& operator[](size_t i) const { return m[i]; }
    
    float& at(size_t row, size_t col) { return m[col * 4 + row]; }
    const float& at(size_t row, size_t col) const { return m[col * 4 + row]; }
};

class Matrix4Utils {
public:
    /**
     * @brief Creates identity matrix.
     */
    static Matrix4 identity() {
        return Matrix4();
    }
    
    /**
     * @brief Creates translation matrix.
     */
    static Matrix4 translate(const Vector3& v) {
        Matrix4 mat;
        mat.at(0, 3) = v.x;
        mat.at(1, 3) = v.y;
        mat.at(2, 3) = v.z;
        return mat;
    }
    
    /**
     * @brief Creates scale matrix.
     */
    static Matrix4 scale(const Vector3& v) {
        Matrix4 mat;
        mat.at(0, 0) = v.x;
        mat.at(1, 1) = v.y;
        mat.at(2, 2) = v.z;
        return mat;
    }
    
    /**
     * @brief Creates rotation matrix around X axis.
     */
    static Matrix4 rotateX(float radians) {
        Matrix4 mat;
        float c = std::cos(radians);
        float s = std::sin(radians);
        mat.at(1, 1) = c;
        mat.at(1, 2) = -s;
        mat.at(2, 1) = s;
        mat.at(2, 2) = c;
        return mat;
    }
    
    /**
     * @brief Creates rotation matrix around Y axis.
     */
    static Matrix4 rotateY(float radians) {
        Matrix4 mat;
        float c = std::cos(radians);
        float s = std::sin(radians);
        mat.at(0, 0) = c;
        mat.at(0, 2) = s;
        mat.at(2, 0) = -s;
        mat.at(2, 2) = c;
        return mat;
    }
    
    /**
     * @brief Creates rotation matrix around Z axis.
     */
    static Matrix4 rotateZ(float radians) {
        Matrix4 mat;
        float c = std::cos(radians);
        float s = std::sin(radians);
        mat.at(0, 0) = c;
        mat.at(0, 1) = -s;
        mat.at(1, 0) = s;
        mat.at(1, 1) = c;
        return mat;
    }
    
    /**
     * @brief Matrix multiplication.
     */
    static Matrix4 multiply(const Matrix4& a, const Matrix4& b) {
        Matrix4 result;
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0;
                for (int k = 0; k < 4; ++k) {
                    sum += a.at(row, k) * b.at(k, col);
                }
                result.at(row, col) = sum;
            }
        }
        return result;
    }
    
    /**
     * @brief Creates perspective projection matrix.
     */
    static Matrix4 perspective(float fovY, float aspect, float near, float far) {
        Matrix4 mat;
        float tanHalfFov = std::tan(fovY / 2.0f);
        
        mat.at(0, 0) = 1.0f / (aspect * tanHalfFov);
        mat.at(1, 1) = 1.0f / tanHalfFov;
        mat.at(2, 2) = -(far + near) / (far - near);
        mat.at(2, 3) = -(2.0f * far * near) / (far - near);
        mat.at(3, 2) = -1.0f;
        mat.at(3, 3) = 0.0f;
        
        return mat;
    }
    
    /**
     * @brief Creates orthographic projection matrix.
     */
    static Matrix4 ortho(float left, float right, float bottom, float top, float near, float far) {
        Matrix4 mat;
        mat.at(0, 0) = 2.0f / (right - left);
        mat.at(1, 1) = 2.0f / (top - bottom);
        mat.at(2, 2) = -2.0f / (far - near);
        mat.at(0, 3) = -(right + left) / (right - left);
        mat.at(1, 3) = -(top + bottom) / (top - bottom);
        mat.at(2, 3) = -(far + near) / (far - near);
        return mat;
    }
    
    /**
     * @brief Creates look-at view matrix.
     */
    static Matrix4 lookAt(const Vector3& eye, const Vector3& center, const Vector3& up) {
        Vector3 f = (center - eye).normalized();
        Vector3 s = Vector3Utils::cross(f, up).normalized();
        Vector3 u = Vector3Utils::cross(s, f);
        
        Matrix4 mat;
        mat.at(0, 0) = s.x;
        mat.at(0, 1) = s.y;
        mat.at(0, 2) = s.z;
        mat.at(1, 0) = u.x;
        mat.at(1, 1) = u.y;
        mat.at(1, 2) = u.z;
        mat.at(2, 0) = -f.x;
        mat.at(2, 1) = -f.y;
        mat.at(2, 2) = -f.z;
        mat.at(0, 3) = -Vector3Utils::dot(s, eye);
        mat.at(1, 3) = -Vector3Utils::dot(u, eye);
        mat.at(2, 3) = Vector3Utils::dot(f, eye);
        
        return mat;
    }
};

} // namespace nexus::utility::math
