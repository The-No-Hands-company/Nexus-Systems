#include <nexus/geometry/Delaunay2D.h>
#include <nexus/geometry/RobustPredicates.h>

#include "DelaunaySuperTriangle.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace nexus::geometry {

// --- Internal triangle helper -------------------------------------------------

namespace {
    struct Tri {
        uint32_t a, b, c;
    };
} // anonymous namespace

// --- triangulate --------------------------------------------------------------

DelaunayResult Delaunay2D::triangulate(const std::vector<Vec2>& points) {
    if (points.size() < 3) {
        DelaunayResult result;
        result.ok = points.size() == 2;
        if (!result.ok)
            result.error = "Need at least 3 points for triangulation";
        return result;
    }

    // Build at increasing super-triangle scales until the result tiles the input's convex
    // hull. A near-collinear triple has an enormous circumcircle that can swallow a small
    // super-triangle, taking real triangles with it when the super-triangle is stripped
    // (see DelaunaySuperTriangle.h). Well-conditioned inputs succeed on the first pass.
    const double hullArea = detail::convexHullArea(points);
    DelaunayResult result;
    for (const float scale : detail::kSuperTriangleScales) {
        result = triangulateAtScale(points, scale);
        double area = 0.0;
        for (const auto& t : result.triangles) {
            area += std::abs(RobustPredicates::orient2D(result.vertices[t[0]], result.vertices[t[1]],
                                                        result.vertices[t[2]])) * 0.5;
        }
        if (detail::coversHull(area, hullArea)) break;
    }
    return result;
}

DelaunayResult Delaunay2D::triangulateAtScale(const std::vector<Vec2>& points, float scale) {
    DelaunayResult result;
    result.vertices = points;

    // --- Compute AABB ---------------------------------------------------------
    float minu = points[0].u, maxu = points[0].u;
    float minv = points[0].v, maxv = points[0].v;
    for (const auto& p : points) {
        minu = std::min(minu, p.u);
        maxu = std::max(maxu, p.u);
        minv = std::min(minv, p.v);
        maxv = std::max(maxv, p.v);
    }
    float du = maxu - minu;
    float dv = maxv - minv;
    if (du < 1e-8f) du = 1.0f;
    if (dv < 1e-8f) dv = 1.0f;
    float expand = std::max(du, dv) * scale;
    float margin = expand * 1.5f;

    float superMinU = minu - margin;
    float superMaxU = maxu + margin;
    float superMinV = minv - margin;
    float superMaxV = maxv + margin;

    uint32_t sv0 = static_cast<uint32_t>(points.size());
    uint32_t sv1 = sv0 + 1;
    uint32_t sv2 = sv0 + 2;

    result.vertices.push_back({superMinU, superMinV});
    result.vertices.push_back({superMaxU + (margin * 0.5f), superMinV});
    result.vertices.push_back({(superMinU + superMaxU) * 0.5f, superMaxV + (margin * 0.5f)});

    // --- Initial super-triangle -----------------------------------------------
    std::vector<Tri> triangles;
    triangles.push_back({sv0, sv1, sv2});

    // --- Insert points one at a time (Bowyer-Watson) --------------------------
    for (size_t pi = 0; pi < points.size(); ++pi) {
        const Vec2& p = points[pi];
        uint32_t pidx = static_cast<uint32_t>(pi);

        // Find triangles whose circumcircle contains p
        std::vector<size_t> badTris;
        std::unordered_map<uint64_t, uint32_t> edgeCount;

        for (size_t ti = 0; ti < triangles.size(); ++ti) {
            const Tri& t = triangles[ti];
            const Vec2& va = result.vertices[t.a];
            const Vec2& vb = result.vertices[t.b];
            const Vec2& vc = result.vertices[t.c];

            double inc = RobustPredicates::inCircle(va, vb, vc, p);
            if (inc > 0.0) {
                badTris.push_back(ti);

                uint32_t es[3][2] = {{t.a, t.b}, {t.b, t.c}, {t.c, t.a}};
                for (int e = 0; e < 3; ++e) {
                    uint32_t lo = std::min(es[e][0], es[e][1]);
                    uint32_t hi = std::max(es[e][0], es[e][1]);
                    uint64_t key = (static_cast<uint64_t>(hi) << 32) | lo;
                    ++edgeCount[key];
                }
            }
        }

        if (badTris.empty()) {
            // Point is inside existing triangulation — find containing triangle
            // This is a degenerate case; skip or handle by just not adding
            continue;
        }

        // --- Collect boundary polygon edges (appear only once among bad tris) ---
        std::vector<std::pair<uint32_t, uint32_t>> boundary;
        for (size_t ti : badTris) {
            const Tri& t = triangles[ti];
            uint32_t es[3][2] = {{t.a, t.b}, {t.b, t.c}, {t.c, t.a}};
            for (int e = 0; e < 3; ++e) {
                uint32_t lo = std::min(es[e][0], es[e][1]);
                uint32_t hi = std::max(es[e][0], es[e][1]);
                uint64_t key = (static_cast<uint64_t>(hi) << 32) | lo;
                if (edgeCount[key] == 1) {
                    boundary.push_back({es[e][0], es[e][1]});
                }
            }
        }

        // --- Remove bad triangles (mark them, then compact) --------------------
        std::vector<bool> removed(triangles.size(), false);
        for (size_t ti : badTris) removed[ti] = true;

        // --- Create new triangles from p to each boundary edge -----------------
        for (auto& [v0, v1] : boundary) {
            triangles.push_back({v0, v1, pidx});
        }

        // --- Compact out removed triangles ------------------------------------
        size_t write = 0;
        for (size_t ti = 0; ti < triangles.size(); ++ti) {
            if (ti < removed.size() && removed[ti]) continue;
            if (write != ti) triangles[write] = triangles[ti];
            ++write;
        }
        triangles.resize(write);
    }

    // --- Remove super-triangle triangles (any triangle using sv0, sv1, sv2) ----
    for (const Tri& t : triangles) {
        if (t.a == sv0 || t.a == sv1 || t.a == sv2 ||
            t.b == sv0 || t.b == sv1 || t.b == sv2 ||
            t.c == sv0 || t.c == sv1 || t.c == sv2) {
            continue;
        }
        result.triangles.push_back({t.a, t.b, t.c});
    }

    // --- Strip super-triangle vertices from vertex list -----------------------
    result.vertices.resize(points.size());

    result.ok = !result.triangles.empty();
    return result;
}

} // namespace nexus::geometry
