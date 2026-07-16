#include <nexus/geometry/AnalyticBRep.h>

#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>

namespace nexus::geometry::brep {

namespace {
Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 scale(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
float length(const Vec3& a) { return std::sqrt(dot(a, a)); }
Vec3 normalize(const Vec3& a)
{
    const float l = length(a);
    return (l > 1e-20f) ? Vec3{a.x / l, a.y / l, a.z / l} : Vec3{0.f, 0.f, 0.f};
}

// Parameter on an analytic curve that evaluates (closest) to point p. For a Line
// this is the projection distance along dir; for a Circle the sweep angle in the
// (ref, dir×ref) frame — matching Curve::eval so eval(paramOnCurve(c,p)) ≈ p when
// p lies on the curve.
float paramOnCurve(const Curve& c, const Vec3& p)
{
    if (c.kind == CurveKind::Circle) {
        const Vec3 bi = cross(c.dir, c.ref);
        const Vec3 w = sub(p, c.origin);
        return std::atan2(dot(w, bi), dot(w, c.ref));
    }
    return dot(sub(p, c.origin), c.dir);  // Line (Nurbs handled elsewhere)
}

// Fraction s in (0,1) at which the straight segment A→B crosses the Line imprint
// `curve` in a single interior point (the two lines meet, i.e. are near-coplanar
// and non-parallel). Returns false otherwise. Only a Line imprint is handled
// here: a Line lies in the planar face's plane, so the resulting cut edge lies on
// the face — geometrically valid. (Circle imprint, whose valid forms are the
// coplanar in-face "bite" and cuts on curved faces, is a dedicated follow-up.)
bool segmentLineCrossing(const Vec3& A, const Vec3& B, const Curve& curve, float eps,
                         float& sOut)
{
    if (curve.kind != CurveKind::Line) return false;
    const Vec3 E = sub(B, A);
    const Vec3 D = curve.dir;
    const Vec3 ExD = cross(E, D);
    const float denom = dot(ExD, ExD);
    if (denom < 1e-20f) return false;  // parallel
    const float s = dot(cross(sub(curve.origin, A), D), ExD) / denom;
    if (s <= 0.02f || s >= 0.98f) return false;  // interior crossing only
    const Vec3 P = add(A, scale(E, s));
    const Vec3 w = sub(P, curve.origin);
    const Vec3 perp = sub(w, scale(D, dot(w, D)));  // component off the imprint line
    if (length(perp) > eps) return false;           // lines skew, no true meeting
    sOut = s;
    return true;
}
// Fractions s in (0.02, 0.98) at which the straight segment A→B crosses the
// circle of centre C radius r (both coplanar): roots of |A + sE − C|² = r².
// Returns 0/1/2 interior crossings (a tangent counts once).
int circleSegmentFracs(const Vec3& A, const Vec3& B, const Vec3& C, float r, float out[2])
{
    const Vec3 E = sub(B, A), d = sub(A, C);
    const float a = dot(E, E);
    if (a < 1e-20f) return 0;
    const float b = 2.f * dot(d, E), c = dot(d, d) - r * r;
    const float disc = b * b - 4.f * a * c;
    if (disc < 0.f) return 0;
    const float sq = std::sqrt(disc);
    int cnt = 0;
    const float s0 = (-b - sq) / (2.f * a), s1 = (-b + sq) / (2.f * a);
    if (s0 > 0.02f && s0 < 0.98f) out[cnt++] = s0;
    if (sq > 1e-9f && s1 > 0.02f && s1 < 0.98f) out[cnt++] = s1;  // skip tangent duplicate
    return cnt;
}

// Point-in-polygon for a planar polygon `poly` with normal `n`, projected to the
// plane's 2D frame (ray-crossing rule).
bool pointInPlanarPolygon(const Vec3& p, const std::vector<Vec3>& poly, const Vec3& n)
{
    if (poly.size() < 3) return false;
    const Vec3 u = normalize(sub(poly[1], poly[0]));
    const Vec3 v = cross(n, u);
    auto x2 = [&](const Vec3& q) { return dot(sub(q, poly[0]), u); };
    auto y2 = [&](const Vec3& q) { return dot(sub(q, poly[0]), v); };
    const float px = x2(p), py = y2(p);
    bool inside = false;
    const size_t m = poly.size();
    for (size_t i = 0, j = m - 1; i < m; j = i++) {
        const float xi = x2(poly[i]), yi = y2(poly[i]);
        const float xj = x2(poly[j]), yj = y2(poly[j]);
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}
uint64_t edgeKey(uint32_t a, uint32_t b)
{
    const uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
    return (static_cast<uint64_t>(lo) << 32) | hi;
}
uint64_t dirKey(uint32_t a, uint32_t b)
{
    return (static_cast<uint64_t>(a) << 32) | b;
}
// Ray/triangle intersection (Möller–Trumbore). On a forward hit (t > tEps)
// returns true and flags `degenerate` when the hit grazes an edge/vertex or the
// ray is near-parallel — the caller then re-casts along a different direction so
// crossing parity stays exact.
bool rayTriangle(const Vec3& o, const Vec3& d, const Vec3& v0, const Vec3& v1,
                 const Vec3& v2, bool& degenerate)
{
    constexpr float kParEps = 1e-8f;   // near-parallel determinant
    constexpr float kEdgeEps = 1e-6f;  // barycentric edge/vertex grazing
    constexpr float kTEps = 1e-7f;     // forward-hit threshold
    const Vec3 e1 = sub(v1, v0), e2 = sub(v2, v0);
    const Vec3 h = cross(d, e2);
    const float a = dot(e1, h);
    if (a > -kParEps && a < kParEps) return false;  // parallel: measure-zero, skip
    const float f = 1.f / a;
    const Vec3 s = sub(o, v0);
    const float u = f * dot(s, h);
    const Vec3 q = cross(s, e1);
    const float v = f * dot(d, q);
    const float t = f * dot(e2, q);
    // Grazing an edge/vertex ⇒ ambiguous parity; ask for a re-cast.
    if (u > -kEdgeEps && u < kEdgeEps) degenerate = true;
    if (v > -kEdgeEps && v < kEdgeEps) degenerate = true;
    if (u + v > 1.f - kEdgeEps && u + v < 1.f + kEdgeEps) degenerate = true;
    if (t > -kTEps && t < kTEps) degenerate = true;
    if (u < 0.f || u > 1.f || v < 0.f || u + v > 1.f) return false;
    return t > kTEps;
}

// Squared distance from point p to triangle (a,b,c). Standard region-based
// closest-point-on-triangle (Ericson, Real-Time Collision Detection).
float pointTriangleDist2(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c)
{
    const Vec3 ab = sub(b, a), ac = sub(c, a), ap = sub(p, a);
    const float d1 = dot(ab, ap), d2 = dot(ac, ap);
    if (d1 <= 0.f && d2 <= 0.f) return dot(ap, ap);
    const Vec3 bp = sub(p, b);
    const float d3 = dot(ab, bp), d4 = dot(ac, bp);
    if (d3 >= 0.f && d4 <= d3) return dot(bp, bp);
    const Vec3 cp = sub(p, c);
    const float d5 = dot(ab, cp), d6 = dot(ac, cp);
    if (d6 >= 0.f && d5 <= d6) return dot(cp, cp);
    const float vc = d1 * d4 - d3 * d2;
    if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
        const float w = d1 / (d1 - d3);
        const Vec3 q = add(a, scale(ab, w));
        return dot(sub(p, q), sub(p, q));
    }
    const float vb = d5 * d2 - d1 * d6;
    if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
        const float w = d2 / (d2 - d6);
        const Vec3 q = add(a, scale(ac, w));
        return dot(sub(p, q), sub(p, q));
    }
    const float va = d3 * d6 - d5 * d4;
    if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
        const float w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
        const Vec3 q = add(b, scale(sub(c, b), w));
        return dot(sub(p, q), sub(p, q));
    }
    const float denom = 1.f / (va + vb + vc);
    const float w2 = vb * denom, w3 = vc * denom;
    const Vec3 q = add(a, add(scale(ab, w2), scale(ac, w3)));
    return dot(sub(p, q), sub(p, q));
}

}  // namespace

// ──────────── Geometry evaluation ────────────────────────────────────────────

Vec3 Curve::eval(float t) const noexcept
{
    switch (kind) {
        case CurveKind::Line:
            return {origin.x + dir.x * t, origin.y + dir.y * t, origin.z + dir.z * t};
        case CurveKind::Circle: {
            const Vec3 bi = cross(dir, ref);  // dir = axis normal, ref = radius dir
            const float c = std::cos(t), s = std::sin(t);
            return {origin.x + radius * (c * ref.x + s * bi.x),
                    origin.y + radius * (c * ref.y + s * bi.y),
                    origin.z + radius * (c * ref.z + s * bi.z)};
        }
        case CurveKind::Nurbs:
        default:
            return origin;  // NURBS store wired in a later increment
    }
}

Vec3 Curve::normalAt(float t) const noexcept
{
    switch (kind) {
        case CurveKind::Line:
            return dir;  // tangent = direction
        case CurveKind::Circle: {
            const Vec3 bi = cross(dir, ref);
            const float c = std::cos(t), s = std::sin(t);
            return {c * ref.x + s * bi.x, c * ref.y + s * bi.y, c * ref.z + s * bi.z};
        }
        case CurveKind::Nurbs:
        default:
            return dir;
    }
}

Vec3 Surface::vAxis() const noexcept { return cross(normal, uAxis); }

Vec3 Surface::eval(float u, float v) const noexcept
{
    switch (kind) {
        case SurfaceKind::Plane: {
            const Vec3 va = vAxis();
            return {origin.x + u * uAxis.x + v * va.x,
                    origin.y + u * uAxis.y + v * va.y,
                    origin.z + u * uAxis.z + v * va.z};
        }
        case SurfaceKind::Cylinder: {
            const Vec3 va = vAxis();
            const float c = std::cos(u), s = std::sin(u);
            return {origin.x + radius * (c * uAxis.x + s * va.x) + v * normal.x,
                    origin.y + radius * (c * uAxis.y + s * va.y) + v * normal.y,
                    origin.z + radius * (c * uAxis.z + s * va.z) + v * normal.z};
        }
        case SurfaceKind::Sphere: {
            const Vec3 va = vAxis();
            const float cu = std::cos(u), su = std::sin(u);
            const float cv = std::cos(v), sv = std::sin(v);
            // u = longitude [-π,π], v = latitude [-π/2, π/2]
            return {origin.x + radius * (su * uAxis.x + cu * cv * va.x + cu * sv * normal.x),
                    origin.y + radius * (su * uAxis.y + cu * cv * va.y + cu * sv * normal.y),
                    origin.z + radius * (su * uAxis.z + cu * cv * va.z + cu * sv * normal.z)};
        }
        case SurfaceKind::Nurbs:
        default:
            return origin;
    }
}

Vec3 Surface::normalAt(float u, float v) const noexcept
{
    switch (kind) {
        case SurfaceKind::Plane:
            return normal;
        case SurfaceKind::Cylinder: {
            const float c = std::cos(u), s = std::sin(u);
            return normalize({c * uAxis.x + s * vAxis().x,
                              c * uAxis.y + s * vAxis().y,
                              c * uAxis.z + s * vAxis().z});
        }
        case SurfaceKind::Sphere: {
            const Vec3 va = vAxis();
            const float cu = std::cos(u), su = std::sin(u);
            const float cv = std::cos(v), sv = std::sin(v);
            // outward normal = position - center = radius * (su*uAxis + cu*cv*va + cu*sv*normal)
            return normalize({su * uAxis.x + cu * cv * va.x + cu * sv * normal.x,
                              su * uAxis.y + cu * cv * va.y + cu * sv * normal.y,
                              su * uAxis.z + cu * cv * va.z + cu * sv * normal.z});
        }
        default:
            (void)v;
            return normal;
    }
}

// ──────────── Construction ───────────────────────────────────────────────────

std::optional<Body> Body::fromFaces(const std::vector<Vec3>& points,
                                     const std::vector<FaceDef>& faces)
{
    if (points.empty() || faces.empty()) return std::nullopt;

    Body b;
    b.m_verts.resize(points.size());
    for (size_t i = 0; i < points.size(); ++i) b.m_verts[i].point = points[i];

    std::unordered_map<uint64_t, uint32_t> edgeMap;    // undirected -> edge id
    std::unordered_map<uint64_t, uint32_t> dirCoedge;  // directed (a,b) -> coedge id

    for (const FaceDef& fd : faces) {
        const size_t n = fd.loop.size();
        if (n < 3) return std::nullopt;
        for (uint32_t vi : fd.loop)
            if (vi >= points.size()) return std::nullopt;

        const uint32_t surfaceId = static_cast<uint32_t>(b.m_surfaces.size());
        b.m_surfaces.push_back(fd.surface);
        if (fd.nurbsSurface.has_value()) {
            const uint32_t handle = static_cast<uint32_t>(b.m_nurbsSurfaces.size());
            b.m_nurbsSurfaces.push_back(*fd.nurbsSurface);
            b.m_surfaces[surfaceId].kind = SurfaceKind::Nurbs;
            b.m_surfaces[surfaceId].nurbs = handle;
        }

        const uint32_t faceId = static_cast<uint32_t>(b.m_faces.size());
        b.m_faces.push_back({});
        const uint32_t loopId = static_cast<uint32_t>(b.m_loops.size());
        b.m_loops.push_back({});
        b.m_faces[faceId].surface = surfaceId;
        b.m_faces[faceId].outerLoop = loopId;
        b.m_loops[loopId].face = faceId;
        b.m_loops[loopId].outer = true;

        const uint32_t firstCoedge = static_cast<uint32_t>(b.m_coedges.size());
        for (size_t j = 0; j < n; ++j) {
            const uint32_t a = fd.loop[j];
            const uint32_t c = fd.loop[(j + 1) % n];
            if (a == c) return std::nullopt;  // degenerate edge

            const uint64_t ek = edgeKey(a, c);
            uint32_t edgeId;
            auto it = edgeMap.find(ek);
            if (it != edgeMap.end()) {
                edgeId = it->second;
            } else {
                const uint32_t curveId = static_cast<uint32_t>(b.m_curves.size());
                Curve cur;
                cur.kind = CurveKind::Line;
                cur.origin = points[a];
                const Vec3 d = sub(points[c], points[a]);
                cur.dir = normalize(d);
                b.m_curves.push_back(cur);

                edgeId = static_cast<uint32_t>(b.m_edges.size());
                Edge ed;
                ed.curve = curveId;
                ed.v0 = a;
                ed.v1 = c;
                ed.t0 = 0.f;
                ed.t1 = length(d);
                b.m_edges.push_back(ed);
                edgeMap.emplace(ek, edgeId);
            }

            const uint32_t coedgeId = static_cast<uint32_t>(b.m_coedges.size());
            Coedge ce;
            ce.edge = edgeId;
            ce.reversed = (b.m_edges[edgeId].v0 != a);  // edge stored a->c or c->a
            ce.loop = loopId;
            b.m_coedges.push_back(ce);

            if (b.m_edges[edgeId].coedge == kInvalid) b.m_edges[edgeId].coedge = coedgeId;
            if (b.m_verts[a].coedge == kInvalid) b.m_verts[a].coedge = coedgeId;

            dirCoedge[dirKey(a, c)] = coedgeId;
            auto pit = dirCoedge.find(dirKey(c, a));
            if (pit != dirCoedge.end()) {
                b.m_coedges[coedgeId].partner = pit->second;
                b.m_coedges[pit->second].partner = coedgeId;
            }
        }

        for (size_t j = 0; j < n; ++j) {
            const uint32_t cur = firstCoedge + static_cast<uint32_t>(j);
            b.m_coedges[cur].next = firstCoedge + static_cast<uint32_t>((j + 1) % n);
            b.m_coedges[cur].prev = firstCoedge + static_cast<uint32_t>((j + n - 1) % n);
        }
        b.m_loops[loopId].first = firstCoedge;
    }

    // One shell of all faces; closed iff every edge is used by two coedges.
    Shell sh;
    sh.faces.resize(b.m_faces.size());
    for (uint32_t f = 0; f < b.m_faces.size(); ++f) { sh.faces[f] = f; b.m_faces[f].shell = 0; }
    std::vector<uint32_t> coedgesPerEdge(b.m_edges.size(), 0u);
    for (const Coedge& ce : b.m_coedges)
        if (ce.edge < coedgesPerEdge.size()) ++coedgesPerEdge[ce.edge];
    sh.closed = true;
    for (uint32_t c : coedgesPerEdge)
        if (c != 2u) { sh.closed = false; break; }
    b.m_shells.push_back(std::move(sh));
    b.m_solids.push_back(Solid{{0u}});

    return b;
}

