#pragma once

// Bowyer-Watson super-triangle sizing (internal to the Delaunay triangulators).
//
// A super-triangle only works if it lies OUTSIDE the circumcircle of every triangle of
// the true Delaunay triangulation. Circumradius is R = abc/(4A), so a near-collinear
// (sliver) triple pushes R towards infinity: for points spanning 1.0 with a 1e-6
// perpendicular deviation, R is on the order of 1e6. When the super-triangle is smaller
// than that, the sliver's circumcircle swallows the super-vertices, the sliver is never
// emitted, and stripping the super-triangle at the end deletes real area — hull edges go
// missing and, in the extreme, the triangulation comes back EMPTY.
//
// No fixed multiple of the bounding box can be correct, because the required scale is
// driven by the input's thinness, not its extent. So the triangulators build at a scale,
// VERIFY that the result covers the input's convex hull, and grow the scale if it does
// not. The exact orientation predicate makes the check reliable at any magnitude, and the
// schedule terminates: a finite point set with any non-degenerate triple has a finite
// maximum circumradius.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/RobustPredicates.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry::detail {

// Super-triangle scale factors (multiples of the input's bounding-box extent), tried in
// order until the triangulation covers the convex hull. The first entry is the historical
// scale, so well-conditioned inputs cost exactly one build and are bit-for-bit unchanged.
inline constexpr float kSuperTriangleScales[] = {20.f, 2e4f, 2e7f, 2e10f, 2e13f};

// Exact convex-hull area (monotone chain on the exact orientation predicate). Returns 0
// for fewer than 3 distinct points or for an exactly-collinear set — both of which have
// no triangulation, so a triangulator that returns nothing for them is correct.
[[nodiscard]] inline double convexHullArea(const std::vector<Vec2>& points) {
    std::vector<Vec2> p = points;
    std::sort(p.begin(), p.end(), [](const Vec2& a, const Vec2& b) {
        return a.u != b.u ? a.u < b.u : a.v < b.v;
    });
    p.erase(std::unique(p.begin(), p.end(),
                        [](const Vec2& a, const Vec2& b) { return a.u == b.u && a.v == b.v; }),
            p.end());
    if (p.size() < 3) return 0.0;

    std::vector<Vec2> hull;
    hull.reserve(p.size() * 2);
    for (int pass = 0; pass < 2; ++pass) {
        const std::size_t start = hull.size();
        for (std::size_t i = 0; i < p.size(); ++i) {
            const Vec2& q = (pass == 0) ? p[i] : p[p.size() - 1 - i];
            while (hull.size() >= start + 2
                   && RobustPredicates::orient2D(hull[hull.size() - 2], hull[hull.size() - 1], q)
                          <= 0.0) {
                hull.pop_back();
            }
            hull.push_back(q);
        }
        hull.pop_back();  // last point is the next pass's first
    }
    if (hull.size() < 3) return 0.0;

    // Fan-triangulate through the orientation predicate rather than applying the shoelace
    // formula to raw coordinates. Shoelace multiplies absolute coordinates and subtracts
    // nearly-equal products, so on a sliver — where the area is many orders of magnitude
    // below the coordinate magnitudes — it loses most of its significant digits (measured:
    // 4e-4 relative error on a 1e-6-area triangle spanning 1.0). orient2D differences the
    // coordinates FIRST, which is exact for float inputs, so the areas being compared here
    // are computed the same accurate way on both sides.
    double twiceArea = 0.0;
    for (std::size_t i = 1; i + 1 < hull.size(); ++i) {
        twiceArea += RobustPredicates::orient2D(hull[0], hull[i], hull[i + 1]);
    }
    return std::abs(twiceArea) * 0.5;
}

// Does `area` account for the whole hull? Both areas are accumulated in double from the
// same float coordinates, so a correct triangulation matches to near machine precision;
// the slack only has to reject the gross deficits a too-small super-triangle produces.
[[nodiscard]] inline bool coversHull(double area, double hullArea) noexcept {
    return area >= hullArea * (1.0 - 1e-6);
}

} // namespace nexus::geometry::detail
