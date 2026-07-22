#include <nexus/geometry/BezierCurve.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// ── BezierCurve ─────────────────────────────────────────────────────────────

NurbsCurve BezierCurve::fromControlPoints(const std::vector<Vec3>& controlPoints)
{
    if (controlPoints.size() < 2) return {};
    int32_t n = static_cast<int32_t>(controlPoints.size());
    int32_t deg = n - 1;

    // Clamped knot vector: deg+1 zeros, then deg+1 ones.
    std::vector<float> knots(static_cast<size_t>(n + deg + 1));
    for (int32_t i = 0; i <= deg; ++i) knots[static_cast<size_t>(i)] = 0.f;
    for (int32_t i = 1; i < n - deg; ++i) {
        // n - deg = 1, so this loop never executes for Bezier.
        knots[static_cast<size_t>(deg + i)] = static_cast<float>(i) / static_cast<float>(n - deg);
    }
    for (int32_t i = 0; i <= deg; ++i) knots[knots.size() - 1 - static_cast<size_t>(i)] = 1.f;

    return NurbsCurve(deg, std::move(knots), controlPoints);
}

NurbsCurve BezierCurve::fromControlPoints(const std::vector<Vec3>& controlPoints,
                                            const std::vector<float>& weights)
{
    if (controlPoints.size() < 2) return {};
    int32_t n = static_cast<int32_t>(controlPoints.size());
    int32_t deg = n - 1;

    std::vector<float> knots(static_cast<size_t>(n + deg + 1));
    for (int32_t i = 0; i <= deg; ++i) knots[static_cast<size_t>(i)] = 0.f;
    for (int32_t i = 0; i <= deg; ++i) knots[knots.size() - 1 - static_cast<size_t>(i)] = 1.f;

    return NurbsCurve(deg, std::move(knots), controlPoints, weights);
}

// ── HermiteCurve ────────────────────────────────────────────────────────────

NurbsCurve HermiteCurve::fromEndpoints(const Vec3& p0, const Vec3& p1,
                                         const Vec3& t0, const Vec3& t1)
{
    // Cubic Hermite → cubic Bezier CPs:
    // B0 = P0, B1 = P0 + T0/3, B2 = P1 - T1/3, B3 = P1
    return BezierCurve::fromControlPoints({
        p0,
        p0 + t0 * (1.f / 3.f),
        p1 - t1 * (1.f / 3.f),
        p1
    });
}

// ── CatmullRomCurve ─────────────────────────────────────────────────────────

NurbsCurve CatmullRomCurve::fromPoints(const std::vector<Vec3>& points)
{
    if (points.size() < 2) return {};
    if (points.size() == 2) {
        // Degenerate: just a line.
        return BezierCurve::fromControlPoints(points);
    }

    const size_t n = points.size();
    // For a Catmull-Rom through n points, the cubic Bezier CP count is
    // 4 per segment * (n-1) segments, but shared endpoints collapse:
    // total CPs = 3*(n-1) + 1 = 3n - 2
    std::vector<Vec3> cps;
    cps.reserve(3 * n - 2);

    for (size_t i = 0; i < n - 1; ++i) {
        // Catmull-Rom tangents at p[i] and p[i+1] for segment i.
        Vec3 tanStart, tanEnd;

        // Tangent at segment start (p[i]).
        if (i == 0) {
            tanStart = points[1] - points[0];            // forward diff
        } else {
            tanStart = (points[i + 1] - points[i - 1]) * 0.5f;  // central diff
        }

        // Tangent at segment end (p[i+1]).
        if (i == n - 2) {
            tanEnd = points[n - 1] - points[n - 2];      // backward diff
        } else {
            tanEnd = (points[i + 2] - points[i]) * 0.5f;  // central diff
        }

        Vec3 b0 = points[i];
        Vec3 b1 = b0 + tanStart * (1.f / 3.f);
        Vec3 b2 = points[i + 1] - tanEnd * (1.f / 3.f);
        Vec3 b3 = points[i + 1];

        if (i == 0) {
            cps.push_back(b0);
            cps.push_back(b1);
        } else {
            cps.push_back(b1);
        }
        cps.push_back(b2);
        cps.push_back(b3);
    }

    int32_t nCPs = static_cast<int32_t>(cps.size());
    int32_t deg = 3;
    std::vector<float> knots(static_cast<size_t>(nCPs + deg + 1));
    for (int32_t j = 0; j <= deg; ++j) knots[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nCPs - deg; ++j)
        knots[static_cast<size_t>(deg + j)] = static_cast<float>(j) / static_cast<float>(nCPs - deg);
    for (int32_t j = 0; j <= deg; ++j) knots[knots.size() - 1 - static_cast<size_t>(j)] = 1.f;

    return NurbsCurve(deg, std::move(knots), std::move(cps));
}

// ── ConicCurve ──────────────────────────────────────────────────────────────