// ──────────── Integrity validation ───────────────────────────────────────────

Body::IntegrityReport Body::checkIntegrity() const
{
    IntegrityReport r;
    const uint32_t V = static_cast<uint32_t>(m_verts.size());
    const uint32_t E = static_cast<uint32_t>(m_edges.size());
    const uint32_t C = static_cast<uint32_t>(m_coedges.size());
    const uint32_t L = static_cast<uint32_t>(m_loops.size());
    const uint32_t F = static_cast<uint32_t>(m_faces.size());

    auto fail = [&](std::string why) -> IntegrityReport {
        IntegrityReport bad;
        bad.ok = false;
        bad.reason = std::move(why);
        return bad;
    };
    auto startV = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v1 : m_edges[e.edge].v0; };
    auto endV   = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v0 : m_edges[e.edge].v1; };
    // Liveness predicates: an entity is live iff in range AND its alive flag set.
    auto vLive = [&](uint32_t i) { return i < V && m_verts[i].alive; };
    auto eLive = [&](uint32_t i) { return i < E && m_edges[i].alive; };
    auto cLive = [&](uint32_t i) { return i < C && m_coedges[i].alive; };
    auto lLive = [&](uint32_t i) { return i < L && m_loops[i].alive; };
    auto fLive = [&](uint32_t i) { return i < F && m_faces[i].alive; };

    // Only LIVE edges are validated; a live edge must reference live vertices.
    for (uint32_t e = 0; e < E; ++e) {
        if (!m_edges[e].alive) continue;
        const Edge& ed = m_edges[e];
        if (ed.curve >= m_curves.size()) return fail("edge " + std::to_string(e) + " has invalid curve");
        if (!vLive(ed.v0) || !vLive(ed.v1)) return fail("live edge " + std::to_string(e) + " references a dead/invalid vertex");
        if (ed.v0 == ed.v1) return fail("edge " + std::to_string(e) + " is degenerate (v0==v1)");
    }

    std::vector<uint32_t> coedgesPerEdge(E, 0u);
    for (uint32_t c = 0; c < C; ++c) {
        if (!m_coedges[c].alive) continue;
        const Coedge& ce = m_coedges[c];
        if (!eLive(ce.edge)) return fail("live coedge " + std::to_string(c) + " references a dead/invalid edge");
        if (!lLive(ce.loop)) return fail("live coedge " + std::to_string(c) + " references a dead/invalid loop");
        if (!cLive(ce.next) || !cLive(ce.prev)) return fail("live coedge " + std::to_string(c) + " has dead/invalid next/prev");
        if (m_coedges[ce.next].prev != c) return fail("next.prev mismatch at coedge " + std::to_string(c));
        if (m_coedges[ce.prev].next != c) return fail("prev.next mismatch at coedge " + std::to_string(c));
        if (m_coedges[ce.next].loop != ce.loop) return fail("loop cycle spans multiple loops at coedge " + std::to_string(c));
        if (endV(c) != startV(ce.next)) return fail("vertex discontinuity at coedge " + std::to_string(c));
        if (ce.partner != kInvalid) {
            if (!cLive(ce.partner)) return fail("live coedge " + std::to_string(c) + " has a dead/invalid partner");
            const Coedge& pt = m_coedges[ce.partner];
            if (pt.partner != c) return fail("partner not reciprocal at coedge " + std::to_string(c));
            if (pt.edge != ce.edge) return fail("partner uses a different edge at coedge " + std::to_string(c));
            if (pt.reversed == ce.reversed) return fail("partner has same orientation at coedge " + std::to_string(c));
            if (pt.loop == ce.loop) return fail("partner on same loop at coedge " + std::to_string(c));
        }
        ++coedgesPerEdge[ce.edge];
        ++r.coedges;
    }

    for (uint32_t l = 0; l < L; ++l) {
        if (!m_loops[l].alive) continue;
        const Loop& lp = m_loops[l];
        if (!fLive(lp.face)) return fail("live loop " + std::to_string(l) + " references a dead/invalid face");
        if (!cLive(lp.first)) return fail("live loop " + std::to_string(l) + " references a dead/invalid first coedge");
        uint32_t walk = lp.first, steps = 0;
        do {
            if (m_coedges[walk].loop != l) return fail("loop " + std::to_string(l) + " ring leaves the loop");
            walk = m_coedges[walk].next;
            if (++steps > C) return fail("loop " + std::to_string(l) + " ring does not close");
        } while (walk != lp.first);
        if (steps < 3) return fail("loop " + std::to_string(l) + " has fewer than 3 coedges");
        ++r.loops;
    }

    for (uint32_t f = 0; f < F; ++f) {
        if (!m_faces[f].alive) continue;
        const Face& fc = m_faces[f];
        if (fc.surface >= m_surfaces.size()) return fail("face " + std::to_string(f) + " has invalid surface");
        if (!lLive(fc.outerLoop)) return fail("live face " + std::to_string(f) + " references a dead/invalid outer loop");
        if (!m_loops[fc.outerLoop].outer) return fail("face " + std::to_string(f) + " outer loop is not marked outer");
        if (m_loops[fc.outerLoop].face != f) return fail("face " + std::to_string(f) + " outer loop points elsewhere");
        for (uint32_t il : fc.innerLoops) {
            if (!lLive(il)) return fail("live face " + std::to_string(f) + " references a dead/invalid inner loop");
            if (m_loops[il].outer) return fail("face " + std::to_string(f) + " inner loop marked outer");
            if (m_loops[il].face != f) return fail("face " + std::to_string(f) + " inner loop points elsewhere");
        }
        ++r.faces;
    }

    for (uint32_t v = 0; v < V; ++v) {
        if (!m_verts[v].alive) continue;
        ++r.vertices;
        if (m_verts[v].coedge != kInvalid) {
            if (!cLive(m_verts[v].coedge)) return fail("live vertex " + std::to_string(v) + " references a dead/invalid coedge");
            if (startV(m_verts[v].coedge) != v) return fail("vertex " + std::to_string(v) + " coedge not rooted here");
        }
    }

    for (uint32_t e = 0; e < E; ++e) {
        if (!m_edges[e].alive) continue;
        ++r.edges;
        if (coedgesPerEdge[e] == 0u) return fail("edge " + std::to_string(e) + " is orphaned (no coedge)");
        if (coedgesPerEdge[e] > 2u) return fail("edge " + std::to_string(e) + " is non-manifold (>2 coedges)");
        if (coedgesPerEdge[e] == 1u) ++r.boundaryEdges;
    }

    r.euler = static_cast<int>(r.vertices) - static_cast<int>(r.edges) + static_cast<int>(r.faces);
    r.shells = static_cast<uint32_t>(m_shells.size());
    return r;
}

bool Body::isClosed() const noexcept
{
    // Topologically closed ⇔ every live edge is used by exactly two coedges (no
    // boundary edge). Reflects the actual complex, incl. holes/open faces, rather
    // than a possibly-stale shell flag.
    std::vector<uint32_t> cnt(m_edges.size(), 0u);
    for (const Coedge& c : m_coedges)
        if (c.alive && c.edge < cnt.size()) ++cnt[c.edge];
    bool anyLive = false;
    for (uint32_t e = 0; e < m_edges.size(); ++e) {
        if (!m_edges[e].alive) continue;
        anyLive = true;
        if (cnt[e] != 2u) return false;
    }
    return anyLive;
}

// ──────────── Geometric-consistency validation ───────────────────────────────

Body::GeometryReport Body::checkGeometry(Tolerance tol) const
{
    GeometryReport r;
    auto fail = [&](std::string why) -> GeometryReport {
        GeometryReport bad;
        bad.ok = false;
        bad.reason = std::move(why);
        return bad;
    };

    // All (live) vertex points are finite.
    for (uint32_t v = 0; v < m_verts.size(); ++v)
        if (m_verts[v].alive && !isFinite(m_verts[v].point))
            return fail("vertex " + std::to_string(v) + " has a non-finite point");

    // All curve geometry is finite.
    for (uint32_t c = 0; c < m_curves.size(); ++c) {
        const Curve& cu = m_curves[c];
        if (!isFinite(cu.origin) || !isFinite(cu.dir) || !isFinite(cu.ref) || !isFinite(cu.radius))
            return fail("curve " + std::to_string(c) + " has non-finite geometry");
    }

    // All surface geometry is finite and normals are unit length.
    for (uint32_t s = 0; s < m_surfaces.size(); ++s) {
        const Surface& su = m_surfaces[s];
        if (!isFinite(su.origin) || !isFinite(su.normal) || !isFinite(su.uAxis) || !isFinite(su.radius))
            return fail("surface " + std::to_string(s) + " has non-finite geometry");
        if (!tol.nearlyEqual(length(su.normal), 1.f))
            return fail("surface " + std::to_string(s) + " normal is not unit length");
    }

    // Each edge's curve reproduces its endpoint vertices over its param range.
    for (uint32_t e = 0; e < m_edges.size(); ++e) {
        if (!m_edges[e].alive) continue;
        const Edge& ed = m_edges[e];
        if (ed.curve >= m_curves.size() || ed.v0 >= m_verts.size() || ed.v1 >= m_verts.size())
            return fail("edge " + std::to_string(e) + " has invalid references");
        const Curve& cu = m_curves[ed.curve];
        if (!coincident(cu.eval(ed.t0), m_verts[ed.v0].point, tol))
            return fail("edge " + std::to_string(e) + " curve does not meet v0 at t0");
        if (!coincident(cu.eval(ed.t1), m_verts[ed.v1].point, tol))
            return fail("edge " + std::to_string(e) + " curve does not meet v1 at t1");
        ++r.checkedEdges;
    }

    // Partnered coedges traverse the shared edge in opposite directions, so the
    // start vertex of one is the end vertex of the other (and vice versa).
    auto sVert = [&](const Coedge& x) { return x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0; };
    auto eVert = [&](const Coedge& x) { return x.reversed ? m_edges[x.edge].v0 : m_edges[x.edge].v1; };
    for (uint32_t c = 0; c < m_coedges.size(); ++c) {
        if (!m_coedges[c].alive) continue;
        const Coedge& ce = m_coedges[c];
        if (ce.partner == kInvalid || ce.partner >= m_coedges.size()) continue;
        const Coedge& pt = m_coedges[ce.partner];
        if (sVert(ce) != eVert(pt) || eVert(ce) != sVert(pt))
            return fail("coedge " + std::to_string(c) + " and partner disagree on shared-edge endpoints");
    }

    return r;
}

// ──────────── Euler operators ────────────────────────────────────────────────

