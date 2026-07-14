#include <nexus/geometry/AnalyticBRep.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>

namespace nexus::geometry::brep {

namespace {
Vec3 sub(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
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
uint64_t edgeKey(uint32_t a, uint32_t b)
{
    const uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
    return (static_cast<uint64_t>(lo) << 32) | hi;
}
uint64_t dirKey(uint32_t a, uint32_t b)
{
    return (static_cast<uint64_t>(a) << 32) | b;
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

    for (uint32_t e = 0; e < E; ++e) {
        const Edge& ed = m_edges[e];
        if (ed.curve >= m_curves.size()) return fail("edge " + std::to_string(e) + " has invalid curve");
        if (ed.v0 >= V || ed.v1 >= V) return fail("edge " + std::to_string(e) + " has invalid vertex");
        if (ed.v0 == ed.v1) return fail("edge " + std::to_string(e) + " is degenerate (v0==v1)");
    }

    std::vector<uint32_t> coedgesPerEdge(E, 0u);
    for (uint32_t c = 0; c < C; ++c) {
        const Coedge& ce = m_coedges[c];
        if (ce.edge >= E) return fail("coedge " + std::to_string(c) + " has invalid edge");
        if (ce.loop >= L) return fail("coedge " + std::to_string(c) + " has invalid loop");
        if (!(ce.next < C) || !(ce.prev < C)) return fail("coedge " + std::to_string(c) + " has invalid next/prev");
        if (m_coedges[ce.next].prev != c) return fail("next.prev mismatch at coedge " + std::to_string(c));
        if (m_coedges[ce.prev].next != c) return fail("prev.next mismatch at coedge " + std::to_string(c));
        if (m_coedges[ce.next].loop != ce.loop) return fail("loop cycle spans multiple loops at coedge " + std::to_string(c));
        // Vertex continuity: this coedge's end vertex is the next's start vertex.
        if (endV(c) != startV(ce.next)) return fail("vertex discontinuity at coedge " + std::to_string(c));
        // Partner: reciprocal, same edge, opposite orientation, different loop.
        if (ce.partner != kInvalid) {
            if (ce.partner >= C) return fail("coedge " + std::to_string(c) + " has invalid partner");
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
        const Loop& lp = m_loops[l];
        if (lp.face >= F) return fail("loop " + std::to_string(l) + " has invalid face");
        if (lp.first >= C) return fail("loop " + std::to_string(l) + " has invalid first coedge");
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
        const Face& fc = m_faces[f];
        if (fc.surface >= m_surfaces.size()) return fail("face " + std::to_string(f) + " has invalid surface");
        if (fc.outerLoop >= L) return fail("face " + std::to_string(f) + " has invalid outer loop");
        if (!m_loops[fc.outerLoop].outer) return fail("face " + std::to_string(f) + " outer loop is not marked outer");
        if (m_loops[fc.outerLoop].face != f) return fail("face " + std::to_string(f) + " outer loop points elsewhere");
        for (uint32_t il : fc.innerLoops) {
            if (il >= L) return fail("face " + std::to_string(f) + " has invalid inner loop");
            if (m_loops[il].outer) return fail("face " + std::to_string(f) + " inner loop marked outer");
            if (m_loops[il].face != f) return fail("face " + std::to_string(f) + " inner loop points elsewhere");
        }
        ++r.faces;
    }

    for (uint32_t v = 0; v < V; ++v) {
        if (m_verts[v].coedge != kInvalid) {
            if (m_verts[v].coedge >= C) return fail("vertex " + std::to_string(v) + " has invalid coedge");
            if (startV(m_verts[v].coedge) != v) return fail("vertex " + std::to_string(v) + " coedge not rooted here");
        }
    }

    for (uint32_t e = 0; e < E; ++e) {
        if (coedgesPerEdge[e] == 0u) return fail("edge " + std::to_string(e) + " is orphaned (no coedge)");
        if (coedgesPerEdge[e] > 2u) return fail("edge " + std::to_string(e) + " is non-manifold (>2 coedges)");
        if (coedgesPerEdge[e] == 1u) ++r.boundaryEdges;
    }

    r.vertices = V;
    r.edges = E;
    r.euler = static_cast<int>(V) - static_cast<int>(E) + static_cast<int>(F);
    r.shells = static_cast<uint32_t>(m_shells.size());
    return r;
}

bool Body::isClosed() const noexcept
{
    for (const Shell& s : m_shells)
        if (!s.closed) return false;
    return !m_shells.empty();
}

// ──────────── Tessellation ───────────────────────────────────────────────────

Mesh Body::toMesh() const
{
    Mesh mesh;
    std::vector<nexus::render::Vec3> pos(m_verts.size());
    for (size_t i = 0; i < m_verts.size(); ++i) pos[i] = m_verts[i].point;
    mesh.attributes().setPositions(pos);

    for (const Face& fc : m_faces) {
        if (fc.outerLoop >= m_loops.size()) continue;
        std::vector<uint32_t> ring;
        const uint32_t first = m_loops[fc.outerLoop].first;
        uint32_t walk = first, steps = 0;
        do {
            const Coedge& ce = m_coedges[walk];
            ring.push_back(ce.reversed ? m_edges[ce.edge].v1 : m_edges[ce.edge].v0);
            walk = ce.next;
            if (++steps > m_coedges.size()) break;
        } while (walk != first);
        for (size_t i = 2; i < ring.size(); ++i) {
            nexus::geometry::Face f;  // the mesh Face, not brep::Face
            f.indices = {ring[0], ring[static_cast<uint32_t>(i) - 1], ring[i]};
            mesh.topology().addFace(std::move(f));
        }
    }
    return mesh;
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
    return b.has_value() ? std::move(*b) : Body{};
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
    return b.has_value() ? std::move(*b) : Body{};
}

}  // namespace nexus::geometry::brep