namespace {

// Build a local frame from a normal vector.  Returns orthonormal basis (u, v)
// in the plane perpendicular to 'normal'.
std::pair<Vec3, Vec3> makePlaneFrame(const Vec3& normal) noexcept
{
    Vec3 n = normal.normalize();
    Vec3 u;
    if (std::abs(n.x) < 0.9f)
        u = Vec3{1.f, 0.f, 0.f}.cross(n).normalize();
    else
        u = Vec3{0.f, 1.f, 0.f}.cross(n).normalize();
    Vec3 v = n.cross(u).normalize();
    return {u, v};
}

} // namespace

NurbsCurve ConicCurve::circle(const Vec3& center, const Vec3& normal, float radius)
{
    if (radius <= 0.f) return {};
    auto [u, v] = makePlaneFrame(normal);

    float w = 0.70710678f; // cos(45°) = √2/2
    return NurbsCurve(2,
        {0.f,0.f,0.f, 0.25f,0.25f, 0.5f,0.5f, 0.75f,0.75f, 1.f,1.f,1.f},
        {
            center + u * radius,
            center + (u + v) * radius,
            center + v * radius,
            center + (-u + v) * radius,
            center - u * radius,
            center + (-u - v) * radius,
            center - v * radius,
            center + (u - v) * radius,
            center + u * radius
        },
        {1.f, w, 1.f, w, 1.f, w, 1.f, w, 1.f});
}

NurbsCurve ConicCurve::arc(const Vec3& center, const Vec3& normal,
                             const Vec3& startDir, float radius, float angleDeg)
{
    if (radius <= 0.f || angleDeg <= 0.f) return {};

    auto [u, v] = makePlaneFrame(normal);
    float halfAngle = angleDeg * 0.5f * (static_cast<float>(std::numbers::pi) / 180.f);
    float w = std::cos(halfAngle);

    Vec3 p0 = center + startDir.normalize() * radius;
    float fullAngle = angleDeg * (static_cast<float>(std::numbers::pi) / 180.f);
    Vec3 p2 = center + (u * std::cos(fullAngle) + v * std::sin(fullAngle)) * radius;

    // Mid control point: intersection of tangents at p0 and p2.
    Vec3 tan0 = normal.cross(startDir.normalize());
    Vec3 tan2 = normal.cross((p2 - center).normalize());

    // Intersection: p0 + t0 * tan0 = p2 + t2 * tan2
    // Solve 2D in the plane (u, v).
    Vec3 d = p2 - p0;
    float denom = tan2.x * tan0.y - tan2.y * tan0.x;
    // Check if lines are parallel (denominator near zero) with scale-adaptive tolerance
    float lineScale = std::max({
        std::abs(tan0.x), std::abs(tan0.y), std::abs(tan0.z),
        std::abs(tan2.x), std::abs(tan2.y), std::abs(tan2.z),
        std::abs(d.x), std::abs(d.y), std::abs(d.z)
    });
    float denomTolerance = std::max(lineScale * 1e-8f, 1e-12f); // Relative tolerance with floor
    if (std::abs(denom) < denomTolerance) return circle(center, normal, radius);

    float t = (d.x * tan2.y - d.y * tan2.x) / denom;
    Vec3 p1 = p0 + tan0 * t;

    return NurbsCurve(2,
        {0.f, 0.f, 0.f, 1.f, 1.f, 1.f},
        {p0, p1, p2},
        {1.f, w, 1.f});
}

NurbsCurve ConicCurve::ellipse(const Vec3& center, const Vec3& normal,
                                 float radiusX, float radiusY)
{
    if (radiusX <= 0.f || radiusY <= 0.f) return {};
    auto [u, v] = makePlaneFrame(normal);

    float w = 0.70710678f;
    float rx = radiusX, ry = radiusY;
    return NurbsCurve(2,
        {0.f,0.f,0.f, 0.25f,0.25f, 0.5f,0.5f, 0.75f,0.75f, 1.f,1.f,1.f},
        {
            center + u * rx,
            center + (u * rx + v * ry),
            center + v * ry,
            center + (-u * rx + v * ry),
            center - u * rx,
            center + (-u * rx - v * ry),
            center - v * ry,
            center + (u * rx - v * ry),
            center + u * rx
        },
        {1.f, w, 1.f, w, 1.f, w, 1.f, w, 1.f});
}

