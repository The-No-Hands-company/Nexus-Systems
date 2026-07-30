#include <nexus/geometry/BRepBoolean.h>

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nexus::geometry::brep {

using nexus::render::Vec3;
using PC = Body::PointContainment;

namespace {
// Undirected key over a pair of welded point indices (the vertex ids fromFaces
// will assign, since it numbers m_verts 1:1 with the welded point list).
uint64_t weldedPairKey(uint32_t a, uint32_t b)
{
    const uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
    return (static_cast<uint64_t>(lo) << 32) | static_cast<uint64_t>(hi);
}

// Outward normal of a (planar) face: the surface normal, flipped when the face
// reverses it.
Vec3 faceOutwardNormal(const Body& body, uint32_t f)
{
    const Face& face = body.face(f);
    Vec3 n{0.f, 0.f, 1.f};
    if (face.surface < body.surfaceCount()) n = body.surface(face.surface).normal;
    return face.reversed ? Vec3{-n.x, -n.y, -n.z} : n;
}

// Fan-triangulate one face into `positions`/`topo`, orienting each triangle to
// the face's outward normal (then globally reversed when `reverse`).
void emitFace(const Body& body, uint32_t f, bool reverse, std::vector<Vec3>& positions,
              MeshTopology& topo)
{
    const std::vector<uint32_t> vs = body.faceVertices(f);
    if (vs.size() < 3) return;
    Vec3 outward = faceOutwardNormal(body, f);
    if (reverse) outward = {-outward.x, -outward.y, -outward.z};

    const uint32_t base = static_cast<uint32_t>(positions.size());
    for (uint32_t v : vs) positions.push_back(body.vertex(v).point);

    for (size_t i = 1; i + 1 < vs.size(); ++i) {
        uint32_t a = base;
        uint32_t b = base + static_cast<uint32_t>(i);
        uint32_t c = base + static_cast<uint32_t>(i) + 1u;
        const Vec3 g = (positions[b] - positions[a]).cross(positions[c] - positions[a]);
        if (g.dot(outward) < 0.f) std::swap(b, c);  // keep winding outward
        nexus::geometry::Face tri;  // mesh face (not the brep::Face topology entity)
        tri.indices = {a, b, c};
        topo.addFace(std::move(tri));
    }
}

// Whether a face of `body` (classified against `other`) is kept, and whether it
// must be reversed, for the given op.
bool keepFace(PC cls, BooleanOp op, bool isA, bool& reverse)
{
    reverse = false;
    switch (op) {
        case BooleanOp::Union:
            return cls == PC::Outside;
        case BooleanOp::Intersection:
            return cls == PC::Inside;
        case BooleanOp::Difference:
            if (isA) return cls == PC::Outside;      // A's outer skin
            if (cls == PC::Inside) { reverse = true; return true; }  // B's cavity wall
            return false;
    }
    return false;
}

// Select a face for the result, handling the COINCIDENT (coplanar-overlap) case
// that keepFace can't: when a face classifies OnBoundary against the other solid
// it lies on a shared coplanar boundary (a coincident face pair). Only the
// A-side of such a pair can survive; the B-side is always dropped so the pair
// contributes at most one face. Whether A's copy survives depends on the
// relative outward orientation, found by probing just inside A's face: if the
// other solid's interior is on the SAME side (probe point is inside it) the two
// interiors overlap here and the face bounds Union/Intersection; if OPPOSITE the
// solids merely touch and the face is interior to Union/Intersection but stays
// on A's outer skin for Difference.
//
//   op \ normals     SAME (interiors same side)   OPPOSITE (touch)
//   Union            keep A                        drop
//   Intersection     keep A                        drop
//   Difference A-B   drop                          keep A
bool selectFace(const Body& body, uint32_t f, const Body& other, BooleanOp op, bool isA,
                Tolerance tol, bool& reverse)
{
    reverse = false;
    const PC cls = body.classifyFace(f, other, tol);
    if (cls != PC::OnBoundary) return keepFace(cls, op, isA, reverse);

    if (!isA) return false;  // coincident pair is represented by the A-side face
    const Vec3 c = body.faceSamplePoint(f);  // on the material, not inside a hole
    const Vec3 nA = faceOutwardNormal(body, f);
    const float eps = std::max(tol.absolute * 100.f, 1e-3f);
    const Vec3 pIn{c.x - eps * nA.x, c.y - eps * nA.y, c.z - eps * nA.z};  // just inside A
    const bool same = (other.classifyPoint(pIn, tol) == PC::Inside);
    switch (op) {
        case BooleanOp::Union:
        case BooleanOp::Intersection:
            return same;
        case BooleanOp::Difference:
            return !same;
    }
    return false;
}
}  // namespace

Mesh booleanToMesh(const Body& a, const Body& b, BooleanOp op, Tolerance tol)
{
    // Copy + segment so no face straddles the other's boundary.
    Body A = a;
    Body B = b;
    // A degenerate/near-tangent config can blow the imprint's face budget (a faceted
    // sphere grazing a box face imprints an O(n²) line arrangement). Bail to an empty
    // result rather than grinding through the exploded body — the honest watertight-
    // or-empty contract, and it keeps the Boolean bounded instead of effectively hung.
    if (!imprintMutually(A, B, tol)) return Mesh{};

    std::vector<Vec3> positions;
    Mesh mesh;
    MeshTopology& topo = mesh.topology();

    // Select + emit A's kept faces (classified against B).
    for (uint32_t f = 0; f < A.faceCount(); ++f) {
        if (!A.face(f).alive) continue;
        bool reverse = false;
        if (selectFace(A, f, B, op, /*isA=*/true, tol, reverse))
            emitFace(A, f, reverse, positions, topo);
    }
    // Select + emit B's kept faces (classified against A).
    for (uint32_t f = 0; f < B.faceCount(); ++f) {
        if (!B.face(f).alive) continue;
        bool reverse = false;
        if (selectFace(B, f, A, op, /*isA=*/false, tol, reverse))
            emitFace(B, f, reverse, positions, topo);
    }

    mesh.attributes().setPositions(std::move(positions));
    // Merge coincident seam vertices so A's and B's kept patches join watertight.
    (void)mesh.weldCoincidentVertices(tol.absolute > 0.f ? tol.absolute * 10.f : 1e-4f);
    return mesh;
}

Body booleanToBody(const Body& a, const Body& b, BooleanOp op, Tolerance tol)
{
    Body A = a;
    Body B = b;
    // Degenerate/near-tangent config → the imprint budget is blown → return a clean
    // empty Body (watertight-or-empty; never a hang on a pathological tangency).
    if (!imprintMutually(A, B, tol)) return Body{};

    const float weldEps = tol.absolute > 0.f ? tol.absolute * 10.f : 1e-4f;
    std::vector<Vec3> points;
    std::vector<Body::FaceDef> defs;

    // Weld a point into the shared list (dedup within weldEps → seam vertices of
    // A and B collapse to one, so fromFaces can partner across the seam).
    auto weld = [&](const Vec3& p) -> uint32_t {
        for (uint32_t i = 0; i < points.size(); ++i) {
            const Vec3 d = points[i] - p;
            if (d.dot(d) <= weldEps * weldEps) return i;
        }
        points.push_back(p);
        return static_cast<uint32_t>(points.size() - 1);
    };

    // Analytic curve geometry of the kept edges, keyed by their WELDED endpoint
    // pair. fromFaces builds every edge as a straight chord between its endpoints
    // (it is handed vertex rings and nothing else), so a curved operand's arc
    // edges — a cylinder's rims, every edge of a sphere — would silently degrade
    // to Lines through any boolean: measured on cyl(12 seg) ∪ box, where the box
    // lies strictly inside the cylinder so the union IS the cylinder, the result
    // came back with all 24 rim arcs as chords. That loses the analytic geometry
    // toMesh(subdivisions) needs to retessellate smoothly, and it is wrong even
    // when the topology is right. Harvest the arcs here and re-apply them after
    // the sew, keyed undirectedly so the key is independent of loop winding and
    // of the `reverse` flip below.
    std::unordered_map<uint64_t, Curve> arcs;

    auto addKept = [&](const Body& body, const Body& other, bool isA) {
        for (uint32_t f = 0; f < body.faceCount(); ++f) {
            if (!body.face(f).alive) continue;
            bool reverse = false;
            if (!selectFace(body, f, other, op, isA, tol, reverse)) continue;
            std::vector<uint32_t> vs = body.faceVertices(f);
            if (vs.size() < 3) continue;
            if (reverse) std::reverse(vs.begin(), vs.end());  // outward for the cavity
            Body::FaceDef fd;
            fd.loop.reserve(vs.size());
            for (uint32_t v : vs) fd.loop.push_back(weld(body.vertex(v).point));

            // Carry the face's HOLES. A seam that pierces the middle of a face — a
            // cylinder through a box's face — leaves it bounded outside and holed
            // inside; faceVertices reports only the outer boundary, so the hole used to
            // be dropped here and the pierced face was reassembled solid, which both
            // loses the opening and leaves the other operand's ring edges with nothing
            // to partner. The same outward flip that reverses the outer loop has to
            // reverse each hole too, or the hole would wind with the boundary instead of
            // against it and bound a second outer region rather than an opening.
            std::vector<std::vector<uint32_t>> holes = body.faceInnerLoopVertices(f);
            for (std::vector<uint32_t>& hole : holes) {
                if (hole.size() < 3) continue;
                if (reverse) std::reverse(hole.begin(), hole.end());
                std::vector<uint32_t> welded;
                welded.reserve(hole.size());
                for (uint32_t v : hole) welded.push_back(weld(body.vertex(v).point));
                fd.innerLoops.push_back(std::move(welded));
            }

            // Record this face's arc edges. Walking the outer loop's coedges gives
            // each edge with its OWN stored endpoints, so no correspondence with
            // the (possibly reversed) `vs` order is needed. weld() only looks up
            // here — every loop vertex was just inserted above.
            const uint32_t loopId = body.face(f).outerLoop;
            if (loopId < body.loopCount() && body.loop(loopId).first < body.coedgeCount()) {
                const uint32_t first = body.loop(loopId).first;
                uint32_t w = first, guard = 0;
                do {
                    const uint32_t e = body.coedge(w).edge;
                    if (e < body.edgeCount()) {
                        const Edge& ed = body.edge(e);
                        if (ed.curve != kInvalid && body.curve(ed.curve).kind == CurveKind::Circle &&
                            ed.v0 < body.vertexCount() && ed.v1 < body.vertexCount()) {
                            const uint32_t wa = weld(body.vertex(ed.v0).point);
                            const uint32_t wb = weld(body.vertex(ed.v1).point);
                            if (wa != wb) arcs.emplace(weldedPairKey(wa, wb), body.curve(ed.curve));
                        }
                    }
                    w = body.coedge(w).next;
                    if (++guard > body.coedgeCount() + 1u) break;  // corrupt loop guard
                } while (w != first);
            }

            const Face& face = body.face(f);
            fd.surface = (face.surface < body.surfaceCount()) ? body.surface(face.surface)
                                                              : Surface{};
            // Reversing a kept face — the cavity wall of a Difference, whose material is on
            // the far side of it — is expressed differently for the two kinds of surface,
            // because `Surface::normal` means two different things.
            //
            // For a PLANE it is the outward normal, so negating it says exactly what is
            // meant, and that is left untouched: it is what every planar boolean in this
            // kernel has been built on.
            //
            // For a CYLINDER, SPHERE or CONE it is the AXIS. Negating it does not reverse
            // the face at all — it re-parameterizes the surface, leaving a cylindrical bore
            // whose stored axis points the wrong way and whose orientation is still
            // unreversed. Nothing downstream noticed, because the tessellator derives its
            // normal as (axis, flipped if Face::reversed) and those two errors cancel there.
            // The analytic mass-properties integrator does not: it needs the axis for the
            // parameterization AND the flag for the orientation, so a difference through a
            // cylinder reported the wrong volume (6.147 against a true 6.653). Say it with
            // the flag instead, and leave the axis alone.
            if (reverse) {
                if (fd.surface.kind == SurfaceKind::Plane)
                    fd.surface.normal = {-fd.surface.normal.x, -fd.surface.normal.y,
                                         -fd.surface.normal.z};
                else
                    fd.reversed = true;
            }
            defs.push_back(std::move(fd));
        }
    };
    addKept(A, B, /*isA=*/true);
    addKept(B, A, /*isA=*/false);

    // Drop duplicate coplanar faces before sewing. When A and B share a coplanar region
    // (e.g. two boxes with matching cross-sections, overlapping in a thin slab), the imprint
    // splits both operands' faces there into coincident sub-faces welded onto the SAME
    // vertices. selectFace resolves the coincident pair by keeping the A-side copy and
    // dropping the B-side one — but that resolution rests on classifyFace's OnBoundary test,
    // which is a metric decision and flips under float noise for thin slivers, occasionally
    // keeping BOTH copies. Two faces on the same welded vertex loop are byte-identical and
    // wind the same way, so every directed edge is traversed twice and fromFaces correctly
    // rejects the non-manifold input — the whole Boolean then bailed to empty on legitimate
    // overlaps (measured: box/box dx≈1.8/1.9, cyl/cyl dx≈1.0). A duplicate coplanar face is
    // never valid in a 2-manifold solid, so removing the redundant copy is always safe and
    // resolves the coincident pair the classifier failed to. Keyed by the sorted vertex set,
    // which is rotation- and reflection-invariant, so it catches the duplicate regardless of
    // where each loop starts or which way it is wound.
    {
        std::vector<Body::FaceDef> unique;
        unique.reserve(defs.size());
        std::vector<std::vector<uint32_t>> seenKeys;
        seenKeys.reserve(defs.size());
        for (auto& fd : defs) {
            std::vector<uint32_t> key = fd.loop;
            std::sort(key.begin(), key.end());
            bool dup = false;
            for (const auto& k : seenKeys) {
                if (k == key) { dup = true; break; }
            }
            if (dup) continue;
            seenKeys.push_back(std::move(key));
            unique.push_back(std::move(fd));
        }
        defs = std::move(unique);
    }

    auto sewn = Body::fromFaces(points, defs);
    // Invariant: booleanToBody returns a WATERTIGHT solid or a clean empty Body —
    // never a corrupt or leaky one. fromFaces already rejects non-manifold sews
    // (returns nullopt); this also drops any result that fails the integrity
    // validator OR is not closed (a near-degenerate sew that dropped/duplicated a
    // face leaves an OPEN shell — checkIntegrity permits boundary edges, so the
    // watertightness must be checked separately) in favour of a clean empty Body.
    if (!sewn.has_value() || !sewn->checkIntegrity().ok || !sewn->isClosed()) return Body{};

    // Restore the analytic arcs the chord-only sew flattened. setEdgeArc re-derives
    // the param range from the edge's own endpoints and REFUSES an edge whose
    // endpoints do not lie on the circle within tol, so a seam vertex that the weld
    // pulled off the arc (or an edge that a later split left spanning a different
    // curve) simply keeps its chord — degrading to exactly the previous behaviour
    // rather than attaching geometry that contradicts the topology. That is why
    // this cannot break checkGeometry, and why the return value is discarded.
    for (uint32_t e = 0; e < static_cast<uint32_t>(sewn->edgeCount()); ++e) {
        const Edge& ed = sewn->edge(e);
        if (!ed.alive) continue;
        const auto it = arcs.find(weldedPairKey(ed.v0, ed.v1));
        if (it == arcs.end()) continue;
        (void)sewn->setEdgeArc(e, it->second.origin, it->second.dir, it->second.radius, tol);
    }
    return std::move(*sewn);
}

namespace {
float getc(const Vec3& v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
void setc(Vec3& v, int i, float x)
{
    if (i == 0) v.x = x;
    else if (i == 1) v.y = x;
    else v.z = x;
}
}  // namespace

Body chamferBoxEdge(const Body& box, int axis, int s1, int s2, float setback)
{
    if (axis < 0 || axis > 2 || setback <= 0.f || !isFinite(setback) || box.vertexCount() == 0)
        return box;

    // Axis-aligned bounds of the box.
    Vec3 lo = box.vertex(0).point, hi = lo;
    for (uint32_t v = 1; v < box.vertexCount(); ++v) {
        const Vec3 p = box.vertex(v).point;
        for (int i = 0; i < 3; ++i) {
            setc(lo, i, std::min(getc(lo, i), getc(p, i)));
            setc(hi, i, std::max(getc(hi, i), getc(p, i)));
        }
    }

    const int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
    const float size1 = getc(hi, a1) - getc(lo, a1);
    const float size2 = getc(hi, a2) - getc(lo, a2);
    // Clamp so the wedge stays within this edge's quadrant (never past the box
    // centre onto the opposite edges), keeping the result a clean single chamfer.
    const float sb = std::min(setback, std::min(size1, size2) * 0.49f);
    const float d1 = (s1 >= 0) ? 1.f : -1.f;
    const float d2 = (s2 >= 0) ? 1.f : -1.f;
    const float c1 = (s1 >= 0) ? getc(hi, a1) : getc(lo, a1);  // edge coord on the two other axes
    const float c2 = (s2 >= 0) ? getc(hi, a2) : getc(lo, a2);
    const float in1 = c1 - d1 * sb;  // setback point into the box
    const float in2 = c2 - d2 * sb;

    // Cutting prism: the right-triangle wedge at the edge, extruded along the edge
    // and past both box ends (so its caps don't coincide with the box's).
    const float axLo = getc(lo, axis), axHi = getc(hi, axis);
    const float margin = (axHi - axLo) * 0.5f + 1.f;
    auto mk = [&](float v1, float v2) {
        Vec3 p{0, 0, 0};
        setc(p, axis, axLo - margin);
        setc(p, a1, v1);
        setc(p, a2, v2);
        return p;
    };
    std::vector<Vec3> tri = {mk(c1, c2), mk(in1, c2), mk(c1, in2)};
    Vec3 dir{0, 0, 0};
    setc(dir, axis, (axHi - axLo) + 2.f * margin);
    const Body tool = extrudeProfile(tri, dir);
    return booleanToBody(box, tool, BooleanOp::Difference);
}

Body filletBoxEdge(const Body& box, int axis, int s1, int s2, float radius, uint32_t segments)
{
    if (axis < 0 || axis > 2 || radius <= 0.f || !isFinite(radius) || box.vertexCount() == 0 ||
        segments < 1)
        return box;

    Vec3 lo = box.vertex(0).point, hi = lo;
    for (uint32_t v = 1; v < box.vertexCount(); ++v) {
        const Vec3 p = box.vertex(v).point;
        for (int i = 0; i < 3; ++i) {
            setc(lo, i, std::min(getc(lo, i), getc(p, i)));
            setc(hi, i, std::max(getc(hi, i), getc(p, i)));
        }
    }

    const int a1 = (axis + 1) % 3, a2 = (axis + 2) % 3;
    const float size1 = getc(hi, a1) - getc(lo, a1);
    const float size2 = getc(hi, a2) - getc(lo, a2);
    const float r = std::min(radius, std::min(size1, size2) * 0.49f);
    const float d1 = (s1 >= 0) ? 1.f : -1.f;
    const float d2 = (s2 >= 0) ? 1.f : -1.f;
    const float c1 = (s1 >= 0) ? getc(hi, a1) : getc(lo, a1);
    const float c2 = (s2 >= 0) ? getc(hi, a2) : getc(lo, a2);
    const float in1 = c1 - d1 * r, in2 = c2 - d2 * r;  // quarter-circle centre

    const float axLo = getc(lo, axis), axHi = getc(hi, axis);
    const float margin = (axHi - axLo) * 0.5f + 1.f;
    auto mk = [&](float v1, float v2) {
        Vec3 p{0, 0, 0};
        setc(p, axis, axLo - margin);
        setc(p, a1, v1);
        setc(p, a2, v2);
        return p;
    };

    // Cutting prism cross-section = the corner beyond the quarter-cylinder: the
    // corner vertex (pushed slightly OUTSIDE the box so the tool's straight legs
    // don't coincide with the box faces — that tangent coincidence otherwise makes
    // the boolean degenerate at certain segment counts) plus the faceted arc from
    // one tangent point to the other. box ∩ tool is unchanged by the outward push.
    const float out = r * 0.5f;
    std::vector<Vec3> poly;
    poly.reserve(segments + 2);
    poly.push_back(mk(c1 + d1 * out, c2 + d2 * out));  // corner, outside the box
    constexpr float kHalfPi = 1.57079632679489662f;
    for (uint32_t k = 0; k <= segments; ++k) {
        const float th = kHalfPi * static_cast<float>(k) / static_cast<float>(segments);
        poly.push_back(mk(in1 + d1 * r * std::cos(th), in2 + d2 * r * std::sin(th)));
    }
    Vec3 dir{0, 0, 0};
    setc(dir, axis, (axHi - axLo) + 2.f * margin);
    const Body tool = extrudeProfile(poly, dir);
    return booleanToBody(box, tool, BooleanOp::Difference);
}

Body hollowBox(float width, float height, float depth, float thickness)
{
    // A degenerate outer box is empty; a non-positive thickness (or one too large
    // to leave a cavity) yields the plain solid box.
    if (!isFinite(width) || !isFinite(height) || !isFinite(depth) || width <= 0.f ||
        height <= 0.f || depth <= 0.f)
        return Body{};
    const Body outer = makeBox(width, height, depth);
    if (outer.faceCount() == 0) return Body{};
    if (!isFinite(thickness) || thickness <= 0.f) return outer;
    const float m = std::min(width, std::min(height, depth));
    if (2.f * thickness >= m) return outer;  // no room for a cavity → stays solid

    // Concentric inner box, each dimension inset by 2·thickness → a sealed shell.
    const Body inner = makeBox(width - 2.f * thickness, height - 2.f * thickness,
                               depth - 2.f * thickness);
    Body shell = booleanToBody(outer, inner, BooleanOp::Difference);
    // Never-corrupt: if the difference failed to sew, fall back to the solid box.
    return shell.checkIntegrity().ok ? std::move(shell) : outer;
}

}  // namespace nexus::geometry::brep
