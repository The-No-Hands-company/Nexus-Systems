#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Camera
//
//  Perspective and orthographic cameras.  Host-side math only;
//  matrices are uploaded to GPU via uniform buffers.
// ─────────────────────────────────────────────────────────────────────────────
#include <array>
#include <cstdint>
#include <cmath>

namespace nexus::render {

// ── Vec3 / Vec4 — must be declared before Mat4 (Mat4 uses Vec4) ──────────────
struct Vec3 {
    float x = 0.f, y = 0.f, z = 0.f;

    [[nodiscard]] Vec3 operator+(const Vec3& o) const noexcept { return {x+o.x, y+o.y, z+o.z}; }
    [[nodiscard]] Vec3 operator-(const Vec3& o) const noexcept { return {x-o.x, y-o.y, z-o.z}; }
    [[nodiscard]] Vec3 operator*(float s)        const noexcept { return {x*s,   y*s,   z*s};   }
    [[nodiscard]] Vec3 operator/(float s)        const noexcept { float inv=1.f/s; return *this*inv; }
    [[nodiscard]] Vec3 operator-()               const noexcept { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) noexcept { x+=o.x; y+=o.y; z+=o.z; return *this; }
    Vec3& operator-=(const Vec3& o) noexcept { x-=o.x; y-=o.y; z-=o.z; return *this; }
    Vec3& operator*=(float s)       noexcept { x*=s; y*=s; z*=s; return *this; }

    [[nodiscard]] float dot(const Vec3& o)   const noexcept { return x*o.x+y*o.y+z*o.z; }
    [[nodiscard]] Vec3  cross(const Vec3& o) const noexcept {
        return {y*o.z - z*o.y, z*o.x - x*o.z, x*o.y - y*o.x};
    }
    [[nodiscard]] float lengthSq() const noexcept { return dot(*this); }
    [[nodiscard]] float length()   const noexcept { return std::sqrt(lengthSq()); }
    [[nodiscard]] Vec3  normalize() const noexcept {
        float l = length();
        return (l > 1e-8f) ? (*this * (1.f/l)) : Vec3{0.f, 0.f, 1.f};
    }
    bool operator==(const Vec3&) const = default;
};
inline Vec3 operator*(float s, const Vec3& v) noexcept { return v * s; }

struct Vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 1.f;

    [[nodiscard]] Vec4 operator+(const Vec4& o) const noexcept { return {x+o.x, y+o.y, z+o.z, w+o.w}; }
    [[nodiscard]] Vec4 operator-(const Vec4& o) const noexcept { return {x-o.x, y-o.y, z-o.z, w-o.w}; }
    [[nodiscard]] Vec4 operator*(float s)        const noexcept { return {x*s, y*s, z*s, w*s}; }
    [[nodiscard]] float dot(const Vec4& o) const noexcept { return x*o.x+y*o.y+z*o.z+w*o.w; }
    [[nodiscard]] Vec3  xyz() const noexcept { return {x, y, z}; }
    bool operator==(const Vec4&) const = default;
};

// ── Minimal 4×4 matrix (row-major, right-handed, depth [0,1] — Vulkan NDC) ──
struct Mat4 {
    float m[4][4] = {};

    [[nodiscard]] static Mat4 identity() noexcept {
        Mat4 r{};
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.f;
        return r;
    }

