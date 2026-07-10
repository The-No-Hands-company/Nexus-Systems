// ─────────────────────────────────────────────────────────────────────────────
//  Camera — host-side math for view/projection matrices
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Camera.h>
#include <cmath>
#include <cstring>
#include <bit>
#include <cstdint>

namespace nexus::render {

static constexpr float kPI = 3.14159265358979323846f;

// Bit-pattern finiteness check: determines if a float is finite (not NaN or Inf).
// Uses IEEE-754 bit layout to detect exponent field == all-1s (special values).
static bool isFiniteFloat(float v) noexcept {
    constexpr uint32_t kExpMask = 0x7F800000u;  // 8 exponent bits (bits 30-23)
    const uint32_t bits = std::bit_cast<uint32_t>(v);
    return (bits & kExpMask) != kExpMask;  // finite if exponent is not all-1s
}

Camera::Camera() : m_target{0,0,0}, m_up{0,1,0}, m_distance{10.0f} {}

void Camera::setPerspective(float fovYDeg, float aspect, float nearZ, float farZ) noexcept
{
    // Reject non-finite camera parameters to prevent NaN/Inf corruption of matrices
    if (!isFiniteFloat(fovYDeg) || !isFiniteFloat(aspect) || 
        !isFiniteFloat(nearZ) || !isFiniteFloat(farZ)) {
        return;
    }

    if (fovYDeg <= 0.0f || fovYDeg >= 180.0f ||
        aspect <= 0.0f ||
        nearZ <= 0.0f ||
        farZ <= nearZ) {
        return;
    }

    m_mode   = CameraMode::Perspective;
    m_fovY   = fovYDeg;
    m_aspect = aspect;
    m_near   = nearZ;
    m_far    = farZ;
    m_ubo.fovY        = fovYDeg;
    m_ubo.aspectRatio = aspect;
    m_ubo.nearPlane   = nearZ;
    m_ubo.farPlane    = farZ;
    rebuildMatrices();
}

void Camera::setOrthographic(float width, float height, float nearZ, float farZ) noexcept
{
    // Reject non-finite camera parameters to prevent NaN/Inf corruption of matrices
    if (!isFiniteFloat(width) || !isFiniteFloat(height) || 
        !isFiniteFloat(nearZ) || !isFiniteFloat(farZ)) {
        return;
    }

    if (width <= 0.0f || height <= 0.0f || nearZ == farZ) {
        return;
    }

    m_mode   = CameraMode::Orthographic;
    m_orthoW = width;
    m_orthoH = height;
    m_near   = nearZ;
    m_far    = farZ;
    rebuildMatrices();
}

void Camera::lookAt(Vec3 eye, Vec3 target, Vec3 up) noexcept
{
    if (!isFiniteFloat(eye.x) || !isFiniteFloat(eye.y) || !isFiniteFloat(eye.z) ||
        !isFiniteFloat(target.x) || !isFiniteFloat(target.y) || !isFiniteFloat(target.z) ||
        !isFiniteFloat(up.x) || !isFiniteFloat(up.y) || !isFiniteFloat(up.z)) {
        return;
    }

    m_ubo.position = {eye.x, eye.y, eye.z, m_near};

    // Forward (right-handed, Z points into the scene = negative Z)
    auto norm3 = [](Vec3 v) -> Vec3 {
        float l = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        if (l < 1e-8f) return {0,0,1};
        return {v.x/l, v.y/l, v.z/l};
    };
    auto cross3 = [](Vec3 a, Vec3 b) -> Vec3 {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    };
    auto dot3 = [](Vec3 a, Vec3 b) -> float { return a.x*b.x + a.y*b.y + a.z*b.z; };

    Vec3 f = norm3({target.x-eye.x, target.y-eye.y, target.z-eye.z});
    Vec3 r = norm3(cross3(f, up));
    Vec3 u = cross3(r, f);

    m_ubo.direction = {f.x, f.y, f.z, m_far};

    Mat4& V = m_ubo.view;
    V = {};  // zero-initialize
    V.m[0][0] =  r.x; V.m[0][1] =  r.y; V.m[0][2] =  r.z; V.m[0][3] = -dot3(r, eye);
    V.m[1][0] =  u.x; V.m[1][1] =  u.y; V.m[1][2] =  u.z; V.m[1][3] = -dot3(u, eye);
    V.m[2][0] = -f.x; V.m[2][1] = -f.y; V.m[2][2] = -f.z; V.m[2][3] =  dot3(f, eye);
    V.m[3][3] = 1.f;

    rebuildMatrices();
    m_target = target;
    m_up = up;
    m_distance = std::sqrt((target.x - eye.x)*(target.x - eye.x) + (target.y - eye.y)*(target.y - eye.y) + (target.z - eye.z)*(target.z - eye.z));
}

void Camera::lookAt(Vec3 target, float distance) noexcept
{
    if (!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z) ||
        !std::isfinite(distance) || distance <= 0.0f) {
        return;
    }

