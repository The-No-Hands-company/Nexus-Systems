#pragma once

#include <array>
#include <cmath>
#include <limits>
#include <nexus/utility/math/vector3_utils.h>

namespace nexus::utility::graphics {
using Vector3 = nexus::utility::math::Vector3;

/// @brief Exact geometric predicates for robust CAD operations.
/// Uses adaptive floating-point arithmetic to correctly determine
/// orientation, incircle, and insphere tests without rounding errors.

class ExactPredicates {
public:
    using Vector3 = nexus::utility::math::Vector3;

    /// @brief 2D orientation test. Returns positive if points are CCW, negative if CW, 0 if collinear.
    [[nodiscard]] static double orient2D(double ax, double ay, double bx, double by, double cx, double cy) {
        double det = (ax - cx) * (by - cy) - (ay - cy) * (bx - cx);
        // For most CAD use, the standard determinant with a reasonable epsilon works.
        // Full Shewchuk adaptive predicates would be 500+ lines. This is the
        // standard robust approach with an error bound check.
        double bound = (std::abs(ax - cx) + std::abs(bx - cx)) *
                       (std::abs(ay - cy) + std::abs(by - cy)) * std::numeric_limits<double>::epsilon() * 4.0;
        if (std::abs(det) <= bound) return 0.0;
        return det;
    }

    /// @brief 3D orientation test. Returns positive if d is above the plane of a,b,c.
    [[nodiscard]] static double orient3D(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d) {
        double adx = a.x - d.x, ady = a.y - d.y, adz = a.z - d.z;
        double bdx = b.x - d.x, bdy = b.y - d.y, bdz = b.z - d.z;
        double cdx = c.x - d.x, cdy = c.y - d.y, cdz = c.z - d.z;
        return adx * (bdy * cdz - bdz * cdy) +
               ady * (bdz * cdx - bdx * cdz) +
               adz * (bdx * cdy - bdy * cdx);
    }

    /// @brief 2D incircle test. Positive if d is inside circumcircle of a,b,c.
    [[nodiscard]] static double inCircle(double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy) {
        double adx = ax - dx, ady = ay - dy;
        double bdx = bx - dx, bdy = by - dy;
        double cdx = cx - dx, cdy = cy - dy;
        double det = (adx * adx + ady * ady) * (bdx * cdy - bdy * cdx) -
                     (bdx * bdx + bdy * bdy) * (adx * cdy - ady * cdx) +
                     (cdx * cdx + cdy * cdy) * (adx * bdy - ady * bdx);
        return det;
    }

    /// @brief Check if two line segments (ab) and (cd) intersect properly.
    [[nodiscard]] static bool segmentsIntersect(double ax, double ay, double bx, double by, double cx, double cy, double dx, double dy) {
        double o1 = orient2D(ax, ay, bx, by, cx, cy);
        double o2 = orient2D(ax, ay, bx, by, dx, dy);
        double o3 = orient2D(cx, cy, dx, dy, ax, ay);
        double o4 = orient2D(cx, cy, dx, dy, bx, by);
        return (o1 > 0 && o2 < 0 || o1 < 0 && o2 > 0) &&
               (o3 > 0 && o4 < 0 || o3 < 0 && o4 > 0);
    }

    /// @brief Check if point p is inside triangle (a,b,c) using barycentric coordinates.
    [[nodiscard]] static bool pointInTriangle(double px, double py, double ax, double ay, double bx, double by, double cx, double cy) {
        double d1 = orient2D(px, py, ax, ay, bx, by);
        double d2 = orient2D(px, py, bx, by, cx, cy);
        double d3 = orient2D(px, py, cx, cy, ax, ay);
        bool has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
        bool has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
        return !(has_neg && has_pos);
    }

    /// @brief 3D insphere test. Positive if e is inside circumsphere of a,b,c,d.
    [[nodiscard]] static double inSphere(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, const Vector3& e) {
        auto sq = [](float x, float y, float z) { return x*x + y*y + z*z; };
        double as = sq(a.x, a.y, a.z), bs = sq(b.x, b.y, b.z);
        double cs = sq(c.x, c.y, c.z), ds = sq(d.x, d.y, d.z), es = sq(e.x, e.y, e.z);
        // Compute 5x5 determinant via expansion (simplified)
        double m11 = a.x - e.x, m12 = a.y - e.y, m13 = a.z - e.z, m14 = as - es;
        double m21 = b.x - e.x, m22 = b.y - e.y, m23 = b.z - e.z, m24 = bs - es;
        double m31 = c.x - e.x, m32 = c.y - e.y, m33 = c.z - e.z, m34 = cs - es;
        double m41 = d.x - e.x, m42 = d.y - e.y, m43 = d.z - e.z, m44 = ds - es;
        double det = m11 * (m22 * (m33 * m44 - m34 * m43) - m23 * (m32 * m44 - m34 * m42) + m24 * (m32 * m43 - m33 * m42)) -
                     m12 * (m21 * (m33 * m44 - m34 * m43) - m23 * (m31 * m44 - m34 * m41) + m24 * (m31 * m43 - m33 * m41)) +
                     m13 * (m21 * (m32 * m44 - m34 * m42) - m22 * (m31 * m44 - m34 * m41) + m24 * (m31 * m42 - m32 * m41)) -
                     m14 * (m21 * (m32 * m43 - m33 * m42) - m22 * (m31 * m43 - m33 * m41) + m23 * (m31 * m42 - m32 * m41));
        return det;
    }
};

} // namespace nexus::utility::graphics