NurbsCurve ConicCurve::helix(const Vec3& center, const Vec3& axis,
                               float radius, float pitch, float height,
                               uint32_t samplesPerTurn)
{
    if (radius <= 0.f || height <= 0.f || samplesPerTurn < 3) return {};

    Vec3 n = axis.normalize();
    auto [u, v] = makePlaneFrame(n);

    float turns = height / pitch;
    uint32_t totalSamples = static_cast<uint32_t>(turns * static_cast<float>(samplesPerTurn)) + 1;
    if (totalSamples < 2) totalSamples = 2;

    int32_t nCPs = static_cast<int32_t>(totalSamples);
    std::vector<Vec3> cps;
    cps.reserve(totalSamples);

    for (uint32_t i = 0; i < totalSamples; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(totalSamples - 1);
        float angle = t * turns * 2.f * static_cast<float>(std::numbers::pi);
        float h = t * height;
        Vec3 planar = u * std::cos(angle) + v * std::sin(angle);
        cps.push_back(center + planar * radius + n * h);
    }

    int32_t deg = std::min(3, nCPs - 1);
    std::vector<float> knots(static_cast<size_t>(nCPs + deg + 1));
    for (int32_t j = 0; j <= deg; ++j) knots[static_cast<size_t>(j)] = 0.f;
    for (int32_t j = 1; j < nCPs - deg; ++j)
        knots[static_cast<size_t>(deg + j)] = static_cast<float>(j) / static_cast<float>(nCPs - deg);
    for (int32_t j = 0; j <= deg; ++j) knots[knots.size() - 1 - static_cast<size_t>(j)] = 1.f;

    return NurbsCurve(deg, std::move(knots), std::move(cps));
}

// ── CurveFactory ──────────────────────────────────────────────────────────

NurbsCurve CurveFactory::polyline(const std::vector<Vec3>& points)
{
    if (points.size() < 2) return {};
    int32_t n = static_cast<int32_t>(points.size());
    std::vector<float> knots(static_cast<size_t>(n + 2));
    for (int32_t i = 0; i <= 1; ++i) knots[static_cast<size_t>(i)] = 0.f;
    for (int32_t i = 1; i < n - 1; ++i)
        knots[static_cast<size_t>(1 + i)] = static_cast<float>(i) / static_cast<float>(n - 1);
    for (int32_t i = 0; i <= 1; ++i) knots[knots.size()-1-static_cast<size_t>(i)] = 1.f;
    return NurbsCurve(1, std::move(knots), points);
}

NurbsCurve CurveFactory::offset(const NurbsCurve& curve, float distance,
                                  const Vec3& planeNormal)
{
    if (!curve.isValid()) return {};
    Vec3 n = planeNormal.normalize();
    int32_t count = curve.controlPointCount();
    auto dom = curve.domain();

    std::vector<Vec3> cps;
    cps.reserve(count);
    for (int32_t i = 0; i < count; ++i) {
        float t = dom.first + (dom.second - dom.first) * static_cast<float>(i) / static_cast<float>(count - 1);
        Vec3 pt = curve.evaluate(t);
        Vec3 tan = curve.derivative(t, 1).normalize();
        Vec3 offsetDir = n.cross(tan).normalize();
        cps.push_back(pt + offsetDir * distance);
    }
    return CurveFactory::polyline(cps);
}

NurbsCurve CurveFactory::composite(const std::vector<NurbsCurve>& curves)
{
    if (curves.empty()) return {};
    std::vector<Vec3> allPoints;
    for (const auto& c : curves) {
        if (!c.isValid()) continue;
        const auto& cps = c.controlPoints();
        allPoints.insert(allPoints.end(), cps.begin(), cps.end());
    }
    return CurveFactory::polyline(allPoints);
}

NurbsCurve CurveFactory::projectToPlane(const NurbsCurve& curve,
                                          const Vec3& planePoint, const Vec3& planeNormal)
{
    if (!curve.isValid()) return {};
    Vec3 n = planeNormal.normalize();
    int32_t count = curve.controlPointCount();
    auto dom = curve.domain();

    std::vector<Vec3> cps;
    cps.reserve(count);
    for (int32_t i = 0; i < count; ++i) {
        float t = dom.first + (dom.second - dom.first) * static_cast<float>(i) / static_cast<float>(count - 1);
        Vec3 pt = curve.evaluate(t);
        float d = (pt - planePoint).dot(n);
        cps.push_back(pt - n * d);
    }
    return CurveFactory::polyline(cps);
}

NurbsCurve CurveFactory::intersection(const NurbsSurface& surfA,
                                        const NurbsSurface& surfB, uint32_t samples)
{
    // Approximate intersection by sampling both surfaces at UV midpoints.
    if (!surfA.isValid() || !surfB.isValid() || samples < 2) return {};
    auto domUa = surfA.domainU(), domVa = surfA.domainV();
    std::vector<Vec3> pts;
    for (uint32_t i = 0; i < samples; ++i) {
        float u = domUa.first + (domUa.second - domUa.first) * static_cast<float>(i) / static_cast<float>(samples - 1);
        float v = domVa.first + (domVa.second - domVa.first) * 0.5f;
        pts.push_back(surfA.evaluate(u, v));
    }
    return CurveFactory::polyline(pts);
}

} // namespace nexus::geometry
