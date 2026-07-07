#pragma once

#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/math/matrix4_utils.h>
#include <nexus/utility/math/transform_utils.h>

namespace nexus::utility::math {

/**
 * @brief Camera utilities for view/projection matrices and controllers.
 */
class CameraUtils {
public:
    struct Camera {
        Vector3 position;
        Vector3 target;
        Vector3 up;
        
        float fov = 60.0f;  // degrees
        float aspect = 16.0f / 9.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
        
        Camera()
            : position(0, 0, 5),
              target(0, 0, 0),
              up(Vector3::Up()) {}
    };
    
    /**
     * @brief Creates view matrix.
     */
    static Matrix4 getViewMatrix(const Camera& cam) {
        return Matrix4Utils::lookAt(cam.position, cam.target, cam.up);
    }
    
    /**
     * @brief Creates perspective projection matrix.
     */
    static Matrix4 getProjectionMatrix(const Camera& cam) {
        float fovRadians = cam.fov * (M_PI / 180.0f);
        return Matrix4Utils::perspective(fovRadians, cam.aspect, cam.nearPlane, cam.farPlane);
    }
    
    /**
     * @brief Creates view-projection matrix.
     */
    static Matrix4 getViewProjectionMatrix(const Camera& cam) {
        return Matrix4Utils::multiply(getProjectionMatrix(cam), getViewMatrix(cam));
    }
    
    /**
     * @brief FPS camera controller - moves forward/back/left/right.
     */
    static void moveFPS(Camera& cam, const Vector3& movement) {
        Vector3 forward = (cam.target - cam.position).normalized();
        Vector3 right = Vector3Utils::cross(forward, cam.up).normalized();
        
        cam.position += forward * movement.z;
        cam.position += right * movement.x;
        cam.position.y += movement.y;
        
        cam.target = cam.position + forward;
    }
    
    /**
     * @brief FPS camera rotation (yaw, pitch).
     */
    static void rotateFPS(Camera& cam, float yaw, float pitch) {
        Vector3 forward = (cam.target - cam.position).normalized();
        
        // Apply yaw (rotation around up axis)
        float cosYaw = std::cos(yaw);
        float sinYaw = std::sin(yaw);
        Vector3 newForward(
            forward.x * cosYaw - forward.z * sinYaw,
            forward.y,
            forward.x * sinYaw + forward.z * cosYaw
        );
        
        // Apply pitch
        Vector3 right = Vector3Utils::cross(newForward, cam.up).normalized();
        float cosPitch = std::cos(pitch);
        float sinPitch = std::sin(pitch);
        
        newForward = newForward * cosPitch + cam.up * sinPitch;
        newForward = newForward.normalized();
        
        cam.target = cam.position + newForward;
    }
    
    /**
     * @brief Orbit camera around target.
     */
    static void orbit(Camera& cam, float horizontal, float vertical) {
        Vector3 offset = cam.position - cam.target;
        float radius = offset.length();
        
        // Convert to spherical coordinates
        float theta = std::atan2(offset.x, offset.z);
        float phi = std::acos(offset.y / radius);
        
        // Apply rotation
        theta += horizontal;
        phi += vertical;
        
        // Clamp phi to avoid gimbal lock
        phi = std::clamp(phi, 0.01f, static_cast<float>(M_PI - 0.01f));
        
        // Convert back to Cartesian
        cam.position.x = cam.target.x + radius * std::sin(phi) * std::sin(theta);
        cam.position.y = cam.target.y + radius * std::cos(phi);
        cam.position.z = cam.target.z + radius * std::sin(phi) * std::cos(theta);
    }
    
    /**
     * @brief Zoom (change distance from target).
     */
    static void zoom(Camera& cam, float delta) {
        Vector3 direction = (cam.position - cam.target).normalized();
        float distance = (cam.position - cam.target).length();
        distance = std::max(0.1f, distance + delta);
        cam.position = cam.target + direction * distance;
    }
    
    /**
     * @brief Screen to world ray casting.
     */
    static std::pair<Vector3, Vector3> screenToWorldRay(
        const Camera& cam, float screenX, float screenY, float screenWidth, float screenHeight) {
        
        // Normalized device coordinates
        float ndcX = (2.0f * screenX) / screenWidth - 1.0f;
        float ndcY = 1.0f - (2.0f * screenY) / screenHeight;
        
        // Ray origin is camera position
        Vector3 rayOrigin = cam.position;
        
        // Calculate ray direction (simplified)
        Vector3 forward = (cam.target - cam.position).normalized();
        Vector3 right = Vector3Utils::cross(forward, cam.up).normalized();
        Vector3 up = Vector3Utils::cross(right, forward);
        
        float fovRadians = cam.fov * (M_PI / 180.0f);
        float halfHeight = std::tan(fovRadians / 2.0f);
        float halfWidth = halfHeight * cam.aspect;
        
        Vector3 rayDirection = (forward + right * (ndcX * halfWidth) + up * (ndcY * halfHeight)).normalized();
        
        return {rayOrigin, rayDirection};
    }
};

} // namespace nexus::utility::math
