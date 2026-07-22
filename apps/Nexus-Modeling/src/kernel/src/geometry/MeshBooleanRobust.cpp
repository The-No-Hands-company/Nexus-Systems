#include <nexus/geometry/MeshBooleanRobust.h>

#include <nexus/geometry/AnalyticBRep.h>  // brep::segmentCrossesTriangleSoS (exact ray parity)
#include <nexus/geometry/MeshCut.h>
#include <nexus/geometry/Tolerance.h>  // scale-aware weld tolerance (Phase T migration)

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
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

// Squared distance from p to triangle abc, and the triangle's (unnormalized) normal
// (Ericson, "Real-Time Collision Detection" — closest point on a triangle by region).
float pointTriangleDist2(const V& p, const V& a, const V& b, const V& c, V& normalOut) noexcept
{
    const V ab{b.x - a.x, b.y - a.y, b.z - a.z};
    const V ac{c.x - a.x, c.y - a.y, c.z - a.z};
    const V ap{p.x - a.x, p.y - a.y, p.z - a.z};
    normalOut = {ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x};

    const float d1 = ab.x * ap.x + ab.y * ap.y + ab.z * ap.z;
    const float d2 = ac.x * ap.x + ac.y * ap.y + ac.z * ap.z;
    V q;
    if (d1 <= 0.f && d2 <= 0.f) {
        q = a;
    } else {
        const V bp{p.x - b.x, p.y - b.y, p.z - b.z};
        const float d3 = ab.x * bp.x + ab.y * bp.y + ab.z * bp.z;
        const float d4 = ac.x * bp.x + ac.y * bp.y + ac.z * bp.z;
        const V cp{p.x - c.x, p.y - c.y, p.z - c.z};
        const float d5 = ab.x * cp.x + ab.y * cp.y + ab.z * cp.z;
        const float d6 = ac.x * cp.x + ac.y * cp.y + ac.z * cp.z;
        const float vc = d1 * d4 - d3 * d2;
        const float vb = d5 * d2 - d1 * d6;
        const float va = d3 * d6 - d5 * d4;
        if (d3 >= 0.f && d4 <= d3) {
            q = b;
        } else if (d6 >= 0.f && d5 <= d6) {
            q = c;
        } else if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
            const float t = d1 / (d1 - d3);
            q = {a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
        } else if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
            const float t = d2 / (d2 - d6);
            q = {a.x + ac.x * t, a.y + ac.y * t, a.z + ac.z * t};
        } else if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
            const float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            q = {b.x + (c.x - b.x) * t, b.y + (c.y - b.y) * t, b.z + (c.z - b.z) * t};
        } else {
            const float denom = 1.f / (va + vb + vc);
            const float v = vb * denom, w = vc * denom;
            q = {a.x + ab.x * v + ac.x * w, a.y + ab.y * v + ac.y * w, a.z + ab.z * v + ac.z * w};
        }
    }
    const float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
    return dx * dx + dy * dy + dz * dz;
}

