#include <nexus/geometry/ConstrainedDelaunay2D.h>
#include <nexus/geometry/RobustPredicates.h>

#include "DelaunaySuperTriangle.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nexus::geometry {

namespace {

constexpr float kEps = 1e-9f;
constexpr uint32_t kInvalid = 0xFFFFFFFFu;

struct EdgeKey {
    uint32_t a, b;
    bool operator==(const EdgeKey& o) const noexcept {
        return (a == o.a && b == o.b) || (a == o.b && b == o.a);
    }
};

struct EdgeKeyHash {
    std::size_t operator()(EdgeKey k) const noexcept {
        uint32_t lo = k.a < k.b ? k.a : k.b;
        uint32_t hi = k.a < k.b ? k.b : k.a;
        return std::hash<uint64_t>{}((static_cast<uint64_t>(lo) << 32) | hi);
    }
};

using EdgeSet = std::unordered_set<EdgeKey, EdgeKeyHash>;

} // namespace

int ConstrainedDelaunay2D::orient2D(const Vec2& a, const Vec2& b, const Vec2& c) noexcept {
    double det = RobustPredicates::orient2D(a, b, c);
    if (det > 0.0) return 1;
    if (det < 0.0) return -1;
    return 0;
}

bool ConstrainedDelaunay2D::inCircle(const Vec2& a, const Vec2& b, const Vec2& c,
                                      const Vec2& d) noexcept {
    return RobustPredicates::inCircle(a, b, c, d) > 0.0;
}

bool ConstrainedDelaunay2D::segmentsCross(const Vec2& p1, const Vec2& p2,
                                           const Vec2& q1, const Vec2& q2) noexcept {
    int o1 = orient2D(p1, p2, q1);
    int o2 = orient2D(p1, p2, q2);
    int o3 = orient2D(q1, q2, p1);
    int o4 = orient2D(q1, q2, p2);
    if (o1 != o2 && o3 != o4) return true;
    auto onSegment = [](const Vec2& a, const Vec2& b, const Vec2& c) {
        return std::min(a.u, b.u) <= c.u + kEps && c.u - kEps <= std::max(a.u, b.u)
            && std::min(a.v, b.v) <= c.v + kEps && c.v - kEps <= std::max(a.v, b.v);
    };
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;
    if (o2 == 0 && onSegment(p1, p2, q2)) return true;
    if (o3 == 0 && onSegment(q1, q2, p1)) return true;
    if (o4 == 0 && onSegment(q1, q2, p2)) return true;
    return false;
}

void ConstrainedDelaunay2D::buildDelaunay() {
    // Build at increasing super-triangle scales until the triangulation covers the input's
    // convex hull. A sliver triple's circumcircle can swallow a too-small super-triangle,
    // in which case the sliver is never emitted and stripping the super-triangle deletes
    // real area — see DelaunaySuperTriangle.h. Well-conditioned inputs succeed on the
    // first (historical) scale, so they are unaffected.
    const double hullArea = detail::convexHullArea(m_vertices);
    for (const float scale : detail::kSuperTriangleScales) {
        if (buildDelaunayAtScale(scale, hullArea)) return;
    }
}

