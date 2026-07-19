#include <nexus/geometry/MeshBooleanRobust.h>

#include <nexus/geometry/AnalyticBRep.h>  // brep::segmentCrossesTriangleSoS (exact ray parity)
#include <nexus/geometry/MeshCut.h>

#include <algorithm>
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

    // Scale-aware probe offset for the along-normal classification. Big enough to
    // clear float noise off a face, far smaller than the nearest parallel feature.
    float lo = 0.f, hi = 0.f;
    for (const Soup* sp : {&soupA, &soupB}) {
        for (const V& q : sp->pos) {
            lo = std::min({lo, q.x, q.y, q.z});
            hi = std::max({hi, q.x, q.y, q.z});
        }
    }
    const float eps = std::max((hi - lo) * 1e-4f, 1e-4f);

    // Along-normal face classification with COINCIDENT-face resolution — the mesh
    // analogue of the B-rep boolean's selectFace (which resolves faces that share a
    // plane with the other solid). For a sub-triangle with outward normal n and
    // centroid c, the region just OUTSIDE its own solid is c+eps·n; probing both
    // sides against `other` (exact pointInside) both classifies the face AND detects
    // coincidence (the two sides fall in different in/out states iff `other`'s
    // boundary passes through c parallel to n). Rule table matches the B-rep boolean:
    // coincident SAME-normal → keep on the A side only (drop B's duplicate); coincident
    // OPPOSITE-normal → kept only for Difference; otherwise the plain region test.
    auto classify = [&](const V& p0, const V& p1, const V& p2, const Soup& other, bool isA,
                        bool& keep, bool& flip) {
        keep = false;
        flip = false;
        const V c{(p0.x + p1.x + p2.x) / 3.f, (p0.y + p1.y + p2.y) / 3.f, (p0.z + p1.z + p2.z) / 3.f};
        V n{(p1.y - p0.y) * (p2.z - p0.z) - (p1.z - p0.z) * (p2.y - p0.y),
            (p1.z - p0.z) * (p2.x - p0.x) - (p1.x - p0.x) * (p2.z - p0.z),
            (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x)};
        const float nl = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (nl < 1e-20f) return;  // degenerate sub-triangle contributes nothing
        n = {n.x / nl * eps, n.y / nl * eps, n.z / nl * eps};
        const bool inFront = pointInside({c.x + n.x, c.y + n.y, c.z + n.z}, other);  // just outside self
        const bool inBehind = pointInside({c.x - n.x, c.y - n.y, c.z - n.z}, other); // just inside self
        if (inFront != inBehind) {
            // Coincident: `other`'s boundary passes through c parallel to n.
            const bool sameNormal = inBehind && !inFront;  // other's material on self's inside
            if (!isA) return;  // drop every B-side coincident face (A's copy represents the pair)
            switch (op) {
                case BooleanOperationType::Union:
                case BooleanOperationType::Intersection: keep = sameNormal; break;
                case BooleanOperationType::Difference:   keep = !sameNormal; break;
            }
            return;
        }
        // Non-coincident: classify by the region just outside self.
        switch (op) {
            case BooleanOperationType::Union:        keep = !inFront; break;
            case BooleanOperationType::Intersection: keep = inFront;  break;
            case BooleanOperationType::Difference:   keep = isA ? !inFront : inFront; flip = !isA; break;
        }
    };

    // A' sub-triangles, classified against B.
    for (size_t f = 0; f < cutR.a.topology().faceCount(); ++f) {
        const Face& fc = cutR.a.topology().face(f);
        if (fc.indices.size() != 3) continue;
        const V p0 = pA[fc.indices[0]], p1 = pA[fc.indices[1]], p2 = pA[fc.indices[2]];
        bool keep = false, flip = false;
        classify(p0, p1, p2, soupB, /*isA=*/true, keep, flip);
        if (keep) addTri(p0, p1, p2, flip);
    }

    // B' sub-triangles, classified against A.
    for (size_t f = 0; f < cutR.b.topology().faceCount(); ++f) {
        const Face& fc = cutR.b.topology().face(f);
        if (fc.indices.size() != 3) continue;
        const V p0 = pB[fc.indices[0]], p1 = pB[fc.indices[1]], p2 = pB[fc.indices[2]];
        bool keep = false, flip = false;
        classify(p0, p1, p2, soupA, /*isA=*/false, keep, flip);
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