// Nearest point on a soup's surface: squared distance, plus that triangle's outward
// normal (soups come from closed meshes with consistent outward winding).
float nearestSurface(const V& p, const Soup& s, V& normalOut) noexcept
{
    float best = std::numeric_limits<float>::max();
    normalOut = {0.f, 0.f, 0.f};
    for (const auto& t : s.tris) {
        V n;
        const float d2 = pointTriangleDist2(p, s.pos[t[0]], s.pos[t[1]], s.pos[t[2]], n);
        if (d2 < best) {
            best = d2;
            normalOut = n;
        }
    }
    return best;
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

    // Coordinate span of the operands. Used for the seam weld below, and as the magnitude
    // reference for the float-noise floor of the classification probe.
    float lo = 0.f, hi = 0.f;
    for (const Soup* sp : {&soupA, &soupB}) {
        for (const V& q : sp->pos) {
            lo = std::min({lo, q.x, q.y, q.z});
            hi = std::max({hi, q.x, q.y, q.z});
        }
    }

    // Face classification with COINCIDENT-face resolution — the mesh analogue of the
    // B-rep boolean's selectFace.
    //
    // This used to offset the centroid along the normal by eps and ask `pointInside` on
    // both sides, using the two answers both to classify the face and to detect
    // coincidence. That made a COMBINATORIAL decision depend on choosing eps correctly,
    // which is impossible in general: eps must clear float noise yet stay nearer than the
    // closest other sheet of surface, and on a finely-tessellated model the seam slivers
    // are smaller than the noise floor itself, so no value satisfies both. Measured on
    // sphere-vs-box, the leak rate rose with tessellation density (12% at 6x10 to 67% at
    // 32x36) purely from that misclassification.
    //
    // The two questions are now answered separately, along the kernel's governing split —
    // combinatorial decisions exact, metric quantities tolerant:
    //
    //   * "Which side is this face's material on?" is COMBINATORIAL, and is answered by
    //     the exact Simulation-of-Simplicity ray parity at the centroid itself. No offset:
    //     SoS resolves a centroid lying exactly on the other surface consistently.
    //   * "Is this face coincident with the other surface?" is METRIC, and is answered by
    //     the distance from the centroid to the other surface against a scale-aware
    //     tolerance — a genuine measurement, where being tolerant is correct.
    //   * A sub-triangle's OUTWARD direction comes from the nearest original triangle of
    //     its own solid, which carries the input's consistent outward winding. That
    //     replaces the previous self-test probe, which had the same unsatisfiable eps.
    const float coincidenceTol = Tolerance{}.at(hi - lo);
    const float coincidenceTol2 = coincidenceTol * coincidenceTol;

    auto classify = [&](const V& p0, const V& p1, const V& p2, const Soup& self, const Soup& other,
                        bool isA, bool& keep, bool& flip) {
        keep = false;
        flip = false;
        const V c{(p0.x + p1.x + p2.x) / 3.f, (p0.y + p1.y + p2.y) / 3.f, (p0.z + p1.z + p2.z) / 3.f};
        V n{(p1.y - p0.y) * (p2.z - p0.z) - (p1.z - p0.z) * (p2.y - p0.y),
            (p1.z - p0.z) * (p2.x - p0.x) - (p1.x - p0.x) * (p2.z - p0.z),
            (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x)};
        const float nl = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (nl < 1e-20f) return;  // degenerate sub-triangle contributes nothing

        // TriangleRetriangulate does not guarantee winding, so orient this sub-triangle
        // outward using the original surface it was cut from.
        V selfNormal{};
        (void)nearestSurface(c, self, selfNormal);
        const float selfDot = n.x * selfNormal.x + n.y * selfNormal.y + n.z * selfNormal.z;
        if (selfDot < 0.f) n = {-n.x, -n.y, -n.z};

        V otherNormal{};
        const float d2 = nearestSurface(c, other, otherNormal);
        // Coincidence needs BOTH proximity and parallelism. Distance alone is not enough:
        // after a cut, every seam face has its centroid close to the other surface — that
        // is what a seam IS — so a distance-only test misreads ordinary seam slivers as
        // coplanar overlaps and keeps or drops them by the wrong rule. A genuinely
        // coincident face also lies FLAT against the other surface.
        const float onl = std::sqrt(otherNormal.x * otherNormal.x + otherNormal.y * otherNormal.y
                                    + otherNormal.z * otherNormal.z);
        const float dot = (nl > 0.f && onl > 0.f)
                              ? (n.x * otherNormal.x + n.y * otherNormal.y + n.z * otherNormal.z)
                                    / (nl * onl)
                              : 0.f;
        constexpr float kParallelCos = 0.99f;  // within ~8 degrees of flat
        if (d2 <= coincidenceTol2 && std::abs(dot) >= kParallelCos) {
            // Coincident: the two surfaces overlap here. Which way the other solid's
            // material lies is decided by comparing outward normals, not by probing.
            const bool sameNormal = dot > 0.f;
            if (!isA) return;  // drop every B-side coincident face (A's copy represents the pair)
            switch (op) {
                case BooleanOperationType::Union:
                case BooleanOperationType::Intersection: keep = sameNormal; break;
                case BooleanOperationType::Difference:   keep = !sameNormal; break;
            }
            return;
        }

        // Non-coincident: the centroid is strictly off the other surface, so the exact
        // parity test at the centroid decides the region outright.
        const bool inside = pointInside(c, other);
        switch (op) {
            case BooleanOperationType::Union:        keep = !inside; break;
            case BooleanOperationType::Intersection: keep = inside;  break;
            case BooleanOperationType::Difference:   keep = isA ? !inside : inside; flip = !isA; break;
        }
    };

    // A' sub-triangles, classified against B.
    for (size_t f = 0; f < cutR.a.topology().faceCount(); ++f) {
        const Face& fc = cutR.a.topology().face(f);
        if (fc.indices.size() != 3) continue;
        const V p0 = pA[fc.indices[0]], p1 = pA[fc.indices[1]], p2 = pA[fc.indices[2]];
        bool keep = false, flip = false;
        classify(p0, p1, p2, soupA, soupB, /*isA=*/true, keep, flip);
        if (keep) addTri(p0, p1, p2, flip);
    }

    // B' sub-triangles, classified against A.
    for (size_t f = 0; f < cutR.b.topology().faceCount(); ++f) {
        const Face& fc = cutR.b.topology().face(f);
        if (fc.indices.size() != 3) continue;
        const V p0 = pB[fc.indices[0]], p1 = pB[fc.indices[1]], p2 = pB[fc.indices[2]];
        bool keep = false, flip = false;
        classify(p0, p1, p2, soupB, soupA, /*isA=*/false, keep, flip);
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
    // Stitch the shared seam with a SCALE-AWARE weld tolerance. A fixed 1e-5 fails to
    // merge coincident seam vertices on large models: at coordinate magnitude M the two
    // independently-computed copies of a seam point differ by ~M·1e-6 (float precision),
    // so a 1e-5 weld leaves boundary loops (a large-scale-only leak). Tolerance{}.at(L)
    // = max(1e-5, 1e-6·L) keeps the unit-scale floor EXACTLY (1e-5 until L>10) and grows
    // in proportion for large models. L is the coordinate span already computed above.
    (void)out.weldCoincidentVertices(Tolerance{}.at(hi - lo));
    (void)out.computeVertexNormals();
    return out;
}

}  // namespace nexus::geometry