    // We need to know the current orientation to place the eye at a distance in the current direction.
    // If we don't have a current orientation (e.g., first call), we'll use a default forward of (0,0,1) and up (0,1,0).
    // But we have stored m_up and we can compute the current forward from the view matrix? 
    // Instead, we'll use the current m_up and assume we want to keep the same azimuth and polar angles as before.
    // However, we don't store the angles. We'll approximate by using the current forward vector from the view matrix.

    // Get current eye and forward from the view matrix.
    Vec3 eye = {m_ubo.position.x, m_ubo.position.y, m_ubo.position.z};
    Vec3 forward = {-m_ubo.view.m[0][2], -m_ubo.view.m[1][2], -m_ubo.view.m[2][2]}; // third column of view matrix is -forward
    // Normalize forward
    float len = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
    if (len < 1e-6f) {
        // If the view matrix is invalid, use a default forward
        forward = {0.0f, 0.0f, 1.0f};
    } else {
        forward = {forward.x / len, forward.y / len, forward.z / len};
    }

    // Compute the new eye position
    eye = {target.x - distance * forward.x,
           target.y - distance * forward.y,
           target.z - distance * forward.z};

    // Use the current up vector (m_up) to maintain orientation
    lookAt(eye, target, m_up);
}

// Orbit the camera around the target by dx degrees horizontally and dy degrees vertically.
void Camera::orbit(float dx, float dy) noexcept
{
    if (m_distance <= 0.0f) return;

    // Convert degrees to radians
    float dxRad = dx * kPI / 180.0f;
    float dyRad = dy * kPI / 180.0f;

    // Get current eye and target
    Vec3 eye = {m_ubo.position.x, m_ubo.position.y, m_ubo.position.z};
    Vec3 target = m_target;

    // Compute forward vector (from eye to target)
    Vec3 forward = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
    float dist = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
    if (dist < 1e-6f) return;
    forward = {forward.x / dist, forward.y / dist, forward.z / dist};

    // Compute right vector = cross(forward, up)
    Vec3 right = {
        forward.y * m_up.z - forward.z * m_up.y,
        forward.z * m_up.x - forward.x * m_up.z,
        forward.x * m_up.y - forward.y * m_up.x
    };
    float rightLen = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
    if (rightLen < 1e-6f) {
        // If forward and up are parallel, choose a different up
        m_up = {0.0f, 1.0f, 0.0f};
        right = {
            m_up.y * forward.z - m_up.z * forward.y,
            m_up.z * forward.x - forward.x * m_up.z,
            forward.x * m_up.y - forward.y * m_up.x
        };
        rightLen = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
        if (rightLen < 1e-6f) {
            right = {1.0f, 0.0f, 0.0f};
            rightLen = 1.0f;
        }
    }
    right = {right.x / rightLen, right.y / rightLen, right.z / rightLen};

    // Recompute up to be orthogonal to right and forward
    Vec3 up = {
        right.y * forward.z - right.z * forward.y,
        right.z * forward.x - right.x * forward.z,
        right.x * forward.y - right.y * forward.x
    };

    // Rotate the eye around the target:
    //   first, yaw around up by dxRad
    //   then, pitch around right by dyRad
    //   The vector from target to eye is: eye - target = -distance * forward
    Vec3 toEye = {eye.x - target.x, eye.y - target.y, eye.z - target.z};

    // Rotate around up axis
    {
        float cosA = std::cos(dxRad);
        float sinA = std::sin(dxRad);
        // Rodrigues' rotation formula: v' = v*cosA + (k x v)*sinA + k*(k·v)*(1-cosA)
        Vec3 k = up;
        Vec3 kCrossV = {k.y * toEye.z - k.z * toEye.y, k.z * toEye.x - k.x * toEye.z, k.x * toEye.y - k.y * toEye.x};
        float kDotV = k.x * toEye.x + k.y * toEye.y + k.z * toEye.z;
        toEye = {
            toEye.x * cosA + kCrossV.x * sinA + k.x * kDotV * (1.0f - cosA),
            toEye.y * cosA + kCrossV.y * sinA + k.y * kDotV * (1.0f - cosA),
            toEye.z * cosA + kCrossV.z * sinA + k.z * kDotV * (1.0f - cosA)
        };
    }

    // Rotate around right axis
    {
        float cosA = std::cos(dyRad);
        float sinA = std::sin(dyRad);
        Vec3 k = right;
        Vec3 kCrossV = {k.y * toEye.z - k.z * toEye.y, k.z * toEye.x - k.x * toEye.z, k.x * toEye.y - k.y * toEye.x};
        float kDotV = k.x * toEye.x + k.y * toEye.y + k.z * toEye.z;
        toEye = {
            toEye.x * cosA + kCrossV.x * sinA + k.x * kDotV * (1.0f - cosA),
            toEye.y * cosA + kCrossV.y * sinA + k.y * kDotV * (1.0f - cosA),
            toEye.z * cosA + kCrossV.z * sinA + k.z * kDotV * (1.0f - cosA)
        };
    }

    // New eye position
    eye = {target.x + toEye.x, target.y + toEye.y, target.z + toEye.z};

    // Update the camera's position and recompute the view matrix
    lookAt(eye, target, up);
}

