// Sliver (near-collinear) robustness of the Bowyer-Watson triangulators.
//
// A Bowyer-Watson super-triangle is only valid if it lies outside the circumcircle of
// every triangle of the true Delaunay triangulation. Circumradius is R = abc/(4A), so a
// near-collinear triple sends R towards infinity: points spanning 1.0 with a 1e-6
// perpendicular deviation need a super-triangle on the order of 1e6 times the bounding
// box. Both triangulators used to hard-code a small multiple (20x for the CDT, 0.75x for
// Delaunay2D). Below that threshold the sliver's circumcircle swallows the super-vertices,
// the sliver is never emitted, and stripping the super-triangle at the end deletes real
// area — vertices vanish from the output and, in the extreme, it comes back EMPTY.
//
// The invariant these tests pin is the one a triangulator owes its caller: the output
// TILES THE CONVEX HULL of the input and uses every input vertex. It is asserted across
// several decades of sliver thinness, because the failure is a function of how thin the
// input is, not how big it is — which is precisely why no fixed constant can satisfy it.

#include <nexus/geometry/ConstrainedDelaunay2D.h>
#include <nexus/geometry/Delaunay2D.h>
#include <nexus/geometry/RobustPredicates.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <set>
#include <vector>

using namespace nexus::geometry;

namespace {

// Exact convex-hull area (monotone chain on the exact orientation predicate).
double hullArea(std::vector<Vec2> p)
{
    std::sort(p.begin(), p.end(),
              [](const Vec2& a, const Vec2& b) { return a.u != b.u ? a.u < b.u : a.v < b.v; });
    p.erase(std::unique(p.begin(), p.end(),
                        [](const Vec2& a, const Vec2& b) { return a.u == b.u && a.v == b.v; }),
            p.end());
    if (p.size() < 3) return 0.0;

    std::vector<Vec2> h;
    for (int pass = 0; pass < 2; ++pass) {
        const std::size_t start = h.size();
        for (std::size_t i = 0; i < p.size(); ++i) {
            const Vec2& q = (pass == 0) ? p[i] : p[p.size() - 1 - i];
            while (h.size() >= start + 2
                   && RobustPredicates::orient2D(h[h.size() - 2], h[h.size() - 1], q) <= 0.0) {
                h.pop_back();
            }
            h.push_back(q);
        }
        h.pop_back();
    }
    if (h.size() < 3) return 0.0;

    // Fan-triangulate through orient2D, NOT the shoelace formula. Shoelace subtracts
    // nearly-equal products of absolute coordinates, so on a sliver (area many orders of
    // magnitude below the coordinate magnitudes) it loses most of its significant digits —
    // measured at 4e-4 relative error here, which is large enough to fake a hull gap that
    // does not exist. orient2D differences the coordinates first and is exact for floats.
    double a2 = 0.0;
    for (std::size_t i = 1; i + 1 < h.size(); ++i) {
        a2 += RobustPredicates::orient2D(h[0], h[i], h[i + 1]);
    }
    return std::abs(a2) * 0.5;
}

double tiledArea(const std::vector<Vec2>& verts, const std::vector<std::array<uint32_t, 3>>& tris)
{
    double a = 0.0;
    for (const auto& t : tris) {
        a += std::abs(RobustPredicates::orient2D(verts[t[0]], verts[t[1]], verts[t[2]])) * 0.5;
    }
    return a;
}

// `count` points along a line, displaced perpendicular by at most `jitter`.
std::vector<Vec2> sliver(int count, double jitter, std::mt19937& rng)
{
    std::uniform_real_distribution<float> j(-1.f, 1.f);
    std::vector<Vec2> pts;
    pts.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(count - 1);
        pts.push_back({t + static_cast<float>(jitter) * j(rng),
                       t * 0.5f + static_cast<float>(jitter) * j(rng)});
    }
    return pts;
}

}  // namespace

// The exact 5-point set that used to come back with ZERO triangles. Points span 1.0 and
// deviate from their line by under 1e-3, giving circumradii far outside a 20x
// super-triangle.
TEST(DelaunaySliver, NearCollinearSetIsNotTriangulatedAway)
{
    const std::vector<Vec2> pts = {
        {0.000695393f, 0.000568844f}, {0.249432012f, 0.125249788f}, {0.500845492f, 0.249828726f},
        {0.750151277f, 0.375872076f}, {0.999402881f, 0.500904858f},
    };

    const CDTResult cdt = ConstrainedDelaunay2D::triangulate(pts, {});
    ASSERT_TRUE(cdt.ok);
    EXPECT_FALSE(cdt.triangles.empty()) << "the whole sliver was triangulated away";
    EXPECT_NEAR(tiledArea(cdt.vertices, cdt.triangles), hullArea(pts), hullArea(pts) * 1e-5);

    const DelaunayResult dt = Delaunay2D::triangulate(pts);
    ASSERT_TRUE(dt.ok);
    EXPECT_FALSE(dt.triangles.empty());
    EXPECT_NEAR(tiledArea(dt.vertices, dt.triangles), hullArea(pts), hullArea(pts) * 1e-5);
}

