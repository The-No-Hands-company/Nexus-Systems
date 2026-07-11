#include <nexus/geometry/ConstrainedDelaunay2D.h>
#include <nexus/geometry/RobustPredicates.h>

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
    m_triangles.clear();
    auto& pts = m_vertices;
    if (pts.size() < 3) return;

    float minU = pts[0].u, maxU = pts[0].u;
    float minV = pts[0].v, maxV = pts[0].v;
    for (const auto& p : pts) {
        minU = std::min(minU, p.u); maxU = std::max(maxU, p.u);
        minV = std::min(minV, p.v); maxV = std::max(maxV, p.v);
    }
    float dx = maxU - minU, dy = maxV - minV;
    float dmax = std::max(dx, dy) * 20.f;
    if (dmax < 1.f) dmax = 20.f;
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

            ta = {a, oppA, oppB};
            tb = {b, oppA, oppB};
            return;
        }
    }
}

void ConstrainedDelaunay2D::enforceConstraint(uint32_t a, uint32_t b) {
    EdgeKey ek{a, b};
    m_constrainedEdges[ek] = true;

    for (int ti = 0; ti < static_cast<int>(m_triangles.size()); ++ti) {
        const auto& t = m_triangles[ti];
        for (int e = 0; e < 3; ++e) {
            uint32_t ea = t[e], eb = t[(e + 1) % 3];
            if ((ea == a && eb == b) || (ea == b && eb == a))
                return;
        }
    }

    for (int iter = 0; iter < 100; ++iter) {
        bool found = false;
        for (int ti = 0; ti < static_cast<int>(m_triangles.size()); ++ti) {
            const auto& t = m_triangles[ti];
            for (int e = 0; e < 3; ++e) {
                uint32_t ea = t[e], eb = t[(e + 1) % 3];
                if ((ea == a && eb == b) || (ea == b && eb == a)) {
                    found = true;
                    break;
                }
            }
            if (found) break;
        }
        if (found) return;

        bool flipped = false;
        for (int ti = 0; ti < static_cast<int>(m_triangles.size()) && !flipped; ++ti) {
            const auto& t = m_triangles[ti];
            for (int e = 0; e < 3 && !flipped; ++e) {
                uint32_t ea = t[e], eb = t[(e + 1) % 3];
                if ((ea == a && eb == b) || (ea == b && eb == a)) continue;

                EdgeKey ek2{ea, eb};
                auto it = m_constrainedEdges.find(ek2);
                if (it != m_constrainedEdges.end() && it->second) continue;

                if (segmentsCross(m_vertices[a], m_vertices[b],
                                  m_vertices[ea], m_vertices[eb])) {
                    flipEdge(ea, eb);
                    flipped = true;
                }
            }
        }
        if (!flipped) break;
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