// Zoom the camera in/out by moving along the forward direction.
void Camera::zoom(float amount) noexcept
{
    if (m_distance <= 0.0f) return;

    // Get current eye and target
    Vec3 eye = {m_ubo.position.x, m_ubo.position.y, m_ubo.position.z};
    Vec3 target = m_target;

    // Compute forward vector (from eye to target)
    Vec3 forward = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
    float dist = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
    if (dist < 1e-6f) return;
    forward = {forward.x / dist, forward.y / dist, forward.z / dist};

    // Update distance
    m_distance -= amount;
    if (m_distance < 0.1f) m_distance = 0.1f;
    if (m_distance > 1000.0f) m_distance = 1000.0f;

    // New eye position
    eye = {target.x - m_distance * forward.x,
           target.y - m_distance * forward.y,
           target.z - m_distance * forward.z};

    // Update the camera
    lookAt(eye, target, m_up);
}

// Accessors (target/up/distance are defined inline in Camera.h)

// Predefined views
void Camera::viewTop() noexcept
{
    // Look from above along the negative Y axis
    // We want to be above the target looking down.
    // We'll set the eye to be at an offset in the up direction from the target.
    Vec3 upDir = m_up;
    float upLen = std::sqrt(upDir.x*upDir.x + upDir.y*upDir.y + upDir.z*upDir.z);
    if (upLen < 1e-6f) upDir = {0.0f, 1.0f, 0.0f};
    else upDir = {upDir.x / upLen, upDir.y / upLen, upDir.z / upLen};

    Vec3 eye = {
        m_target.x + upDir.x * m_distance,
        m_target.y + upDir.y * m_distance,
        m_target.z + upDir.z * m_distance
    };

    lookAt(eye, m_target, upDir);
}

