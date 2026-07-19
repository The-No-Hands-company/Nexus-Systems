#include <nexus/geometry/MeshBooleanRobust.h>

#include <nexus/geometry/AnalyticBRep.h>  // brep::segmentCrossesTriangleSoS (exact ray parity)
#include <nexus/geometry/MeshCut.h>

#include <array>
#include <bit>
#include <cmath>
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

// EXACT point-in-mesh classification via Simulation-of-Simplicity ray parity.
//
// The previous float Möller-Trumbore ray-cast (fixed off-axis dir + 1e-7 band)
// was a NON-exact combinatorial decision: a centroid lying near the other surface
// — pervasive after cutting, since sub-triangle centroids sit right against the
// seam — could flip the crossing count, misclassifying a sub-triangle and opening
// a boundary. This shoots a segment from p to a far endpoint beyond the mesh and
// counts crossings with brep::segmentCrossesTriangleSoS, whose SoS perturbation
// makes EVERY orient3D test non-zero and counts each shared triangle edge exactly
// once (watertight parity). The decision is now exact and direction-independent —
// the same p is resolved consistently against every triangle. Odd count → inside.
bool pointInside(const V& p, const Soup& s) noexcept
{
    if (s.tris.empty()) return false;
    // Far endpoint beyond every mesh vertex along a fixed generic direction; SoS
    // makes the parity independent of the direction, so any non-axis dir works.
    float maxD2 = 1.f;
    for (const V& q : s.pos) {
        const float dx = q.x - p.x, dy = q.y - p.y, dz = q.z - p.z;
        const float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 > maxD2) maxD2 = d2;
    }
    const float reach = 2.f * std::sqrt(maxD2) + 1.f;
    constexpr float dl = 0.4680f, dm = 0.6301f, dn = 0.6201f;  // generic, |·|≈1
    const float inv = reach / std::sqrt(dl * dl + dm * dm + dn * dn);
    const V B{p.x + dl * inv, p.y + dm * inv, p.z + dn * inv};
    int crossings = 0;
    for (const auto& t : s.tris) {
        if (brep::segmentCrossesTriangleSoS(p, B, s.pos[t[0]], s.pos[t[1]], s.pos[t[2]]))
            ++crossings;
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