    [[nodiscard]] Mat4 operator*(const Mat4& o) const noexcept {
        Mat4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k)
                    r.m[i][j] += m[i][k] * o.m[k][j];
        return r;
    }

    [[nodiscard]] Vec4 operator*(const Vec4& v) const noexcept {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3]*v.w,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3]*v.w,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3]*v.w,
            m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3]*v.w,
        };
    }

    [[nodiscard]] Mat4 transpose() const noexcept {
        Mat4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r.m[i][j] = m[j][i];
        return r;
    }

    // Analytic 4x4 inverse via cofactor expansion (row-major m[row][col]).
    // Returns identity if matrix is singular.
    [[nodiscard]] Mat4 inverse() const noexcept {
        auto sub = [](float a0,float a1,float a2,
                      float b0,float b1,float b2,
                      float c0,float c1,float c2) noexcept -> float {
            return a0*(b1*c2-b2*c1) - a1*(b0*c2-b2*c0) + a2*(b0*c1-b1*c0);
        };
        float c00= sub(m[1][1],m[1][2],m[1][3],m[2][1],m[2][2],m[2][3],m[3][1],m[3][2],m[3][3]);
        float c01=-sub(m[1][0],m[1][2],m[1][3],m[2][0],m[2][2],m[2][3],m[3][0],m[3][2],m[3][3]);
        float c02= sub(m[1][0],m[1][1],m[1][3],m[2][0],m[2][1],m[2][3],m[3][0],m[3][1],m[3][3]);
        float c03=-sub(m[1][0],m[1][1],m[1][2],m[2][0],m[2][1],m[2][2],m[3][0],m[3][1],m[3][2]);
        float det = m[0][0]*c00 + m[0][1]*c01 + m[0][2]*c02 + m[0][3]*c03;
        if (std::abs(det) < 1e-8f) return identity();
        float inv = 1.f / det;
        float c10=-sub(m[0][1],m[0][2],m[0][3],m[2][1],m[2][2],m[2][3],m[3][1],m[3][2],m[3][3]);
        float c11= sub(m[0][0],m[0][2],m[0][3],m[2][0],m[2][2],m[2][3],m[3][0],m[3][2],m[3][3]);
        float c12=-sub(m[0][0],m[0][1],m[0][3],m[2][0],m[2][1],m[2][3],m[3][0],m[3][1],m[3][3]);
        float c13= sub(m[0][0],m[0][1],m[0][2],m[2][0],m[2][1],m[2][2],m[3][0],m[3][1],m[3][2]);
        float c20= sub(m[0][1],m[0][2],m[0][3],m[1][1],m[1][2],m[1][3],m[3][1],m[3][2],m[3][3]);
        float c21=-sub(m[0][0],m[0][2],m[0][3],m[1][0],m[1][2],m[1][3],m[3][0],m[3][2],m[3][3]);
        float c22= sub(m[0][0],m[0][1],m[0][3],m[1][0],m[1][1],m[1][3],m[3][0],m[3][1],m[3][3]);
        float c23=-sub(m[0][0],m[0][1],m[0][2],m[1][0],m[1][1],m[1][2],m[3][0],m[3][1],m[3][2]);
        float c30=-sub(m[0][1],m[0][2],m[0][3],m[1][1],m[1][2],m[1][3],m[2][1],m[2][2],m[2][3]);
        float c31= sub(m[0][0],m[0][2],m[0][3],m[1][0],m[1][2],m[1][3],m[2][0],m[2][2],m[2][3]);
        float c32=-sub(m[0][0],m[0][1],m[0][3],m[1][0],m[1][1],m[1][3],m[2][0],m[2][1],m[2][3]);
        float c33= sub(m[0][0],m[0][1],m[0][2],m[1][0],m[1][1],m[1][2],m[2][0],m[2][1],m[2][2]);
        // A^-1[i][j] = cofactor[j][i] / det  (adjugate = transpose of cofactor matrix)
        Mat4 r{};
        r.m[0][0]=c00*inv; r.m[0][1]=c10*inv; r.m[0][2]=c20*inv; r.m[0][3]=c30*inv;
        r.m[1][0]=c01*inv; r.m[1][1]=c11*inv; r.m[1][2]=c21*inv; r.m[1][3]=c31*inv;
        r.m[2][0]=c02*inv; r.m[2][1]=c12*inv; r.m[2][2]=c22*inv; r.m[2][3]=c32*inv;
        r.m[3][0]=c03*inv; r.m[3][1]=c13*inv; r.m[3][2]=c23*inv; r.m[3][3]=c33*inv;
        return r;
    }
};

// ── Axis-aligned bounding box (world space) ──────────────────────────────────
struct Aabb {
    Vec3 min = {0.f, 0.f, 0.f};
    Vec3 max = {0.f, 0.f, 0.f};

    [[nodiscard]] Vec3 center()  const noexcept { return (min + max) * 0.5f; }
    [[nodiscard]] Vec3 extents() const noexcept { return (max - min) * 0.5f; } // half-size
    bool operator==(const Aabb&) const = default;

    // Axis-aligned bounds of this box after the affine transform m (rotation/scale
    // in the upper 3x3, translation in the last column). Transforms the center
    // fully and grows the half-extents by the absolute linear part (Arvo's method)
    // — the tight world AABB of the transformed box.
    [[nodiscard]] Aabb transformed(const Mat4& m) const noexcept {
        const Vec3 c = center();
        const Vec3 e = extents();
        const Vec3 nc{
            m.m[0][0]*c.x + m.m[0][1]*c.y + m.m[0][2]*c.z + m.m[0][3],
            m.m[1][0]*c.x + m.m[1][1]*c.y + m.m[1][2]*c.z + m.m[1][3],
            m.m[2][0]*c.x + m.m[2][1]*c.y + m.m[2][2]*c.z + m.m[2][3],
        };
        const Vec3 ne{
            std::abs(m.m[0][0])*e.x + std::abs(m.m[0][1])*e.y + std::abs(m.m[0][2])*e.z,
            std::abs(m.m[1][0])*e.x + std::abs(m.m[1][1])*e.y + std::abs(m.m[1][2])*e.z,
            std::abs(m.m[2][0])*e.x + std::abs(m.m[2][1])*e.y + std::abs(m.m[2][2])*e.z,
        };
        return Aabb{ nc - ne, nc + ne };
    }
};

// ── Frustum planes (6 planes for culling) ────────────────────────────────────
//
// Planes are stored with inward-pointing, unit-length normals (see
// Camera::extractFrustum, Gribb-Hartmann). A point p is inside a plane when
// dot(n, p) + d >= 0; inside the frustum when that holds for all six. The
// intersection queries below are conservative: they never reject a volume that
// is actually visible (an AABB test may keep a few false positives near the
// edges, which is the standard space/accuracy trade for broad-phase culling).
struct Frustum {
    Vec4 planes[6]; // each plane: {nx, ny, nz, d} (normal + distance)