void Camera::viewBottom() noexcept
{
    // Similar to viewTop but in the opposite direction
    Vec3 upDir = m_up;
    float upLen = std::sqrt(upDir.x*upDir.x + upDir.y*upDir.y + upDir.z*upDir.z);
    if (upLen < 1e-6f) upDir = {0.0f, -1.0f, 0.0f};
    else upDir = {upDir.x / upLen, upDir.y / upLen, upDir.z / upLen};

    Vec3 eye = {
        m_target.x - upDir.x * m_distance,
        m_target.y - upDir.y * m_distance,
        m_target.z - upDir.z * m_distance
    };

    lookAt(eye, m_target, upDir);
}

void Camera::viewLeft() noexcept
{
    // We want to be to the left of the target.
    // We'll compute a left vector as -right, where right = cross(forward, up)
    // We'll get the current forward and up from the camera.

    Vec3 eye = {m_ubo.position.x, m_ubo.position.y, m_ubo.position.z};
    Vec3 target = m_target;
    Vec3 up = m_up;

    // Compute forward
    Vec3 forward = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
    float fLen = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
    if (fLen < 1e-6f) {
        // If we are too close, use a default forward
        forward = {0.0f, 0.0f, 1.0f};
        fLen = 1.0f;
    }
    forward = {forward.x / fLen, forward.y / fLen, forward.z / fLen};

    // Compute right = cross(forward, up)
    Vec3 right = {
        forward.y * up.z - forward.z * up.y,
        forward.z * up.x - forward.x * up.z,
        forward.x * up.y - forward.y * up.x
    };
    float rLen = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
    if (rLen < 1e-6f) {
        // If forward and up are parallel, choose a different up
        up = {0.0f, 1.0f, 0.0f};
        right = {
            forward.y * up.z - forward.z * up.y,
            forward.z * up.x - forward.x * up.z,
            forward.x * up.y - forward.y * up.x
        };
        rLen = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
        if (rLen < 1e-6f) {
            // Still degenerate, use a default
            right = {1.0f, 0.0f, 0.0f};
            rLen = 1.0f;
        }
    }
    right = {right.x / rLen, right.y / rLen, right.z / rLen};

    // Left is -right
    Vec3 left = {-right.x, -right.y, -right.z};

    // Set the eye to be at target + left * distance
    eye = {
        target.x + left.x * m_distance,
        target.y + left.y * m_distance,
        target.z + left.z * m_distance
    };

    // For the up vector in the view, we want to keep the same up as before? 
    // We'll use the current up vector (m_up) as the up for the camera.
    lookAt(eye, target, m_up);
}

void Camera::viewRight() noexcept
{
    // Similar to viewLeft but use right vector
    Vec3 eye = {m_ubo.position.x, m_ubo.position.y, m_ubo.position.z};
    Vec3 target = m_target;
    Vec3 up = m_up;

    Vec3 forward = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
    float fLen = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
    if (fLen < 1e-6f) {
        forward = {0.0f, 0.0f, 1.0f};
        fLen = 1.0f;
    }
    forward = {forward.x / fLen, forward.y / fLen, forward.z / fLen};

    Vec3 right = {
        forward.y * up.z - forward.z * up.y,
        forward.z * up.x - forward.x * up.z,
        forward.x * up.y - forward.y * up.x
    };
    float rLen = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
    if (rLen < 1e-6f) {
        up = {0.0f, 1.0f, 0.0f};
        right = {
            forward.y * up.z - forward.z * up.y,
            forward.z * up.x - forward.x * up.z,
            forward.x * up.y - forward.y * up.x
        };
        rLen = std::sqrt(right.x*right.x + right.y*right.y + right.z*right.z);
        if (rLen < 1e-6f) {
            right = {1.0f, 0.0f, 0.0f};
            rLen = 1.0f;
        }
    }
    right = {right.x / rLen, right.y / rLen, right.z / rLen};

    // Set the eye to be at target + right * distance
    eye = {
        target.x + right.x * m_distance,
        target.y + right.y * m_distance,
        target.z + right.z * m_distance
    };

    lookAt(eye, target, m_up);
}

