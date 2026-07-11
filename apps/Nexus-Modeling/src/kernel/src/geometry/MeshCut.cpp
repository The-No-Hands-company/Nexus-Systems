#include <nexus/geometry/MeshCut.h>

#include <nexus/geometry/TriTriIntersect.h>
#include <nexus/geometry/TriangleRetriangulate.h>

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

namespace nexus::geometry {

namespace {

using V = nexus::render::Vec3;

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
    (void)m.weldCoincidentVertices(1e-5f);
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
    for (size_t i = 0; i < TA.size(); ++i) {
        if (!TA[i].box.valid) continue;
        for (size_t j = 0; j < TB.size(); ++j) {
            if (!TB[j].box.valid || !overlap(TA[i].box, TB[j].box, kTol)) {
                continue;
            }
            const TriTriResult r =
                TriTriIntersect::intersect(TA[i].a, TA[i].b, TA[i].c, TB[j].a, TB[j].b, TB[j].c);
            if (r.kind == TriTriResult::Kind::Segment) {
                aSegs[i].push_back(IntersectionSegment{r.p0, r.p1});
                bSegs[j].push_back(IntersectionSegment{r.p0, r.p1});
            }
        }
    }

    out.a = rebuild(TA, aSegs);
    out.b = rebuild(TB, bSegs);
    return out;
}

}  // namespace nexus::geometry
