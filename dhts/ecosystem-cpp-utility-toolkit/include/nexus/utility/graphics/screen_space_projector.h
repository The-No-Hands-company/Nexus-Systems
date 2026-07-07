#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <sstream>
#include <string>
#include <nexus/utility/math/vector3_utils.h>
#include <nexus/utility/math/matrix4_utils.h>
#include <nexus/utility/graphics/bounding_box.h>

namespace nexus::utility::graphics {

/// @brief Projects 3D world-space coordinates to 2D screen-space pixels.
/// Handles model, view, projection, and viewport transforms.
/// Essential for matching "what the math says" with "what the screen shows."

class ScreenSpaceProjector {
public:
    using Vector3 = nexus::utility::math::Vector3;
    using Vector4 = std::array<float, 4>;
    using Matrix4 = std::array<float, 16>;

    struct Viewport { int x, y, width, height; };
    struct ScreenPoint { int x; int y; float depth; bool behind; };

    ScreenSpaceProjector() { setIdentity(); }

    /// @brief Set viewport dimensions (pixels).
    void setViewport(int w, int h, int x = 0, int y = 0) {
        vp_ = {x, y, w, h};
    }

    /// @brief Set perspective projection matrix (column-major, OpenGL convention).
    void setProjection(const Matrix4& proj) { proj_ = proj; }

    /// @brief Set view (camera) matrix.
    void setView(const Matrix4& view) { view_ = view; }

    /// @brief Set model (world) matrix.
    void setModel(const Matrix4& model) { model_ = model; }

    /// @brief Build perspective projection from FOV.
    void setPerspective(float fovY, float aspect, float near, float far) {
        float f = 1.0f / std::tan(fovY * 0.5f);
        proj_ = {f/aspect,0,0,0, 0,f,0,0, 0,0,(far+near)/(near-far),-1, 0,0,(2*far*near)/(near-far),0};
    }

    /// @brief Build look-at view matrix.
    void setLookAt(const Vector3& eye, const Vector3& center, const Vector3& up) {
        Vector3 f = {(center.x-eye.x), (center.y-eye.y), (center.z-eye.z)};
        float fl = std::sqrt(f.x*f.x + f.y*f.y + f.z*f.z);
        f = {f.x/fl, f.y/fl, f.z/fl};
        Vector3 s = {f.y*up.z - f.z*up.y, f.z*up.x - f.x*up.z, f.x*up.y - f.y*up.x};
        float sl = std::sqrt(s.x*s.x + s.y*s.y + s.z*s.z);
        s = {s.x/sl, s.y/sl, s.z/sl};
        Vector3 u = {s.y*f.z - s.z*f.y, s.z*f.x - s.x*f.z, s.x*f.y - s.y*f.x};
        view_ = {s.x,u.x,-f.x,0, s.y,u.y,-f.y,0, s.z,u.z,-f.z,0,
                 -(s.x*eye.x+s.y*eye.y+s.z*eye.z), -(u.x*eye.x+u.y*eye.y+u.z*eye.z), f.x*eye.x+f.y*eye.y+f.z*eye.z, 1};
    }

    /// @brief Set identity for all matrices.
    void setIdentity() {
        model_ = view_ = proj_ = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    }

    /// @brief Project a 3D world point to 2D screen coordinates.
    /// Returns {x, y, depth, behind} where x,y are pixel coordinates.
    [[nodiscard]] ScreenPoint project(const Vector3& world) const {
        Vector4 clip = transformPoint(model_, world);
        clip = transformPoint(view_, {clip[0], clip[1], clip[2]});
        clip = transformVec(proj_, clip);

        if (std::abs(clip[3]) < 1e-10f) return {0, 0, 0, true};

        float invW = 1.0f / clip[3];
        float ndcX = clip[0] * invW;
        float ndcY = clip[1] * invW;
        float ndcZ = clip[2] * invW;

        if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f)
            return {0, 0, ndcZ, true};

        int px = vp_.x + static_cast<int>((ndcX * 0.5f + 0.5f) * vp_.width);
        int py = vp_.y + static_cast<int>((1.0f - (ndcY * 0.5f + 0.5f)) * vp_.height);
        return {px, py, ndcZ * 0.5f + 0.5f, false};
    }

    /// @brief Project multiple points and return all screen coordinates.
    [[nodiscard]] std::vector<ScreenPoint> projectMany(const std::vector<Vector3>& points) const {
        std::vector<ScreenPoint> result; result.reserve(points.size());
        for (auto& p : points) result.push_back(project(p));
        return result;
    }