bool ConstrainedDelaunay2D::buildDelaunayAtScale(float scale, double hullArea) {
    m_triangles.clear();
    auto& pts = m_vertices;
    if (pts.size() < 3) return true;

    float minU = pts[0].u, maxU = pts[0].u;
    float minV = pts[0].v, maxV = pts[0].v;
    for (const auto& p : pts) {
        minU = std::min(minU, p.u); maxU = std::max(maxU, p.u);
        minV = std::min(minV, p.v); maxV = std::max(maxV, p.v);
    }
    float dx = maxU - minU, dy = maxV - minV;
    float dmax = std::max(dx, dy) * scale;
    if (dmax < 1.f) dmax = scale;
    float midU = (minU + maxU) * 0.5f, midV = (minV + maxV) * 0.5f;

    uint32_t s0 = static_cast<uint32_t>(pts.size());
    uint32_t s1 = s0 + 1, s2 = s0 + 2;
    pts.push_back({midU - dmax, midV - dmax * 0.5f});
    pts.push_back({midU, midV + dmax * 1.5f});
    pts.push_back({midU + dmax, midV - dmax * 0.5f});
    // Super-triangle must be CCW so inCircle() has a consistent orientation.
    if (orient2D(pts[s0], pts[s1], pts[s2]) < 0) {
        m_triangles.push_back({s0, s2, s1});
    } else {
        m_triangles.push_back({s0, s1, s2});
    }

    for (uint32_t pi = 0; pi < static_cast<uint32_t>(pts.size()) - 3; ++pi) {
        const Vec2& p = pts[pi];

        // Bowyer-Watson: every triangle whose circumcircle contains p is "bad".
        // (The previous guard `t[*] < pi && t[0] != s0…` never flagged the
        //  super-triangle, so no point was ever inserted — the triangulation was
        //  always empty.)
        std::vector<bool> badMask(m_triangles.size(), false);
        for (size_t ti = 0; ti < m_triangles.size(); ++ti) {
            const auto& t = m_triangles[ti];
            if (inCircle(pts[t[0]], pts[t[1]], pts[t[2]], p)) {
                badMask[ti] = true;
            }
        }

        std::unordered_map<EdgeKey, int, EdgeKeyHash> edgeCount;
        for (size_t ti = 0; ti < m_triangles.size(); ++ti) {
            if (!badMask[ti]) continue;
            const auto& t = m_triangles[ti];
            for (int e = 0; e < 3; ++e) {
                EdgeKey ek{t[e], t[(e + 1) % 3]};
                edgeCount[ek]++;
            }
        }

        std::vector<std::array<uint32_t, 3>> newTris;
        for (size_t ti = 0; ti < m_triangles.size(); ++ti) {
            if (!badMask[ti]) {
                newTris.push_back(m_triangles[ti]);
            }
        }
        for (const auto& [ek, cnt] : edgeCount) {
            if (cnt == 1) {
                // Emit each cavity-boundary triangle CCW so orientation stays
                // consistent for subsequent inCircle tests.
                if (orient2D(pts[ek.a], pts[ek.b], pts[pi]) < 0) {
                    newTris.push_back({ek.b, ek.a, pi});
                } else {
                    newTris.push_back({ek.a, ek.b, pi});
                }
            }
        }

        m_triangles = std::move(newTris);
    }

    for (auto it = m_triangles.begin(); it != m_triangles.end(); ) {
        if (it->at(0) >= s0 || it->at(1) >= s0 || it->at(2) >= s0) {
            it = m_triangles.erase(it);
        } else {
            ++it;
        }
    }
    pts.resize(s0);

    // Coverage check: the triangles that survived stripping must tile the convex hull. A
    // deficit means the super-triangle was inside some sliver's circumcircle and real
    // triangles were consumed with it — the caller retries at a larger scale.
    double area = 0.0;
    for (const auto& t : m_triangles) {
        area += std::abs(RobustPredicates::orient2D(pts[t[0]], pts[t[1]], pts[t[2]])) * 0.5;
    }
    return detail::coversHull(area, hullArea);
}

int ConstrainedDelaunay2D::findTriangleContaining(const Vec2& p) const noexcept {
    for (int ti = 0; ti < static_cast<int>(m_triangles.size()); ++ti) {
        const auto& t = m_triangles[ti];
        int o1 = orient2D(m_vertices[t[1]], m_vertices[t[2]], p);
        int o2 = orient2D(m_vertices[t[2]], m_vertices[t[0]], p);
        int o3 = orient2D(m_vertices[t[0]], m_vertices[t[1]], p);
        if ((o1 >= 0 && o2 >= 0 && o3 >= 0) || (o1 <= 0 && o2 <= 0 && o3 <= 0))
            return ti;
    }
    return -1;
}

int ConstrainedDelaunay2D::findAdjacentTriangle(uint32_t edgeA, uint32_t edgeB,
                                                 int excludeTri) const noexcept {
    for (int ti = 0; ti < static_cast<int>(m_triangles.size()); ++ti) {
        if (ti == excludeTri) continue;
        const auto& t = m_triangles[ti];
        for (int e = 0; e < 3; ++e) {
            uint32_t a = t[e], b = t[(e + 1) % 3];
            if ((a == edgeA && b == edgeB) || (a == edgeB && b == edgeA))
                return ti;
        }
    }
    return -1;
}