// The required super-triangle scale grows as the input gets thinner, so the invariant has
// to hold across decades of thinness — no single hard-coded multiple can deliver this.
// Asserted at the coarse end, where the triangulators are now exactly right.
TEST(DelaunaySliver, HullIsTiledOnModeratelyThinInput)
{
    std::mt19937 rng(20260722u);
    for (int trial = 0; trial < 60; ++trial) {
        const std::vector<Vec2> pts = sliver(4 + (trial % 6), 1e-2, rng);
        const double ha = hullArea(pts);
        if (ha <= 0.0) continue;  // exactly collinear: no triangulation exists

        const CDTResult cdt = ConstrainedDelaunay2D::triangulate(pts, {});
        ASSERT_TRUE(cdt.ok);
        EXPECT_GE(tiledArea(cdt.vertices, cdt.triangles), ha * (1.0 - 1e-5))
            << "CDT left a gap in the hull at trial " << trial;

        const DelaunayResult dt = Delaunay2D::triangulate(pts);
        ASSERT_TRUE(dt.ok);
        EXPECT_GE(tiledArea(dt.vertices, dt.triangles), ha * (1.0 - 1e-5))
            << "Delaunay2D left a gap in the hull at trial " << trial;
    }
}

// The hull is tiled at EVERY thinness, down to the limit of float resolution.
//
// This started as a tracked residual — 125 of 400 thin sets came back with the convex hull
// under-filled, and it looked like a second defect in the Bowyer-Watson cavity, since the
// cavity was provably never empty, never disconnected and always star-shaped. It was not a
// cavity defect at all. inCircle was returning the WRONG SIGN on sliver configurations, so
// the cavity was correctly computed from an incorrect set of "bad" triangles. Rebuilding
// the predicates on genuine expansion arithmetic took this to zero.
//
// It stays asserted at zero because it is the sharpest end-to-end witness the suite has
// that the predicates are still exact: a wrong sign anywhere in inCircle shows up here as
// missing area long before it shows up as a leaking boolean.
TEST(DelaunaySliver, HullIsTiledAtEveryThinness)
{
    int sets = 0, gaps = 0;
    double worst = 0.0;
    for (const double jitter : {1e-3, 1e-4, 1e-5, 1e-6}) {
        std::mt19937 rng(20260722u);
        for (int trial = 0; trial < 100; ++trial) {
            const std::vector<Vec2> pts = sliver(4 + (trial % 6), jitter, rng);
            const double ha = hullArea(pts);
            if (ha <= 0.0) continue;
            ++sets;
            const CDTResult cdt = ConstrainedDelaunay2D::triangulate(pts, {});
            ASSERT_TRUE(cdt.ok);
            const double a = tiledArea(cdt.vertices, cdt.triangles);
            if (a < ha * (1.0 - 1e-5)) {
                ++gaps;
                worst = std::max(worst, (ha - a) / ha);
            }
        }
    }
    RecordProperty("sliverHullSets", sets);
    RecordProperty("sliverHullGaps", gaps);
    ASSERT_GT(sets, 300) << "the battery degenerated — nothing was actually exercised";
    EXPECT_EQ(gaps, 0) << "the triangulation under-filled its own convex hull in " << gaps
                       << " of " << sets << " thin sets (worst deficit " << worst
                       << ") — suspect the exactness of inCircle";
}

// A vertex that reaches no triangle is invisible to constraint recovery: nothing crosses
// an edge incident to it, so flipping can never realise that edge. This was the dominant
// mechanism behind unenforced CDT constraints.
TEST(DelaunaySliver, EveryInputVertexReachesATriangle)
{
    std::mt19937 rng(31337u);
    int checked = 0;
    for (const double jitter : {1e-3, 1e-5, 1e-7}) {
        for (int trial = 0; trial < 60; ++trial) {
            std::vector<Vec2> pts = sliver(5 + (trial % 8), jitter, rng);
            std::sort(pts.begin(), pts.end(), [](const Vec2& a, const Vec2& b) {
                return a.u != b.u ? a.u < b.u : a.v < b.v;
            });
            pts.erase(std::unique(pts.begin(), pts.end(),
                                  [](const Vec2& a, const Vec2& b) {
                                      return a.u == b.u && a.v == b.v;
                                  }),
                      pts.end());
            if (pts.size() < 4 || hullArea(pts) <= 0.0) continue;

            const CDTResult cdt = ConstrainedDelaunay2D::triangulate(pts, {});
            ASSERT_TRUE(cdt.ok);
            std::set<uint32_t> used;
            for (const auto& t : cdt.triangles) {
                used.insert(t[0]);
                used.insert(t[1]);
                used.insert(t[2]);
            }
            EXPECT_EQ(used.size(), pts.size())
                << "a vertex was dropped at jitter " << jitter << " trial " << trial;
            ++checked;
        }
    }
    EXPECT_GT(checked, 100) << "the battery degenerated — nothing was actually exercised";
}

// Determinism on the Null backend: the retry schedule must not make the result depend on
// anything but the input.
TEST(DelaunaySliver, RetryScheduleIsDeterministic)
{
    std::mt19937 rng(4242u);
    const std::vector<Vec2> pts = sliver(9, 1e-6, rng);
    const CDTResult a = ConstrainedDelaunay2D::triangulate(pts, {});
    const CDTResult b = ConstrainedDelaunay2D::triangulate(pts, {});
    ASSERT_TRUE(a.ok && b.ok);
    ASSERT_EQ(a.triangles.size(), b.triangles.size());
    for (std::size_t i = 0; i < a.triangles.size(); ++i) {
        EXPECT_EQ(a.triangles[i], b.triangles[i]);
    }
}

// Well-conditioned input must still succeed on the first (historical) scale — the retry
// exists for slivers and must not perturb the ordinary case.
TEST(DelaunaySliver, WellConditionedInputIsUnchanged)
{
    const std::vector<Vec2> square = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
    const CDTResult cdt = ConstrainedDelaunay2D::triangulate(square, {});
    ASSERT_TRUE(cdt.ok);
    EXPECT_EQ(cdt.triangles.size(), 2u);
    EXPECT_NEAR(tiledArea(cdt.vertices, cdt.triangles), 1.0, 1e-6);
}