    /// @brief Compute the screen-space bounding rectangle for a 3D bounding box.
    [[nodiscard]] std::array<int, 4> projectBox(const Vector3& bmin, const Vector3& bmax) const {
        Vector3 corners[8] = {
            {bmin.x,bmin.y,bmin.z},{bmax.x,bmin.y,bmin.z},{bmin.x,bmax.y,bmin.z},{bmax.x,bmax.y,bmin.z},
            {bmin.x,bmin.y,bmax.z},{bmax.x,bmin.y,bmax.z},{bmin.x,bmax.y,bmax.z},{bmax.x,bmax.y,bmax.z}
        };
        int sminX = INT_MAX, sminY = INT_MAX, smaxX = INT_MIN, smaxY = INT_MIN;
        for (auto& c : corners) {
            auto sp = project(c);
            if (sp.behind) continue;
            sminX = std::min(sminX, sp.x); sminY = std::min(sminY, sp.y);
            smaxX = std::max(smaxX, sp.x); smaxY = std::max(smaxY, sp.y);
        }
        return {sminX, sminY, smaxX - sminX, smaxY - sminY};
    }

    /// @brief Unproject screen coordinates back to a world-space ray.
    [[nodiscard]] Ray unproject(int sx, int sy) const {
        float ndcX = (2.0f * (sx - vp_.x)) / vp_.width - 1.0f;
        float ndcY = 1.0f - (2.0f * (sy - vp_.y)) / vp_.height;
        Vector4 near = {ndcX, ndcY, -1.0f, 1.0f};
        Vector4 far  = {ndcX, ndcY,  1.0f, 1.0f};

        Matrix4 invVP = invert(multiply(proj_, view_));
        near = transformVec(invVP, near); far = transformVec(invVP, far);
        float nw = 1.0f/near[3], fw = 1.0f/far[3];
        Vector3 origin = {near[0]*nw, near[1]*nw, near[2]*nw};
        Vector3 dir = {far[0]*fw - origin.x, far[1]*fw - origin.y, far[2]*fw - origin.z};
        float dl = std::sqrt(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        return {origin, {dir.x/dl, dir.y/dl, dir.z/dl}};
    }

    [[nodiscard]] const Matrix4& getModel() const { return model_; }
    [[nodiscard]] const Matrix4& getView() const { return view_; }
    [[nodiscard]] const Matrix4& getProjection() const { return proj_; }
    [[nodiscard]] Matrix4 getMVP() const { return multiply(proj_, multiply(view_, model_)); }

private:
    Viewport vp_{0, 0, 1920, 1080};
    Matrix4 model_, view_, proj_;

    [[nodiscard]] static Vector4 transformPoint(const Matrix4& m, const Vector3& v) {
        return {m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12], m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13],
                m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14], m[3]*v.x+m[7]*v.y+m[11]*v.z+m[15]};
    }
    [[nodiscard]] static Vector4 transformVec(const Matrix4& m, const Vector4& v) {
        return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12]*v[3], m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13]*v[3],
                m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]*v[3], m[3]*v[0]+m[7]*v[1]+m[11]*v[2]+m[15]*v[3]};
    }
    [[nodiscard]] static Matrix4 multiply(const Matrix4& a, const Matrix4& b) {
        Matrix4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                for (int k = 0; k < 4; ++k) r[i + j*4] += a[i + k*4] * b[k + j*4];
        return r;
    }
    [[nodiscard]] static Matrix4 invert(const Matrix4& m) {
        // Adjugate method for 4x4
        float det = determinant(m);
        Matrix4 r{};
        for (int i = 0; i < 4; ++i)
            for (int j = 0; j < 4; ++j)
                r[j*4 + i] = cofactor(m, i, j) / det;
        return r;
    }
    [[nodiscard]] static float determinant(const Matrix4& m) {
        return m[0]*cofactor(m,0,0) + m[4]*cofactor(m,0,1) + m[8]*cofactor(m,0,2) + m[12]*cofactor(m,0,3);
    }
    [[nodiscard]] static float cofactor(const Matrix4& m, int row, int col) {
        float s[9]; int idx = 0;
        for (int i = 0; i < 4; ++i) {
            if (i == row) continue;
            for (int j = 0; j < 4; ++j) {
                if (j == col) continue;
                s[idx++] = m[i * 4 + j];
            }
        }
        float det = s[0]*(s[4]*s[8]-s[5]*s[7]) - s[1]*(s[3]*s[8]-s[5]*s[6]) + s[2]*(s[3]*s[7]-s[4]*s[6]);
        return ((row + col) % 2 == 0) ? det : -det;
    }
};

} // namespace nexus::utility::graphics