void Camera::viewFront() noexcept
{
    // We want to be in front of the target (along the negative forward direction? 
    // Actually, we want to be where the camera is looking from, i.e., the eye is in front of the target looking at the target.
    // But note: the camera's forward vector is from eye to target.
    // So to be in front of the target, we want to be in the direction of the forward vector from the target? 
    //   If we are in front of the target, then the target is behind us, so the forward vector (from eye to target) points backwards.
    //   We want to be at a position such that when we look at the target, we are looking forward.
    //   That is, we want to be at: target - forward * distance.
    //   Then the forward vector (from eye to target) is forward.

    Vec3 eye = {m_ubo.position.x, m_ubo.position.y, m_ubo.position.z};
    Vec3 target = m_target;

    Vec3 forward = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
    float fLen = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
    if (fLen < 1e-6f) {
        forward = {0.0f, 0.0f, 1.0f};
        fLen = 1.0f;
    }
    forward = {forward.x / fLen, forward.y / fLen, forward.z / fLen};

    // Set the eye to be at target - forward * distance
    Vec3 newEye = {
        target.x - forward.x * m_distance,
        target.y - forward.y * m_distance,
        target.z - forward.z * m_distance
    };

    lookAt(newEye, target, m_up);
}

void Camera::viewBack() noexcept
{
    // We want to be behind the target, i.e., the eye is in the direction of -forward from the target.
    //   Then the forward vector (from eye to target) is forward.
    //   So we want: eye = target + forward * distance.

    Vec3 eye = {m_ubo.position.x, m_ubo.position.y, m_ubo.position.z};
    Vec3 target = m_target;

    Vec3 forward = {target.x - eye.x, target.y - eye.y, target.z - eye.z};
    float fLen = std::sqrt(forward.x*forward.x + forward.y*forward.y + forward.z*forward.z);
    if (fLen < 1e-6f) {
        forward = {0.0f, 0.0f, 1.0f};
        fLen = 1.0f;
    }
    forward = {forward.x / fLen, forward.y / fLen, forward.z / fLen};

    // Set the eye to be at target + forward * distance
    Vec3 newEye = {
        target.x + forward.x * m_distance,
        target.y + forward.y * m_distance,
        target.z + forward.z * m_distance
    };

    lookAt(newEye, target, m_up);
}

void Camera::viewIsometric() noexcept
{
    // We'll use an isometric view: 45 degrees around the up axis and 35.264 degrees up from the horizon.
    // But we don't have the angles stored. We'll approximate by using a fixed offset.
    // We'll set the eye to be at: target + (x, y, z) where x = d * sin(45) * cos(45), y = d * cos(45), z = d * sin(45) * sin(45)
    //   with d = m_distance.
    //   and we'll keep the up vector as m_up.

    float theta = 45.0f * kPI / 180.0f; // 45 degrees in radians
    float phi = 35.264f * kPI / 180.0f; // approximately 35.264 for isometric

    float sinTheta = std::sin(theta);
    float cosTheta = std::cos(theta);
    float sinPhi = std::sin(phi);
    float cosPhi = std::cos(phi);

    Vec3 offset = {
        m_distance * sinPhi * sinTheta,
        m_distance * cosPhi,
        m_distance * sinPhi * cosTheta
    };

    Vec3 eye = {
        m_target.x + offset.x,
        m_target.y + offset.y,
        m_target.z + offset.z
    };

    lookAt(eye, m_target, m_up);
}

void Camera::setJitter(float jx, float jy) noexcept {
    // Reject non-finite jitter values to prevent temporal reprojection artifacts
    if (!isFiniteFloat(jx) || !isFiniteFloat(jy)) {
        return;
    }

    m_ubo.jitter.z = m_ubo.jitter.x;
    m_ubo.jitter.w = m_ubo.jitter.y;
    m_ubo.jitter.x = jx;
    m_ubo.jitter.y = jy;
}
void Camera::clearJitter() noexcept { m_ubo.jitter = {}; }

void Camera::tick() noexcept {
    m_ubo.prevViewProj = m_ubo.viewProj;
}