uint32_t Body::splitEdge(uint32_t edgeId, float t)
{
    if (edgeId >= m_edges.size()) return kInvalid;
    const uint32_t curveId = m_edges[edgeId].curve;
    const uint32_t v0 = m_edges[edgeId].v0;
    const uint32_t v1 = m_edges[edgeId].v1;
    if (curveId >= m_curves.size() || v0 >= m_verts.size() || v1 >= m_verts.size()) return kInvalid;
    const float et0 = m_edges[edgeId].t0, et1 = m_edges[edgeId].t1;
    t = std::clamp(t, 0.01f, 0.99f);
    const float tm = et0 + (et1 - et0) * t;

    // New vertex on the shared curve.
    const uint32_t nv = static_cast<uint32_t>(m_verts.size());
    {
        Vertex vtx;
        vtx.point = m_curves[curveId].eval(tm);
        m_verts.push_back(vtx);
    }

    // Edge A reuses `edgeId` (v0 -> nv over [t0,tm]); edge B is new (nv -> v1).
    const uint32_t edgeA = edgeId;
    const uint32_t edgeB = static_cast<uint32_t>(m_edges.size());
    {
        Edge b;
        b.curve = curveId;
        b.v0 = nv;
        b.v1 = v1;
        b.t0 = tm;
        b.t1 = et1;
        m_edges.push_back(b);
    }
    m_edges[edgeA].v1 = nv;
    m_edges[edgeA].t1 = tm;

    // The (up to two) coedges over the original edge.
    const uint32_t c = m_edges[edgeA].coedge;
    if (c >= m_coedges.size()) return kInvalid;
    const uint32_t p = m_coedges[c].partner;

    // Split one coedge: reuse it as sub1, insert a sub2 after it in its loop.
    // A forward coedge (reversed=false) reads v0->v1, so sub1 is over edge A and
    // sub2 over edge B; a reversed coedge reads v1->v0, so sub1 is over edge B
    // and sub2 over edge A. Returns sub2's id.
    auto splitCoedge = [&](uint32_t ce) -> uint32_t {
        const bool rev = m_coedges[ce].reversed;
        const uint32_t oldNext = m_coedges[ce].next;
        const uint32_t sub2 = static_cast<uint32_t>(m_coedges.size());
        Coedge nc;
        nc.reversed = rev;
        nc.loop = m_coedges[ce].loop;
        nc.prev = ce;
        nc.next = oldNext;
        nc.edge = rev ? edgeA : edgeB;
        m_coedges.push_back(nc);
        m_coedges[ce].next = sub2;
        if (oldNext < m_coedges.size()) m_coedges[oldNext].prev = sub2;
        m_coedges[ce].edge = rev ? edgeB : edgeA;
        return sub2;
    };

    const uint32_t cNew = splitCoedge(c);
    uint32_t pNew = kInvalid;
    if (p != kInvalid && p < m_coedges.size()) pNew = splitCoedge(p);

    // Of a (coedge, its new sub2) pair, which sub sits over edge A / edge B.
    auto overA = [&](uint32_t ce, uint32_t ceNew) { return m_coedges[ce].reversed ? ceNew : ce; };
    auto overB = [&](uint32_t ce, uint32_t ceNew) { return m_coedges[ce].reversed ? ce : ceNew; };

    if (pNew != kInvalid) {
        const uint32_t cA = overA(c, cNew), cB = overB(c, cNew);
        const uint32_t pA = overA(p, pNew), pB = overB(p, pNew);
        m_coedges[cA].partner = pA; m_coedges[pA].partner = cA;
        m_coedges[cB].partner = pB; m_coedges[pB].partner = cB;
    } else {
        m_coedges[c].partner = kInvalid;
        m_coedges[cNew].partner = kInvalid;
    }

    // Edge back-references and the new vertex's outgoing coedge.
    m_edges[edgeA].coedge = overA(c, cNew);
    m_edges[edgeB].coedge = overB(c, cNew);
    // The coedge that starts at nv: over edge B if c is forward (nv->v1), over
    // edge A if c is reversed (nv->v0).
    m_verts[nv].coedge = m_coedges[c].reversed ? overA(c, cNew) : overB(c, cNew);

    return nv;
}

std::vector<uint32_t> Body::faceVertices(uint32_t faceId) const
{
    std::vector<uint32_t> out;
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return out;
    const uint32_t loopId = m_faces[faceId].outerLoop;
    if (loopId >= m_loops.size()) return out;
    uint32_t w = m_loops[loopId].first, guard = 0;
    if (w >= m_coedges.size()) return out;
    do {
        const Coedge& x = m_coedges[w];
        out.push_back(x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0);
        w = m_coedges[w].next;
        if (++guard > m_coedges.size() + 1) break;
    } while (w != m_loops[loopId].first);
    return out;
}

uint32_t Body::splitFace(uint32_t faceId, uint32_t vA, uint32_t vB)
{
    return cutFaceBetween(faceId, vA, vB, nullptr);
}

uint32_t Body::cutFaceBetween(uint32_t faceId, uint32_t vA, uint32_t vB,
                              const Curve* explicitCurve)
{
    if (faceId >= m_faces.size() || vA == vB) return kInvalid;
    const uint32_t loopId = m_faces[faceId].outerLoop;
    if (loopId >= m_loops.size() || m_loops[loopId].first >= m_coedges.size()) return kInvalid;

    // Walk the outer loop: coedges in order + their start vertices.
    std::vector<uint32_t> ce, sv;
    {
        uint32_t w = m_loops[loopId].first, guard = 0;
        do {
            ce.push_back(w);
            const Coedge& x = m_coedges[w];
            sv.push_back(x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0);
            w = m_coedges[w].next;
            if (++guard > m_coedges.size() + 1) return kInvalid;
        } while (w != m_loops[loopId].first);
    }
    const int n = static_cast<int>(ce.size());
    if (n < 4) return kInvalid;  // need a non-adjacent diagonal

    int posA = -1, posB = -1;
    for (int i = 0; i < n; ++i) {
        if (sv[static_cast<size_t>(i)] == vA) posA = i;
        if (sv[static_cast<size_t>(i)] == vB) posB = i;
    }
    if (posA < 0 || posB < 0) return kInvalid;
    if (posA > posB) std::swap(posA, posB);
    if (posB - posA == 1 || (posA == 0 && posB == n - 1)) return kInvalid;  // adjacent

    const uint32_t vStart = sv[static_cast<size_t>(posA)];
    const uint32_t vEnd = sv[static_cast<size_t>(posB)];

    // Partition the loop's coedges into the two sub-loops (segment A: posA..posB-1;
    // segment B: posB..end, 0..posA-1).
    std::vector<uint32_t> segA, segB;
    for (int i = posA; i < posB; ++i) segA.push_back(ce[static_cast<size_t>(i)]);
    for (int i = posB; i < n; ++i) segB.push_back(ce[static_cast<size_t>(i)]);
    for (int i = 0; i < posA; ++i) segB.push_back(ce[static_cast<size_t>(i)]);

    // New cut edge (vStart -> vEnd). splitFace builds a straight Line chord;
    // imprintCurve supplies the intersection curve itself, its param range set to
    // reproduce the two endpoints so checkGeometry holds.
    const uint32_t curveId = static_cast<uint32_t>(m_curves.size());
    float et0 = 0.f, et1 = 0.f;
    if (explicitCurve != nullptr) {
        m_curves.push_back(*explicitCurve);
        et0 = paramOnCurve(*explicitCurve, m_verts[vStart].point);
        et1 = paramOnCurve(*explicitCurve, m_verts[vEnd].point);
    } else {
        const Vec3 d = sub(m_verts[vEnd].point, m_verts[vStart].point);
        Curve cu;
        cu.kind = CurveKind::Line;
        cu.origin = m_verts[vStart].point;
        cu.dir = normalize(d);
        m_curves.push_back(cu);
        et1 = length(d);
    }
    const uint32_t edgeId = static_cast<uint32_t>(m_edges.size());
    {
        Edge e;
        e.curve = curveId;
        e.v0 = vStart;
        e.v1 = vEnd;
        e.t0 = et0;
        e.t1 = et1;
        m_edges.push_back(e);
    }

    // Face B + loop B (face A reuses faceId + its loop).
    const uint32_t faceB = static_cast<uint32_t>(m_faces.size());
    {
        Face f;
        f.surface = m_faces[faceId].surface;
        f.reversed = m_faces[faceId].reversed;
        f.shell = m_faces[faceId].shell;
        m_faces.push_back(f);
    }
    const uint32_t loopB = static_cast<uint32_t>(m_loops.size());
    {
        Loop l;
        l.face = faceB;
        l.outer = true;
        m_loops.push_back(l);
    }
    m_faces[faceB].outerLoop = loopB;

    // Diagonal coedges: dA closes loop A (vEnd->vStart, reversed on the new
    // edge), dB closes loop B (vStart->vEnd, forward). They are partners.
    const uint32_t dA = static_cast<uint32_t>(m_coedges.size());
    { Coedge x; x.edge = edgeId; x.reversed = true;  x.loop = loopId; m_coedges.push_back(x); }
    const uint32_t dB = static_cast<uint32_t>(m_coedges.size());
    { Coedge x; x.edge = edgeId; x.reversed = false; x.loop = loopB;  m_coedges.push_back(x); }
    m_coedges[dA].partner = dB;
    m_coedges[dB].partner = dA;

    for (uint32_t cb : segB) m_coedges[cb].loop = loopB;

    // Wire loop A: segA (in existing order) then dA back to the front.
    const uint32_t aFirst = segA.front(), aLast = segA.back();
    m_coedges[aLast].next = dA; m_coedges[dA].prev = aLast;
    m_coedges[dA].next = aFirst; m_coedges[aFirst].prev = dA;
    m_loops[loopId].first = aFirst;

    // Wire loop B: segB then dB back to the front.
    const uint32_t bFirst = segB.front(), bLast = segB.back();
    m_coedges[bLast].next = dB; m_coedges[dB].prev = bLast;
    m_coedges[dB].next = bFirst; m_coedges[bFirst].prev = dB;
    m_loops[loopB].first = bFirst;

    const uint32_t sh = m_faces[faceId].shell;
    if (sh < m_shells.size()) m_shells[sh].faces.push_back(faceB);
    m_edges[edgeId].coedge = dA;
    return faceB;
}

uint32_t Body::imprintCurve(uint32_t faceId, const Curve& curve, Tolerance tol)
{
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return kInvalid;
    if (curve.kind != CurveKind::Line && curve.kind != CurveKind::Circle) return kInvalid;
    const uint32_t loopId = m_faces[faceId].outerLoop;
    if (loopId >= m_loops.size() || m_loops[loopId].first >= m_coedges.size()) return kInvalid;
    // Crossing test tolerance, proportioned to the face (unit-ish characteristic).
    const float eps = tol.at(1.f) * 10.f;

    // Walk the outer loop → ordered boundary vertices + their outgoing edges.
    std::vector<uint32_t> bVerts, bEdges;
    {
        uint32_t w = m_loops[loopId].first, guard = 0;
        do {
            const Coedge& x = m_coedges[w];
            bVerts.push_back(x.reversed ? m_edges[x.edge].v1 : m_edges[x.edge].v0);
            bEdges.push_back(x.edge);
            w = x.next;
            if (++guard > m_coedges.size() + 1) return kInvalid;
        } while (w != m_loops[loopId].first);
    }
    const size_t n = bVerts.size();

    // ── Circle imprint (arc bite crossing two distinct boundary edges) ──────────
    if (curve.kind == CurveKind::Circle) {
        // The face must be planar and the circle coplanar with it, so the arc lies
        // ON the face (axis ⟂ the plane, centre on the plane).
        const uint32_t sid = m_faces[faceId].surface;
        if (sid >= m_surfaces.size() || m_surfaces[sid].kind != SurfaceKind::Plane) return kInvalid;
        const Vec3 fn = m_surfaces[sid].normal;
        if (std::abs(dot(normalize(curve.dir), fn)) < 1.f - 1e-4f) return kInvalid;
        if (std::abs(dot(sub(curve.origin, m_surfaces[sid].origin), fn)) > eps) return kInvalid;

        // Boundary polygon (captured before any split) for the inside-arc test.
        std::vector<Vec3> poly;
        poly.reserve(n);
        for (uint32_t v : bVerts) poly.push_back(m_verts[v].point);

        // Circle × each Line boundary edge → crossings; need exactly two, on two
        // DISTINCT edges. (Same-edge bite and fully-interior circle → inner-loop
        // hole are follow-up increments.)
        struct CCross { uint32_t edge; float frac; };
        std::vector<CCross> cc;
        for (size_t i = 0; i < n; ++i) {
            const uint32_t e = bEdges[i];
            if (e >= m_edges.size()) continue;
            const uint32_t cu = m_edges[e].curve;
            if (cu >= m_curves.size() || m_curves[cu].kind != CurveKind::Line) continue;
            float fr[2];
            const int k = circleSegmentFracs(m_verts[m_edges[e].v0].point,
                                             m_verts[m_edges[e].v1].point, curve.origin,
                                             curve.radius, fr);
            for (int j = 0; j < k; ++j) cc.push_back({e, fr[j]});
        }
        constexpr float kTwoPi = 6.28318530717958647692f;

        // Arc bite: the circle crosses the boundary at two points on two distinct
        // edges → split the face along the arc between them.
        if (cc.size() == 2 && cc[0].edge != cc[1].edge) {
            const uint32_t vA = splitEdge(cc[0].edge, cc[0].frac);
            const uint32_t vB = splitEdge(cc[1].edge, cc[1].frac);
            if (vA == kInvalid || vB == kInvalid || vA == vB) return kInvalid;

            const uint32_t ce = static_cast<uint32_t>(m_edges.size());  // the cut edge id
            const uint32_t newFace = cutFaceBetween(faceId, vA, vB, &curve);
            if (newFace == kInvalid) return kInvalid;

            // cutFaceBetween set the cut edge's param range from the raw endpoint
            // angles, tracing one of the two arcs. Keep the one lying INSIDE the
            // face (both share the endpoints, so checkGeometry holds either way).
            if (ce < m_edges.size()) {
                const float t0 = m_edges[ce].t0, t1 = m_edges[ce].t1;
                if (!pointInPlanarPolygon(curve.eval((t0 + t1) * 0.5f), poly, fn))
                    m_edges[ce].t1 = (t1 > t0) ? (t1 - kTwoPi) : (t1 + kTwoPi);
            }
            return newFace;
        }
        if (!cc.empty()) return kInvalid;  // same-edge bite / odd crossings deferred

        // Fully-interior circle → an INNER LOOP (hole) of the face. The centre and
        // the whole circle must lie inside the outer boundary.
        if (!pointInPlanarPolygon(curve.origin, poly, fn)) return kInvalid;
        constexpr uint32_t K = 8;  // arc segments around the hole ring
        for (uint32_t k = 0; k < K; ++k)
            if (!pointInPlanarPolygon(curve.eval(kTwoPi * static_cast<float>(k) / K), poly, fn))
                return kInvalid;

        // One shared Circle curve + K arc edges + K coedges wound CW (opposite the
        // outer loop, so the ring bounds a hole) + one inner loop on this face.
        const uint32_t cid = static_cast<uint32_t>(m_curves.size());
        m_curves.push_back(curve);
        const uint32_t v0 = static_cast<uint32_t>(m_verts.size());
        for (uint32_t k = 0; k < K; ++k) {
            Vertex vt;
            vt.point = curve.eval(kTwoPi * static_cast<float>(k) / K);
            m_verts.push_back(vt);
        }
        const uint32_t e0 = static_cast<uint32_t>(m_edges.size());
        for (uint32_t k = 0; k < K; ++k) {
            Edge e;
            e.curve = cid;
            e.v0 = v0 + k;
            e.v1 = v0 + (k + 1) % K;
            e.t0 = kTwoPi * static_cast<float>(k) / K;
            e.t1 = kTwoPi * static_cast<float>(k + 1) / K;
            m_edges.push_back(e);
        }
        const uint32_t loopId = static_cast<uint32_t>(m_loops.size());
        {
            Loop l;
            l.face = faceId;
            l.outer = false;
            m_loops.push_back(l);
        }
        const uint32_t c0 = static_cast<uint32_t>(m_coedges.size());
        // Coedge CE_k reverses edge E_k (traverses V_{k+1}→V_k); the ring goes
        // CE_0 → CE_{K-1} → … → CE_1 → CE_0 (clockwise about the axis).
        for (uint32_t k = 0; k < K; ++k) {
            Coedge c;
            c.edge = e0 + k;
            c.reversed = true;
            c.loop = loopId;
            c.next = c0 + (k + K - 1) % K;
            c.prev = c0 + (k + 1) % K;
            m_coedges.push_back(c);
        }
        m_loops[loopId].first = c0;
        for (uint32_t k = 0; k < K; ++k) {
            m_edges[e0 + k].coedge = c0 + k;
            m_verts[v0 + k].coedge = c0 + (k + K - 1) % K;  // coedge starting at V_k
        }
        m_faces[faceId].innerLoops.push_back(loopId);
        return faceId;
    }

    // ── Line imprint ────────────────────────────────────────────────────────────
    // Perpendicular distance of a point to the (infinite) imprint line.
    auto distToLine = [&](const Vec3& p) {
        const Vec3 w = sub(p, curve.origin);
        return length(sub(w, scale(curve.dir, dot(w, curve.dir))));
    };

    // A boundary crossing is EITHER an existing boundary vertex the line passes
    // through, OR an interior point on a boundary edge. Recognising the vertex
    // case is essential for repeated/mutual imprinting: a neighbouring face's
    // imprint drops a vertex on the shared edge exactly where a later cut line
    // meets this face, so that meeting point is a vertex, not an edge interior.
    std::vector<bool> vOnLine(n, false);
    for (size_t i = 0; i < n; ++i) vOnLine[i] = distToLine(m_verts[bVerts[i]].point) <= eps;

    struct Cross { bool isVertex; uint32_t vertex; uint32_t edge; float frac; };
    std::vector<Cross> crossings;
    for (size_t i = 0; i < n; ++i) {
        if (vOnLine[i]) {
            crossings.push_back({true, bVerts[i], kInvalid, 0.f});
            continue;
        }
        // Interior crossing on edge i (skip if its far endpoint is on the line —
        // that crossing is already represented by the vertex case).
        if (vOnLine[(i + 1) % n]) continue;
        const uint32_t e = bEdges[i];
        if (e >= m_edges.size()) continue;
        const uint32_t cu = m_edges[e].curve;
        if (cu >= m_curves.size() || m_curves[cu].kind != CurveKind::Line) continue;
        float s = 0.f;
        if (segmentLineCrossing(m_verts[m_edges[e].v0].point, m_verts[m_edges[e].v1].point,
                                curve, eps, s))
            crossings.push_back({false, kInvalid, e, s});
    }
    if (crossings.size() != 2) return kInvalid;  // need a clean entry + exit

    // Resolve each crossing to a vertex (splitting the edge where interior). The
    // two edges are distinct, so splitting one leaves the other's frac valid.
    auto resolve = [&](const Cross& c) { return c.isVertex ? c.vertex : splitEdge(c.edge, c.frac); };
    const uint32_t vA = resolve(crossings[0]);
    const uint32_t vB = resolve(crossings[1]);
    if (vA == kInvalid || vB == kInvalid || vA == vB) return kInvalid;

    // Cut the face between the two crossing vertices with the imprint curve itself.
    return cutFaceBetween(faceId, vA, vB, &curve);
}

