#include <nexus/geometry/MeshCut.h>

#include <nexus/geometry/MeshBVH.h>

#include <nexus/geometry/RobustPredicates.h>
#include <nexus/geometry/TriTriIntersect.h>
#include <nexus/geometry/TriangleRetriangulate.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

namespace nexus::geometry {

namespace {

using V = nexus::render::Vec3;

// ── Coplanar seam imprint ─────────────────────────────────────────────────────
// When two faces are COPLANAR-overlapping, TriTriIntersect reports Kind::Coplanar
// with no segment (the shared "curve" is a 2D area, not a line). Left unhandled,
// the two surfaces never share seam vertices along the overlap boundary and the
// boolean's stitch leaves boundary loops (the dominant mesh-boolean leak). We fix
// it by IMPRINTING each triangle's edges onto the other: every edge of B, clipped
// to A's triangle, becomes a constrained seam segment on A (and vice versa), so
// after retriangulation both surfaces carry matching edges along the overlap
// boundary. The in/out CLASSIFICATION at each clip is EXACT (Shewchuk orient2D);
// only the crossing coordinate is metric (a point on the plane, tolerance-managed).

// Drop the dominant axis of the (coplanar) normal so the projection is non-degenerate.
int dropAxis(const V& a, const V& b, const V& c) noexcept
{
    const V e1{b.x - a.x, b.y - a.y, b.z - a.z};
    const V e2{c.x - a.x, c.y - a.y, c.z - a.z};
    const V n{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
    const float ax = std::abs(n.x), ay = std::abs(n.y), az = std::abs(n.z);
    if (ax >= ay && ax >= az) return 0;
    if (ay >= az) return 1;
    return 2;
}

Vec2 project(const V& p, int drop) noexcept
{
    switch (drop) {
        case 0:  return {p.y, p.z};
        case 1:  return {p.z, p.x};
        default: return {p.x, p.y};
    }
}

V lerp3(const V& p, const V& q, double t) noexcept
{
    const float tf = static_cast<float>(t);
    return {p.x + (q.x - p.x) * tf, p.y + (q.y - p.y) * tf, p.z + (q.z - p.z) * tf};
}

// Clip the 3D segment P→Q to triangle (A0,A1,A2) in the shared plane (projected by
// `drop`). Sidedness is exact orient2D; returns the surviving sub-segment (if any,
// non-degenerate) in out0/out1. The triangle is treated as a convex clip region.
bool clipSegmentToTri(const V& P, const V& Q, const V& A0, const V& A1, const V& A2,
                      int drop, V& out0, V& out1) noexcept
{
    const Vec2 pA0 = project(A0, drop), pA1 = project(A1, drop), pA2 = project(A2, drop);
    const Vec2 pP = project(P, drop), pQ = project(Q, drop);
    const double o = RobustPredicates::orient2D(pA0, pA1, pA2);
    if (o == 0.0) return false;  // degenerate triangle
    const double s = (o > 0.0) ? 1.0 : -1.0;  // normalize to CCW-inside convention

    const Vec2 e0[3] = {pA0, pA1, pA2};
    const Vec2 e1[3] = {pA1, pA2, pA0};
    double t0 = 0.0, t1 = 1.0;
    for (int k = 0; k < 3; ++k) {
        const double dP = RobustPredicates::orient2D(e0[k], e1[k], pP) * s;  // ≥0 inside
        const double dQ = RobustPredicates::orient2D(e0[k], e1[k], pQ) * s;
        if (dP < 0.0 && dQ < 0.0) return false;   // wholly outside this half-plane
        if (dP >= 0.0 && dQ >= 0.0) continue;     // wholly inside → no clip
        const double t = dP / (dP - dQ);          // crossing parameter (metric)
        if (dP < 0.0) t0 = std::max(t0, t);       // entering
        else          t1 = std::min(t1, t);       // leaving
    }
    if (t1 - t0 <= 1e-7) return false;            // empty / point-like overlap
    out0 = lerp3(P, Q, t0);
    out1 = lerp3(P, Q, t1);
    const V d{out1.x - out0.x, out1.y - out0.y, out1.z - out0.z};
    return (d.x * d.x + d.y * d.y + d.z * d.z) > 1e-20;  // non-degenerate length
}

// Imprint tool triangle (t0,t1,t2)'s edges onto target triangle (g0,g1,g2),
// appending the surviving in-plane sub-segments to `dst`.
void imprintCoplanarEdges(const V& t0, const V& t1, const V& t2,
                          const V& g0, const V& g1, const V& g2, int drop,
                          std::vector<IntersectionSegment>& dst)
{
    const V te0[3] = {t0, t1, t2};
    const V te1[3] = {t1, t2, t0};
    for (int k = 0; k < 3; ++k) {
        V c0{}, c1{};
        if (clipSegmentToTri(te0[k], te1[k], g0, g1, g2, drop, c0, c1)) {
            dst.push_back(IntersectionSegment{c0, c1});
        }
    }
}

struct Aabb {
    V lo{}, hi{};
    bool valid = false;
};

Aabb triAabb(const V& a, const V& b, const V& c) noexcept
{
    Aabb box;
    box.lo = {std::min({a.x, b.x, c.x}), std::min({a.y, b.y, c.y}), std::min({a.z, b.z, c.z})};
    box.hi = {std::max({a.x, b.x, c.x}), std::max({a.y, b.y, c.y}), std::max({a.z, b.z, c.z})};
    box.valid = true;
    return box;
}

bool overlap(const Aabb& x, const Aabb& y, float tol) noexcept
{
    if (!x.valid || !y.valid) return false;
    return x.lo.x <= y.hi.x + tol && x.hi.x >= y.lo.x - tol && x.lo.y <= y.hi.y + tol
        && x.hi.y >= y.lo.y - tol && x.lo.z <= y.hi.z + tol && x.hi.z >= y.lo.z - tol;
}

struct Tri {
    V    a{}, b{}, c{};
    Aabb box{};
};

std::vector<Tri> gatherTris(const Mesh& m)
{
    const auto&       pos  = m.attributes().positions();
    const MeshTopology& topo = m.topology();
    std::vector<Tri>  tris;
    tris.reserve(topo.faceCount());
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const Face& fc = topo.face(f);
        if (fc.indices.size() != 3 || !fc.indicesInBounds(pos.size())) {
            tris.push_back({});  // non-triangle placeholder (kept for index parity)
            continue;
        }
        const V a = pos[fc.indices[0]];
        const V b = pos[fc.indices[1]];
        const V c = pos[fc.indices[2]];
        tris.push_back({a, b, c, triAabb(a, b, c)});
    }
    return tris;
}

// Rebuild a mesh: pass through triangles with no segments, retriangulate the
// rest, then weld the shared vertices into a coherent mesh.
Mesh rebuild(const std::vector<Tri>& tris, const std::vector<std::vector<IntersectionSegment>>& segs)
{
    std::vector<V>    outPos;
    std::vector<Face> outFaces;
    auto addTri = [&](const V& a, const V& b, const V& c) {
        const uint32_t base = static_cast<uint32_t>(outPos.size());
        outPos.push_back(a);
        outPos.push_back(b);
        outPos.push_back(c);
        Face f;
        f.indices = {base, base + 1, base + 2};
        outFaces.push_back(std::move(f));
    };

    for (size_t i = 0; i < tris.size(); ++i) {
        const Tri& t = tris[i];
        if (!t.box.valid) {
            continue;  // skip non-triangle placeholders
        }
        if (segs[i].empty()) {
            addTri(t.a, t.b, t.c);
        } else {
            const RetriangulationResult rt = TriangleRetriangulate::apply(t.a, t.b, t.c, segs[i]);
            for (const auto& tri : rt.triangles) {
                addTri(rt.points[tri[0]], rt.points[tri[1]], rt.points[tri[2]]);
            }
        }
    }

    Mesh m;
    m.attributes().setPositions(std::move(outPos));
    for (Face& f : outFaces) {
        m.topology().addFace(std::move(f));
    }
    (void)m.weldCoincidentVertices(1e-5f, Mesh::WeldCollapsePolicy::DropCollapsedFace);
    (void)m.computeVertexNormals();
    return m;
}

}  // namespace

MeshCutResult MeshCut::cut(const Mesh& a, const Mesh& b) noexcept
{
    MeshCutResult out;

    Mesh ta = a;
    Mesh tb = b;
    (void)ta.topology().triangulate();
    (void)tb.topology().triangulate();

    const std::vector<Tri> TA = gatherTris(ta);
    const std::vector<Tri> TB = gatherTris(tb);

    std::vector<std::vector<IntersectionSegment>> aSegs(TA.size());
    std::vector<std::vector<IntersectionSegment>> bSegs(TB.size());

    constexpr float kTol = 1e-6f;

    // Broad phase. Pairing every triangle of A against every triangle of B is quadratic
    // and was the dominant cost of a boolean once classification had been accelerated
    // (235 ms of 312 on a 12-vs-8,568-triangle case). B's hierarchy answers "which
    // triangles can possibly meet this box" in log time instead.
    //
    // The candidate set is then filtered by the SAME exact box test as before and visited
    // in ASCENDING triangle order, so the pairs considered — and the order they are
    // considered in — are identical to the brute-force loop. That matters beyond tidiness:
    // the order segments are appended in feeds the per-triangle retriangulation, so a
    // different order could produce a different (still valid) tessellation. Sorting keeps
    // this a pure acceleration, and the output byte-for-byte unchanged.
    MeshBVH bvhB;
    bvhB.build(tb);
    const bool useBroadPhase = bvhB.isValid();
    std::vector<uint32_t> candidates;
    std::vector<size_t>   pairs;

    for (size_t i = 0; i < TA.size(); ++i) {
        if (!TA[i].box.valid) continue;

        pairs.clear();
        if (useBroadPhase) {
            const V qlo{TA[i].box.lo.x - kTol, TA[i].box.lo.y - kTol, TA[i].box.lo.z - kTol};
            const V qhi{TA[i].box.hi.x + kTol, TA[i].box.hi.y + kTol, TA[i].box.hi.z + kTol};
            bvhB.collectBoxCandidates(qlo, qhi, candidates);
            const auto& bt = bvhB.tris();
            pairs.reserve(candidates.size());
            for (const uint32_t ci : candidates) {
                if (ci < bt.size()) pairs.push_back(static_cast<size_t>(bt[ci].srcFace));
            }
            std::sort(pairs.begin(), pairs.end());
            pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
        } else {
            pairs.reserve(TB.size());
            for (size_t j = 0; j < TB.size(); ++j) pairs.push_back(j);
        }

        for (const size_t j : pairs) {
            if (j >= TB.size()) continue;
            if (!TB[j].box.valid || !overlap(TA[i].box, TB[j].box, kTol)) {
                continue;
            }
            const TriTriResult r =
                TriTriIntersect::intersect(TA[i].a, TA[i].b, TA[i].c, TB[j].a, TB[j].b, TB[j].c);
            if (r.kind == TriTriResult::Kind::Segment) {
                aSegs[i].push_back(IntersectionSegment{r.p0, r.p1});
                bSegs[j].push_back(IntersectionSegment{r.p0, r.p1});
            } else if (r.kind == TriTriResult::Kind::Coplanar) {
                // Coplanar-overlap: imprint each triangle's boundary onto the other
                // so both surfaces share seam vertices along the overlap boundary.
                const int drop = dropAxis(TA[i].a, TA[i].b, TA[i].c);
                imprintCoplanarEdges(TB[j].a, TB[j].b, TB[j].c, TA[i].a, TA[i].b, TA[i].c, drop, aSegs[i]);
                imprintCoplanarEdges(TA[i].a, TA[i].b, TA[i].c, TB[j].a, TB[j].b, TB[j].c, drop, bSegs[j]);
            }
        }
    }

    out.a = rebuild(TA, aSegs);
    out.b = rebuild(TB, bSegs);
    return out;
}

}  // namespace nexus::geometry
