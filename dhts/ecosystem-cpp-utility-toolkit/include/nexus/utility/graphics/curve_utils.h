#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Bezier and NURBS curve evaluation utilities for CAD.
/// Supports Bezier curves of arbitrary degree (via de Casteljau)
/// and NURBS basis function evaluation.

class CurveUtils {
public:
    using Vector3 = nexus::utility::math::Vector3;

    // ── Bezier (de Casteljau) ──────────────────────────────────────────

    /// @brief Evaluate a Bezier curve at parameter t ∈ [0,1] using de Casteljau.
    [[nodiscard]] static Vector3 bezierEval(const std::vector<Vector3>& pts, float t) {
        std::vector<Vector3> tmp = pts;
        for (size_t r = 1; r < tmp.size(); ++r)
            for (size_t i = 0; i < tmp.size() - r; ++i)
                tmp[i] = lerp(tmp[i], tmp[i + 1], t);
        return tmp[0];
    }

    /// @brief Bezier derivative (hodograph) at parameter t.
    [[nodiscard]] static Vector3 bezierDerivative(const std::vector<Vector3>& pts, float t) {
        if (pts.size() < 2) return {0, 0, 0};
        size_t n = pts.size() - 1;
        std::vector<Vector3> diffs;
        diffs.reserve(n);
        for (size_t i = 0; i < n; ++i)
            diffs.push_back({(pts[i + 1].x - pts[i].x) * static_cast<float>(n),
                             (pts[i + 1].y - pts[i].y) * static_cast<float>(n),
                             (pts[i + 1].z - pts[i].z) * static_cast<float>(n)});
        return bezierEval(diffs, t);
    }

    /// @brief Subdivide a Bezier curve at t, returning two new control polygon halves.
    [[nodiscard]] static std::pair<std::vector<Vector3>, std::vector<Vector3>>
    bezierSubdivide(const std::vector<Vector3>& pts, float t) {
        std::vector<std::vector<Vector3>> rows;
        rows.push_back(pts);
        for (size_t r = 1; r < pts.size(); ++r) {
            rows.push_back({});
            for (size_t i = 0; i < pts.size() - r; ++i)
                rows[r].push_back(lerp(rows[r - 1][i], rows[r - 1][i + 1], t));
        }
        std::vector<Vector3> left, right;
        for (size_t i = 0; i < pts.size(); ++i) { left.push_back(rows[i][0]); right.push_back(rows[pts.size() - 1 - i][i]); }
        std::reverse(right.begin(), right.end());
        return {left, right};
    }

    /// @brief Approximate arc length of Bezier via chord-length sampling.
    [[nodiscard]] static float bezierLength(const std::vector<Vector3>& pts, int samples = 32) {
        float len = 0;
        Vector3 prev = bezierEval(pts, 0);
        for (int i = 1; i <= samples; ++i) {
            Vector3 cur = bezierEval(pts, static_cast<float>(i) / samples);
            float dx = cur.x - prev.x, dy = cur.y - prev.y, dz = cur.z - prev.z;
            len += std::sqrt(dx * dx + dy * dy + dz * dz);
            prev = cur;
        }
        return len;
    }

    // ── NURBS Basis ────────────────────────────────────────────────────

    /// @brief Evaluate B-spline basis function N_{i,p}(t) using Cox-de Boor recursion.
    [[nodiscard]] static float bsplineBasis(int i, int p, float t, const std::vector<float>& knots) {
        if (p == 0)
            return (t >= knots[i] && t < knots[i + 1]) ? 1.0f : 0.0f;
        float left = (t - knots[i]) / (knots[i + p] - knots[i] + 1e-12f);
        float right = (knots[i + p + 1] - t) / (knots[i + p + 1] - knots[i + 1] + 1e-12f);
        return left * bsplineBasis(i, p - 1, t, knots) + right * bsplineBasis(i + 1, p - 1, t, knots);
    }

    /// @brief Evaluate a NURBS curve at parameter t.
    [[nodiscard]] static Vector3 nurbsEval(const std::vector<Vector3>& pts,
                                            const std::vector<float>& weights,
                                            const std::vector<float>& knots,
                                            int degree, float t) {
        Vector3 num{0, 0, 0};
        float den = 0;
        for (size_t i = 0; i < pts.size(); ++i) {
            float b = bsplineBasis(static_cast<int>(i), degree, t, knots) * weights[i];
            num.x += pts[i].x * b; num.y += pts[i].y * b; num.z += pts[i].z * b;
            den += b;
        }
        if (den < 1e-10f) return pts[0];
        return {num.x / den, num.y / den, num.z / den};
    }

    /// @brief Generate uniform knot vector for a clamped NURBS curve.
    [[nodiscard]] static std::vector<float> uniformKnots(size_t n, int degree) {
        std::vector<float> k;
        for (int i = 0; i <= degree; ++i) k.push_back(0);
        size_t segs = n - static_cast<size_t>(degree);
        for (size_t i = 1; i < segs; ++i)
            k.push_back(static_cast<float>(i) / static_cast<float>(segs));
        for (int i = 0; i <= degree; ++i) k.push_back(1);
        return k;
    }

    /// @brief Circular arc approximated by a quadratic rational Bezier.
    [[nodiscard]] static std::vector<Vector3> circularArc(const Vector3& center, float radius, float startAngle, float endAngle, const Vector3& normal) {
        Vector3 u = {std::cos(startAngle), std::sin(startAngle), 0};
        Vector3 v = {std::cos(endAngle), std::sin(endAngle), 0};
        float halfAngle = (endAngle - startAngle) * 0.5f;
        float w = std::cos(halfAngle);
        Vector3 mid = {(u.x + v.x) * 0.5f, (u.y + v.y) * 0.5f, 0};
        float midLen = std::sqrt(mid.x * mid.x + mid.y * mid.y);
        mid = {mid.x / midLen * radius / w, mid.y / midLen * radius / w, 0};
        return {{u.x * radius + center.x, u.y * radius + center.y, 0},
                {mid.x + center.x, mid.y + center.y, 0},
                {v.x * radius + center.x, v.y * radius + center.y, 0}};
    }

private:
    [[nodiscard]] static Vector3 lerp(const Vector3& a, const Vector3& b, float t) {
        return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    }
};

} // namespace nexus::utility::graphics