bool Body::joinEdges(uint32_t nv) { return joinEdgesImpl(nv, /*requireSameCurve=*/true, {}); }

bool Body::joinEdgesImpl(uint32_t nv, bool requireSameCurve, Tolerance tol)
{
    const uint32_t C = static_cast<uint32_t>(m_coedges.size());
    if (nv >= m_verts.size() || !m_verts[nv].alive) return false;

    auto startV = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v1 : m_edges[e.edge].v0; };
    auto endV   = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v0 : m_edges[e.edge].v1; };

    // The two live edges incident to nv (must be exactly two).
    uint32_t e1 = kInvalid, e2 = kInvalid;
    for (uint32_t e = 0; e < m_edges.size(); ++e) {
        if (!m_edges[e].alive) continue;
        if (m_edges[e].v0 == nv || m_edges[e].v1 == nv) {
            if (e1 == kInvalid) e1 = e;
            else if (e2 == kInvalid) e2 = e;
            else return false;  // degree > 2
        }
    }
    if (e1 == kInvalid || e2 == kInvalid) return false;

    // Non-nv endpoints.
    const uint32_t a1 = (m_edges[e1].v0 == nv) ? m_edges[e1].v1 : m_edges[e1].v0;
    const uint32_t a2 = (m_edges[e2].v0 == nv) ? m_edges[e2].v1 : m_edges[e2].v0;
    if (a1 == a2) return false;  // would collapse to a duplicate/degenerate edge

    // Parameters of a1 / a2 in the SURVIVOR (e1) curve's frame. When the two
    // edges already share a curve (joinEdges, e.g. re-joining a split arc), use
    // the stored params directly. Otherwise (mergeCollinearEdges) the two edges
    // must be COLLINEAR Lines — verify and project the far endpoints onto e1's
    // line to get their params.
    float pa1, pa2;
    if (m_edges[e1].curve == m_edges[e2].curve) {
        pa1 = (m_edges[e1].v0 == nv) ? m_edges[e1].t1 : m_edges[e1].t0;
        pa2 = (m_edges[e2].v0 == nv) ? m_edges[e2].t1 : m_edges[e2].t0;
    } else {
        if (requireSameCurve) return false;
        const uint32_t cid1 = m_edges[e1].curve, cid2 = m_edges[e2].curve;
        if (cid1 >= m_curves.size() || cid2 >= m_curves.size()) return false;
        const Curve& cs = m_curves[cid1];
        const Curve& co = m_curves[cid2];
        if (cs.kind != CurveKind::Line || co.kind != CurveKind::Line) return false;
        if (std::abs(dot(cs.dir, co.dir)) < 1.f - 1e-4f) return false;  // not parallel
        const float dTol = tol.at(1.f) * 10.f;
        auto onLine = [&](uint32_t v) {
            const Vec3 w = sub(m_verts[v].point, cs.origin);
            return length(sub(w, scale(cs.dir, dot(w, cs.dir)))) <= dTol;
        };
        if (!onLine(a2)) return false;  // e2 not on e1's supporting line
        auto param = [&](uint32_t v) { return dot(sub(m_verts[v].point, cs.origin), cs.dir); };
        pa1 = param(a1);
        pa2 = param(a2);
    }

    const uint32_t vLow  = (pa1 < pa2) ? a1 : a2;
    const uint32_t vHigh = (pa1 < pa2) ? a2 : a1;
    const float    tLow  = (pa1 < pa2) ? pa1 : pa2;
    const float    tHigh = (pa1 < pa2) ? pa2 : pa1;

    // Merge pairs: each coedge that ENDS at nv (over e1 or e2), with its next
    // (which starts at nv over the other edge). One pair per incident face.
    struct Pair { uint32_t ceIn, ceOut, sIn; };
    std::vector<Pair> pairs;
    for (uint32_t c = 0; c < C; ++c) {
        if (!m_coedges[c].alive) continue;
        const uint32_t ed = m_coedges[c].edge;
        if (ed != e1 && ed != e2) continue;
        if (endV(c) != nv) continue;
        const uint32_t ceOut = m_coedges[c].next;
        if (ceOut >= C || !m_coedges[ceOut].alive || startV(ceOut) != nv) return false;
        pairs.push_back({c, ceOut, startV(c)});
    }
    if (pairs.empty() || pairs.size() > 2) return false;

    const uint32_t survivor = e1;
    std::vector<uint32_t> merged;
    for (const Pair& pr : pairs) {
        const uint32_t on = m_coedges[pr.ceOut].next;
        m_coedges[pr.ceIn].edge = survivor;
        m_coedges[pr.ceIn].reversed = (pr.sIn == vHigh);
        m_coedges[pr.ceIn].next = on;
        if (on < C) m_coedges[on].prev = pr.ceIn;
        const uint32_t lp = m_coedges[pr.ceIn].loop;
        if (lp < m_loops.size() && m_loops[lp].first == pr.ceOut) m_loops[lp].first = pr.ceIn;
        m_coedges[pr.ceOut].alive = false;
        merged.push_back(pr.ceIn);
    }

    // Survivor edge now spans vLow -> vHigh over [tLow, tHigh].
    m_edges[survivor].v0 = vLow;
    m_edges[survivor].v1 = vHigh;
    m_edges[survivor].t0 = tLow;
    m_edges[survivor].t1 = tHigh;
    m_edges[survivor].coedge = merged[0];

    if (merged.size() == 2) {
        m_coedges[merged[0]].partner = merged[1];
        m_coedges[merged[1]].partner = merged[0];
    } else {
        m_coedges[merged[0]].partner = kInvalid;
    }

    // Tombstone the removed vertex + edge.
    m_edges[e2].alive = false;
    m_verts[nv].alive = false;

    // Re-root the surviving endpoints onto a live outgoing coedge.
    auto reroot = [&](uint32_t x) {
        const uint32_t cur = m_verts[x].coedge;
        if (cur < C && m_coedges[cur].alive && startV(cur) == x) return;
        m_verts[x].coedge = kInvalid;
        for (uint32_t c = 0; c < static_cast<uint32_t>(m_coedges.size()); ++c)
            if (m_coedges[c].alive && startV(c) == x) { m_verts[x].coedge = c; break; }
    };
    reroot(vLow);
    reroot(vHigh);
    return true;
}

bool Body::mergeFaces(uint32_t edgeId)
{
    const uint32_t C = static_cast<uint32_t>(m_coedges.size());
    if (edgeId >= m_edges.size() || !m_edges[edgeId].alive) return false;

    auto startV = [&](uint32_t c) { const Coedge& e = m_coedges[c]; return e.reversed ? m_edges[e.edge].v1 : m_edges[e.edge].v0; };

    const uint32_t cA = m_edges[edgeId].coedge;
    if (cA >= C || !m_coedges[cA].alive) return false;
    const uint32_t cB = m_coedges[cA].partner;
    if (cB == kInvalid || cB >= C || !m_coedges[cB].alive) return false;  // boundary edge

    const uint32_t lA = m_coedges[cA].loop, lB = m_coedges[cB].loop;
    if (lA >= m_loops.size() || lB >= m_loops.size() || lA == lB) return false;
    const uint32_t fA = m_loops[lA].face, fB = m_loops[lB].face;
    if (fA == fB) return false;  // both coedges on one face (a seam/slit) — not a merge

    const uint32_t pA = m_coedges[cA].prev, nA = m_coedges[cA].next;
    const uint32_t pB = m_coedges[cB].prev, nB = m_coedges[cB].next;
    // If either side would collapse (a face was a single-edge sliver), refuse.
    if (pA == cA || nA == cA || pB == cB || nB == cB) return false;

    // Collect loop B's coedges (excluding cB) before splicing, to re-home them.
    std::vector<uint32_t> loopBCoedges;
    {
        uint32_t w = nB, guard = 0;
        while (w != cB) {
            loopBCoedges.push_back(w);
            w = m_coedges[w].next;
            if (++guard > C) return false;
        }
    }

    // Splice the two rings into one by bypassing cA and cB:
    //   cA.prev -> cB.next   and   cB.prev -> cA.next.
    m_coedges[pA].next = nB; m_coedges[nB].prev = pA;
    m_coedges[pB].next = nA; m_coedges[nA].prev = pB;

    // Re-home loop B's coedges onto loop A (the surviving loop).
    for (uint32_t c : loopBCoedges) m_coedges[c].loop = lA;
    m_loops[lA].first = pA;  // pA is live and on loop A

    // Tombstone the removed edge, its two coedges, face B and its loop.
    m_edges[edgeId].alive = false;
    m_coedges[cA].alive = false;
    m_coedges[cB].alive = false;
    m_faces[fB].alive = false;
    m_loops[lB].alive = false;

    // Drop face B from its shell's list (defensive: no dead id lingering).
    const uint32_t sh = m_faces[fB].shell;
    if (sh < m_shells.size()) {
        auto& fs = m_shells[sh].faces;
        fs.erase(std::remove(fs.begin(), fs.end(), fB), fs.end());
    }

    // Re-root the removed edge's endpoints onto a live outgoing coedge.
    auto reroot = [&](uint32_t x) {
        const uint32_t cur = m_verts[x].coedge;
        if (cur < C && m_coedges[cur].alive && startV(cur) == x) return;
        m_verts[x].coedge = kInvalid;
        for (uint32_t c = 0; c < static_cast<uint32_t>(m_coedges.size()); ++c)
            if (m_coedges[c].alive && startV(c) == x) { m_verts[x].coedge = c; break; }
    };
    reroot(m_edges[edgeId].v0);
    reroot(m_edges[edgeId].v1);
    return true;
}