void ConstrainedDelaunay2D::flipEdge(uint32_t a, uint32_t b) {
    for (int ti = 0; ti < static_cast<int>(m_triangles.size()); ++ti) {
        auto& t = m_triangles[ti];
        for (int e = 0; e < 3; ++e) {
            uint32_t ea = t[e], eb = t[(e + 1) % 3];
            if ((ea != a || eb != b) && (ea != b || eb != a)) continue;

            EdgeKey ek{ea, eb};
            auto it = m_constrainedEdges.find(ek);
            if (it != m_constrainedEdges.end() && it->second) return;

            int adj = findAdjacentTriangle(a, b, ti);
            if (adj < 0) return;

            auto& ta = m_triangles[ti];
            auto& tb = m_triangles[adj];

            uint32_t oppA = kInvalid, oppB = kInvalid;
            for (int i = 0; i < 3; ++i) {
                if (ta[i] != a && ta[i] != b) oppA = ta[i];
                if (tb[i] != a && tb[i] != b) oppB = tb[i];
            }
            if (oppA == kInvalid || oppB == kInvalid) return;

            // Emit both new triangles CCW so orientation stays consistent across the
            // mesh (buildDelaunay establishes CCW; a raw flip must preserve it,
            // otherwise the triangulation acquires mixed winding).
            ta = (orient2D(m_vertices[a], m_vertices[oppA], m_vertices[oppB]) < 0)
                     ? std::array<uint32_t, 3>{a, oppB, oppA}
                     : std::array<uint32_t, 3>{a, oppA, oppB};
            tb = (orient2D(m_vertices[b], m_vertices[oppA], m_vertices[oppB]) < 0)
                     ? std::array<uint32_t, 3>{b, oppB, oppA}
                     : std::array<uint32_t, 3>{b, oppA, oppB};
            return;
        }
    }
}

