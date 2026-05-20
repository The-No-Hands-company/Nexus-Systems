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
        // Reversed-Z perspective (better depth precision): maps far -> 0, near -> 1
        // Right-handed, Vulkan NDC (Y flipped)
        P.m[0][0] =  1.f / (m_aspect * tanHalf);
        P.m[1][1] = -1.f / tanHalf;    // Vulkan Y flip
        P.m[2][2] =  m_near / (m_near - m_far);   // reversed-Z
        P.m[2][3] = -1.f;
        P.m[3][2] =  (m_near * m_far) / (m_near - m_far);
    } else {
        P.m[0][0] =  2.f / m_orthoW;
        P.m[1][1] = -2.f / m_orthoH;  // Vulkan Y flip
        P.m[2][2] =  1.f / (m_near - m_far);
        P.m[2][3] =  m_near / (m_near - m_far);
        P.m[3][3] =  1.f;
    }

    // Apply jitter to projection (for TAA / DLSS sample patterns)
    P.m[0][3] += m_ubo.jitter.x;
    P.m[1][3] += m_ubo.jitter.y;

    m_ubo.viewProj    = m_ubo.view * P;
    m_ubo.invViewProj = m_ubo.viewProj.inverse();

    extractFrustum();
}

void Camera::extractFrustum() noexcept
{
    // Extract 6 frustum planes from the combined VP matrix (Gribb-Hartmann method)
    const auto& m = m_ubo.viewProj.m;

    auto extractPlane = [&](int a, int b, float sign, Vec4& plane) {
        plane.x = m[3][0] + sign * m[a][0];
        plane.y = m[3][1] + sign * m[a][1];
        plane.z = m[3][2] + sign * m[a][2];
        plane.w = m[3][3] + sign * m[a][3];
        float l = std::sqrt(plane.x*plane.x + plane.y*plane.y + plane.z*plane.z);
        if (l > 1e-8f) { plane.x/=l; plane.y/=l; plane.z/=l; plane.w/=l; }
        (void)b;
    };

    extractPlane(0,  0,  1.f, m_frustum.planes[0]); // left
    extractPlane(0,  0, -1.f, m_frustum.planes[1]); // right
    extractPlane(1,  1,  1.f, m_frustum.planes[2]); // bottom
    extractPlane(1,  1, -1.f, m_frustum.planes[3]); // top
    extractPlane(2,  2,  1.f, m_frustum.planes[4]); // near
    extractPlane(2,  2, -1.f, m_frustum.planes[5]); // far
}

} // namespace nexus::render