uint32_t Body::mergeCoplanarFaces(Tolerance tol)
{
    auto faceOfCoedge = [&](uint32_t c) -> uint32_t {
        if (c >= m_coedges.size() || !m_coedges[c].alive) return kInvalid;
        const uint32_t l = m_coedges[c].loop;
        return l < m_loops.size() ? m_loops[l].face : kInvalid;
    };
    // Outward normal of a planar face (surface normal, flipped when reversed).
    auto outwardN = [&](uint32_t f) -> Vec3 {
        const Face& fc = m_faces[f];
        const Vec3 n = (fc.surface < m_surfaces.size()) ? m_surfaces[fc.surface].normal
                                                        : Vec3{0.f, 0.f, 1.f};
        return fc.reversed ? Vec3{-n.x, -n.y, -n.z} : n;
    };
    // Coplanar: planar faces, equal outward normal, and fB's points on fA's plane.
    auto coplanar = [&](uint32_t fA, uint32_t fB) -> bool {
        const Face &FA = m_faces[fA], &FB = m_faces[fB];
        if (FA.surface >= m_surfaces.size() || FB.surface >= m_surfaces.size()) return false;
        const Surface &sA = m_surfaces[FA.surface], &sB = m_surfaces[FB.surface];
        if (sA.kind != SurfaceKind::Plane || sB.kind != SurfaceKind::Plane) return false;
        const Vec3 nA = outwardN(fA), nB = outwardN(fB);
        if (dot(nA, nB) < 1.f - 1e-4f) return false;  // parallel & co-oriented
        const std::vector<uint32_t> vs = faceVertices(fB);
        if (vs.empty()) return false;
        const float dTol = tol.at(1.f) * 10.f;
        for (uint32_t v : vs)
            if (std::abs(dot(sub(m_verts[v].point, sA.origin), nA)) > dTol) return false;
        return true;
    };
    // Number of edges the two faces share (via a coedge and its partner).
    auto sharedEdges = [&](uint32_t fA, uint32_t fB) -> int {
        int shared = 0;
        const uint32_t loopId = m_faces[fA].outerLoop;
        if (loopId >= m_loops.size()) return 0;
        uint32_t w = m_loops[loopId].first, guard = 0;
        if (w >= m_coedges.size()) return 0;
        do {
            const uint32_t p = m_coedges[w].partner;
            if (p != kInvalid && p < m_coedges.size() && m_coedges[p].alive &&
                faceOfCoedge(p) == fB)
                ++shared;
            w = m_coedges[w].next;
            if (++guard > m_coedges.size() + 1) break;
        } while (w != m_loops[loopId].first);
        return shared;
    };

    uint32_t merges = 0;
    bool changed = true;
    size_t safety = 0;
    while (changed && ++safety < 100000) {
        changed = false;
        for (uint32_t e = 0; e < m_edges.size(); ++e) {
            if (!m_edges[e].alive) continue;
            const uint32_t cA = m_edges[e].coedge;
            if (cA >= m_coedges.size() || !m_coedges[cA].alive) continue;
            const uint32_t cB = m_coedges[cA].partner;
            if (cB == kInvalid || cB >= m_coedges.size() || !m_coedges[cB].alive) continue;
            const uint32_t fA = faceOfCoedge(cA), fB = faceOfCoedge(cB);
            if (fA == kInvalid || fB == kInvalid || fA == fB) continue;
            if (!coplanar(fA, fB)) continue;
            if (sharedEdges(fA, fB) != 1) continue;  // 2+ shared → would slit
            if (mergeFaces(e)) {
                ++merges;
                changed = true;
                break;  // indices shifted; restart the scan
            }
        }
    }
    return merges;
}

uint32_t Body::mergeCollinearEdges(Tolerance tol)
{
    uint32_t removed = 0;
    bool changed = true;
    size_t safety = 0;
    while (changed && ++safety < 100000) {
        changed = false;
        for (uint32_t v = 0; v < m_verts.size(); ++v) {
            if (!m_verts[v].alive) continue;
            if (joinEdgesImpl(v, /*requireSameCurve=*/false, tol)) {
                ++removed;
                changed = true;
                break;  // liveness shifted; restart the scan
            }
        }
    }
    return removed;
}

uint32_t Body::simplify(Tolerance tol)
{
    uint32_t total = 0, pass = 0;
    do {
        // Each pass unlocks the other: merging collinear edges exposes new
        // single-shared-edge coplanar pairs, and merging faces exposes new
        // degree-2 collinear vertices.
        pass = mergeCoplanarFaces(tol) + mergeCollinearEdges(tol);
        total += pass;
    } while (pass > 0);
    return total;
}

// ──────────── Surface evaluation ─────────────────────────────────────────────

Vec3 Body::surfacePoint(uint32_t surfaceId, float u, float v) const
{
    if (surfaceId >= m_surfaces.size()) return {};
    const Surface& s = m_surfaces[surfaceId];
    if (s.kind == SurfaceKind::Nurbs && s.nurbs < m_nurbsSurfaces.size())
        return m_nurbsSurfaces[s.nurbs].evaluate(u, v);
    return s.eval(u, v);
}

Body::PointContainment Body::classifyPoint(const Vec3& p, Tolerance tol) const
{
    // Tessellate the shell to a watertight, crack-free triangle set. subdivisions
    // are placed per shared edge, so curved faces are approximated coherently.
    Mesh mesh = toMesh(6);
    (void)mesh.topology().triangulate();
    const auto& pos = mesh.attributes().positions();
    const auto& topo = mesh.topology();
    const size_t triCount = topo.faceCount();
    if (triCount == 0) return PointContainment::Outside;

    // OnBoundary: within tol of the tessellated boundary.
    const float tolAbs2 = tol.absolute * tol.absolute;
    for (size_t i = 0; i < triCount; ++i) {
        const auto& idx = topo.face(i).indices;
        if (idx.size() != 3) continue;
        if (pointTriangleDist2(p, pos[idx[0]], pos[idx[1]], pos[idx[2]]) <= tolAbs2)
            return PointContainment::OnBoundary;
    }

    // Parity ray cast. Try a small fixed set of irrational directions and use the
    // first that yields no grazing/degenerate hit, so the count is exact and the
    // result is deterministic.
    static constexpr Vec3 kDirs[4] = {
        {0.4680f, 0.6301f, 0.6201f},
        {0.8017f, -0.2673f, 0.5345f},
        {-0.3333f, 0.6667f, -0.6667f},
        {0.5774f, -0.5774f, -0.5774f},
    };
    for (const Vec3& raw : kDirs) {
        const Vec3 d = normalize(raw);
        bool degenerate = false;
        int crossings = 0;
        for (size_t i = 0; i < triCount; ++i) {
            const auto& idx = topo.face(i).indices;
            if (idx.size() != 3) continue;
            if (rayTriangle(p, d, pos[idx[0]], pos[idx[1]], pos[idx[2]], degenerate))
                ++crossings;
            if (degenerate) break;  // ambiguous: re-cast along the next direction
        }
        if (!degenerate)
            return (crossings & 1) ? PointContainment::Inside : PointContainment::Outside;
    }
    // Extremely unlikely fallback: last direction's parity (all directions grazed).
    const Vec3 d = normalize(kDirs[0]);
    bool degenerate = false;
    int crossings = 0;
    for (size_t i = 0; i < triCount; ++i) {
        const auto& idx = topo.face(i).indices;
        if (idx.size() != 3) continue;
        if (rayTriangle(p, d, pos[idx[0]], pos[idx[1]], pos[idx[2]], degenerate)) ++crossings;
    }
    return (crossings & 1) ? PointContainment::Inside : PointContainment::Outside;
}

Vec3 Body::faceCentroid(uint32_t faceId) const
{
    const std::vector<uint32_t> vs = faceVertices(faceId);
    if (vs.empty()) return {};
    Vec3 c{0.f, 0.f, 0.f};
    for (uint32_t v : vs)
        if (v < m_verts.size()) c = add(c, m_verts[v].point);
    return scale(c, 1.f / static_cast<float>(vs.size()));
}

Body::PointContainment Body::classifyFace(uint32_t faceId, const Body& other, Tolerance tol) const
{
    if (faceId >= m_faces.size() || !m_faces[faceId].alive) return PointContainment::Outside;
    return other.classifyPoint(faceCentroid(faceId), tol);
}