void ConstrainedDelaunay2D::enforceConstraint(uint32_t a, uint32_t b) {
    m_constrainedEdges[EdgeKey{a, b}] = true;

    auto edgeExists = [&](uint32_t x, uint32_t y) -> bool {
        for (const auto& t : m_triangles) {
            for (int e = 0; e < 3; ++e) {
                const uint32_t ea = t[e], eb = t[(e + 1) % 3];
                if ((ea == x && eb == y) || (ea == y && eb == x)) return true;
            }
        }
        return false;
    };
    if (edgeExists(a, b)) return;

    // If a vertex lies EXACTLY on the constraint's interior, the edge a-b cannot be a
    // single triangle edge — split the constraint at the NEAREST such vertex and
    // recover each half. This is the collinear / T-junction case that edge flipping
    // alone can never resolve (a straddling triangle would survive otherwise). The
    // on-segment decision is EXACT: orient2D == 0 ⇔ collinear (Shewchuk predicate).
    {
        const Vec2&  va     = m_vertices[a];
        const Vec2&  vb     = m_vertices[b];
        const double abu    = static_cast<double>(vb.u) - va.u;
        const double abv    = static_cast<double>(vb.v) - va.v;
        const double abLen2 = abu * abu + abv * abv;
        uint32_t     best   = kInvalid;
        double       bestT  = 2.0;
        if (abLen2 > 0.0) {
            for (uint32_t m = 0; m < static_cast<uint32_t>(m_vertices.size()); ++m) {
                if (m == a || m == b) continue;
                if (orient2D(va, vb, m_vertices[m]) != 0) continue;  // not collinear (exact)
                const double t = ((static_cast<double>(m_vertices[m].u) - va.u) * abu
                                  + (static_cast<double>(m_vertices[m].v) - va.v) * abv) / abLen2;
                if (t > 1e-6 && t < 1.0 - 1e-6 && t < bestT) { bestT = t; best = m; }
            }
        }
        if (best != kInvalid) {
            enforceConstraint(a, best);
            enforceConstraint(best, b);
            return;
        }
    }

    // Recover the edge by flipping — but ONLY an edge whose two triangles form a
    // strictly CONVEX quad (a non-convex flip produces overlapping/inverted
    // triangles), AND whose flip makes PROGRESS: the new diagonal must no longer
    // cross a-b. Anglada's theorem guarantees such an edge always exists while any
    // edge still crosses the constraint, so this terminates and recovers a-b. Every
    // sidedness test is EXACT orient2D.
    auto apexes = [&](uint32_t ea, uint32_t eb, uint32_t& oppA, uint32_t& oppB) {
        oppA = kInvalid; oppB = kInvalid;
        for (const auto& t : m_triangles) {
            bool ha = false, hb = false; uint32_t o = kInvalid;
            for (int i = 0; i < 3; ++i) {
                if (t[i] == ea)      ha = true;
                else if (t[i] == eb) hb = true;
                else                 o = t[i];
            }
            if (ha && hb) { if (oppA == kInvalid) oppA = o; else if (oppB == kInvalid) oppB = o; }
        }
    };
    auto properCross = [&](uint32_t s0, uint32_t s1, uint32_t t0, uint32_t t1) -> bool {
        if (s0 == t0 || s0 == t1 || s1 == t0 || s1 == t1) return false;  // share an endpoint
        const int o1 = orient2D(m_vertices[s0], m_vertices[s1], m_vertices[t0]);
        const int o2 = orient2D(m_vertices[s0], m_vertices[s1], m_vertices[t1]);
        const int o3 = orient2D(m_vertices[t0], m_vertices[t1], m_vertices[s0]);
        const int o4 = orient2D(m_vertices[t0], m_vertices[t1], m_vertices[s1]);
        return o1 != 0 && o2 != 0 && o3 != 0 && o4 != 0 && o1 != o2 && o3 != o4;
    };

    // The crossing parameter of edge (ea,eb) along a-b (0 at a, 1 at b), for Sloan's
    // proven-terminating ORDERED flipping: process crossings nearest `a` first.
    auto crossParam = [&](uint32_t ea, uint32_t eb) -> double {
        const double da = RobustPredicates::orient2D(m_vertices[ea], m_vertices[eb], m_vertices[a]);
        const double db = RobustPredicates::orient2D(m_vertices[ea], m_vertices[eb], m_vertices[b]);
        const double den = da - db;
        return (den != 0.0) ? da / den : 0.5;
    };

    constexpr int kMaxIter = 4096;
    for (int iter = 0; iter < kMaxIter; ++iter) {
        if (edgeExists(a, b)) return;
        // Among the convex, non-constrained edges that cross a-b, flip the one whose
        // crossing point is NEAREST `a` (Sloan's order). A convex flip never INCREASES
        // the number of edges crossing a-b, and processing crossings front-to-back
        // guarantees termination and recovery — even through configurations where NO
        // single flip makes immediate progress (the case a strict "progress only" rule
        // deadlocked on). Every sidedness decision is an exact orient2D.
        uint32_t fea = kInvalid, feb = kInvalid;
        double   bestT = 2.0;
        for (const auto& t : m_triangles) {
            for (int e = 0; e < 3; ++e) {
                const uint32_t ea = t[e], eb = t[(e + 1) % 3];
                if (isConstrained(ea, eb)) continue;
                if (!properCross(a, b, ea, eb)) continue;            // edge crosses a-b
                uint32_t oppA = kInvalid, oppB = kInvalid;
                apexes(ea, eb, oppA, oppB);
                if (oppA == kInvalid || oppB == kInvalid) continue;  // boundary edge, no flip
                const int s1 = orient2D(m_vertices[oppA], m_vertices[oppB], m_vertices[ea]);
                const int s2 = orient2D(m_vertices[oppA], m_vertices[oppB], m_vertices[eb]);
                if (s1 == 0 || s2 == 0 || s1 == s2) continue;        // quad not strictly convex
                const double tc = crossParam(ea, eb);
                if (tc < bestT) { bestT = tc; fea = ea; feb = eb; }
            }
        }
        if (fea == kInvalid) break;  // no convex crossing edge available → give up safely
        flipEdge(fea, feb);
    }
}

CDTResult ConstrainedDelaunay2D::triangulate(
    const std::vector<Vec2>& points,
    const std::vector<ConstraintEdge>& constraints) {

    CDTResult result;

    if (points.size() < 3) {
        result.ok = false;
        result.error = "Need at least 3 points";
        return result;
    }

    for (const auto& c : constraints) {
        if (c.a >= points.size() || c.b >= points.size()) {
            result.ok = false;
            result.error = "Constraint references out-of-range vertex index";
            return result;
        }
        if (c.a == c.b) {
            result.ok = false;
            result.error = "Constraint edge must have distinct endpoints";
            return result;
        }
    }

    ConstrainedDelaunay2D impl;
    impl.m_vertices = points;
    impl.buildDelaunay();

    for (const auto& c : constraints) {
        impl.enforceConstraint(c.a, c.b);
    }

    result.ok = true;
    result.triangles = impl.m_triangles;
    result.vertices = impl.m_vertices;
    result.constraints = constraints;
    return result;
}

bool ConstrainedDelaunay2D::isConstrained(uint32_t a, uint32_t b) const {
    EdgeKey ek{a, b};
    auto it = m_constrainedEdges.find(ek);
    return it != m_constrainedEdges.end() && it->second;
}

} // namespace nexus::geometry
