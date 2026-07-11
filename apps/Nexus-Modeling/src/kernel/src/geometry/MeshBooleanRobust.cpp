#include <nexus/geometry/MeshBooleanRobust.h>

#include <nexus/geometry/MeshCut.h>

#include <array>
#include <bit>
#include <cstdint>
#include <utility>
#include <vector>

namespace nexus::geometry {

namespace {

using V = nexus::render::Vec3;

bool isFiniteF(float x) noexcept
{
    constexpr uint32_t kExpMask = 0x7F800000u;
    return (std::bit_cast<uint32_t>(x) & kExpMask) != kExpMask;
}

V    sub(const V& a, const V& b) noexcept { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V    cross(const V& a, const V& b) noexcept
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
float dot(const V& a, const V& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

struct Soup {
    std::vector<V>                       pos;
    std::vector<std::array<uint32_t, 3>> tris;
};

Soup gather(const Mesh& src)
{
    Mesh m = src;
    (void)m.topology().triangulate();
    Soup s;
    s.pos = m.attributes().positions();
    for (size_t f = 0; f < m.topology().faceCount(); ++f) {
        const Face& fc = m.topology().face(f);
        if (fc.indices.size() == 3 && fc.indicesInBounds(s.pos.size())) {
            s.tris.push_back({fc.indices[0], fc.indices[1], fc.indices[2]});
        }
    }
    return s;
}

// Ray-cast point-in-mesh: shoot a slightly off-axis ray and count triangle
// crossings (Möller-Trumbore). Odd count → inside.
bool pointInside(const V& p, const Soup& s) noexcept
{
    const V   dir{1.f, 0.0013f, 0.0007f};  // off-axis avoids edge/vertex degeneracies
    int       crossings = 0;
    constexpr float kEps = 1e-7f;
    for (const auto& t : s.tris) {
        const V& v0 = s.pos[t[0]];
        const V& v1 = s.pos[t[1]];
        const V& v2 = s.pos[t[2]];
        const V  e1 = sub(v1, v0);
        const V  e2 = sub(v2, v0);
        const V  h  = cross(dir, e2);
        const float a = dot(e1, h);
        if (a > -kEps && a < kEps) continue;  // ray parallel to triangle
        const float f = 1.f / a;
        const V     sv = sub(p, v0);
        const float u = f * dot(sv, h);
        if (u < 0.f || u > 1.f) continue;
        const V     q = cross(sv, e1);
        const float v = f * dot(dir, q);
        if (v < 0.f || u + v > 1.f) continue;
        const float tt = f * dot(e2, q);
        if (tt > kEps) ++crossings;
    }
    return (crossings & 1) != 0;
}

}  // namespace

Mesh robustMeshBoolean(const Mesh& a, const Mesh& b, BooleanOperationType op) noexcept
{
    // Cut both meshes along the intersection curve so faces never straddle.
    const MeshCutResult cutR = MeshCut::cut(a, b);
    // Solid classification uses the original surfaces.
    const Soup soupA = gather(a);
    const Soup soupB = gather(b);

    std::vector<V>    outPos;
    std::vector<Face> outFaces;
    auto addTri = [&](const V& p0, const V& p1, const V& p2, bool flip) {
        const uint32_t base = static_cast<uint32_t>(outPos.size());
        outPos.push_back(p0);
        if (flip) {
            outPos.push_back(p2);
            outPos.push_back(p1);
        } else {
            outPos.push_back(p1);
            outPos.push_back(p2);
        }
        Face f;
        f.indices = {base, base + 1, base + 2};
        outFaces.push_back(std::move(f));
    };

    const auto& pA = cutR.a.attributes().positions();
    const auto& pB = cutR.b.attributes().positions();

    // A' sub-triangles, classified against B.
    for (size_t f = 0; f < cutR.a.topology().faceCount(); ++f) {
        const Face& fc = cutR.a.topology().face(f);
        if (fc.indices.size() != 3) continue;
        const V p0 = pA[fc.indices[0]], p1 = pA[fc.indices[1]], p2 = pA[fc.indices[2]];
        const V centroid{(p0.x + p1.x + p2.x) / 3.f, (p0.y + p1.y + p2.y) / 3.f, (p0.z + p1.z + p2.z) / 3.f};
        const bool inB = pointInside(centroid, soupB);
        bool keep = false, flip = false;
        switch (op) {
            case BooleanOperationType::Union:        keep = !inB; break;
            case BooleanOperationType::Intersection: keep = inB;  break;
            case BooleanOperationType::Difference:   keep = !inB; break;  // A outside B
        }
        if (keep) addTri(p0, p1, p2, flip);
    }

    // B' sub-triangles, classified against A.
    for (size_t f = 0; f < cutR.b.topology().faceCount(); ++f) {
        const Face& fc = cutR.b.topology().face(f);
        if (fc.indices.size() != 3) continue;
        const V p0 = pB[fc.indices[0]], p1 = pB[fc.indices[1]], p2 = pB[fc.indices[2]];
        const V centroid{(p0.x + p1.x + p2.x) / 3.f, (p0.y + p1.y + p2.y) / 3.f, (p0.z + p1.z + p2.z) / 3.f};
        const bool inA = pointInside(centroid, soupA);
        bool keep = false, flip = false;
        switch (op) {
            case BooleanOperationType::Union:        keep = !inA; break;
            case BooleanOperationType::Intersection: keep = inA;  break;
            case BooleanOperationType::Difference:   keep = inA;  flip = true; break;  // B inside A, inward-facing
        }
        if (keep) addTri(p0, p1, p2, flip);
    }

    Mesh out;
    for (const V& p : outPos) {
        if (!isFiniteF(p.x) || !isFiniteF(p.y) || !isFiniteF(p.z)) {
            return Mesh{};  // guard against non-finite payloads
        }
    }
    out.attributes().setPositions(std::move(outPos));
    for (Face& f : outFaces) {
        out.topology().addFace(std::move(f));
    }
    (void)out.weldCoincidentVertices(1e-5f);  // stitch the shared seam
    (void)out.computeVertexNormals();
    return out;
}

}  // namespace nexus::geometry