nexus::geometry::MassProperties Body::massProperties(float density) const
{
    // Integrate volume / centroid / inertia over the watertight boundary
    // triangulation via the divergence theorem — Eberly's exact "Polyhedral Mass
    // Properties" surface integrals. Subdivisions refine the curved (arc) edges so
    // cylinder/sphere converge; box faces are flat, so they stay exact.
    nexus::geometry::MassProperties mp;
    const Mesh mesh = toMesh(3);
    const auto& pos = mesh.attributes().positions();
    const auto& topo = mesh.topology();

    // intg: volume, ∫x ∫y ∫z, ∫x² ∫y² ∫z², ∫xy ∫yz ∫zx (un-normalised).
    double intg[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto sub = [](double w0, double w1, double w2, double& f1, double& f2, double& f3,
                  double& g0, double& g1, double& g2) {
        const double t0 = w0 + w1, t1 = w0 * w0, t2 = t1 + w1 * t0;
        f1 = t0 + w2;
        f2 = t2 + w2 * f1;
        f3 = w0 * t1 + w1 * t2 + w2 * f2;
        g0 = f2 + w0 * (f1 + w0);
        g1 = f2 + w1 * (f1 + w1);
        g2 = f2 + w2 * (f1 + w2);
    };
    for (size_t t = 0; t < topo.faceCount(); ++t) {
        const auto& id = topo.face(t).indices;
        if (id.size() != 3) continue;
        const nexus::render::Vec3 p0 = pos[id[0]], p1 = pos[id[1]], p2 = pos[id[2]];
        const double x0 = p0.x, y0 = p0.y, z0 = p0.z;
        const double x1 = p1.x, y1 = p1.y, z1 = p1.z;
        const double x2 = p2.x, y2 = p2.y, z2 = p2.z;
        const double a1 = x1 - x0, b1 = y1 - y0, c1 = z1 - z0;
        const double a2 = x2 - x0, b2 = y2 - y0, c2 = z2 - z0;
        const double d0 = b1 * c2 - b2 * c1;  // (p1-p0)×(p2-p0), un-normalised normal
        const double d1 = a2 * c1 - a1 * c2;
        const double d2 = a1 * b2 - a2 * b1;
        double f1x, f2x, f3x, g0x, g1x, g2x, f1y, f2y, f3y, g0y, g1y, g2y, f1z, f2z, f3z, g0z,
            g1z, g2z;
        sub(x0, x1, x2, f1x, f2x, f3x, g0x, g1x, g2x);
        sub(y0, y1, y2, f1y, f2y, f3y, g0y, g1y, g2y);
        sub(z0, z1, z2, f1z, f2z, f3z, g0z, g1z, g2z);
        intg[0] += d0 * f1x;
        intg[1] += d0 * f2x;
        intg[2] += d1 * f2y;
        intg[3] += d2 * f2z;
        intg[4] += d0 * f3x;
        intg[5] += d1 * f3y;
        intg[6] += d2 * f3z;
        intg[7] += d0 * (y0 * g0x + y1 * g1x + y2 * g2x);
        intg[8] += d1 * (z0 * g0y + z1 * g1y + z2 * g2y);
        intg[9] += d2 * (x0 * g0z + x1 * g1z + x2 * g2z);
    }
    static constexpr double mult[10] = {1.0 / 6,  1.0 / 24, 1.0 / 24, 1.0 / 24,  1.0 / 60,
                                        1.0 / 60, 1.0 / 60, 1.0 / 120, 1.0 / 120, 1.0 / 120};
    for (int i = 0; i < 10; ++i) intg[i] *= mult[i];

    const double vol = intg[0];
    if (vol <= 1e-12) return mp;  // open / degenerate / inward → zero result
    mp.volume = static_cast<float>(vol);
    const double cx = intg[1] / vol, cy = intg[2] / vol, cz = intg[3] / vol;
    mp.centroid = {static_cast<float>(cx), static_cast<float>(cy), static_cast<float>(cz)};

    // Inertia tensor about the centroid, scaled by density (mass = density·vol).
    const double d = static_cast<double>(density);
    const double Ixx = d * (intg[5] + intg[6] - vol * (cy * cy + cz * cz));
    const double Iyy = d * (intg[4] + intg[6] - vol * (cz * cz + cx * cx));
    const double Izz = d * (intg[4] + intg[5] - vol * (cx * cx + cy * cy));
    const double Ixy = d * -(intg[7] - vol * cx * cy);
    const double Iyz = d * -(intg[8] - vol * cy * cz);
    const double Ixz = d * -(intg[9] - vol * cz * cx);
    mp.inertia[0][0] = static_cast<float>(Ixx);
    mp.inertia[1][1] = static_cast<float>(Iyy);
    mp.inertia[2][2] = static_cast<float>(Izz);
    mp.inertia[0][1] = mp.inertia[1][0] = static_cast<float>(Ixy);
    mp.inertia[1][2] = mp.inertia[2][1] = static_cast<float>(Iyz);
    mp.inertia[0][2] = mp.inertia[2][0] = static_cast<float>(Ixz);
    return mp;
}

bool Body::transform(const nexus::render::Mat4& mat)
{
    using nexus::render::Vec4;
    // Reject non-finite matrices up front (before any mutation).
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            if (!isFinite(mat.m[i][j])) return false;
    // The bottom row must be affine [0 0 0 1].
    if (mat.m[3][0] != 0.f || mat.m[3][1] != 0.f || mat.m[3][2] != 0.f || mat.m[3][3] != 1.f)
        return false;
    // NURBS-backed faces store control points we do not transform here — a follow-up.
    if (!m_nurbsSurfaces.empty()) return false;

    // Linear part columns (image of the basis vectors). Require a proper rotation
    // times a single UNIFORM scale: equal-length, mutually orthogonal, det > 0.
    const Vec3 a0{mat.m[0][0], mat.m[1][0], mat.m[2][0]};
    const Vec3 a1{mat.m[0][1], mat.m[1][1], mat.m[2][1]};
    const Vec3 a2{mat.m[0][2], mat.m[1][2], mat.m[2][2]};
    const float s0 = length(a0), s1 = length(a1), s2 = length(a2);
    if (s0 < 1e-12f || s1 < 1e-12f || s2 < 1e-12f) return false;
    const float sTol = 1e-4f * s0;
    if (std::abs(s0 - s1) > sTol || std::abs(s0 - s2) > sTol) return false;  // non-uniform
    const float oTol = 1e-4f * s0 * s0;
    if (std::abs(dot(a0, a1)) > oTol || std::abs(dot(a0, a2)) > oTol ||
        std::abs(dot(a1, a2)) > oTol)
        return false;  // shear
    if (dot(a0, cross(a1, a2)) <= 0.f) return false;  // reflection / degenerate
    const float s = s0;  // uniform scale factor

    auto xformPoint = [&](const Vec3& p) {
        const Vec4 r = mat * Vec4{p.x, p.y, p.z, 1.f};
        return Vec3{r.x, r.y, r.z};
    };
    // Direction: linear part only, renormalized (proper rotation ⇒ unit preserved).
    auto xformDir = [&](const Vec3& d) {
        const Vec4 r = mat * Vec4{d.x, d.y, d.z, 0.f};
        return normalize(Vec3{r.x, r.y, r.z});
    };

    for (auto& v : m_verts) v.point = xformPoint(v.point);
    for (auto& c : m_curves) {
        c.origin = xformPoint(c.origin);
        c.dir = xformDir(c.dir);
        c.ref = xformDir(c.ref);
        c.radius *= s;  // Circle radius scales; Line ignores radius
    }
    // Line edge params are arc-length (distance) ⇒ scale by s; Circle params are
    // angles ⇒ preserved. (Curve::eval then still reproduces the endpoints.)
    for (auto& e : m_edges) {
        if (e.curve < m_curves.size() && m_curves[e.curve].kind == CurveKind::Line) {
            e.t0 *= s;
            e.t1 *= s;
        }
    }
    for (auto& sf : m_surfaces) {
        sf.origin = xformPoint(sf.origin);
        sf.normal = xformDir(sf.normal);
        sf.uAxis = xformDir(sf.uAxis);
        sf.radius *= s;  // Cylinder / Sphere radius; Plane ignores radius
    }
    return true;
}

bool Body::translate(const Vec3& t)
{
    nexus::render::Mat4 m = nexus::render::Mat4::identity();
    m.m[0][3] = t.x;
    m.m[1][3] = t.y;
    m.m[2][3] = t.z;
    return transform(m);
}

// ──────────── Tessellation ───────────────────────────────────────────────────

Mesh Body::toMesh(uint32_t subdivisions) const
{
    Mesh mesh;

    // Output vertices: live B-rep vertices (compacted), then `subdivisions`
    // intermediate points per live edge — placed via the edge's curve so BOTH
    // incident faces reference the SAME points (crack-free / watertight).
    std::vector<nexus::render::Vec3> pos;
    std::vector<int> vOut(m_verts.size(), -1);
    for (uint32_t v = 0; v < m_verts.size(); ++v) {
        if (!m_verts[v].alive) continue;
        vOut[v] = static_cast<int>(pos.size());
        pos.push_back(m_verts[v].point);
    }
    std::vector<std::vector<uint32_t>> edgeMid(m_edges.size());
    if (subdivisions > 0) {
        for (uint32_t e = 0; e < m_edges.size(); ++e) {
            if (!m_edges[e].alive || m_edges[e].curve >= m_curves.size()) continue;
            const Edge& ed = m_edges[e];
            const Curve& cu = m_curves[ed.curve];
            edgeMid[e].reserve(subdivisions);
            for (uint32_t k = 1; k <= subdivisions; ++k) {
                const float f = static_cast<float>(k) / static_cast<float>(subdivisions + 1u);
                edgeMid[e].push_back(static_cast<uint32_t>(pos.size()));
                pos.push_back(cu.eval(ed.t0 + (ed.t1 - ed.t0) * f));
            }
        }
    }
    mesh.attributes().setPositions(pos);

    // Output-vertex indices around a loop, with per-edge subdivision points
    // inserted in traversal order (crack-free with the neighbouring face).
    auto buildRing = [&](uint32_t firstCoedge) {
        std::vector<uint32_t> ring;
        if (firstCoedge >= m_coedges.size()) return ring;
        uint32_t walk = firstCoedge, steps = 0;
        do {
            const Coedge& ce = m_coedges[walk];
            const uint32_t sv = ce.reversed ? m_edges[ce.edge].v1 : m_edges[ce.edge].v0;
            if (sv < vOut.size() && vOut[sv] >= 0) ring.push_back(static_cast<uint32_t>(vOut[sv]));
            const std::vector<uint32_t>& mids = edgeMid[ce.edge];
            if (!ce.reversed)
                for (uint32_t m : mids) ring.push_back(m);
            else
                for (auto it = mids.rbegin(); it != mids.rend(); ++it) ring.push_back(*it);
            walk = ce.next;
            if (++steps > m_coedges.size()) break;
        } while (walk != firstCoedge);
        return ring;
    };
    auto emitTri = [&](uint32_t a, uint32_t b, uint32_t c, const nexus::render::Vec3& nrm) {
        const nexus::render::Vec3 g = cross(sub(pos[b], pos[a]), sub(pos[c], pos[a]));
        nexus::geometry::Face f;  // the mesh Face, not brep::Face
        if (dot(g, nrm) < 0.f) f.indices = {a, c, b};
        else f.indices = {a, b, c};
        mesh.topology().addFace(std::move(f));
    };

    for (const Face& fc : m_faces) {
        if (!fc.alive || fc.outerLoop >= m_loops.size()) continue;
        const std::vector<uint32_t> ring = buildRing(m_loops[fc.outerLoop].first);
        if (ring.size() < 3) continue;

        if (fc.innerLoops.empty()) {
            for (size_t i = 2; i < ring.size(); ++i) {
                nexus::geometry::Face f;
                f.indices = {ring[0], ring[static_cast<uint32_t>(i) - 1], ring[i]};
                mesh.topology().addFace(std::move(f));
            }
            continue;
        }

        // Face with a hole: bridge the first inner ring into the outer ring to
        // form one simple polygon, then ear-clip it. (Multiple holes on one face
        // are a rare follow-up.)
        nexus::render::Vec3 nrm{0.f, 0.f, 1.f};
        if (fc.surface < m_surfaces.size()) nrm = m_surfaces[fc.surface].normal;
        if (fc.reversed) nrm = {-nrm.x, -nrm.y, -nrm.z};

        std::vector<uint32_t> inner;
        for (uint32_t il : fc.innerLoops) {
            if (il >= m_loops.size()) continue;
            inner = buildRing(m_loops[il].first);
            if (inner.size() >= 3) break;
            inner.clear();
        }
        if (inner.empty()) {  // no usable hole → fan the outer as a fallback
            for (size_t i = 2; i < ring.size(); ++i)
                emitTri(ring[0], ring[static_cast<uint32_t>(i) - 1], ring[i], nrm);
            continue;
        }

        // 2D frame of the face plane.
        const nexus::render::Vec3 u = normalize(sub(pos[ring[1]], pos[ring[0]]));
        const nexus::render::Vec3 vv = cross(nrm, u);
        const nexus::render::Vec3 org = pos[ring[0]];
        auto X = [&](uint32_t idx) { return dot(sub(pos[idx], org), u); };
        auto Y = [&](uint32_t idx) { return dot(sub(pos[idx], org), vv); };

        // Bridge the rightmost inner vertex M to the rightmost outer vertex P
        // (both to the right of the hole → the bridge crosses neither ring, for a
        // convex-ish outer + convex hole).
        size_t pOut = 0;
        for (size_t k = 1; k < ring.size(); ++k)
            if (X(ring[k]) > X(ring[pOut])) pOut = k;
        size_t mIn = 0;
        for (size_t k = 1; k < inner.size(); ++k)
            if (X(inner[k]) > X(inner[mIn])) mIn = k;

        std::vector<uint32_t> poly;
        poly.reserve(ring.size() + inner.size() + 2);
        for (size_t k = 0; k <= pOut; ++k) poly.push_back(ring[k]);
        for (size_t k = 0; k < inner.size(); ++k) poly.push_back(inner[(mIn + k) % inner.size()]);
        poly.push_back(inner[mIn]);
        for (size_t k = pOut; k < ring.size(); ++k) poly.push_back(ring[k]);

        // Ear-clip the simple polygon (CCW). Duplicate bridge vertices share their
        // pos index, so they are excluded from the containment test by identity.
        auto cross2 = [&](uint32_t a, uint32_t b, uint32_t c) {
            return (X(b) - X(a)) * (Y(c) - Y(a)) - (Y(b) - Y(a)) * (X(c) - X(a));
        };
        std::vector<uint32_t> idx(poly.size());
        for (uint32_t k = 0; k < poly.size(); ++k) idx[k] = k;
        float area = 0.f;
        for (size_t k = 0; k < idx.size(); ++k)
            area += cross2(poly[idx[0]], poly[idx[k]],
                           poly[idx[(k + 1) % idx.size()]]);  // sign only
        if (area < 0.f) std::reverse(idx.begin(), idx.end());
        auto inTri = [&](uint32_t p, uint32_t a, uint32_t b, uint32_t c) {
            const float d1 = cross2(a, b, p), d2 = cross2(b, c, p), d3 = cross2(c, a, p);
            const bool neg = d1 < 0.f || d2 < 0.f || d3 < 0.f;
            const bool ppos = d1 > 0.f || d2 > 0.f || d3 > 0.f;
            return !(neg && ppos);
        };
        size_t guard = 0;
        while (idx.size() > 3 && guard++ < 20000) {
            bool clipped = false;
            const size_t m2 = idx.size();
            for (size_t a = 0; a < m2; ++a) {
                const uint32_t pv = poly[idx[(a + m2 - 1) % m2]];
                const uint32_t cv = poly[idx[a]];
                const uint32_t nv = poly[idx[(a + 1) % m2]];
                if (cross2(pv, cv, nv) <= 1e-12f) continue;  // reflex / collinear
                bool ok = true;
                for (size_t k = 0; k < m2 && ok; ++k) {
                    const uint32_t q = poly[idx[k]];
                    if (q == pv || q == cv || q == nv) continue;  // skip bridge dups
                    if (inTri(q, pv, cv, nv)) ok = false;
                }
                if (!ok) continue;
                emitTri(pv, cv, nv, nrm);
                idx.erase(idx.begin() + static_cast<long>(a));
                clipped = true;
                break;
            }
            if (!clipped) break;
        }
        if (idx.size() == 3) emitTri(poly[idx[0]], poly[idx[1]], poly[idx[2]], nrm);
    }
    return mesh;
}

bool Body::setEdgeArc(uint32_t edgeId, const Vec3& center, const Vec3& axis, float radius, Tolerance tol)
{
    if (edgeId >= m_edges.size() || !m_edges[edgeId].alive) return false;
    const uint32_t v0 = m_edges[edgeId].v0, v1 = m_edges[edgeId].v1;
    if (v0 >= m_verts.size() || v1 >= m_verts.size()) return false;
    const Vec3 p0 = m_verts[v0].point, p1 = m_verts[v1].point;
    const Vec3 ax = normalize(axis);
    const Vec3 d0 = sub(p0, center), d1 = sub(p1, center);
    // Both endpoints must lie on the circle (radius, in the plane through center
    // perpendicular to axis).
    if (!tol.nearlyEqual(length(d0), radius) || !tol.nearlyEqual(length(d1), radius)) return false;
    if (!tol.isZero(dot(d0, ax)) || !tol.isZero(dot(d1, ax))) return false;

    const Vec3 ref0 = normalize(d0);      // radius direction at v0 (t = 0)
    const Vec3 bi = cross(ax, ref0);      // sweep direction
    const float ang = std::atan2(dot(d1, bi), dot(d1, ref0));  // signed short-arc angle

    const uint32_t curveId = m_edges[edgeId].curve;
    Curve& cu = m_curves[curveId];        // per-edge curve (not shared) — safe to retag
    cu.kind = CurveKind::Circle;
    cu.origin = center;
    cu.dir = ax;
    cu.ref = ref0;
    cu.radius = radius;
    m_edges[edgeId].t0 = 0.f;
    m_edges[edgeId].t1 = ang;
    return true;
}

// ──────────── Serialization ──────────────────────────────────────────────────

namespace {
constexpr std::uint32_t kBRepMagic = 0x5242584Eu;  // 'NXBR'
constexpr std::uint32_t kBRepVersion = 1u;

void putU8(std::vector<std::uint8_t>& o, std::uint8_t v) { o.push_back(v); }
void putU32(std::vector<std::uint8_t>& o, std::uint32_t v)
{
    o.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
    o.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
}
void putI32(std::vector<std::uint8_t>& o, std::int32_t v) { putU32(o, std::bit_cast<std::uint32_t>(v)); }
void putF32(std::vector<std::uint8_t>& o, float v) { putU32(o, std::bit_cast<std::uint32_t>(v)); }
void putVec3(std::vector<std::uint8_t>& o, const Vec3& v) { putF32(o, v.x); putF32(o, v.y); putF32(o, v.z); }

struct Reader {
    const std::vector<std::uint8_t>& b;
    std::size_t off = 0;
    bool ok = true;
    bool u8(std::uint8_t& out)
    {
        if (!ok || off + 1 > b.size()) return ok = false;
        out = b[off++];
        return true;
    }
    bool u32(std::uint32_t& out)
    {
        if (!ok || off + 4 > b.size()) return ok = false;
        out = static_cast<std::uint32_t>(b[off]) | (static_cast<std::uint32_t>(b[off + 1]) << 8) |
              (static_cast<std::uint32_t>(b[off + 2]) << 16) |
              (static_cast<std::uint32_t>(b[off + 3]) << 24);
        off += 4;
        return true;
    }
    bool i32(std::int32_t& out)
    {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        out = std::bit_cast<std::int32_t>(u);
        return true;
    }
    bool f32(float& out)
    {
        std::uint32_t u = 0;
        if (!u32(u)) return false;
        out = std::bit_cast<float>(u);
        if (!isFinite(out)) return ok = false;  // reject non-finite on read
        return true;
    }
    bool vec3(Vec3& out) { return f32(out.x) && f32(out.y) && f32(out.z); }
    bool boolean(bool& out)
    {
        std::uint8_t v = 0;
        if (!u8(v)) return false;
        out = (v != 0);
        return true;
    }
    // A length prefix that cannot exceed the remaining bytes (each element ≥1B),
    // so a garbage buffer can't trigger a huge allocation.
    bool count(std::uint32_t& out)
    {
        if (!u32(out)) return false;
        if (out > b.size()) return ok = false;
        return true;
    }
    bool u32Vec(std::vector<std::uint32_t>& out)
    {
        std::uint32_t n = 0;
        if (!count(n)) return false;
        out.resize(n);
        for (auto& e : out)
            if (!u32(e)) return false;
        return true;
    }
    bool floatVec(std::vector<float>& out)
    {
        std::uint32_t n = 0;
        if (!count(n)) return false;
        out.resize(n);
        for (auto& e : out)
            if (!f32(e)) return false;
        return true;
    }
    bool vec3Vec(std::vector<Vec3>& out)
    {
        std::uint32_t n = 0;
        if (!count(n)) return false;
        out.resize(n);
        for (auto& e : out)
            if (!vec3(e)) return false;
        return true;
    }
};
}  // namespace

std::vector<std::uint8_t> Body::serialize() const
{
    std::vector<std::uint8_t> o;
    putU32(o, kBRepMagic);
    putU32(o, kBRepVersion);

    putU32(o, static_cast<std::uint32_t>(m_verts.size()));
    for (const Vertex& v : m_verts) {
        putVec3(o, v.point);
        putU32(o, v.coedge);
        putU8(o, v.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_edges.size()));
    for (const Edge& e : m_edges) {
        putU32(o, e.curve);
        putU32(o, e.v0);
        putU32(o, e.v1);
        putF32(o, e.t0);
        putF32(o, e.t1);
        putU32(o, e.coedge);
        putU8(o, e.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_coedges.size()));
    for (const Coedge& c : m_coedges) {
        putU32(o, c.edge);
        putU8(o, c.reversed ? 1u : 0u);
        putU32(o, c.loop);
        putU32(o, c.next);
        putU32(o, c.prev);
        putU32(o, c.partner);
        putU8(o, c.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_loops.size()));
    for (const Loop& l : m_loops) {
        putU32(o, l.face);
        putU32(o, l.first);
        putU8(o, l.outer ? 1u : 0u);
        putU8(o, l.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_faces.size()));
    for (const Face& f : m_faces) {
        putU32(o, f.surface);
        putU8(o, f.reversed ? 1u : 0u);
        putU32(o, f.outerLoop);
        putU32(o, static_cast<std::uint32_t>(f.innerLoops.size()));
        for (std::uint32_t il : f.innerLoops) putU32(o, il);
        putU32(o, f.shell);
        putU8(o, f.alive ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_shells.size()));
    for (const Shell& s : m_shells) {
        putU32(o, static_cast<std::uint32_t>(s.faces.size()));
        for (std::uint32_t fi : s.faces) putU32(o, fi);
        putU8(o, s.closed ? 1u : 0u);
    }
    putU32(o, static_cast<std::uint32_t>(m_solids.size()));
    for (const Solid& s : m_solids) {
        putU32(o, static_cast<std::uint32_t>(s.shells.size()));
        for (std::uint32_t sh : s.shells) putU32(o, sh);
    }
    putU32(o, static_cast<std::uint32_t>(m_curves.size()));
    for (const Curve& c : m_curves) {
        putU8(o, static_cast<std::uint8_t>(c.kind));
        putVec3(o, c.origin);
        putVec3(o, c.dir);
        putVec3(o, c.ref);
        putF32(o, c.radius);
        putU32(o, c.nurbs);
    }
    putU32(o, static_cast<std::uint32_t>(m_surfaces.size()));
    for (const Surface& s : m_surfaces) {
        putU8(o, static_cast<std::uint8_t>(s.kind));
        putVec3(o, s.origin);
        putVec3(o, s.normal);
        putVec3(o, s.uAxis);
        putF32(o, s.radius);
        putU32(o, s.nurbs);
    }
    putU32(o, static_cast<std::uint32_t>(m_nurbsSurfaces.size()));
    for (const NurbsSurface& n : m_nurbsSurfaces) {
        putI32(o, n.degreeU());
        putI32(o, n.degreeV());
        putI32(o, n.controlPointCountU());
        putI32(o, n.controlPointCountV());
        putU32(o, static_cast<std::uint32_t>(n.knotU().size()));
        for (float k : n.knotU()) putF32(o, k);
        putU32(o, static_cast<std::uint32_t>(n.knotV().size()));
        for (float k : n.knotV()) putF32(o, k);
        putU32(o, static_cast<std::uint32_t>(n.controlPoints().size()));
        for (const Vec3& p : n.controlPoints()) putVec3(o, p);
        putU32(o, static_cast<std::uint32_t>(n.weights().size()));
        for (float w : n.weights()) putF32(o, w);
    }
    return o;
}

std::optional<Body> Body::deserialize(const std::vector<std::uint8_t>& bytes)
{
    Reader r{bytes};
    std::uint32_t magic = 0, version = 0;
    if (!r.u32(magic) || magic != kBRepMagic) return std::nullopt;
    if (!r.u32(version) || version < 1u || version > kBRepVersion) return std::nullopt;

    Body b;
    std::uint32_t n = 0;

    if (!r.count(n)) return std::nullopt;
    b.m_verts.resize(n);
    for (Vertex& v : b.m_verts)
        if (!r.vec3(v.point) || !r.u32(v.coedge) || !r.boolean(v.alive)) return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_edges.resize(n);
    for (Edge& e : b.m_edges)
        if (!r.u32(e.curve) || !r.u32(e.v0) || !r.u32(e.v1) || !r.f32(e.t0) || !r.f32(e.t1) ||
            !r.u32(e.coedge) || !r.boolean(e.alive))
            return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_coedges.resize(n);
    for (Coedge& c : b.m_coedges)
        if (!r.u32(c.edge) || !r.boolean(c.reversed) || !r.u32(c.loop) || !r.u32(c.next) ||
            !r.u32(c.prev) || !r.u32(c.partner) || !r.boolean(c.alive))
            return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_loops.resize(n);
    for (Loop& l : b.m_loops)
        if (!r.u32(l.face) || !r.u32(l.first) || !r.boolean(l.outer) || !r.boolean(l.alive))
            return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_faces.resize(n);
    for (Face& f : b.m_faces) {
        if (!r.u32(f.surface) || !r.boolean(f.reversed) || !r.u32(f.outerLoop)) return std::nullopt;
        if (!r.u32Vec(f.innerLoops)) return std::nullopt;
        if (!r.u32(f.shell) || !r.boolean(f.alive)) return std::nullopt;
    }

    if (!r.count(n)) return std::nullopt;
    b.m_shells.resize(n);
    for (Shell& s : b.m_shells) {
        if (!r.u32Vec(s.faces)) return std::nullopt;
        if (!r.boolean(s.closed)) return std::nullopt;
    }

    if (!r.count(n)) return std::nullopt;
    b.m_solids.resize(n);
    for (Solid& s : b.m_solids)
        if (!r.u32Vec(s.shells)) return std::nullopt;

    if (!r.count(n)) return std::nullopt;
    b.m_curves.resize(n);
    for (Curve& c : b.m_curves) {
        std::uint8_t kind = 0;
        if (!r.u8(kind) || !r.vec3(c.origin) || !r.vec3(c.dir) || !r.vec3(c.ref) ||
            !r.f32(c.radius) || !r.u32(c.nurbs))
            return std::nullopt;
        c.kind = static_cast<CurveKind>(kind);
    }

    if (!r.count(n)) return std::nullopt;
    b.m_surfaces.resize(n);
    for (Surface& s : b.m_surfaces) {
        std::uint8_t kind = 0;
        if (!r.u8(kind) || !r.vec3(s.origin) || !r.vec3(s.normal) || !r.vec3(s.uAxis) ||
            !r.f32(s.radius) || !r.u32(s.nurbs))
            return std::nullopt;
        s.kind = static_cast<SurfaceKind>(kind);
    }

    if (!r.count(n)) return std::nullopt;
    b.m_nurbsSurfaces.reserve(n);
    for (std::uint32_t i = 0; i < n; ++i) {
        std::int32_t degU = 0, degV = 0, nU = 0, nV = 0;
        std::vector<float> knotU, knotV, weights;
        std::vector<Vec3> ctl;
        if (!r.i32(degU) || !r.i32(degV) || !r.i32(nU) || !r.i32(nV)) return std::nullopt;
        if (!r.floatVec(knotU) || !r.floatVec(knotV) || !r.vec3Vec(ctl) || !r.floatVec(weights))
            return std::nullopt;
        if (weights.empty())
            b.m_nurbsSurfaces.emplace_back(degU, degV, std::move(knotU), std::move(knotV),
                                           std::move(ctl), nU, nV);
        else
            b.m_nurbsSurfaces.emplace_back(degU, degV, std::move(knotU), std::move(knotV),
                                           std::move(ctl), nU, nV, std::move(weights));
    }

    if (!r.ok) return std::nullopt;
    return b;
}

// ──────────── Primitives ─────────────────────────────────────────────────────

Body makeBox(float width, float height, float depth)
{
    const float w = width * 0.5f, h = height * 0.5f, d = depth * 0.5f;
    const std::vector<Vec3> pts = {
        {-w, -h, -d}, {w, -h, -d}, {w, h, -d}, {-w, h, -d},
        {-w, -h, d},  {w, -h, d},  {w, h, d},  {-w, h, d},
    };
    // Each face wound CCW as seen from outside (so shared edges are traversed in
    // opposite directions by adjacent faces → coedges partner and the shell is
    // watertight). The Plane surface takes the geometric outward normal.
    const uint32_t rings[6][4] = {
        {0, 3, 2, 1},  // -Z
        {4, 5, 6, 7},  // +Z
        {0, 1, 5, 4},  // -Y
        {2, 3, 7, 6},  // +Y
        {0, 4, 7, 3},  // -X
        {1, 2, 6, 5},  // +X
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(6);
    for (const auto& ring : rings) {
        Body::FaceDef fd;
        fd.loop = {ring[0], ring[1], ring[2], ring[3]};
        const Vec3& p0 = pts[ring[0]];
        const Vec3 e1 = sub(pts[ring[1]], p0);
        const Vec3 e2 = sub(pts[ring[3]], p0);
        Surface s;
        s.kind = SurfaceKind::Plane;
        s.origin = p0;
        s.normal = normalize(cross(e1, e2));  // outward for CCW-from-outside winding
        s.uAxis = normalize(e1);
        fd.surface = s;
        defs.push_back(std::move(fd));
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

// A capped cylinder along +Z: n side quads on a Cylinder surface + top/bottom
// planar n-gon caps. V=2n, E=3n, F=n+2 → euler 2.
Body makeCylinder(float radius, float height, uint32_t segments)
{
    const uint32_t n = std::max(segments, 3u);
    const float h = height * 0.5f;
    const float twoPi = 6.28318530717958647692f;

    std::vector<Vec3> pts(static_cast<size_t>(n) * 2u);
    for (uint32_t i = 0; i < n; ++i) {
        const float ang = twoPi * static_cast<float>(i) / static_cast<float>(n);
        const float cx = std::cos(ang) * radius, cy = std::sin(ang) * radius;
        pts[i] = {cx, cy, -h};              // bottom ring
        pts[static_cast<size_t>(n) + i] = {cx, cy, h};  // top ring
    }

    auto planeSurface = [](const Vec3& o, const Vec3& nrm) {
        Surface s;
        s.kind = SurfaceKind::Plane;
        s.origin = o;
        s.normal = nrm;
        s.uAxis = {1.f, 0.f, 0.f};
        return s;
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(static_cast<size_t>(n) + 2u);

    // Bottom cap: descending indices → CW in XY → CCW seen from −Z (outward −Z).
    Body::FaceDef bottom;
    bottom.loop.reserve(n);
    bottom.loop.push_back(0u);
    for (uint32_t i = n; i-- > 1;) bottom.loop.push_back(i);
    bottom.surface = planeSurface({0.f, 0.f, -h}, {0.f, 0.f, -1.f});
    defs.push_back(std::move(bottom));

    // Top cap: ascending indices → CCW seen from +Z (outward +Z).
    Body::FaceDef top;
    top.loop.reserve(n);
    for (uint32_t i = 0; i < n; ++i) top.loop.push_back(n + i);
    top.surface = planeSurface({0.f, 0.f, h}, {0.f, 0.f, 1.f});
    defs.push_back(std::move(top));

    // Side quads: {bottom[i], bottom[i+1], top[i+1], top[i]} — traverses each
    // shared edge opposite to its cap / neighbour side, so coedges partner.
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = (i + 1) % n;
        Body::FaceDef side;
        side.loop = {i, j, n + j, n + i};
        side.surface.kind = SurfaceKind::Cylinder;
        side.surface.origin = {0.f, 0.f, 0.f};
        side.surface.normal = {0.f, 0.f, 1.f};  // axis
        side.surface.uAxis = {1.f, 0.f, 0.f};
        side.surface.radius = radius;
        defs.push_back(std::move(side));
    }

    auto b = Body::fromFaces(pts, defs);
    if (!b.has_value()) return Body{};
    // Upgrade the ring edges (both endpoints at the same height) to Circle arcs
    // about the axis, so toMesh(subdivisions) renders the cylinder smoothly.
    for (uint32_t e = 0; e < static_cast<uint32_t>(b->edgeCount()); ++e) {
        const Vec3& p0 = b->vertex(b->edge(e).v0).point;
        const Vec3& p1 = b->vertex(b->edge(e).v1).point;
        if (std::abs(p0.z - p1.z) < 1e-5f)
            b->setEdgeArc(e, {0.f, 0.f, p0.z}, {0.f, 0.f, 1.f}, radius);
    }
    return std::move(*b);
}

// A cone along +Z: apex + bottom ring, n triangular sides + one planar cap.
// V=n+1, E=2n, F=n+1 → euler 2.
Body makeCone(float radius, float height, uint32_t segments)
{
    const uint32_t n = std::max(segments, 3u);
    const float h = height * 0.5f;
    const float twoPi = 6.28318530717958647692f;

    std::vector<Vec3> pts(static_cast<size_t>(n) + 1u);
    for (uint32_t i = 0; i < n; ++i) {
        const float ang = twoPi * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {std::cos(ang) * radius, std::sin(ang) * radius, -h};
    }
    const uint32_t apex = n;
    pts[apex] = {0.f, 0.f, h};

    std::vector<Body::FaceDef> defs;
    defs.reserve(static_cast<size_t>(n) + 1u);

    // Bottom cap (descending → outward −Z), opposite winding to the side tris.
    Body::FaceDef bottom;
    bottom.loop.reserve(n);
    bottom.loop.push_back(0u);
    for (uint32_t i = n; i-- > 1;) bottom.loop.push_back(i);
    bottom.surface.kind = SurfaceKind::Plane;
    bottom.surface.origin = {0.f, 0.f, -h};
    bottom.surface.normal = {0.f, 0.f, -1.f};
    bottom.surface.uAxis = {1.f, 0.f, 0.f};
    defs.push_back(std::move(bottom));

    // Side triangles: {bottom[i], bottom[i+1], apex} — traverses bottom[i]→
    // bottom[i+1] (opposite the cap) and shares apex spokes with neighbours.
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t j = (i + 1) % n;
        Body::FaceDef side;
        side.loop = {i, j, apex};
        side.surface.kind = SurfaceKind::Cylinder;  // conical surface approx (Nurbs later)
        side.surface.origin = {0.f, 0.f, 0.f};
        side.surface.normal = {0.f, 0.f, 1.f};
        side.surface.uAxis = {1.f, 0.f, 0.f};
        side.surface.radius = radius;
        defs.push_back(std::move(side));
    }

    auto b = Body::fromFaces(pts, defs);
    return b.has_value() ? std::move(*b) : Body{};
}

// A UV sphere: south + north pole vertices, (lat-1) latitude rings of `lon`
// verts, a triangle fan at each pole and quad bands between rings. All verts on
// the radius. V = 2 + (lat-1)*lon, F = lat*lon, E = lon*(2*lat-1) → euler 2.
Body makeSphere(float radius, uint32_t latSegments, uint32_t lonSegments)
{
    const uint32_t lat = std::max(latSegments, 2u);
    const uint32_t lon = std::max(lonSegments, 3u);
    const float pi = 3.14159265358979323846f;
    const float twoPi = 6.28318530717958647692f;

    // Vertex of latitude ring L (1..lat-1) at longitude j (wrapped).
    auto vid = [lon](uint32_t L, uint32_t j) -> uint32_t {
        return 1u + (L - 1u) * lon + (j % lon);
    };
    const uint32_t southPole = 0u;
    const uint32_t northPole = 1u + (lat - 1u) * lon;

    std::vector<Vec3> pts(2u + static_cast<size_t>(lat - 1u) * lon);
    pts[southPole] = {0.f, 0.f, -radius};
    pts[northPole] = {0.f, 0.f, radius};
    for (uint32_t L = 1u; L < lat; ++L) {
        const float v = -0.5f * pi + pi * static_cast<float>(L) / static_cast<float>(lat);
        const float cv = std::cos(v), sv = std::sin(v);
        for (uint32_t j = 0u; j < lon; ++j) {
            const float u = twoPi * static_cast<float>(j) / static_cast<float>(lon);
            pts[vid(L, j)] = {radius * cv * std::cos(u), radius * cv * std::sin(u), radius * sv};
        }
    }

    auto sphereFace = [radius](std::vector<uint32_t> loop) {
        Body::FaceDef fd;
        fd.loop = std::move(loop);
        fd.surface.kind = SurfaceKind::Sphere;
        fd.surface.origin = {0.f, 0.f, 0.f};
        fd.surface.normal = {0.f, 0.f, 1.f};
        fd.surface.uAxis = {1.f, 0.f, 0.f};
        fd.surface.radius = radius;
        return fd;
    };

    std::vector<Body::FaceDef> defs;
    defs.reserve(static_cast<size_t>(lat) * lon);

    // South pole fan — wound opposite to band 1 along the ring-1 edges.
    for (uint32_t j = 0u; j < lon; ++j)
        defs.push_back(sphereFace({southPole, vid(1u, j + 1u), vid(1u, j)}));

    // Latitude bands between ring L and L+1.
    for (uint32_t L = 1u; L + 1u < lat; ++L)
        for (uint32_t j = 0u; j < lon; ++j)
            defs.push_back(sphereFace({vid(L, j), vid(L, j + 1u), vid(L + 1u, j + 1u), vid(L + 1u, j)}));

    // North pole fan — wound opposite to the last band along the top-ring edges.
    const uint32_t topRing = lat - 1u;
    for (uint32_t j = 0u; j < lon; ++j)
        defs.push_back(sphereFace({northPole, vid(topRing, j), vid(topRing, j + 1u)}));

    auto b = Body::fromFaces(pts, defs);
    if (!b.has_value()) return Body{};
    // Every sphere edge is a circle arc: latitude rings are small circles about
    // the axis at their height; meridians (band verticals + pole spokes) are
    // great circles through the sphere centre. Upgrading them makes toMesh
    // tessellate exactly on the sphere.
    for (uint32_t e = 0; e < static_cast<uint32_t>(b->edgeCount()); ++e) {
        const Vec3& p0 = b->vertex(b->edge(e).v0).point;
        const Vec3& p1 = b->vertex(b->edge(e).v1).point;
        const float r0 = std::sqrt(p0.x * p0.x + p0.y * p0.y);
        const float r1 = std::sqrt(p1.x * p1.x + p1.y * p1.y);
        if (std::abs(p0.z - p1.z) < 1e-5f && std::abs(r0 - r1) < 1e-5f) {
            b->setEdgeArc(e, {0.f, 0.f, p0.z}, {0.f, 0.f, 1.f}, r0);  // latitude ring
        } else {
            const Vec3 ax = normalize(cross(p0, p1));                 // meridian great circle
            b->setEdgeArc(e, {0.f, 0.f, 0.f}, ax, radius);
        }
    }
    return std::move(*b);
}

// ──────────── Creation ───────────────────────────────────────────────────────

Body extrudeProfile(const std::vector<Vec3>& profile, const Vec3& dir)
{
    const size_t n = profile.size();
    if (n < 3) return Body{};
    for (const Vec3& p : profile)
        if (!isFinite(p)) return Body{};
    if (!isFinite(dir)) return Body{};

    // Profile plane normal (Newell's method; length = 2·area) → consistent with a
    // CCW winding about +N.
    Vec3 N{0.f, 0.f, 0.f};
    for (size_t i = 0; i < n; ++i) {
        const Vec3& a = profile[i];
        const Vec3& b = profile[(i + 1) % n];
        N.x += (a.y - b.y) * (a.z + b.z);
        N.y += (a.z - b.z) * (a.x + b.x);
        N.z += (a.x - b.x) * (a.y + b.y);
    }
    const float area2 = length(N);
    if (area2 < 1e-10f) return Body{};  // degenerate / near-zero-area profile
    N = scale(N, 1.f / area2);

    float h = dot(dir, N);
    if (h > -1e-9f && h < 1e-9f) return Body{};  // dir parallel to the plane

    // Orient so the profile is CCW about the extrusion direction (+N side gets the
    // top cap, giving a positive-volume outward solid regardless of dir's sign).
    std::vector<Vec3> poly = profile;
    if (h < 0.f) {
        std::reverse(poly.begin(), poly.end());
        N = {-N.x, -N.y, -N.z};
    }

    // Points: [0,n) bottom = poly, [n,2n) top = poly + dir.
    std::vector<Vec3> pts;
    pts.reserve(2 * n);
    for (const Vec3& p : poly) pts.push_back(p);
    for (const Vec3& p : poly) pts.push_back(add(p, dir));

    const Vec3 uAxis = normalize(sub(poly[1], poly[0]));
    std::vector<Body::FaceDef> defs;
    defs.reserve(n + 2);

    // Bottom cap: poly reversed → outward −N.
    {
        Body::FaceDef fd;
        for (size_t k = 0; k < n; ++k) fd.loop.push_back(static_cast<uint32_t>(n - 1 - k));
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = poly[0];
        fd.surface.normal = {-N.x, -N.y, -N.z};
        fd.surface.uAxis = uAxis;
        defs.push_back(std::move(fd));
    }
    // Top cap: poly order (shifted) → outward +N.
    {
        Body::FaceDef fd;
        for (size_t k = 0; k < n; ++k) fd.loop.push_back(static_cast<uint32_t>(n + k));
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[n];
        fd.surface.normal = N;
        fd.surface.uAxis = uAxis;
        defs.push_back(std::move(fd));
    }
    // Side quads: [b_i, b_{i+1}, t_{i+1}, t_i] — CCW from outside.
    for (size_t i = 0; i < n; ++i) {
        const uint32_t bi = static_cast<uint32_t>(i);
        const uint32_t bj = static_cast<uint32_t>((i + 1) % n);
        const uint32_t tj = static_cast<uint32_t>(n + (i + 1) % n);
        const uint32_t ti = static_cast<uint32_t>(n + i);
        Body::FaceDef fd;
        fd.loop = {bi, bj, tj, ti};
        const Vec3 e1 = sub(pts[bj], pts[bi]);
        const Vec3 e2 = sub(pts[ti], pts[bi]);
        fd.surface.kind = SurfaceKind::Plane;
        fd.surface.origin = pts[bi];
        fd.surface.normal = normalize(cross(e1, e2));
        fd.surface.uAxis = normalize(e1);
        defs.push_back(std::move(fd));
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

Body revolveProfile(const std::vector<Vec3>& profile, const Vec3& axisOrigin,
                    const Vec3& axisDir, uint32_t segments)
{
    const size_t m = profile.size();
    if (m < 3 || segments < 3) return Body{};
    for (const Vec3& p : profile)
        if (!isFinite(p)) return Body{};
    if (!isFinite(axisOrigin) || !isFinite(axisDir)) return Body{};
    const float axisLen = length(axisDir);
    if (axisLen < 1e-9f) return Body{};
    const Vec3 n = scale(axisDir, 1.f / axisLen);

    // Non-degenerate profile area (Newell).
    Vec3 Nprof{0.f, 0.f, 0.f};
    for (size_t i = 0; i < m; ++i) {
        const Vec3& a = profile[i];
        const Vec3& b = profile[(i + 1) % m];
        Nprof.x += (a.y - b.y) * (a.z + b.z);
        Nprof.y += (a.z - b.z) * (a.x + b.x);
        Nprof.z += (a.x - b.x) * (a.y + b.y);
    }
    if (length(Nprof) < 1e-10f) return Body{};

    // The profile must lie to ONE side of the axis: it may TOUCH the axis (a
    // vertex on it → a pole), but not cross to the other side. Off-axis vertices
    // must all share the same radial side.
    std::vector<bool> onAxis(m, false);
    Vec3 rHat{0.f, 0.f, 0.f};
    bool haveRHat = false;
    for (size_t i = 0; i < m; ++i) {
        const Vec3 v = sub(profile[i], axisOrigin);
        const Vec3 rad = sub(v, scale(n, dot(v, n)));
        const float rl = length(rad);
        if (rl < 1e-6f) {
            onAxis[i] = true;  // pole vertex
            continue;
        }
        if (!haveRHat) { rHat = scale(rad, 1.f / rl); haveRHat = true; }
        else if (dot(rad, rHat) <= 1e-6f) return Body{};  // crossed to the other side
    }
    if (!haveRHat) return Body{};  // entirely on the axis → degenerate

    // Rotate a point about the axis by `ang` (Rodrigues).
    auto rot = [&](const Vec3& p, float ang) {
        const Vec3 v = sub(p, axisOrigin);
        const float c = std::cos(ang), s = std::sin(ang);
        const Vec3 r = add(add(scale(v, c), scale(cross(n, v), s)),
                           scale(n, dot(n, v) * (1.f - c)));
        return add(axisOrigin, r);
    };

    // Vertices: an on-axis profile vertex welds to ONE pole point; an off-axis one
    // fans into `segments` rotated copies.
    const float twoPi = 6.28318530717958647692f;
    std::vector<Vec3> pts;
    std::vector<uint32_t> ringStart(m);  // pole index, or start of the ring of copies
    for (size_t i = 0; i < m; ++i) {
        ringStart[i] = static_cast<uint32_t>(pts.size());
        if (onAxis[i]) {
            pts.push_back(profile[i]);  // single welded pole vertex
        } else {
            for (uint32_t j = 0; j < segments; ++j)
                pts.push_back(rot(profile[i], twoPi * static_cast<float>(j) /
                                                  static_cast<float>(segments)));
        }
    }
    auto vAt = [&](size_t i, uint32_t j) {
        return onAxis[i] ? ringStart[i] : ringStart[i] + (j % segments);
    };

    // One face per (profile edge, angular step): a quad, or — where an endpoint is
    // a pole — a collapsed triangle; a profile edge lying ON the axis contributes
    // nothing. A full revolution needs no end caps.
    std::vector<Body::FaceDef> defs;
    defs.reserve(m * segments);
    for (uint32_t j = 0; j < segments; ++j) {
        for (size_t i = 0; i < m; ++i) {
            const size_t i1 = (i + 1) % m;
            // Outward winding (away from the axis).
            const uint32_t L[4] = {vAt(i, j), vAt(i, j + 1), vAt(i1, j + 1), vAt(i1, j)};
            std::vector<uint32_t> loop;
            for (int k = 0; k < 4; ++k)
                if (loop.empty() || loop.back() != L[k]) loop.push_back(L[k]);
            if (loop.size() >= 2 && loop.front() == loop.back()) loop.pop_back();
            if (loop.size() < 3) continue;  // degenerate (edge on the axis)

            Body::FaceDef fd;
            fd.loop = loop;
            fd.surface.kind = SurfaceKind::Plane;
            fd.surface.origin = pts[loop[0]];
            fd.surface.normal = normalize(
                cross(sub(pts[loop[1]], pts[loop[0]]), sub(pts[loop[2]], pts[loop[0]])));
            fd.surface.uAxis = normalize(sub(pts[loop[1]], pts[loop[0]]));
            defs.push_back(std::move(fd));
        }
    }

    auto body = Body::fromFaces(pts, defs);
    return body.has_value() ? std::move(*body) : Body{};
}

// ──────────── Boolean building blocks ────────────────────────────────────────

namespace {
// Imprint onto `target` every intersection Line where a planar face of `tool`
// transects one of target's faces, iterating to a fixpoint: each successful
// imprintCurve splits a straddling face, and the resulting sub-faces are
// re-scanned until no tool plane cuts any target face's interior. This leaves no
// target face straddling a tool face-plane.
void imprintOneWay(Body& target, const Body& tool, Tolerance tol)
{
    // Snapshot tool surfaces once (tool is not modified here).
    std::vector<Surface> toolSurfaces;
    toolSurfaces.reserve(tool.faceCount());
    for (uint32_t ft = 0; ft < tool.faceCount(); ++ft) {
        if (!tool.face(ft).alive) continue;
        const uint32_t sIdx = tool.face(ft).surface;
        if (sIdx < tool.surfaceCount()) toolSurfaces.push_back(tool.surface(sIdx));
    }

    // Fixpoint: one imprint per outer pass (face indices shift as sub-faces are
    // appended, so we re-scan from the front after each successful cut).
    bool changed = true;
    size_t safety = 0;
    const size_t cap = 100000;
    while (changed && ++safety < cap) {
        changed = false;
        const uint32_t fcount = static_cast<uint32_t>(target.faceCount());
        for (uint32_t f = 0; f < fcount && !changed; ++f) {
            if (!target.face(f).alive) continue;
            const uint32_t saIdx = target.face(f).surface;
            if (saIdx >= target.surfaceCount()) continue;
            const Surface sa = target.surface(saIdx);
            for (const Surface& sb : toolSurfaces) {
                const SurfaceIntersection si = intersectSurfaces(sa, sb, tol);
                if (si.kind != SurfaceIntersectionKind::Line) continue;
                if (target.imprintCurve(f, si.curve, tol) != kInvalid) {
                    changed = true;  // f was split; restart the scan
                    break;
                }
            }
        }
    }
}
}  // namespace

void imprintMutually(Body& a, Body& b, Tolerance tol)
{
    imprintOneWay(a, b, tol);
    imprintOneWay(b, a, tol);
}

}  // namespace nexus::geometry::brep
