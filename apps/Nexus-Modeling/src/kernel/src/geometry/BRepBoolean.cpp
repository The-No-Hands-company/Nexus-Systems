#include <nexus/geometry/BRepBoolean.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace nexus::geometry::brep {

using nexus::render::Vec3;
using PC = Body::PointContainment;

namespace {
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
}  // namespace

Mesh booleanToMesh(const Body& a, const Body& b, BooleanOp op, Tolerance tol)
{
    // Copy + segment so no face straddles the other's boundary.
    Body A = a;
    Body B = b;
    imprintMutually(A, B, tol);

    std::vector<Vec3> positions;
    Mesh mesh;
    MeshTopology& topo = mesh.topology();

    // Select + emit A's kept faces (classified against B).
    for (uint32_t f = 0; f < A.faceCount(); ++f) {
        if (!A.face(f).alive) continue;
        bool reverse = false;
        if (keepFace(A.classifyFace(f, B, tol), op, /*isA=*/true, reverse))
            emitFace(A, f, reverse, positions, topo);
    }
    // Select + emit B's kept faces (classified against A).
    for (uint32_t f = 0; f < B.faceCount(); ++f) {
        if (!B.face(f).alive) continue;
        bool reverse = false;
        if (keepFace(B.classifyFace(f, A, tol), op, /*isA=*/false, reverse))
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
    imprintMutually(A, B, tol);

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

    auto addKept = [&](const Body& body, const Body& other, bool isA) {
        for (uint32_t f = 0; f < body.faceCount(); ++f) {
            if (!body.face(f).alive) continue;
            bool reverse = false;
            if (!keepFace(body.classifyFace(f, other, tol), op, isA, reverse)) continue;
            std::vector<uint32_t> vs = body.faceVertices(f);
            if (vs.size() < 3) continue;
            if (reverse) std::reverse(vs.begin(), vs.end());  // outward for the cavity
            Body::FaceDef fd;
            fd.loop.reserve(vs.size());
            for (uint32_t v : vs) fd.loop.push_back(weld(body.vertex(v).point));
            const Face& face = body.face(f);
            fd.surface = (face.surface < body.surfaceCount()) ? body.surface(face.surface)
                                                              : Surface{};
            if (reverse) fd.surface.normal = {-fd.surface.normal.x, -fd.surface.normal.y,
                                              -fd.surface.normal.z};
            defs.push_back(std::move(fd));
        }
    };
    addKept(A, B, /*isA=*/true);
    addKept(B, A, /*isA=*/false);

    auto sewn = Body::fromFaces(points, defs);
    return sewn.has_value() ? std::move(*sewn) : Body{};
}

}  // namespace nexus::geometry::brep