    [[nodiscard]] bool containsPoint(const Vec3& p) const noexcept {
        for (const auto& pl : planes) {
            if (pl.x * p.x + pl.y * p.y + pl.z * p.z + pl.w < 0.f) return false;
        }
        return true;
    }

    // True unless the sphere lies entirely outside one plane.
    [[nodiscard]] bool intersectsSphere(const Vec3& center, float radius) const noexcept {
        for (const auto& pl : planes) {
            const float dist = pl.x * center.x + pl.y * center.y + pl.z * center.z + pl.w;
            if (dist < -radius) return false;
        }
        return true;
    }

    // Positive-vertex (p-vertex) test: for each plane pick the box corner
    // furthest along the plane normal; if even that corner is outside, the whole
    // box is outside that plane and therefore the frustum.
    [[nodiscard]] bool intersectsAabb(const Vec3& min, const Vec3& max) const noexcept {
        for (const auto& pl : planes) {
            const float px = (pl.x >= 0.f) ? max.x : min.x;
            const float py = (pl.y >= 0.f) ? max.y : min.y;
            const float pz = (pl.z >= 0.f) ? max.z : min.z;
            if (pl.x * px + pl.y * py + pl.z * pz + pl.w < 0.f) return false;
        }
        return true;
    }
    [[nodiscard]] bool intersectsAabb(const Aabb& box) const noexcept {
        return intersectsAabb(box.min, box.max);
    }
};

// ── Camera uniform data (uploaded directly to GPU) ────────────────────────────
struct CameraUBO {
    Mat4   view;
    Mat4   projection;
    Mat4   viewProj;
    Mat4   invViewProj;
    Vec4   position;         // xyz = world pos, w = near plane
    Vec4   direction;        // xyz = forward,   w = far plane
    Vec4   viewport;         // xy = size, zw = inv size
    float  fovY        = 0.f;
    float  aspectRatio = 0.f;
    float  nearPlane   = 0.1f;
    float  farPlane    = 10000.f;
    // Temporal reprojection support (for TAA / DLSS / XeSS)
    Mat4   prevViewProj;
    Vec4   jitter;           // xy = current jitter, zw = previous
};

// ── Camera modes ─────────────────────────────────────────────────────────────
enum class CameraMode : uint8_t {
    Perspective,
    Orthographic,
};

// ── Camera ────────────────────────────────────────────────────────────────────
class Camera {
public:
    Camera();

    // ── Setup ──────────────────────────────────────────────────────────────
    void setPerspective(float fovYDeg, float aspect, float nearZ, float farZ) noexcept;
    void setOrthographic(float width, float height, float nearZ, float farZ) noexcept;
    void lookAt(Vec3 eye, Vec3 target, Vec3 up = {0.f, 1.f, 0.f}) noexcept;
    void lookAt(Vec3 target, float distance) noexcept; // new overload

    // ── Orbit controls ─────────────────────────────────────────────────────
    void orbit(float dx, float dy) noexcept; // dx, dy in degrees
    void zoom(float amount) noexcept; // positive to zoom in

    // ── Jitter (Halton for TAA / DLSS sample patterns) ─────────────────────
    void setJitter(float jx, float jy) noexcept;
    void clearJitter() noexcept;

    // ── Accessors ──────────────────────────────────────────────────────────
    [[nodiscard]] const CameraUBO& ubo()     const noexcept { return m_ubo; }
    [[nodiscard]] const Frustum&   frustum() const noexcept { return m_frustum; }
    [[nodiscard]] Vec3             position()const noexcept { return {m_ubo.position.x, m_ubo.position.y, m_ubo.position.z}; }
    [[nodiscard]] Vec3             target()  const noexcept { return m_target; }
    [[nodiscard]] Vec3             up()      const noexcept { return m_up; }
    [[nodiscard]] float            distance()const noexcept { return m_distance; }

    // ── Predefined views ───────────────────────────────────────────────────
    void viewTop() noexcept;
    void viewBottom() noexcept;
    void viewLeft() noexcept;
    void viewRight() noexcept;
    void viewFront() noexcept;
    void viewBack() noexcept;
    void viewIsometric() noexcept;

    // ── Called each frame to advance temporal state ───────────────────────
    void tick() noexcept;

private:
    void rebuildMatrices() noexcept;
    void extractFrustum()  noexcept;

    CameraUBO  m_ubo{};
    Frustum    m_frustum{};
    CameraMode m_mode   = CameraMode::Perspective;
    float      m_fovY   = 60.f;
    float      m_aspect = 1.f;
    float      m_near   = 0.1f;
    float      m_far    = 10000.f;
    float      m_orthoW = 1.f;
    float      m_orthoH = 1.f;

    // Cached orbit parameters
    Vec3       m_target{0.f, 0.f, 0.f};
    Vec3       m_up{0.f, 1.f, 0.f};
    float      m_distance = 10.f;
};

} // namespace nexus::render