void Camera::rebuildMatrices() noexcept
{
    Mat4& P = m_ubo.projection;
    P = {};  // zero-initialize

    if (m_mode == CameraMode::Perspective) {
        float tanHalf = std::tan((m_fovY * kPI / 180.f) * 0.5f);
        float denomA = m_aspect * tanHalf;
        float denomD = m_far - m_near;
        if(denomA < 1e-12f || denomD < 1e-12f) return;
        P.m[0][0] =  1.f / denomA;
        P.m[1][1] = -1.f / tanHalf;
        P.m[2][2] =  m_near / denomD;
        P.m[2][3] =  (m_near * m_far) / denomD;
        P.m[3][2] = -1.f;
    } else {
        if(m_orthoW < 1e-12f || m_orthoH < 1e-12f) return;
        float denomD = m_far - m_near;
        if(denomD < 1e-12f) return;
        P.m[0][0] =  2.f / m_orthoW;
        P.m[1][1] = -2.f / m_orthoH;
        P.m[2][2] =  1.f / denomD;
        P.m[2][3] =  m_far / denomD;
        P.m[3][3] =  1.f;
    }

    // Apply jitter to projection (for TAA / DLSS sample patterns)
    P.m[0][3] += m_ubo.jitter.x;
    P.m[1][3] += m_ubo.jitter.y;

    // Column-vector convention (clip = M * v, as Mat4::operator*(Vec4) implements):
    // the combined transform is P * V so that clip = P * (V * world).
    m_ubo.viewProj    = P * m_ubo.view;
    m_ubo.invViewProj = m_ubo.viewProj.inverse();

    extractFrustum();
}

void Camera::extractFrustum() noexcept
{
    // Gribb-Hartmann plane extraction for the column-vector combined matrix
    // M = P * V (clip = M * world, row k of M giving clip component k). A world
    // point is inside the clip volume when, for Vulkan NDC:
    //     -w <= x <= w,  -w <= y <= w,  0 <= z <= w.
    // Each inequality yields an inward plane built from a row combination of M.
    // Note the NEAR plane is z >= 0 -> row2 alone (Vulkan [0,1] depth), NOT
    // row3 + row2 (that is the OpenGL [-1,1] form). With reversed-Z the row2/
    // row3-row2 pair maps to far/near geometrically, which is fine for culling.
    const auto& m = m_ubo.viewProj.m;

    auto setPlane = [](Vec4& plane, float x, float y, float z, float w) noexcept {
        const float l = std::sqrt(x*x + y*y + z*z);
        if (!isFiniteFloat(l) || l <= 1e-8f) {
            plane = {0.f, 0.f, 0.f, 0.f};
        } else {
            const float inv = 1.f / l;
            plane = {x*inv, y*inv, z*inv, w*inv};
        }
    };

    // left:   x + w >= 0  -> row0 + row3
    setPlane(m_frustum.planes[0], m[0][0]+m[3][0], m[0][1]+m[3][1], m[0][2]+m[3][2], m[0][3]+m[3][3]);
    // right:  w - x >= 0  -> row3 - row0
    setPlane(m_frustum.planes[1], m[3][0]-m[0][0], m[3][1]-m[0][1], m[3][2]-m[0][2], m[3][3]-m[0][3]);
    // bottom: y + w >= 0  -> row1 + row3
    setPlane(m_frustum.planes[2], m[1][0]+m[3][0], m[1][1]+m[3][1], m[1][2]+m[3][2], m[1][3]+m[3][3]);
    // top:    w - y >= 0  -> row3 - row1
    setPlane(m_frustum.planes[3], m[3][0]-m[1][0], m[3][1]-m[1][1], m[3][2]-m[1][2], m[3][3]-m[1][3]);
    // near:   z >= 0      -> row2          (Vulkan clip)
    setPlane(m_frustum.planes[4], m[2][0], m[2][1], m[2][2], m[2][3]);
    // far:    w - z >= 0  -> row3 - row2
    setPlane(m_frustum.planes[5], m[3][0]-m[2][0], m[3][1]-m[2][1], m[3][2]-m[2][2], m[3][3]-m[2][3]);
}

} // namespace nexus::render